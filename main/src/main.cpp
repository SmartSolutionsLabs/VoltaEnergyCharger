#define MBEDTLS_ALLOW_PRIVATE_ACCESS 

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h" // 🔥 Librería para el generador aleatorio por hardware
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "nvs_flash.h"
#include "nvs.h"

// Librerías de proyecto
#include "ModbusSlave.hpp"
#include "SlaveParameters.hpp"
#include "MCP23017.hpp"

// Criptografía y eFuse
#include "esp_hmac.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"
#include "mbedtls/md.h"

static const char* TAG = "VOLTA_MAIN";
static const char* IDENTIDAD_EQUIPO = "VOLTA_CHARGER_001";
static const uint8_t HMAC_MESSAGE[] = "Volta_Cargador_Secreto_V1"; 

static uint8_t LLAVE_MAESTRA_PRUEBA[32] = { 
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38 
};

// --- VARIABLES DE TRANSACCIÓN ---
static uint32_t otp_esperado_actual = 0; // 🔥 OTP aleatorio guardado en memoria
static uint16_t minutos_autorizados = 0; 
static bool llave_publica_lista = false;
static uint8_t llave_publica_interna[32] = {0}; 

// --- HARDWARE ---
#define MCP_ENABLE_PIN GPIO_NUM_15
#define MCP_EXTRA_PIN  GPIO_NUM_41
#define I2C_SDA_PIN    GPIO_NUM_5
#define I2C_SCL_PIN    GPIO_NUM_4
#define MCP_I2C_ADDR   0x27

input_reg_params_t   my_inputs = {}; 
holding_reg_params_t my_holdings = {};
MCP23017* MCP = nullptr;
bool puerto_ocupado[6] = {false};

struct ChargeOrder { uint16_t puerto_id; uint16_t minutos; };

// --- DERIVACIÓN DE LLAVE PRIVADA (Bypass Simulación) ---
esp_err_t derivar_llave_privada(uint8_t* output_key) {
#ifdef CONFIG_EFUSE_VIRTUAL
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, LLAVE_MAESTRA_PRUEBA, 32);
    mbedtls_md_hmac_update(&ctx, HMAC_MESSAGE, sizeof(HMAC_MESSAGE) - 1);
    mbedtls_md_hmac_finish(&ctx, output_key);
    mbedtls_md_free(&ctx);
    return ESP_OK;
#else
    return esp_hmac_calculate(HMAC_KEY0, HMAC_MESSAGE, sizeof(HMAC_MESSAGE) - 1, output_key);
#endif
}

// --- FIRMA ECDSA ---
bool generar_firma_y_extraer_publica(uint16_t terminal, uint16_t minutos, uint16_t precio, uint8_t* out_signature) {
    uint8_t llave_privada[32]; 
    if (derivar_llave_privada(llave_privada) != ESP_OK) return false;

    mbedtls_ecdsa_context ecdsa;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ecdsa_init(&ecdsa);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"v_rng", 5);

    mbedtls_ecp_group_load(&ecdsa.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    mbedtls_mpi_read_binary(&ecdsa.MBEDTLS_PRIVATE(d), llave_privada, 32);
    mbedtls_ecp_mul(&ecdsa.MBEDTLS_PRIVATE(grp), &ecdsa.MBEDTLS_PRIVATE(Q), &ecdsa.MBEDTLS_PRIVATE(d), &ecdsa.MBEDTLS_PRIVATE(grp).G, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_mpi_write_binary(&ecdsa.MBEDTLS_PRIVATE(Q).MBEDTLS_PRIVATE(X), llave_publica_interna, 32);
    llave_publica_lista = true;

    uint8_t hash[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, (const unsigned char*)IDENTIDAD_EQUIPO, strlen(IDENTIDAD_EQUIPO));
    uint16_t datos[3] = {terminal, minutos, precio};
    mbedtls_sha256_update(&sha, (const unsigned char*)datos, sizeof(datos));
    mbedtls_sha256_finish(&sha, hash);
    mbedtls_sha256_free(&sha);

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);
    int ret = mbedtls_ecdsa_sign(&ecdsa.MBEDTLS_PRIVATE(grp), &r, &s, &ecdsa.MBEDTLS_PRIVATE(d), hash, 32, mbedtls_ctr_drbg_random, &ctr_drbg);
    
    if (ret == 0) {
        mbedtls_mpi_write_binary(&r, out_signature, 32);
        mbedtls_mpi_write_binary(&s, out_signature + 32, 32);
    }

    mbedtls_ecdsa_free(&ecdsa); mbedtls_ctr_drbg_free(&ctr_drbg); mbedtls_entropy_free(&entropy);
    return (ret == 0);
}

// --- TAREA CONTROL CARGA ---
void task_control_carga(void *pvParameters) {
    ChargeOrder *order = (ChargeOrder *)pvParameters;
    uint16_t puerto_real = order->puerto_id;
    uint16_t idx = puerto_real - 1;
    uint32_t m = order->minutos;

    puerto_ocupado[idx] = true;
    MCP->digital_write(idx, 1);
    ESP_LOGI("HARDWARE", "Puerto %d ENCENDIDO por %lu min.", puerto_real, m);

    vTaskDelay(pdMS_TO_TICKS(m * 1000));

    MCP->digital_write(idx, 0); 
    puerto_ocupado[idx] = false;
    ESP_LOGW("HARDWARE", "Puerto %d APAGADO.", puerto_real);

    delete order;
    vTaskDelete(NULL);
}

// --- HARDWARE INIT ---
void initialize_all_hw() {
    gpio_config_t io = { .pin_bit_mask = (1ULL<<MCP_ENABLE_PIN)|(1ULL<<MCP_EXTRA_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    gpio_set_level(MCP_ENABLE_PIN, 1);
    gpio_set_level(MCP_EXTRA_PIN, 1);

    i2c_config_t i2c = { .mode = I2C_MODE_MASTER, .sda_io_num = I2C_SDA_PIN, .scl_io_num = I2C_SCL_PIN, .sda_pullup_en = 1, .scl_pullup_en = 1, .master = {.clk_speed = 400000} };
    i2c_param_config(I2C_NUM_0, &i2c);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    MCP = new MCP23017(I2C_NUM_0, MCP_I2C_ADDR);
    if (MCP->begin()) {
        for(int i=0; i<8; i++) MCP->pin_mode(i, 0); 
        MCP->write_port_a(0x00);
    }
}

extern "C" void app_main() {
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    initialize_all_hw();

    // Configuración Modbus Slave (UART 48/38)
    ModbusSlave slave(0x01);
    slave.init(48, 38, 115200);
    slave.setup_reg_maps(&my_inputs, &my_holdings);
    mbc_slave_start(slave.getContext());

    ESP_LOGI(TAG, "Cargador Volta Online (OTP Aleatorio HWRNG).");

    while(1) {
        mb_event_group_t ev = mbc_slave_check_event(slave.getContext(), MB_EVENT_HOLDING_REG_WR);
        
        if (ev & MB_EVENT_HOLDING_REG_WR) {
            
            // CASO A: Firma QR y Generación de OTP Random
            if (my_holdings.req_minutes > 0 && my_holdings.user_pin == 0) {
                uint16_t p = my_holdings.terminal_id;
                if (p >= 1 && p <= 6) {
                    minutos_autorizados = my_holdings.req_minutes;
                    uint16_t precio = minutos_autorizados * 100;
                    
                    if (generar_firma_y_extraer_publica(p, minutos_autorizados, precio, my_inputs.signature)) {
                        my_inputs.terminal_id = p;
                        my_inputs.price = precio;

                        // 🔥 GENERACIÓN ALEATORIA POR HARDWARE (6 DÍGITOS)
                        otp_esperado_actual = esp_random() % 1000000;

                        printf("\n****************************************************\n");
                        printf("🎲 NUEVO OTP ALEATORIO GENERADO (HWRNG)\n");
                        printf("   Para el puerto %d, ingresa este código:\n", p);
                        printf("   👉 OTP: %06lu\n", otp_esperado_actual);
                        printf("****************************************************\n");
                    }
                }
                my_holdings.req_minutes = 0;
            }

            // CASO B: Activación por validación del OTP guardado
            if (my_holdings.user_pin != 0) {
                if (my_holdings.user_pin == otp_esperado_actual) {
                    uint16_t p = my_holdings.terminal_id;
                    uint16_t idx = p - 1;

                    if (p >= 1 && p <= 6 && !puerto_ocupado[idx] && minutos_autorizados > 0) {
                        ESP_LOGI(TAG, "✅ OTP ALEATORIO CORRECTO. Iniciando carga.");
                        
                        ChargeOrder *o = new ChargeOrder{p, minutos_autorizados};
                        xTaskCreate(task_control_carga, "chg", 4096, o, 5, NULL);
                        
                        // Actualizar mapa bits status
                        my_inputs.status = 0;
                        for(int i=0; i<6; i++) if(puerto_ocupado[i]) my_inputs.status |= (1 << i);
                        
                        minutos_autorizados = 0;
                        otp_esperado_actual = 0; // Limpiar para que no se use de nuevo
                    }
                } else {
                    ESP_LOGE(TAG, "❌ OTP INCORRECTO. Recibido: %lu | Esperado: %lu", my_holdings.user_pin, otp_esperado_actual);
                }
                my_holdings.user_pin = 0;
                my_holdings.req_minutes = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#define MBEDTLS_ALLOW_PRIVATE_ACCESS 
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

#include "SlaveParameters.hpp"
#include "ModbusSlave.hpp"
#include "MCP23017.hpp"

// Criptografía
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "ChargePoint.hpp"

static const char* TAG = "VOLTA_MAIN";
#define NUM_TERMINALES 8

// --- HARDWARE ---
#define MCP_ENABLE_PIN GPIO_NUM_15
#define MCP_EXTRA_PIN  GPIO_NUM_41

// --- INSTANCIAS DE MEMORIA ---
holding_terminal_price_request_t        mb_hld_terminal_price_request;        // 0x0000
input_terminal_status_response_t        mb_in_terminal_status_response;        // 0x0000
input_terminal_price_response_t         mb_in_terminal_price_response;         // 0x0100
holding_terminal_user_pin_request_t     mb_hld_terminal_user_pin_request;      // 0x0100

input_terminal_valid_pin_response_t     mb_in_terminal_valid_pin_response;     // 0x0200
input_charge_point_status_response_t    mb_in_charge_point_status_response;    // 0x0300
input_attributes_response_t             mb_in_attributes_response;             // 0x0400 

static const char* IDENTIDAD = "VOLTA_CHG_001";
static uint8_t LLAVE_MAESTRA[32] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38 };


uint32_t otps_esperados[NUM_TERMINALES] = {0};
uint16_t minutos_por_puerto[NUM_TERMINALES] = {0};
MCP23017* MCP = nullptr;

// Función de Firma Real (Sin simulaciones)
bool generar_firma_real(uint16_t id, uint16_t min, uint32_t price, uint8_t* sig_out) {
    mbedtls_ecdsa_context ecdsa;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ecdsa_init(&ecdsa);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"v_rng", 5);
    
    mbedtls_ecp_group_load(&ecdsa.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    mbedtls_mpi_read_binary(&ecdsa.MBEDTLS_PRIVATE(d), LLAVE_MAESTRA, 32);
    
    uint8_t hash[32];
    mbedtls_sha256((const unsigned char*)IDENTIDAD, strlen(IDENTIDAD), hash, 0);

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);
    int ret = mbedtls_ecdsa_sign(&ecdsa.MBEDTLS_PRIVATE(grp), &r, &s, &ecdsa.MBEDTLS_PRIVATE(d), hash, 32, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret == 0) {
        mbedtls_mpi_write_binary(&r, sig_out, 32);
        mbedtls_mpi_write_binary(&s, sig_out + 32, 32);
    }
    mbedtls_ecdsa_free(&ecdsa); mbedtls_ctr_drbg_free(&ctr_drbg); mbedtls_entropy_free(&entropy);
    return (ret == 0);
}

struct SignatureTaskParams {
    uint16_t id;
    uint32_t price;
    uint16_t req_minutes;
};

void task_process_signature(void *pvParameters) {
    SignatureTaskParams* params = (SignatureTaskParams*)pvParameters;
    uint16_t id = params->id;
    uint16_t idx = id - 1;
    uint16_t req_minutes = params->req_minutes;
    uint32_t price = params->price;
    delete params; // Liberamos memoria dinámica

    ESP_LOGI(TAG, "⚙️ [P%d] Generando firma ECDSA en background...", id);

    uint8_t temp_signature[64];
    if (generar_firma_real(id, req_minutes, price, temp_signature)) {
        // En ESP32 esto es seguro si el maestro asume que está PROCESSING hasta ver DONE
        memcpy(mb_in_terminal_price_response.signature, temp_signature, 64);
        mb_in_terminal_price_response.terminal_id = id;
        mb_in_terminal_price_response.price = price;
        
        otps_esperados[idx] = esp_random() % 1000000;
        minutos_por_puerto[idx] = req_minutes;

        ESP_LOGW(TAG, "🔑 OTP GENERADO [P%d]: %06lu", id, otps_esperados[idx]);
        
        // Estado DONE (3) indicando al maestro que ya tiene los datos completos
        mb_in_terminal_status_response.work_status = static_cast<uint16_t>(ChargeWorkMode::DONE); 
        ESP_LOGI(TAG, "✅ [P%d] Firma completada, estado actualizado a DONE", id);
    } else {
        mb_in_terminal_status_response.work_status = static_cast<uint16_t>(ChargeWorkMode::ERROR); // ERROR
        ESP_LOGE(TAG, "❌ [P%d] Error al generar firma", id);
    }
    vTaskDelete(NULL);
}

void task_control_carga(void *pvParameters) {
    uint16_t puerto = (uint16_t)(uintptr_t)pvParameters;
    uint16_t idx = puerto - 1;

    // SACAMOS EL TIEMPO DEL ARRAY, NO DE LA VARIABLE GLOBAL
    uint32_t tiempo_minutos = minutos_por_puerto[idx];

    if (tiempo_minutos == 0) {
        ESP_LOGE(TAG, "❌ [P%d] Error: Tiempo es 0. Abortando.", puerto);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGW(TAG, "🔌 [P%d] RELÉ ON - Tiempo: %lu min", puerto, tiempo_minutos);
    mb_in_charge_point_status_response.ChargePointStatus = static_cast<uint16_t>(ChargePointStatus::OCCUPIED);
    // Cambiamos estado a "CARGANDO" (puedes inventar un código nuevo como 4)

    MCP->digital_write(idx, 1);

    // Delay seguro
    vTaskDelay(pdMS_TO_TICKS(tiempo_minutos * 1000));

    MCP->digital_write(idx, 0);
    
    minutos_por_puerto[idx] = 0; // Limpiamos su tiempo
    
    ESP_LOGW(TAG, "🏁 [P%d] RELÉ OFF - Carga completa", puerto);
    mb_in_charge_point_status_response.ChargePointStatus = static_cast<uint16_t>(ChargePointStatus::AVAILABLE);
    vTaskDelete(NULL);
}

extern "C" void app_main() {
    nvs_flash_init();
    
    // CORRECCIÓN 2: I2C Init con todos los campos
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = 5;
    conf.scl_io_num = 4;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    conf.clk_flags = 0; 

    gpio_config_t io = { .pin_bit_mask = (1ULL<<MCP_ENABLE_PIN)|(1ULL<<MCP_EXTRA_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    gpio_set_level(MCP_ENABLE_PIN, 1);
    gpio_set_level(MCP_EXTRA_PIN, 1);

    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    MCP = new MCP23017(I2C_NUM_0, 0x27);
    if (MCP->begin()) { for(int i=0; i<8; i++) MCP->pin_mode(i, 0); }

    // --- CONFIGURACIÓN MODBUS MODULAR ---
    ModbusSlave slave(0x01);
    slave.init(48, 38, 115200);

    // Registramos cada bloque por separado
    slave.register_area(MB_PARAM_HOLDING, 0x0000, &mb_hld_terminal_price_request       , sizeof(mb_hld_terminal_price_request));
    slave.register_area(MB_PARAM_HOLDING, 0x0100, &mb_hld_terminal_user_pin_request    , sizeof(mb_hld_terminal_user_pin_request));
    slave.register_area(MB_PARAM_INPUT,   0x0000, &mb_in_terminal_status_response      , sizeof(mb_in_terminal_status_response));
    slave.register_area(MB_PARAM_INPUT,   0x0100, &mb_in_terminal_price_response       , sizeof(mb_in_terminal_price_response));
    slave.register_area(MB_PARAM_INPUT,   0x0200, &mb_in_terminal_valid_pin_response   , sizeof(mb_in_terminal_valid_pin_response));
    slave.register_area(MB_PARAM_INPUT,   0x0300, &mb_in_charge_point_status_response  , sizeof(mb_in_charge_point_status_response));
    slave.register_area(MB_PARAM_INPUT,   0x0400, &mb_in_attributes_response           , sizeof(mb_in_attributes_response));
    
    // Construyo los atributos que leera la pantalla
    mb_in_attributes_response.terminals_quantity = NUM_TERMINALES;            // 6 terminales de carga
    mb_in_attributes_response.minute_value = 5;          // 5 centavos = 0.05 Sol
    mb_in_attributes_response.min_charge_time = 15;     // 15 minutos minimo
    mb_in_attributes_response.max_charge_time = 240;    // 240 minutos maximo
    mb_in_attributes_response.step_charge_time = 15;    // 15 minutos por paso
    //
    mb_in_terminal_status_response.terminal_id = 0;
    mb_in_terminal_status_response.work_status = static_cast<uint16_t>(ChargeWorkMode::IDLE);
    mb_in_charge_point_status_response.ChargePointStatus = static_cast<uint16_t>(ChargePointStatus::AVAILABLE);

    mbc_slave_start(slave.getContext());
    mb_event_group_t all_events = (mb_event_group_t)(MB_EVENT_HOLDING_REG_WR | 
                                                 MB_EVENT_INPUT_REG_RD   | 
                                                 MB_EVENT_HOLDING_REG_RD);

    while(1) {
    // 1. Escuchamos todos los eventos (Escritura y Lectura)
    mb_event_group_t ev = mbc_slave_check_event(slave.getContext(), all_events);
    mb_param_info_t reg_info;
    esp_err_t err = mbc_slave_get_param_info(slave.getContext(), &reg_info, 0);

    if (err == ESP_OK) {
        uint16_t addr = reg_info.mb_offset;

        // --- MONITOR DE ESCRITURAS (MAESTRO -> ESP32) ---
        if (ev & MB_EVENT_HOLDING_REG_WR) {
            // Nota: En algunas versiones de ESP-IDF, reg_info.size representa REGISTROS (1 req = 2 bytes)
            // Si vemos que el tamaño es 35 registros (70 bytes), ajustamos la impresión.
            int display_sz = (reg_info.size == 35) ? 70 : (reg_info.size * 2);
            ESP_LOGW(TAG, "📥 [WRITE] Offset: 0x%04X | Size reportado: %d. Imprimiendo %d bytes", addr, (int)reg_info.size, display_sz);
            
            // Volcado Hexadecimal de UNA SOLA LINEA para fácil validación
            char hex_dump[256] = {0}; 
            uint8_t* val_ptr = (uint8_t*)reg_info.address;
            int max_print = display_sz > 120 ? 120 : display_sz; // Límite seguro para el buffer
            for(int i = 0; i < max_print; i++) {
                sprintf(&hex_dump[i * 3], "%02X ", val_ptr[i]);
            }
            ESP_LOGI(TAG, "📦 [RAW HEX DUMP]: %s", hex_dump);

            if (addr < 0x0100) { // PRICE REQUEST (Espera exactamente 2 registros)
                if (reg_info.size != 2) {
                    ESP_LOGW(TAG, "⚠️ Ignorando petición de Precio. Tamaño incorrecto (%d regs), se esperaban 2.", (int)reg_info.size);
                } else {
                    uint16_t id = mb_hld_terminal_price_request.terminal_id;
                    if (id >= 1 && id <= NUM_TERMINALES) {
                        // Seteamos estado inicial
                        mb_in_terminal_status_response.terminal_id = id;
                        mb_in_terminal_status_response.work_status = static_cast<uint16_t>(ChargeWorkMode::PROCESSING); // PROCESSING

                        // Lanzamos la tarea de firma en background (8192 bytes de stack para la criptografía)
                        SignatureTaskParams* params = new SignatureTaskParams{id, 1500, mb_hld_terminal_price_request.req_minutes};
                        xTaskCreate(task_process_signature, "sig_gen", 8192, (void*)params, 5, NULL);

                        mb_hld_terminal_price_request.req_minutes = 0;
                    }
                }
            } 
            else if (addr >= 0x0100) { // PIN VALIDATE (Espera 3 registros: 1 para ID, 2 para el PIN de 32 bits)
                if (reg_info.size != 3) {
                    ESP_LOGW(TAG, "⚠️ Ignorando petición de PIN. Tamaño incorrecto (%d regs), se esperaban 3.", (int)reg_info.size);
                } else {
                    uint16_t id = mb_hld_terminal_user_pin_request.terminal_id;
                    mb_in_terminal_valid_pin_response.terminal_id = id; // Por defecto es 0 (false)

                    mb_in_terminal_status_response.work_status = static_cast<uint16_t>(ChargeWorkMode::PROCESSING); // PROCESSING
                    mb_in_terminal_valid_pin_response.valid_pin = 0; // Por defecto es 0 (false)

                    if (id >= 1 && id <= NUM_TERMINALES) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                        if (mb_hld_terminal_user_pin_request.user_pin == otps_esperados[id-1]) {
                            ESP_LOGI(TAG, "✅ PIN CORRECTO P%d", id);
                            mb_in_terminal_valid_pin_response.valid_pin = 1; // lo hace true
                            mb_in_terminal_status_response.work_status = static_cast<uint16_t>(ChargeWorkMode::DONE); // PROCESSING
                            xTaskCreate(task_control_carga, "chg", 2048, (void*)(uintptr_t)id, 5, NULL);
                        } else {
                            ESP_LOGE(TAG, "❌ PIN INCORRECTO P%d", id);
                            mb_in_terminal_valid_pin_response.valid_pin = 0; // asegura false
                        }
                        mb_hld_terminal_user_pin_request.user_pin = 0;
                    }
                }
            }
        }

        // --- MONITOR DE LECTURAS (ESP32 -> MAESTRO) ---
        // Aquí es donde verás la firma y el precio saliendo por el cable
        if (ev & MB_EVENT_INPUT_REG_RD) {
            int display_sz = (reg_info.size == 35) ? 70 : reg_info.size;
            if (addr < 0x0100) {
                ESP_LOGI(TAG, "📤 [READ] Maestro pidiendo STATUS (Offset: 0x%04X)", addr);
                ESP_LOG_BUFFER_HEX(TAG, reg_info.address, display_sz);
                ESP_LOGW(TAG, "🔍 [DETALLE LEÍDO] Terminal ID: %d, status: %lu", 
                         mb_in_terminal_status_response.terminal_id, 
                         mb_in_terminal_status_response.work_status);
            } 
            else if(addr >= 0x0100 && addr < 0x0200){
                ESP_LOGI(TAG, "📤 [READ] Maestro pidiendo FIRMA/PRECIO (Offset: 0x%04X)", addr);
                ESP_LOG_BUFFER_HEX(TAG, reg_info.address, display_sz);

                
                // Desglose de lo que el ESP32 tiene almacenado en memoria 
                // y está enviando actualmente al maestro como respuesta
                ESP_LOGW(TAG, "🔍 [DETALLE LEÍDO] Terminal ID: %d, Precio: %lu", 
                         mb_in_terminal_price_response.terminal_id, 
                         mb_in_terminal_price_response.price);
                         
                // Construimos la firma en un solo string para sortear cualquier límite de LOG_BUFFER_HEX
                char sig_str[129];
                for(int i = 0; i < 64; i++) {
                    sprintf(&sig_str[i * 2], "%02X", mb_in_terminal_price_response.signature[i]);
                }
                ESP_LOGW(TAG, "🔍 [FIRMA TEXTO SINGLE LINE]: %s", sig_str);
            }
            else if(addr >= 0x0200 && addr < 0x0300){
                ESP_LOGI(TAG, "📤 [READ] Maestro pidiendo si el Pin es Valido (Offset: 0x%04X)", addr);
                ESP_LOG_BUFFER_HEX(TAG, reg_info.address, display_sz);

                
                // Desglose de lo que el ESP32 tiene almacenado en memoria 
                // y está enviando actualmente al maestro como respuesta
                ESP_LOGW(TAG, "🔍 [DETALLE LEÍDO] Terminal ID: %d, Valido: %d", 
                         mb_in_terminal_valid_pin_response.terminal_id, 
                         mb_in_terminal_valid_pin_response.valid_pin);
                
                mb_in_terminal_valid_pin_response.valid_pin = 0; // reset de la validacion del pin (false)
                         
            }
            else if(addr >= 0x0300 && addr < 0x0400){
                ESP_LOGI(TAG, "📤 [READ] Maestro pidiendo Estado Del Punto de Carga (Offset: 0x%04X)", addr);
                ESP_LOG_BUFFER_HEX(TAG, reg_info.address, display_sz);

                
                // Desglose de lo que el ESP32 tiene almacenado en memoria 
                // y está enviando actualmente al maestro como respuesta
                ESP_LOGW(TAG, "🔍 [DETALLE LEÍDO] Terminal ID: %d, Estado del Punto de Carga: %d", 
                         mb_in_charge_point_status_response.terminal_id, 
                         mb_in_charge_point_status_response.ChargePointStatus);
            }
            else if(addr >= 0x0400 && addr < 0x0500){
                ESP_LOGI(TAG, "📤 [READ] Maestro pidiendo atributos (Offset: 0x%04X)", addr);
                ESP_LOG_BUFFER_HEX(TAG, reg_info.address, display_sz);

                
                // Desglose de lo que el ESP32 tiene almacenado en memoria 
                // y está enviando actualmente al maestro como respuesta
                ESP_LOGW(TAG, "🔍 [DETALLE LEÍDO] Terminales: %d, Valor por minuto: %d", 
                         mb_in_attributes_response.terminals_quantity, 
                         mb_in_attributes_response.minute_value);
            }
        }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
}
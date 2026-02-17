#include <stdio.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

// Librerías del Proyecto
#include "ads1115.hpp"
#include "mcp23017.hpp"
#include "wifiManager.hpp"
#include "PortalWeb.hpp"
#include "LoggerFS.hpp"
#include "CommandManager.hpp"
#include "GitHubClient.hpp"

static const char* TAG = "MOTO_CHARGER_MAIN";

// --- GLOBALES Y PERIFÉRICOS ---
// Mutex para el bus I2C compartido
SemaphoreHandle_t g_i2c_mutex = NULL;

// Instancias de hardware
ADS1115* g_ads = nullptr;
MCP23017* g_mcp_1 = nullptr; // Control de Relays (0x20)
MCP23017* g_mcp_2 = nullptr; // Control de SD y otros (0x25)

// Instancia de Logs en SD
LoggerFS g_logger("/sd"); //

// Variables de Estado (externas para PortalWeb y CommandManager)
float g_corriente_actual = 0.0f;
int g_potenciometro_mv = 0;
volatile bool g_scr_enabled = false;
volatile bool g_is_wifi_scanning = false; // Bloquea I2C durante escaneo

// --- CONFIGURACIÓN DE PINES (MCP23017 Relays) ---
#define RELAY_CH1 0
#define RELAY_CH2 1
#define RELAY_CH3 2
#define RELAY_CH4 3


struct ChargePoint {
    int relay_pin;
    uint32_t seconds_left;
    bool active;
};

// Estado de los 4 puntos de carga para motos
ChargePoint moto_points[4] = {
    {0, 0, false}, // Relay CH1
    {1, 0, false}, // Relay CH2
    {2, 0, false}, // Relay CH3
    {3, 0, false}  // Relay CH4
};
/**
 * @brief Tarea de Control de Carga y Temporizadores
 */
void task_charging_control(void* pvParameters) {
    while (1) {
        for (int i = 0; i < 4; i++) {
            if (moto_points[i].active) {
                if (moto_points[i].seconds_left > 0) {
                    moto_points[i].seconds_left--;
                    
                    // Cada 60 segundos registrar en el log
                    if (moto_points[i].seconds_left % 60 == 0) {
                        g_logger.registrarEstructurado(RectEvent::PROCESS_START, 
                            "CH" + std::to_string(i+1), 
                            "Tiempo restante: " + std::to_string(moto_points[i].seconds_left/60) + " min");
                    }
                } else {
                    // Tiempo agotado: Apagar relay
                    moto_points[i].active = false;
                    g_mcp_1->digital_write(moto_points[i].relay_pin, false); // Apagar
                    g_logger.registrarEstructurado(RectEvent::PROCESS_STOP, "CH" + std::to_string(i+1), "Carga finalizada");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Tick de 1 segundo
    }
}

/**
 * @brief Inicialización de Buses y Dispositivos
 */
void setup_hardware() {
    g_i2c_mutex = xSemaphoreCreateMutex();

    // Configuración I2C Master
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_41, // Ajustar a tu hardware
        .scl_io_num = GPIO_NUM_42,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = { .clk_speed = 400000 }
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    // Inicializar ADS1115
    g_ads = new ADS1115(I2C_NUM_0, 0x48, g_i2c_mutex);
    
    // Inicializar MCP23017 para Relays
    g_mcp_1 = new MCP23017(I2C_NUM_0, 0x20);
    if (g_mcp_1->begin()) {
        for(int i=0; i<4; i++) g_mcp_1->pin_mode(i, 0); // Salidas
    }

    // Inicializar MCP23017 para SD (CS/CD)
    g_mcp_2 = new MCP23017(I2C_NUM_0, 0x25);
    g_mcp_2->begin();

    // Montar Tarjeta SD y Logger
    if (g_logger.begin()) {
        g_logger.registrarEstructurado(RectEvent::BOOT, "v1.0.0", "Sistema Iniciado");
    }
}

void task_serial_reader(void* pvParameters) {
    char incoming_data[128];
    while (1) {
        // Leer desde el monitor serial (stdin)
        if (fgets(incoming_data, sizeof(incoming_data), stdin)) {
            std::string command(incoming_data);
            // Procesar el comando (ej: "start.ch1")
            std::string response = CommandManager::execute(command);
            printf("Respuesta: %s\n", response.c_str());
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Iniciando Cargador de Motas VoltaEnergy...");

    // 1. Hardware y Almacenamiento
    setup_hardware();

    // 2. Conectividad WiFi
    WifiManager::init();

    WifiManager::connect_saved();

    // Espera activa para verificar conexión real
    ESP_LOGI(TAG, "Esperando conexión WiFi...");
    vTaskDelay(pdMS_TO_TICKS(10000)); // Aumentamos a 10s para dar margen a los reintentos

    if (!WifiManager::is_connected()) { 
        ESP_LOGW(TAG, "Fallo de conexión detectado. Iniciando Modo AP...");
        WifiManager::start_ap();
    } else {
        ESP_LOGI(TAG, "Conexión confirmada. IP: %s", WifiManager::get_ip().c_str());
    }
    // 3. Servidor Web y Portal
    PortalWeb* portal = new PortalWeb();
    portal->start();

    // 4. Tareas de Sistema
    xTaskCreatePinnedToCore(task_charging_control, "charge_task", 4096, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "Sistema listo. Versión: %s", GitHubClient::get_current_version().c_str());
}
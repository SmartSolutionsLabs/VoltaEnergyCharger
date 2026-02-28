#include <stdio.h>
#include <unistd.h>      // Para fileno()

#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/usb_serial_jtag.h" // Driver de bajo nivel
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_log.h"

// Librerías
#include "CommandManager.hpp" 
#include "mcp23017.hpp"
#include "wifiManager.hpp"
#include "LoggerFS.hpp"

static const char* TAG = "MOTO_CHARGER";
std::string g_current_typing = ""; 
std::string g_last_response = "Esperando..."; // Global para mantener el mensaje
struct ChargePoint {
    int relay_pin;
    uint32_t seconds_left;
    bool active;
};

MCP23017* g_mcp_1 = nullptr;
ChargePoint moto_points[4] = {
    {0, 0, false}, {1, 0, false}, {2, 0, false}, {3, 0, false}
};

class MotoChargeHandler : public Command {
public:
    std::string execute(const std::vector<std::string>& args) override {
        if (args.size() < 3) return "ERROR: Use charge.CH.MIN";
        int ch = std::stoi(args[1]);
        int min = std::stoi(args[2]);

        if (ch >= 1 && ch <= 4) {
            int idx = ch - 1;
            moto_points[idx].seconds_left = min * 60;
            moto_points[idx].active = true;
            if (g_mcp_1) g_mcp_1->digital_write(moto_points[idx].relay_pin, true);
            return "SUCCESS: CH" + std::to_string(ch) + " ACTIVADO";
        }
        return "ERROR: Canal invalido";
    }
};

void update_dashboard(std::string current_input, std::string last_res) {
    // 1. Ocultar cursor y volver a posición inicial
    printf("\033[?25l\033[H"); 

    // 2. Dibujar Canales
    for (int i = 0; i < 4; i++) {
        printf("\033[%d;1H\033[KCH%d : [", i + 1, i + 1);
        if (moto_points[i].active) {
            printf("██████████] %-4lu s", (unsigned long)moto_points[i].seconds_left);
        } else {
            printf("          ] OFF   ");
        }
    }

    // 3. Dibujar CMD y RES
    printf("\033[6;1H\033[KCMD : %s", current_input.c_str());
    printf("\033[7;1H\033[KRES : %s", last_res.c_str());

    // 4. Forzar al driver USB a escupir los datos AHORA
    fflush(stdout);
    tcdrain(fileno(stdout)); // Sincronización extra de bajo nivel
}



void task_control_tiempos(void* p) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while(1) {
        // Usamos vTaskDelayUntil para que sea EXACTAMENTE 1000ms 
        // sin importar cuánto tarde el código interno
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));

        for(int i=0; i<4; i++) {
            if(moto_points[i].active) {
                if(moto_points[i].seconds_left > 0) {
                    moto_points[i].seconds_left--;
                } else {
                    moto_points[i].active = false;
                    if(g_mcp_1) g_mcp_1->digital_write(moto_points[i].relay_pin, false);
                }
            }
        }
        
        update_dashboard(g_current_typing, g_last_response);
    }
}

void task_serial_reader(void* p) {
    uint8_t rx_buf[128];
    std::string line_accumulator = "";

    // Limpiar pantalla completa al inicio
    printf("\033[2J"); 

    while (1) {
        int len = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));
        
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char c = (char)rx_buf[i];

                if (c == '\r' || c == '\n') {
                    if (!line_accumulator.empty()) {
                        // Ejecutar y guardar respuesta
                        g_last_response = CommandManager::run(line_accumulator);
                        line_accumulator = "";
                        g_current_typing = "";
                    }
                } 
                else if (c == 8 || c == 127) { // Borrar
                    if (!line_accumulator.empty()) {
                        line_accumulator.pop_back();
                        g_current_typing = line_accumulator;
                    }
                }
                else if (c >= 32 && c <= 126) { // Escribir
                    line_accumulator += c;
                    g_current_typing = line_accumulator;
                }
                // Repintar inmediatamente al presionar tecla
                update_dashboard(g_current_typing, g_last_response);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void initialize_mcp_hardware_pins() {
    // GPIO 15: RESET del MCP23017
    // GPIO 41: Pin de habilitación extra
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_15) | (1ULL << GPIO_NUM_41),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_config);

    // Sacamos el chip del reset (High)
    gpio_set_level(GPIO_NUM_15, 1);
    gpio_set_level(GPIO_NUM_41, 1);
    
    // Pequeño delay para que el chip estabilice
    vTaskDelay(pdMS_TO_TICKS(10));
}

extern "C" void app_main(void) {
    // 1. Configuración de Consola Interactiva (USB Nativo S3)
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = 256,
        .rx_buffer_size = 256,
    };
    usb_serial_jtag_driver_install(&usb_config);
    setvbuf(stdout, NULL, _IONBF, 0); // Deshabilitar buffering para respuesta instantánea

    // 2. Habilitar Hardware MCP23017 (Reset y Enable)
    initialize_mcp_hardware_pins();

    // 3. Inicializar Bus I2C (Pines del Rectificador: SDA 5, SCL 4)
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_5;
    conf.scl_io_num = GPIO_NUM_4;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    // 4. Inicializar MCP23017 (Dirección 0x27 según tu main.cpp subido)
    // Nota: Si usas el módulo de relés en 0x20, cambia el 0x27 por 0x20.
    g_mcp_1 = new MCP23017(I2C_NUM_0, 0x27); 
    
    if (g_mcp_1->begin()) {
        ESP_LOGI(TAG, "MCP23017 detectado correctamente.");
        // Configurar los primeros 4 pines del Puerto A como salidas para los relés
        for(int i = 0; i < 4; i++) {
            g_mcp_1->pin_mode(i, 0); // 0 = OUTPUT
        }
        // Aseguramos que inicien apagados
        g_mcp_1->write_port_a(0x00); 
    } else {
        ESP_LOGE(TAG, "No se encontró el MCP23017 en la dirección 0x27");
    }

    // 5. Configurar Comandos Polimórficos
    CommandManager::addCommand("charge", std::make_unique<MotoChargeHandler>());

    // 6. Lanzar Tareas en Núcleos Separados
    // Tarea de tiempos (Reloj/Dashboard) en Core 1 (Prioridad Alta)
    xTaskCreatePinnedToCore(task_control_tiempos, "tmr", 4096, NULL, 10, NULL, 1);
    
    // Tarea de consola (CLI) en Core 0 (Prioridad Media)
    xTaskCreatePinnedToCore(task_serial_reader, "cli", 4096, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "Cargador Profesional Online sobre Hardware SCB-RAIDI-8.");
}
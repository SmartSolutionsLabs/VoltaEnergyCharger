#include <stdio.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Headers específicos de esp-modbus v2.x
#include "mbcontroller.h"       
#include "esp_modbus_common.h"  
#include "esp_modbus_slave.h"   

static const char *TAG = "VOLTA_PLC";

// --- CONFIGURACIÓN ---
#define MB_UART_PORT        UART_NUM_0      
#define MB_DEVICE_ADDR      1               
#define MB_UART_BAUD        115200

// Handler de instancia
static void* mbc_slave_handler = NULL;

// Mapa de registros según tus descriptores
typedef struct __attribute__((packed)) {
    uint16_t status;        // 0x0000 (CID 0)
    uint16_t energy;        // 0x0001 (CID 1)
    uint16_t start_charge;  // 0x0002 (CID 2)
} slave_reg_params_t;

slave_reg_params_t slave_data;

static esp_err_t init_services(void) {
    // 1. Inicializar controlador (Requiere que el menuconfig esté bien)
    esp_err_t res = mbc_slave_init(MB_PORT_SERIAL_SLAVE, &mbc_slave_handler);
    if (res != ESP_OK) return res;

    // 2. Configuración de comunicación RTU
    mb_communication_info_t comm_info = {};
    comm_info.ser_opts.port = MB_UART_PORT;
    comm_info.ser_opts.baudrate = MB_UART_BAUD;
    comm_info.ser_opts.parity = MB_PARITY_NONE;
    comm_info.ser_opts.mode = MB_RTU; 
    comm_info.ser_opts.data_bits = UART_DATA_8_BITS;
    comm_info.ser_opts.stop_bits = UART_STOP_BITS_1;

    res = mbc_slave_setup(mbc_slave_handler, &comm_info);
    if (res != ESP_OK) return res;

    // 3. Mapeo de áreas de memoria
    mb_register_area_descriptor_t reg_area;
    
    // Status (0) y Energy (1)
    reg_area.type = MB_PARAM_INPUT;
    reg_area.start_offset = 0x0000;
    reg_area.address = (void*)&slave_data.status;
    reg_area.size = 2 * sizeof(uint16_t);
    mbc_slave_set_descriptor(mbc_slave_handler, reg_area);

    // StartCharge (2)
    reg_area.type = MB_PARAM_HOLDING;
    reg_area.start_offset = 0x0002; 
    reg_area.address = (void*)&slave_data.start_charge;
    reg_area.size = sizeof(uint16_t);
    mbc_slave_set_descriptor(mbc_slave_handler, reg_area);

    return ESP_OK;
}

extern "C" void app_main(void) {
    slave_data.status = 0;
    slave_data.energy = 0;
    slave_data.start_charge = 0;

    if (init_services() != ESP_OK) {
        ESP_LOGE(TAG, "Error: Revisa que activaste el soporte SERIAL en menuconfig");
        return;
    }

    ESP_ERROR_CHECK(mbc_slave_start(mbc_slave_handler));
    ESP_LOGI(TAG, "PLC Esclavo iniciado en UART0");

    while (1) {
        // Chequeo de eventos con nombres v5.5.1
        mb_event_group_t event = mbc_slave_check_event(mbc_slave_handler, 
                        (mb_event_group_t)(MB_EVENT_HOLDING_REG_WR | MB_EVENT_INPUT_REG_RD));

        if (event & MB_EVENT_HOLDING_REG_WR) {
            ESP_LOGI(TAG, "Comando recibido: %u", slave_data.start_charge);
            slave_data.status = (slave_data.start_charge > 0) ? 1 : 0;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
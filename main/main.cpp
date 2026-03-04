#include <stdio.h>
#include "esp_log.h"
#include "mbcontroller.h" 
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PLC_SLAVE";

// --- CONFIGURACIÓN DE HARDWARE ---
#define MB_UART_PORT        UART_NUM_0
#define MB_DEVICE_ADDR      1
#define MB_UART_BAUD        115200
// Nota: En el S3, UART0 suele estar en GPIO 43 (TX) y 44 (RX) por defecto para el USB-Serial interno.
#define MB_UART_TXD         UART_PIN_NO_CHANGE 
#define MB_UART_RXD         UART_PIN_NO_CHANGE

// --- ESTRUCTURA DE REGISTROS ---
// Usamos __attribute__((packed)) para asegurar que no haya "padding" de memoria
typedef struct __attribute__((packed)) {
    uint16_t status;        // Address 0x0000 (Input)
    uint16_t energy;        // Address 0x0001 (Input)
    uint16_t start_charge;  // Address 0x0002 (Holding)
} slave_reg_params_t;

slave_reg_params_t slave_data;

static esp_err_t init_services(void) {
    void* slave_handler = NULL;
    esp_err_t res = mbc_slave_init(MB_PORT_SERIAL_SLAVE, &slave_handler);
    if (res != ESP_OK) return res;

    // Configuración de comunicación limpia para C++
    mb_communication_info_t comm_info = {};
    comm_info.ser_opts.port = MB_UART_PORT;
    comm_info.ser_opts.baudrate = MB_UART_BAUD;
    comm_info.ser_opts.parity = MB_PARITY_NONE;
    comm_info.ser_opts.mode = MB_MODE_RTU;
    comm_info.ser_opts.data_bits = UART_DATA_8_BITS;
    comm_info.ser_opts.stop_bits = UART_STOP_BITS_1;

    res = mbc_slave_setup((void*)&comm_info);
    if (res != ESP_OK) return res;

    // Mapeo Input Registers (Status + Energy)
    mb_register_area_descriptor_t reg_area;
    reg_area.type = MB_PARAM_INPUT;
    reg_area.start_offset = 0x0000;
    reg_area.address = (void*)&slave_data.status;
    reg_area.size = 2 * sizeof(uint16_t); 
    res = mbc_slave_set_descriptor(reg_area);

    // Mapeo Holding Registers (StartCharge)
    reg_area.type = MB_PARAM_HOLDING;
    reg_area.start_offset = 0x0002; 
    reg_area.address = (void*)&slave_data.start_charge;
    reg_area.size = sizeof(uint16_t);
    res = mbc_slave_set_descriptor(reg_area);

    return res;
}

extern "C" void app_main(void) {
    slave_data.status = 0;
    slave_data.energy = 0;
    slave_data.start_charge = 0;

    if (init_services() != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando Modbus");
        return;
    }

    ESP_ERROR_CHECK(mbc_slave_start());
    ESP_LOGI(TAG, "PLC Esclavo iniciado (ID: %d)", MB_DEVICE_ADDR);

    while (1) {
        // Bloquea hasta que ocurra un evento (escritura del maestro)
        mb_event_group_t event = mbc_slave_check_event(
            (mb_event_group_t)(MB_EVENT_HOLDING_REG_WRITTEN | MB_EVENT_INPUT_REG_READ)
        );

        if (event & MB_EVENT_HOLDING_REG_WRITTEN) {
            ESP_LOGI(TAG, "Cambio en StartCharge: %u", slave_data.start_charge);
            slave_data.status = (slave_data.start_charge > 0) ? 1 : 0;
        }

        // Simulación de lectura de energía
        slave_data.energy = (slave_data.energy + 1) % 1000;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
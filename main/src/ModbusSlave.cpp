#include "ModbusSlave.hpp"
#include "esp_log.h"
#include "driver/uart.h"

static const char* TAG = "MB_SLAVE_CLASS";

ModbusSlave::ModbusSlave(uint8_t unitId) : m_unitId(unitId), m_slaveHandle(nullptr) {}

esp_err_t ModbusSlave::init(int txPin, int rxPin, uint32_t baudrate) {
    // 1. Configuración de comunicación EXACTA a tu código
    mb_communication_info_t comm = {};
    comm.ser_opts.port = UART_NUM_1; 
    comm.ser_opts.mode = MB_RTU;           
    comm.ser_opts.baudrate = baudrate;
    comm.ser_opts.parity = MB_PARITY_NONE;
    comm.ser_opts.uid = m_unitId;          
    comm.ser_opts.data_bits = UART_DATA_8_BITS;
    comm.ser_opts.stop_bits = UART_STOP_BITS_1;

    // Crear el esclavo usando la API Moderna
    esp_err_t err = mbc_slave_create_serial(&comm, &m_slaveHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error creando esclavo serial: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Configurar Pines física
    err = uart_set_pin(UART_NUM_1, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    
    ESP_LOGI(TAG, "Modbus Slave Inicializado en ID %d", m_unitId);
    return uart_set_mode(UART_NUM_1, UART_MODE_UART);
}

esp_err_t ModbusSlave::register_area(mb_param_type_t type, uint16_t offset, void* ptr, size_t size) {
    if (m_slaveHandle == nullptr) {
        ESP_LOGE(TAG, "No podés registrar áreas sin antes llamar a init()");
        return ESP_ERR_INVALID_STATE;
    }

    mb_register_area_descriptor_t area = {};
    area.type = type;
    area.start_offset = offset;
    area.address = ptr;
    area.size = size;

    // Importante: mbc_slave_set_descriptor ahora requiere el m_slaveHandle
    esp_err_t err = mbc_slave_set_descriptor(m_slaveHandle, area);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al registrar área en offset 0x%04X: %s", offset, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Área registrada: Tipo %d | Offset 0x%04X | Tamaño %d", type, offset, (int)size);
    }
    return err;
}
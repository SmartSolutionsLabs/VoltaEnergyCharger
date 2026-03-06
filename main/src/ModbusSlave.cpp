#include "ModbusSlave.hpp"
#include "mbcontroller.h"       // Cabecera base del controlador
#include "esp_modbus_slave.h"   // Interfaz de esclavo
#include "esp_modbus_common.h"  // Definiciones comunes (MB_MODE_RTU, etc.)
#include "esp_log.h"

static const char* TAG = "MB_SLAVE_CLASS";

ModbusSlave::ModbusSlave(uint8_t unitId) : m_unitId(unitId), m_slaveHandle(nullptr) {}

esp_err_t ModbusSlave::init(int txPin, int rxPin, uint32_t baudrate) {
    // 1. Configuración de comunicación (¡TU código exacto que funciona!)
    mb_communication_info_t comm = {};
    comm.ser_opts.port = UART_NUM_1; 
    comm.ser_opts.mode = MB_RTU;           // API Moderna
    comm.ser_opts.baudrate = baudrate;
    comm.ser_opts.parity = MB_PARITY_NONE;
    comm.ser_opts.uid = m_unitId;          // UID moderno
    comm.ser_opts.data_bits = UART_DATA_8_BITS;
    comm.ser_opts.stop_bits = UART_STOP_BITS_1;

    // Crear el esclavo y obtener el handle
    esp_err_t err = mbc_slave_create_serial(&comm, &m_slaveHandle);
    if (err != ESP_OK) return err;

    // 2. Configurar Pines (TX: 48, RX: 38)
    err = uart_set_pin(UART_NUM_1, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    
    // Configurar modo UART (sin RS485 físico)
    return uart_set_mode(UART_NUM_1, UART_MODE_UART);
}

// 3. Este es el método que conecta TUS structs de memoria (Ed25519) con el stack Modbus
esp_err_t ModbusSlave::setup_reg_maps(input_reg_params_t* inputs, holding_reg_params_t* holdings) {
    esp_err_t err;
    mb_register_area_descriptor_t reg_area = {};

    // --- Configurar Holding Registers (Donde el maestro escribe comandos/PIN) ---
    reg_area.type = MB_PARAM_HOLDING;
    reg_area.start_offset = 0; 
    reg_area.address = (void*)holdings;
    reg_area.size = sizeof(holding_reg_params_t); 
    
    err = mbc_slave_set_descriptor(m_slaveHandle, reg_area);
    if (err != ESP_OK) return err;

    // --- Configurar Input Registers (Donde pones la FIRMA de 64 bytes) ---
    reg_area.type = MB_PARAM_INPUT;
    reg_area.start_offset = 0; 
    reg_area.address = (void*)inputs;
    reg_area.size = sizeof(input_reg_params_t);
    
    err = mbc_slave_set_descriptor(m_slaveHandle, reg_area);
    if (err != ESP_OK) return err;

    return ESP_OK;
}
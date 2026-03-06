#pragma once
#include "mbcontroller.h"
#include "driver/uart.h"
#include "SlaveParameters.hpp"

class ModbusSlave {
public:
    ModbusSlave(uint8_t unitId);
    
    // Inicializa la UART (38/48) y el controlador Modbus
    esp_err_t init(int txPin, int rxPin, uint32_t baudrate);
    
    // Vincula los structs del main con el stack de Modbus
    esp_err_t setup_reg_maps(input_reg_params_t* inputs, holding_reg_params_t* holdings);
    
    void* getContext() { return m_slaveHandle; }

private:
    uint8_t m_unitId;
    void* m_slaveHandle;
    // Ya no necesitamos m_regs; usamos los punteros pasados en setup_reg_maps
};
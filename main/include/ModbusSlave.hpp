#ifndef MODBUS_SLAVE_HPP
#define MODBUS_SLAVE_HPP

#include "mbcontroller.h"
#include "esp_modbus_slave.h"
#include "SlaveParameters.hpp"

class ModbusSlave {
public:
    ModbusSlave(uint8_t unitId);
    
    // Tu inicialización que funciona
    esp_err_t init(int txPin, int rxPin, uint32_t baudrate);
    
    // Función dinámica para registrar CUALQUIER estructura
    esp_err_t register_area(mb_param_type_t type, uint16_t offset, void* ptr, size_t size);

    void* getContext() { return m_slaveHandle; }

private:
    uint8_t m_unitId;
    void* m_slaveHandle;
};

#endif
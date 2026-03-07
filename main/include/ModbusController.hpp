#ifndef MODBUS_CONTROLLER_HPP
#define MODBUS_CONTROLLER_HPP

#include "ModbusSlave.hpp"
#include "ModbusDataMap.hpp"
#include "IModbusEventHandler.hpp"

class ModbusController {
public:
    ModbusController(uint8_t unitId, IModbusEventHandler* eventHandler);
    ~ModbusController();

    esp_err_t init(int txPin, int rxPin, uint32_t baudrate);
    void start();
    void pollEvents();

    ModbusDataMap& getDataMap();

private:
    ModbusSlave m_slave;
    ModbusDataMap m_dataMap;
    IModbusEventHandler* m_eventHandler;

    void handleWriteEvent(uint16_t address, size_t size, void* addressPtr);
    void handleReadEvent(uint16_t address, size_t size, void* addressPtr);
};

#endif // MODBUS_CONTROLLER_HPP

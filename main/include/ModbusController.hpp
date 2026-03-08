#ifndef MODBUS_CONTROLLER_HPP
#define MODBUS_CONTROLLER_HPP

#include "ModbusSlave.hpp"
#include "ModbusDataMap.hpp"
#include <functional>

using PriceRequestCallback = std::function<void(uint16_t, uint16_t)>;
using PinValidationCallback = std::function<void(uint16_t, uint32_t)>;

class ModbusController {
public:
    ModbusController(uint8_t unitId);
    ~ModbusController();

    esp_err_t init(int txPin, int rxPin, uint32_t baudrate);
    void start();
    void pollEvents();

    void setOnPriceRequest(PriceRequestCallback cb) { m_onPriceRequest = cb; }
    void setOnPinValidation(PinValidationCallback cb) { m_onPinValidation = cb; }

    ModbusDataMap& getDataMap();

private:
    ModbusSlave m_slave;
    ModbusDataMap m_dataMap;
    
    PriceRequestCallback m_onPriceRequest;
    PinValidationCallback m_onPinValidation;

    void handleWriteEvent(uint16_t address, size_t size, void* addressPtr);
    void handleReadEvent(uint16_t address, size_t size, void* addressPtr);
};

#endif // MODBUS_CONTROLLER_HPP

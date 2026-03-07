#ifndef I_MODBUS_EVENT_HANDLER_HPP
#define I_MODBUS_EVENT_HANDLER_HPP

#include <stdint.h>

class IModbusEventHandler {
public:
    virtual ~IModbusEventHandler() = default;
    virtual void onPriceRequest(uint16_t terminalId, uint16_t minutes) = 0;
    virtual void onPinValidationRequest(uint16_t terminalId, uint32_t pin) = 0;
};

#endif // I_MODBUS_EVENT_HANDLER_HPP

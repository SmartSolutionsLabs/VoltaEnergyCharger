#ifndef CHARGE_POINT_DEVICE_HPP
#define CHARGE_POINT_DEVICE_HPP

#include <stdint.h>
#include "ChargePoint.hpp" // Para ChargePointStatus y ChargeWorkMode
#include "MCP23017.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Orquestador de una terminal de carga individual.
 */
class ChargePointDevice {
public:
    ChargePointDevice(uint8_t terminalId, MCP23017* hwExpander);
    ~ChargePointDevice();

    void setExpectedOtp(uint32_t otp);
    bool validateOtp(uint32_t pin);
    void startCharge(uint16_t minutes);
    void stopCharge();

    uint8_t getId() const;
    ChargePointStatus getStatus() const;
    void setStatus(ChargePointStatus status);

private:
    uint8_t m_terminalId; // 1 to 8
    ChargePointStatus m_status;
    MCP23017* m_hwExpander;
    uint32_t m_currentOtp;

    static void taskControlCargaWrapper(void* pvParameters);
    void runChargeCycle(uint16_t minutes);
};

#endif // CHARGE_POINT_DEVICE_HPP

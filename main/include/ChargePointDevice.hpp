#ifndef CHARGE_POINT_DEVICE_HPP
#define CHARGE_POINT_DEVICE_HPP

#include <stdint.h>
#include "ChargePoint.hpp" // Para ChargePointStatus y ChargeWorkMode
#include "MCP23017.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <functional>

/**
 * @brief Orquestador de una terminal de carga individual.
 */
using StatusChangedCallback = std::function<void(uint8_t, ChargePointStatus)>;

class ChargePointDevice {
public:
    ChargePointDevice(uint8_t terminalId, MCP23017* hwExpander);
    ~ChargePointDevice();

    void setStatusCallback(StatusChangedCallback cb) { m_onStatusChanged = cb; }

    void setExpectedOtp(uint32_t otp, uint16_t expectedMinutes);
    bool validateOtp(uint32_t pin);
    uint16_t getExpectedMinutes() const;
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
    uint16_t m_expectedMinutes;
    StatusChangedCallback m_onStatusChanged = nullptr;

    static void taskControlCargaWrapper(void* pvParameters);
    void runChargeCycle(uint16_t minutes);
};

#endif // CHARGE_POINT_DEVICE_HPP

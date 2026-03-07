#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP

#include "ChargePointDevice.hpp"
#include "ModbusController.hpp"
#include "IPaymentProcessor.hpp"
#include "MCP23017.hpp"

#define NUM_TERMINALES 8

class SystemManager : public IModbusEventHandler {
public:
    SystemManager(IPaymentProcessor* paymentProcessor);
    ~SystemManager();

    void init();
    void run();

    // From IModbusEventHandler
    void onPriceRequest(uint16_t terminalId, uint16_t minutes) override;
    void onPinValidationRequest(uint16_t terminalId, uint32_t pin) override;

private:
    IPaymentProcessor* m_paymentProcessor;
    MCP23017* m_mcp;
    ModbusController* m_modbusCtrl;
    ChargePointDevice* m_chargePoints[NUM_TERMINALES];

    void initHardware();
};

#endif // SYSTEM_MANAGER_HPP

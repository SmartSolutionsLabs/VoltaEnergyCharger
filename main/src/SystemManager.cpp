#include "SystemManager.hpp"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"

static const char* TAG = "SystemManager";

#define MCP_ENABLE_PIN GPIO_NUM_15
#define MCP_EXTRA_PIN  GPIO_NUM_41

SystemManager::SystemManager(IPaymentProcessor* paymentProcessor) 
    : m_paymentProcessor(paymentProcessor), m_mcp(nullptr), m_modbusCtrl(nullptr) {
    for (int i = 0; i < NUM_TERMINALES; i++) {
        m_chargePoints[i] = nullptr;
    }
}

SystemManager::~SystemManager() {
    delete m_modbusCtrl;
    for (int i = 0; i < NUM_TERMINALES; i++) {
        delete m_chargePoints[i];
    }
    delete m_mcp;
}

void SystemManager::initHardware() {
    // I2C Init
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = 5;
    conf.scl_io_num = 4;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    conf.clk_flags = 0; 

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << MCP_ENABLE_PIN) | (1ULL << MCP_EXTRA_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&io);
    gpio_set_level(MCP_ENABLE_PIN, 1);
    gpio_set_level(MCP_EXTRA_PIN, 1);

    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    m_mcp = new MCP23017(I2C_NUM_0, 0x27);
    if (m_mcp->begin()) { 
        for(int i=0; i<8; i++) m_mcp->pin_mode(i, 0); 
    }
}

void SystemManager::init() {
    initHardware();

    for (int i = 0; i < NUM_TERMINALES; i++) {
        m_chargePoints[i] = new ChargePointDevice(i + 1, m_mcp);
    }

    m_modbusCtrl = new ModbusController(0x01, this);
    m_modbusCtrl->init(48, 38, 115200);

    // Configuración base de Modbus
    ModbusDataMap& map = m_modbusCtrl->getDataMap();
    map.updateAttributes(NUM_TERMINALES, 15, 240, 15, 5); // 5 centavos
    
    // Set status
    map.status_res.terminal_id = 0;
    map.status_res.work_status = static_cast<uint16_t>(SignatureWorkStatus::IDLE);
    map.cp_status_res.ChargePointStatus = static_cast<uint16_t>(ChargePointStatus::AVAILABLE);

    m_modbusCtrl->start();
}

void SystemManager::run() {
    while(1) {
        m_modbusCtrl->pollEvents();

        // Extraer actualización de status asíncrono
        if (m_paymentProcessor) {
            SignatureWorkStatus p_status = m_paymentProcessor->getWorkStatus();
            if (p_status == SignatureWorkStatus::DONE && 
                m_modbusCtrl->getDataMap().status_res.work_status == static_cast<uint16_t>(SignatureWorkStatus::PROCESSING)) {
                
                m_modbusCtrl->getDataMap().status_res.work_status = static_cast<uint16_t>(p_status);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void SystemManager::onPriceRequest(uint16_t terminalId, uint16_t minutes) {
    if (terminalId < 1 || terminalId > NUM_TERMINALES) return;
    if (!m_paymentProcessor) return;

    ModbusDataMap& map = m_modbusCtrl->getDataMap();
    map.status_res.terminal_id = terminalId;
    map.status_res.work_status = static_cast<uint16_t>(SignatureWorkStatus::PROCESSING);

    // Generate a valid OTP
    uint32_t otp = esp_random() % 1000000;
    ESP_LOGI(TAG, "🔑 [P%d] PIN GENERADO: %06lu", terminalId, otp);
    m_chargePoints[terminalId - 1]->setExpectedOtp(otp);

    // Calculate expected price based on incoming minutes and the configured minute value
    uint32_t calculatedPrice = minutes * map.attr_res.minute_value;
    map.price_res.terminal_id = terminalId;
    map.price_res.price = calculatedPrice;

    m_paymentProcessor->buildSignatureAsync(
        terminalId, 
        minutes, 
        calculatedPrice,
        map.price_res.signature,
        &map.status_res.work_status
    );
}

void SystemManager::onPinValidationRequest(uint16_t terminalId, uint32_t pin) {
    if (terminalId < 1 || terminalId > NUM_TERMINALES) return;

    ModbusDataMap& map = m_modbusCtrl->getDataMap();
    map.pin_res.terminal_id = terminalId;
    map.status_res.work_status = static_cast<uint16_t>(SignatureWorkStatus::PROCESSING);
    map.pin_res.valid_pin = 0;

    vTaskDelay(pdMS_TO_TICKS(100)); // To simulate logic as in main

    bool isValid = m_chargePoints[terminalId - 1]->validateOtp(pin);
    
    if (isValid) {
        ESP_LOGI(TAG, "✅ PIN CORRECTO P%d", terminalId);
        map.pin_res.valid_pin = 1;
        map.status_res.work_status = static_cast<uint16_t>(SignatureWorkStatus::DONE);
        
        // Use 5 minutes as fallback if we don't store requested time, but usually we'd track it.
        // For testing, just hardcode 5 or use a stored value. main.cpp used 5 directly in task creation.
        m_chargePoints[terminalId - 1]->startCharge(5); 

    } else {
        ESP_LOGE(TAG, "❌ PIN INCORRECTO P%d", terminalId);
        map.pin_res.valid_pin = 0;
        map.status_res.work_status = static_cast<uint16_t>(SignatureWorkStatus::DONE);
    }
}

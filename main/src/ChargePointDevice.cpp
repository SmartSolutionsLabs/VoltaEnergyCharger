#include "ChargePointDevice.hpp"
#include "esp_log.h"

static const char* TAG = "ChargePointDevice";

ChargePointDevice::ChargePointDevice(uint8_t terminalId, MCP23017* hwExpander) 
    : m_terminalId(terminalId), m_status(ChargePointStatus::AVAILABLE), 
      m_hwExpander(hwExpander), m_currentOtp(0) {
}

ChargePointDevice::~ChargePointDevice() {
    stopCharge();
}

void ChargePointDevice::setExpectedOtp(uint32_t otp) {
    m_currentOtp = otp;
}

bool ChargePointDevice::validateOtp(uint32_t pin) {
    if (m_currentOtp == 0) return false;
    bool isValid = (m_currentOtp == pin);
    if (isValid) {
        m_currentOtp = 0; // Consume the OTP
    }
    return isValid;
}

void ChargePointDevice::startCharge(uint16_t minutes) {
    if (minutes == 0) return;

    // Lanzar la tarea para manejar el relé asíncronamente
    // Argumento empaquetado
    struct ChargeArgs {
        ChargePointDevice* instance;
        uint16_t mins;
    };
    
    ChargeArgs* args = new ChargeArgs{this, minutes};

    char taskName[16];
    snprintf(taskName, sizeof(taskName), "chg_%d", m_terminalId);

    xTaskCreate(ChargePointDevice::taskControlCargaWrapper, taskName, 2048, args, 5, NULL);
}

void ChargePointDevice::stopCharge() {
    // Si queremos abortar anticipadamente, necesitaríamos un handle a la tarea
    // Por ahora solo apagamos el relé.
    if (m_hwExpander) {
        m_hwExpander->digital_write(m_terminalId - 1, 0); // 0-indexed
    }
    m_status = ChargePointStatus::AVAILABLE;
}

uint8_t ChargePointDevice::getId() const {
    return m_terminalId;
}

ChargePointStatus ChargePointDevice::getStatus() const {
    return m_status;
}

void ChargePointDevice::setStatus(ChargePointStatus status) {
    m_status = status;
}

void ChargePointDevice::taskControlCargaWrapper(void* pvParameters) {
    struct ChargeArgs {
        ChargePointDevice* instance;
        uint16_t mins;
    };
    ChargeArgs* args = (ChargeArgs*)pvParameters;
    
    args->instance->runChargeCycle(args->mins);

    delete args;
    vTaskDelete(NULL);
}

void ChargePointDevice::runChargeCycle(uint16_t minutes) {
    ESP_LOGW(TAG, "🔌 [P%d] RELÉ ON - Tiempo: %u min", m_terminalId, minutes);
    
    m_status = ChargePointStatus::OCCUPIED;

    if (m_hwExpander) {
         m_hwExpander->digital_write(m_terminalId - 1, 1);
    }

    // Delay
    vTaskDelay(pdMS_TO_TICKS(minutes * 1000)); // Usar 1000 como en main.cpp original (1 min = 1 seg para debug)

    if (m_hwExpander) {
        m_hwExpander->digital_write(m_terminalId - 1, 0);
    }
    
    ESP_LOGW(TAG, "🏁 [P%d] RELÉ OFF - Carga completa", m_terminalId);
    m_status = ChargePointStatus::AVAILABLE;
}

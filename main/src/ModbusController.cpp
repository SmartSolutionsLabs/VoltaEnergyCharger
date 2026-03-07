#include "ModbusController.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ModbusCtrl";

ModbusController::ModbusController(uint8_t unitId, IModbusEventHandler* eventHandler)
    : m_slave(unitId), m_eventHandler(eventHandler) {
}

ModbusController::~ModbusController() {
}

esp_err_t ModbusController::init(int txPin, int rxPin, uint32_t baudrate) {
    esp_err_t err = m_slave.init(txPin, rxPin, baudrate);
    if (err == ESP_OK) {
        m_dataMap.registerAreas(&m_slave);
    }
    return err;
}

void ModbusController::start() {
    mbc_slave_start(m_slave.getContext());
}

ModbusDataMap& ModbusController::getDataMap() {
    return m_dataMap;
}

void ModbusController::pollEvents() {
    mb_event_group_t all_events = (mb_event_group_t)(MB_EVENT_HOLDING_REG_WR | 
                                                     MB_EVENT_INPUT_REG_RD   | 
                                                     MB_EVENT_HOLDING_REG_RD);

    mb_event_group_t ev = mbc_slave_check_event(m_slave.getContext(), all_events);
    mb_param_info_t reg_info;
    
    // Si hay evento, leemos la info
    if (ev != 0) {
        esp_err_t err = mbc_slave_get_param_info(m_slave.getContext(), &reg_info, 0);
        if (err == ESP_OK) {
            uint16_t addr = reg_info.mb_offset;
            if (ev & MB_EVENT_HOLDING_REG_WR) {
                handleWriteEvent(addr, reg_info.size, reg_info.address);
            }
            if (ev & MB_EVENT_INPUT_REG_RD) {
                handleReadEvent(addr, reg_info.size, reg_info.address);
            }
        }
    }
}

void ModbusController::handleWriteEvent(uint16_t address, size_t size, void* addressPtr) {
    int display_sz = (size == 35) ? 70 : (size * 2);
    ESP_LOGI(TAG, "📥 [WRITE] Offset: 0x%04X | Size: %d", address, (int)size);

    if (address < 0x0100) { // PRICE REQUEST
        if (size == 2) {
            uint16_t id = m_dataMap.price_req.terminal_id;
            uint16_t mins = m_dataMap.price_req.req_minutes;
            m_dataMap.price_req.req_minutes = 0; // Clear it
            
            if (m_eventHandler) {
                m_eventHandler->onPriceRequest(id, mins);
            }
        }
    } 
    else if (address >= 0x0100) { // PIN VALIDATE
        if (size == 3) {
            uint16_t id = m_dataMap.pin_req.terminal_id;
            uint32_t pin = m_dataMap.pin_req.user_pin;
            m_dataMap.pin_req.user_pin = 0; // Clear it

            if (m_eventHandler) {
                m_eventHandler->onPinValidationRequest(id, pin);
            }
        }
    }
}

void ModbusController::handleReadEvent(uint16_t address, size_t size, void* addressPtr) {
    int display_sz = (size == 35) ? 70 : size;
    ESP_LOGD(TAG, "📤 [READ] Offset: 0x%04X", address);
    // Para simplificar, ya que los punteros apuntan directamente a ModbusDataMap,
    // el esclavo ESP-Modbus responderá automáticamente con los valores actualizados en memoria.
    // Solo manejamos el flag de pin reset como en main.cpp original.
    
    if (address >= 0x0200 && address < 0x0300) {
        // Maestro pidiendo si el Pin es Valido
        // Después de leerlo, se limpia (reset de la validacion del pin a false)
        // Agregamos un pequeño delay igual que en main.cpp si es necesario, 
        // pero limpiar directamente funciona para Modbus
        m_dataMap.pin_res.valid_pin = 0; 
    }
}

#include "ModbusDataMap.hpp"
#include <string.h>

ModbusDataMap::ModbusDataMap() {
    memset(&price_req, 0, sizeof(price_req));
    memset(&pin_req, 0, sizeof(pin_req));
    memset(&all_stats, 0, sizeof(all_stats));

    memset(&status_res, 0, sizeof(status_res));
    memset(&price_res, 0, sizeof(price_res));
    memset(&pin_res, 0, sizeof(pin_res));
    memset(&cp_status_res, 0, sizeof(cp_status_res));
    memset(&attr_res, 0, sizeof(attr_res));
}

void ModbusDataMap::registerAreas(ModbusSlave* slave) {
    if (!slave) return;

    slave->register_area(MB_PARAM_HOLDING, 0x0000, &price_req, sizeof(price_req));
    slave->register_area(MB_PARAM_HOLDING, 0x0100, &pin_req, sizeof(pin_req));
    slave->register_area(MB_PARAM_HOLDING, 0x0200, &all_stats, sizeof(all_stats));

    slave->register_area(MB_PARAM_INPUT,   0x0000, &status_res, sizeof(status_res));
    slave->register_area(MB_PARAM_INPUT,   0x0100, &price_res, sizeof(price_res));
    slave->register_area(MB_PARAM_INPUT,   0x0200, &pin_res, sizeof(pin_res));
    slave->register_area(MB_PARAM_INPUT,   0x0300, &cp_status_res, sizeof(cp_status_res));
    slave->register_area(MB_PARAM_INPUT,   0x0400, &attr_res, sizeof(attr_res));
}

void ModbusDataMap::updateAttributes(uint16_t qty, uint16_t minTime, uint16_t maxTime, uint16_t step, uint16_t val) {
    attr_res.terminals_quantity = qty;
    attr_res.min_charge_time = minTime;
    attr_res.max_charge_time = maxTime;
    attr_res.step_charge_time = step;
    attr_res.minute_value = val;
}

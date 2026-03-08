#ifndef MODBUS_DATA_MAP_HPP
#define MODBUS_DATA_MAP_HPP

#include "SlaveParameters.hpp"
#include "ModbusSlave.hpp"

class ModbusDataMap {
public:
    holding_terminal_price_request_t        price_req;          // 0x0000
    holding_terminal_user_pin_request_t     pin_req;            // 0x0100
    
    input_terminal_status_response_t        status_res;         // 0x0000
    input_terminal_price_response_t         price_res;          // 0x0100
    input_terminal_valid_pin_response_t     pin_res;            // 0x0200
    input_charge_point_status_response_t    cp_status_res;      // 0x0300
    input_attributes_response_t             attr_res;           // 0x0400 
    input_all_status_response_t             all_stats;          // 0x0500

    ModbusDataMap();

    void registerAreas(ModbusSlave* slave);
    void updateAttributes(uint16_t qty, uint16_t minTime, uint16_t maxTime, uint16_t step, uint16_t val);
};

#endif // MODBUS_DATA_MAP_HPP

#ifndef SLAVE_PARAMETERS_HPP
#define SLAVE_PARAMETERS_HPP

#include <stdint.h>

#pragma pack(push, 1)

typedef struct {                 // 0x0000
    uint16_t terminal_id;
    uint16_t req_minutes;
} holding_terminal_price_request_t;

typedef struct {                // 0x0000
    uint16_t terminal_id;
    uint16_t work_status; 
} input_terminal_status_response_t;

typedef struct {                // 0x1000
    uint16_t terminal_id;
    uint32_t price;
    uint8_t  signature[64];
} input_terminal_price_response_t;

typedef struct {                // 0x0100
    uint16_t terminal_id;
    uint32_t user_pin;
} holding_terminal_user_pin_request_t;

typedef struct {                // 0x0200
    uint16_t terminal_id;
    uint16_t valid_pin;
} input_terminal_valid_pin_response_t;

typedef struct{                 // 0x0300
    uint16_t terminal_id;
    uint16_t ChargePointStatus;
} input_charge_point_status_response_t;

typedef struct{                 // 0x0400
    uint16_t terminals_quantity;
    uint16_t minute_value;
    uint16_t min_charge_time;
    uint16_t max_charge_time;
    uint16_t step_charge_time;
} input_attributes_response_t;


#pragma pack(pop)
#endif
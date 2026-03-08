#ifndef SLAVE_PARAMETERS_HPP
#define SLAVE_PARAMETERS_HPP

#include <stdint.h>

#define MAX_TERMINALS 10

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
 
typedef struct{                //ESTRUCTURA USADA EN EL 0X0500
    uint16_t status;
    uint16_t remaining_time;
    uint16_t used_energy;
} terminal_status_block_t;

typedef struct{                // 0X0200  por el tipo input
    terminal_status_block_t terminals[MAX_TERMINALS];
} input_all_status_response_t;

#pragma pack(pop)

#endif
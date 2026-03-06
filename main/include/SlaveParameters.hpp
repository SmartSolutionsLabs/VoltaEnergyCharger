#ifndef SLAVE_PARAMETERS_HPP
#define SLAVE_PARAMETERS_HPP

#include <stdint.h>

#pragma pack(push, 1)

// Estructura para Input Registers (Lectura desde el Maestro)
typedef struct {
    uint16_t terminal_id;    // 0x0000
    uint8_t  signature[64];  // 0x0001 - 0x0020 (32 regs)
    uint32_t price;          // 0x0021 - 0x0022
    uint16_t status;         // 0x0023
    uint16_t elapsed_time;   // 0x0024
    uint16_t energy;         // 0x0025
} input_reg_params_t;

// Estructura para Holding Registers (Escritura desde el Maestro)
typedef struct {
    uint16_t terminal_id;    // 0x0000
    uint16_t req_minutes;    // 0x0001
    uint32_t user_pin;       // 0x0002 - 0x0003
    uint16_t enable_point;   // 0x0004
    uint32_t unit_price;     // 0x0005 - 0x0006
} holding_reg_params_t;

#pragma pack(pop)

#endif
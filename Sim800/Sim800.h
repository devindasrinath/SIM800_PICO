#pragma once

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define RETRY_LIMIT 5

// // AT command modes
// enum AT_COMMAND_MODES {
//     COMMAND = 0,
//     DATA = 1,
//     IGNORE = 2,
//     RESPONSE = 3,
//     NO_MODE = 4
// };


// Uart config for sim 800 module
typedef struct{
    uart_inst_t *uart;
    uint baudrate;
    uint8_t tx;
    uint8_t rx;
    uint8_t data_bits;
    uint8_t stop_bit;
    uint8_t parity;
}UartConfig;


// Error types
enum SIM800_ERROR{
    NO_ERROR = 0,
    SMS_ERROR = 1,
    BEGIN_FAILED = 2
};

void sim800_init(UartConfig *uart_config);
void sim800_deinit();
enum SIM800_ERROR sim800_begin();
enum SIM800_ERROR sim800_send_sms(const char* msg);
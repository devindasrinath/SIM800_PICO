/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#include "Sim800.h"

#define UART_ID uart1

#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 4
#define UART_RX_PIN 5

UartConfig gsm_uart = {UART_ID ,BAUD_RATE,UART_TX_PIN,UART_RX_PIN,DATA_BITS,STOP_BITS,PARITY};


int main() {
    // Enable std io
    stdio_init_all();

    sleep_ms(5000);

    sim800_init(&gsm_uart);

    enum SIM800_ERROR error1 = sim800_begin();

    for(int u = 0; u<6; u++){
        if (error1 == ( enum SIM800_ERROR)NO_ERROR){
            error1 = sim800_send_sms("hello world....\r\n");
        }
    }


    while (1)
        tight_loop_contents();
}

/// \end:uart_advanced[]

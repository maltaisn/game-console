
/*
 * Copyright 2021 Nicolas Maltais
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/comm.h>

#include <sys/uart.h>
#include <sys/power.h>
#include <sys/led.h>
#include <sys/input.h>
#include <sys/time.h>
#include <sys/spi.h>

#include <avr/io.h>

typedef enum {
    SPI_CS_FLASH = 0b00,
    SPI_CS_EEPROM = 0b01,
    SPI_CS_DISPLAY = 0b10,
} spi_cs_sel_t;

uint8_t comm_payload_buf[PAYLOAD_MAX_SIZE];

static void handle_packet_version(void) {
    comm_payload_buf[0] = VERSION_MAJOR;
    comm_payload_buf[1] = VERSION_MINOR;
    comm_transmit(PACKET_VERSION, 2);
}

static void handle_packet_battery(void) {
    comm_payload_buf[0] = power_get_battery_status();
    comm_payload_buf[1] = power_get_battery_percent();
    uint16_t voltage = power_get_battery_voltage();
    comm_payload_buf[2] = voltage & 0xff;
    comm_payload_buf[3] = voltage >> 8;
    comm_transmit(PACKET_BATTERY, 4);
}

static void handle_packet_led(void) {
    if (comm_payload_buf[0] == 1) {
        led_set();
    } else {
        led_clear();
    }
    // (no TX packet)
}

static void handle_packet_input(void) {
    comm_payload_buf[0] = input_get_state();
    comm_transmit(PACKET_INPUT, 1);
}

static void handle_packet_spi(uint8_t length) {
    // assert CS line for the selected peripheral
    const uint8_t options = comm_payload_buf[0];
    const uint8_t cs = options & 0x3;
    // TODO put in spi module
    if (cs == SPI_CS_FLASH) {
        VPORTF.OUT &= ~PIN0_bm;
    } else if (cs == SPI_CS_EEPROM) {
        VPORTF.OUT &= ~PIN1_bm;
    } else if (cs == SPI_CS_DISPLAY) {
        VPORTC.OUT &= ~PIN1_bm;
    }

    // transceive SPI data
    spi_transceive(length - 1, comm_payload_buf + 1);
    comm_transmit(PACKET_SPI, length);

    // if last transfer, deassert the CS line.
    if (options & 0x80) {
        VPORTF.OUT |= PIN0_bm | PIN1_bm;
        VPORTC.OUT |= PIN1_bm;
    }
}

static void handle_packet_time(void) {
    const systime_t time = time_get();
    comm_payload_buf[0] = time & 0xff;
    comm_payload_buf[1] = (time >> 8) & 0xff;
    comm_payload_buf[2] = time >> 16;
    comm_transmit(PACKET_TIME, 3);
}

static void handle_packet_fast_mode(void) {
    comm_transmit(PACKET_FAST_MODE, 0);
    uart_flush();

    if (comm_payload_buf[0]) {
        uart_set_fast_mode();
        while (uart_is_in_fast_mode()) {
            // in fast mode we're continuously decoding packets to avoid losing any data.
            // the loop will end when a fast mode disable packet is sent.
            comm_receive();
        }
    } else {
        uart_set_normal_mode();
    }
}

static void handle_packet_reset(void) {
    _PROTECTED_WRITE(RSTCTRL.SWRR, RSTCTRL_SWRE_bm);
}

void comm_receive(void) {
    if (!uart_available()) return;
    if (uart_read() != PACKET_SIGNATURE) return;

    const packet_type_t type = uart_read();
    const uint8_t length = uart_read();
    uint8_t count = length;
    uint8_t* ptr = comm_payload_buf;
    while (count--) {
        *ptr++ = uart_read();
    }

    if (type == PACKET_VERSION) {
        handle_packet_version();
    } else if (type == PACKET_BATTERY) {
        handle_packet_battery();
    } else if (type == PACKET_LED) {
        handle_packet_led();
    } else if (type == PACKET_INPUT) {
        handle_packet_input();
    } else if (type == PACKET_SPI) {
        handle_packet_spi(length);
    } else if (type == PACKET_TIME) {
        handle_packet_time();
    } else if (type == PACKET_FAST_MODE) {
        handle_packet_fast_mode();
    } else if (type == PACKET_RESET) {
        handle_packet_reset();
    } else {
        comm_undef_packet_callback(type, length);
    }
}

void comm_transmit(uint8_t type, uint8_t length) {
    uart_write(PACKET_SIGNATURE);
    uart_write(type);
    uart_write(length);
    const uint8_t* payload = comm_payload_buf;
    while (length--) {
        uart_write(*payload++);
    }
}

__attribute__((weak)) void comm_undef_packet_callback(uint8_t type, uint8_t length) {
    // undefined packets ignored by default.
}
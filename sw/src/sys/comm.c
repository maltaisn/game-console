
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

// buffer used to receive RX payload and send TX payload.
// TODO eventually, put this in the same data space as display buffer since
//  both aren't used at the same time, using linker script.
static uint8_t packet_payload[PAYLOAD_MAX_SIZE];

static void handle_packet_version(void) {
    packet_payload[0] = VERSION_MAJOR;
    packet_payload[1] = VERSION_MINOR;
    comm_transmit(PACKET_VERSION, 2, packet_payload);
}

static void handle_packet_battery(void) {
    packet_payload[0] = power_get_battery_status();
    packet_payload[1] = power_get_battery_percent();
    uint16_t voltage = power_get_battery_voltage();
    packet_payload[2] = voltage & 0xff;
    packet_payload[3] = voltage >> 8;
    comm_transmit(PACKET_BATTERY, 4, packet_payload);
}

static void handle_packet_led(void) {
    if (packet_payload[0] == 1) {
        led_set();
    } else {
        led_clear();
    }
    // (no TX packet)
}

static void handle_packet_input(void) {
    packet_payload[0] = input_get_state();
    comm_transmit(PACKET_INPUT, 1, packet_payload);
}

static void handle_packet_spi(uint8_t length) {
    // assert CS line for the selected peripheral
    const uint8_t options = packet_payload[0];
    const uint8_t cs = options & 0x3;
    if (cs == SPI_CS_FLASH) {
        VPORTF.OUT &= ~PIN0_bm;
    } else if (cs == SPI_CS_EEPROM) {
        VPORTF.OUT &= ~PIN1_bm;
    } else if (cs == SPI_CS_DISPLAY) {
        VPORTC.OUT &= ~PIN1_bm;
    }

    // transceive SPI data
    spi_transceive(length - 1, packet_payload + 1);
    comm_transmit(PACKET_SPI, length, packet_payload);

    // if last transfer, deassert the CS line.
    if (options & 0x80) {
        VPORTF.OUT |= PIN0_bm | PIN1_bm;
        VPORTC.OUT |= PIN1_bm;
    }
}

static void handle_packet_time(void) {
    const systime_t time = time_get();
    packet_payload[0] = time & 0xff;
    packet_payload[1] = (time >> 8) & 0xff;
    packet_payload[2] = time >> 16;
    comm_transmit(PACKET_TIME, 3, packet_payload);
}

void comm_receive(void) {
    if (!uart_available()) return;
    if (uart_read() != PACKET_SIGNATURE) return;

    const packet_type_t type = uart_read();
    const uint8_t length = uart_read();
    uint8_t count = length;
    uint8_t* ptr = packet_payload;
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
    } else {
        comm_undef_packet_callback(type, length, packet_payload);
    }
}

void comm_transmit(uint8_t type, uint8_t length, const uint8_t payload[length]) {
    uart_write(PACKET_SIGNATURE);
    uart_write(type);
    uart_write(length);
    while (length--) {
        uart_write(*payload++);
    }
}

__attribute__((weak)) void comm_undef_packet_callback(
        uint8_t type, uint8_t length, uint8_t payload[length]) {
    // undefined packets ignored by default.
}


/*
 * Copyright 2022 Nicolas Maltais
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

#include <comm.h>
#include <system.h>

#include <sys/uart.h>
#include <sys/spi.h>

#include <core/defs.h>

#ifdef SIMULATION
#include <sim/uart.h>
#include <sim/flash.h>
#include <sim/eeprom.h>
#endif

enum {
    SPI_CS_FLASH = 0x0,
    SPI_CS_EEPROM = 0x1,
    SPI_CS_DISPLAY = 0x2,
};

// When a packet is being received, the comm_receive function will block until the packet has been
// fully received. Hence, the payload buffer can share memory with the display buffer.
SHARED_DISP_BUF uint8_t comm_payload_buf[PAYLOAD_MAX_SIZE];

static bool in_fast_mode;

static void handle_packet_version(void) {
    comm_payload_buf[0] = APP_VERSION & 0xff;
    comm_payload_buf[1] = APP_VERSION >> 8;
    comm_payload_buf[2] = BOOT_VERSION & 0xff;
    comm_payload_buf[3] = BOOT_VERSION >> 8;
    comm_payload_buf[4] = VERSION_PROG_COMP & 0xff;
    comm_payload_buf[5] = VERSION_PROG_COMP >> 8;
    comm_transmit(PACKET_VERSION, 6);
}

static void handle_packet_spi(uint8_t length) {
    // assert CS line for the selected peripheral
    const uint8_t options = comm_payload_buf[0];
    const uint8_t cs = options & 0x3;
    if (cs == SPI_CS_FLASH) {
        sys_spi_select_flash();
        state.flags |= SYSTEM_FLAG_FLASH_DIRTY;
    } else if (cs == SPI_CS_EEPROM) {
        sys_spi_select_eeprom();
        state.flags |= SYSTEM_FLAG_EEPROM_DIRTY;
    } else if (cs == SPI_CS_DISPLAY) {
        sys_spi_select_display();
    }

    // transceive SPI data
    sys_spi_transceive(length - 1, comm_payload_buf + 1);
    comm_transmit(PACKET_SPI, length);

    // if last transfer, deassert the CS line.
    if (options & 0x80) {
        sys_spi_deselect_all();
    }
}
static void handle_packet_fast_mode(void) {
    comm_transmit(PACKET_FAST_MODE, 0);
    sys_uart_flush();

    if (comm_payload_buf[0]) {
        sys_uart_set_baud(SYS_UART_BAUD_RATE(UART_BAUD_FAST));
        in_fast_mode = true;
    } else {
        sys_uart_set_baud(SYS_UART_BAUD_RATE(UART_BAUD));
        in_fast_mode = false;
    }
}

static void comm_receive_internal(void) {
    if (!sys_uart_available()) return;
    if (sys_uart_read() != PACKET_SIGNATURE) return;

    const packet_type_t type = sys_uart_read();
    const uint8_t length = sys_uart_read();
    uint8_t count = length;
    uint8_t* ptr = comm_payload_buf;
    while (count--) {
        *ptr++ = sys_uart_read();
    }

    if (type == PACKET_VERSION) {
        handle_packet_version();
    } else if (type == PACKET_SPI) {
        handle_packet_spi(length);
    } else if (type == PACKET_FAST_MODE) {
        handle_packet_fast_mode();
    }
}

void comm_receive(void) {
    // in fast mode, we must receive continuously to avoid losing data, since
    // this function is called only a few times per second and the receive buffer size is limited.
    do {
        comm_receive_internal();
    } while (in_fast_mode);
}

void comm_transmit(uint8_t type, uint8_t length) {
    sys_uart_write(PACKET_SIGNATURE);
    sys_uart_write(type);
    sys_uart_write(length);
    const uint8_t* payload = comm_payload_buf;
    while (length--) {
        sys_uart_write(*payload++);
    }
}

#ifdef SIMULATION
void sim_uart_connection_lost_callback(void) {
    // Flash and EEPROM may have been changed, save them.
    sim_flash_save();
    sim_eeprom_save();
}
#endif
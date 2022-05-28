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


#ifdef SYS_UART_ENABLE

#include <sim/uart.h>
#include <sys/uart.h>

#include <core/trace.h>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

static const char* SOCKET_NAME = "/tmp/gcsim";

int socket_fd = -1;
int connected_fd = -1;

void sys_uart_init(uint16_t baud_calc) {
    socket_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (socket_fd < 0) {
        return;
    }

    unlink(SOCKET_NAME);
    struct sockaddr addr;
    addr.sa_family = AF_UNIX;
    strcpy(addr.sa_data, SOCKET_NAME);
    if (bind(socket_fd, &addr, sizeof addr) < 0) {
        socket_fd = -1;
        return;
    }

    if (listen(socket_fd, 1) < 0) {
        socket_fd = -1;
        return;
    }

    sys_uart_set_baud(baud_calc);
}

void sys_uart_set_baud(uint16_t baud_calc) {
    trace("UART baud rate set to %d", baud_calc * 100);
}

void sys_uart_write(uint8_t c) {
    if (connected_fd < 0) {
        // data will be lost
        return;
    }
    send(connected_fd, &c, 1, MSG_NOSIGNAL);
}

uint8_t sys_uart_read(void) {
    if (connected_fd < 0) {
        return 0;
    }
    uint8_t c;
    recv(connected_fd, &c, 1, MSG_WAITALL);
    return c;
}

bool sys_uart_available(void) {
    if (connected_fd < 0) {
        return false;
    }
    int count;
    ioctl(connected_fd, FIONREAD, &count);
    return count > 0;
}

void sys_uart_flush(void) {
    // no buffer, no-op
}

void sim_uart_listen(void) {
    if (socket_fd < 0) {
        return;
    }

    if (connected_fd > 0) {
        // check if socket connection is still alive by pinging with a 0x00 byte
        // this byte should be ignored by the client.
        uint8_t c = 0;
        if (send(connected_fd, &c, 1, MSG_DONTWAIT | MSG_NOSIGNAL) < 0) {
            connected_fd = -1;
            trace("UART server lost connection.");
            sim_uart_connection_lost_callback();
        } else {
            return;
        }
    }

    connected_fd = accept(socket_fd, 0, 0);
    if (connected_fd > 0) {
        trace("UART server established connection.");
    }
}

void sim_uart_end(void) {
    if (connected_fd > 0) {
        close(connected_fd);
    }
    if (socket_fd > 0) {
        close(socket_fd);
    }
}

__attribute__((weak))
void sim_uart_connection_lost_callback(void) {
    // do nothing by default.
}

#endif
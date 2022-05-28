
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

#ifndef SIM_UART_H
#define SIM_UART_H

/**
 * Listen for client connection for the UART socket.
 */
void sim_uart_listen(void);

/**
 * Close the simulated UART pipe, after it has been initialized.
 */
void sim_uart_end(void);

/**
 * Function called when the client connection is lost on the UART server.
 */
void sim_uart_connection_lost_callback(void);

#endif //SIM_UART_H

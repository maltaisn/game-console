
# UART communication setup
# The RX buffer size is 256 bytes to fit at most one full size packet.
DEFINES += SYS_UART_ENABLE UART_BAUD=250000 SYS_UART_RX_BUFFER_SIZE=256

# gcprog version compatibility
DEFINES += VERSION_PROG_COMP=3

# use absolute memory access in simulation
DEFINES += SIM_MEMORY_ABSOLUTE

# dialog setup
DEFINES += DIALOG_MAX_ITEMS=5 DIALOG_NO_CHOICE DIALOG_NO_NUMBER DIALOG_NO_TEXT

# Common base for all projects targeting the simulator

CC := gcc
AR := gcc-ar

DEFINES += SIMULATION

CFLAGS += -std=c11 -g3 -O0 -fshort-enums -fpack-struct

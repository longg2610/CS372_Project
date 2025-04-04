#ifndef SYSSUPPORT
#define SYSSUPPORT
#include "./initProc.h"

void support_syscall_handler();

void terminate();

void get_TOD();

void write_to_printer();

void write_to_terminal();

void read_from_terminal();

void program_trap();

#endif
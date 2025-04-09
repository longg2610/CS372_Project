#ifndef SYSSUPPORT
#define SYSSUPPORT
#include "./initProc.h"

void support_syscall_handler();

void terminate(int* semaphore, support_t* curr_support_struct);

void get_TOD(support_t* curr_support_struct);

void write_to_printer(support_t* curr_support_struct);

void write_to_terminal(support_t* curr_support_struct);

void read_from_terminal(support_t* curr_support_struct);

void program_trap(int* semaphore, support_t* curr_support_struct);

#endif
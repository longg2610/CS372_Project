#ifndef VMSUPPORT
#define VMSUPPORT

#include "./types.h"
#include "./initProc.h"
#include "../h/const.h"

extern void pager();
extern void initSwapStructs();
extern int swap_pool_sem;

extern void readFlashDevice(support_t* curr_support_struct, memaddr frameAddr, int read_content);
extern void writeFlashDevice(support_t* curr_support_struct, memaddr frameAddr, int write_content);
extern void enableInt();
extern void disableInt();

extern swap_pool_t swap_pool[2 * UPROCMAX];

#endif
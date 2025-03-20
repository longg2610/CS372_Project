#ifndef VMSUPPORT
#define VMSUPPORT

#include "./types.h"
#include "./initProc.h"
#include "./initial.h"

// extern void TLB_refill_handler();
extern void pager();
extern void initSwapStructs();
extern int swap_pool_sem;

extern void readFlashDevice();
extern void writeFlashDevice();

extern swap_pool_t swap_pool[2 * UPROCMAX];
extern support_t *curr_proc_support_struct;
#endif
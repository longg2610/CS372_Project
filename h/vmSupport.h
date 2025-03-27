#ifndef VMSUPPORT
#define VMSUPPORT

#include "./types.h"
#include "./initProc.h"
#include "./initial.h"
#include "../h/const.h"

extern void pager();
extern void initSwapStructs();
extern int swap_pool_sem;

extern void readFlashDevice(int asid, swap_pool_t *frameAddr, int read_content);
extern void writeFlashDevice(int asid, swap_pool_t *frameAddr, int read_content);
extern void enableInt();
extern void disableInt();

extern swap_pool_t swap_pool[2 * UPROCMAX];
extern support_t *curr_proc_support_struct;

#endif
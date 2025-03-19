#ifndef VMSUPPORT
#define VMSUPPORT

#include "./types.h"
#include "./initProc.h"
#include "./initial.h"

extern void TLB_refill_handler();
extern void pager();

#endif
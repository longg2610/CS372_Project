/******************************* INITIAL.h *************************************
* Written by Long Pham & Vuong Nguyen
* Module: Global state and initialization declarations for Pandos OS
*
* Global Variables:
* - process_cnt: Count of non-terminated processes
* - softblock_cnt: Count of blocked processes
* - ready_queue: Queue of processes ready to run
* - curr_proc: Currently executing process
* - device_sem[49]: Device semaphore array
*
* External Functions:
* - test(): Test entry point
*
* Dependencies:
* - const.h: System constants
* - types.h: Type definitions
*****************************************************************************/

#ifndef INITIAL
#define INITIAL

#include "../h/const.h"
#include "../h/types.h"

extern int process_cnt;     /*number of started, but not yet terminated processes*/
extern int softblock_cnt;   /*number of started, but not terminated processes that in are the “blocked” state due to an I/O or timer request*/
extern pcb_PTR ready_queue; /*tail pointer to a queue of pcbs that are in the “ready” state*/
extern pcb_PTR curr_proc;   /*pointer to the pcb that is in the “running” state, i.e. the current executing process.*/
extern int device_sem [49]; /*array of device semaphores*/
extern void test();
extern void debug(int a0, int a1, int a2, int a3);
#endif

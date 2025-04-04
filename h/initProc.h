/*initilize U'proc field*/
#ifndef INITPROC
#define INITPROC

#include "../h/initial.h"
#include "../h/pcb.h"
#include "../h/vmSupport.h"
#include "../h/const.h"

/*data structure of the Support Level*/

/*init swap pool*/
/*swap pool should be set to 2 x UPROCMAX RAM frames*/
extern swap_pool_t swap_pool[2 * UPROCMAX];

extern void debug(int a0, int a1, int a2, int a3);
/* semaphore*/
extern int masterSemaphore;

/*devices semaphore*/
/*each U-proc has its own peripheral devices*/
extern int deviceSemaphores[48];

extern void test();
#endif
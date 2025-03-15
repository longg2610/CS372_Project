/*initilize U'proc field*/
#ifndef INITPROC

#include "../h/initial.h"
#include "../h/pcb.h"

#define UPROCMAX 8
/*data structure of the Support Level*/

/* 8 processes in user-mode */
extern pcb_PTR Uproc_pcbs[UPROCMAX];

/*init swap pool*/
/*swap pool should be set to 2 x UPROCMAX RAM frames*/
extern swap_pool_t swap_pool[2 * UPROCMAX];

/*swap pool semaphore*/
extern int swap_pool_sem;

/*backing store*/

/*flash device*/

#endif 
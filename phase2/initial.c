/******************************* INITIAL.c *************************************
* Written by Long Pham & Vuong Nguyen
* Module: System initialization and first process setup for Pandos OS
* 
* Components:
* - Pass Up Vector: where the BIOS finds the address of the Nucleus functions to pass 
*                   control to for both TLB-Refill events and all other exceptions. 
* - Process Control: PCBs, ASL, ready queue
* - Device Management: Device semaphores array
* - System Variables: Process counts and blocked process count
*
* Global State:
* - process_cnt: Count of non-terminated processes
* - softblock_cnt: Count of blocked but not terminated processes 
* - ready_queue: Queue of ready processes
* - curr_proc: Currently executing process
* - device_sem[49]: Device semaphore array including semaphores of 49 devices
*
* Entry Point: main() - Initializes system and launches first process
***************************************************************************/


#include "../h/pcb.h"
#include "../h/asl.h"

#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/scheduler.h"

int process_cnt;        /*number of started, but not yet terminated processes*/
int softblock_cnt;      /*number of started, but not terminated processes that in are the “blocked” state due to an I/O or timer request*/
pcb_PTR ready_queue;    /*tail pointer to a queue of pcbs that are in the “ready” state*/
pcb_PTR curr_proc;      /*pointer to the pcb that is in the “running” state, i.e. the current executing process.*/
int device_sem [DEVSEMNO];    /*device semaphores array*/
void test();

/*************************************************/
/* Main Entry Point                              */
/* Purpose: Initialize system and launch first    */
/* process                                       */
/*                                               */
/* Implementation:                                */
/* 1. Initialize Pass Up Vector                  */
/* 2. Initialize system data structures          */
/* 3. Setup first process                        */
/* 4. Launch scheduler                           */
/*                                               */
/* Returns: 1 on error (should never return)     */
/*************************************************/
void debug(int a0, int a1, int a2, int a3){
    return;
}
int main()
{
    debug(1, 1, 1, 1);
    /* Initialize Pass Up Vector for exceptions:
     * - TLB refill handler and stack
     * - General exception handler and stack */
    passupvector_t* passupvec = (passupvector_t*) PASSUPVECTOR;
    passupvec->tlb_refll_handler = (memaddr) uTLB_RefillHandler;
    passupvec->tlb_refll_stackPtr = STACKPAGETOP;
    passupvec->execption_handler =(memaddr) generalExceptionHandler;
    passupvec->exception_stackPtr = STACKPAGETOP;

    /*Initialize Level 2 variables: Process Control Blocks, Active Semaphore List*/
    initPcbs();
    initASL();
    
    /*Initialize Nucleus maintained variables*/
    process_cnt = 0;                    /*number of started but not terminated process*/
    softblock_cnt = 0;                  /* process in blocked state*/
    ready_queue = mkEmptyProcQ();       /*process in ready state*/
    curr_proc = NULL;

    /*initialize all device semaphores to 0*/
    int i;
    for(i = 0; i < DEVSEMNO; i++)
        device_sem[i] = 0;


    LDIT(CLOCKINTERVAL);                    /*Load the system-wide Interval Timer with 100 milliseconds*/
    curr_proc = allocPcb();                 /*Instantiate a single process*/
    insertProcQ(&ready_queue, curr_proc);   /*place process in Ready Queue*/
    process_cnt++;
    
    curr_proc->p_prnt = NULL;
    curr_proc->p_child = NULL;
    curr_proc->p_sib = NULL;
    curr_proc->p_sib_left = NULL;

    /* Initialize process control fields */
    curr_proc->p_time = (cpu_t) 0;
    curr_proc->p_semAdd = NULL;
    curr_proc->p_supportStruct = NULL;

    /*call the Scheduler*/
    scheduler();

    /*do nothing here, scheduler never returns*/ 
    return 1;           /*error if scheduler returns*/
}
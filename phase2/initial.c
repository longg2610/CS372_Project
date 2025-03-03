
#include "../h/pcb.h"
#include "../h/asl.h"

#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/scheduler.h"

int process_cnt; /*number of started, but not yet terminated processes*/
int softblock_cnt; /*number of started, but not terminated processes that in are the “blocked” state due to an I/O or timer request*/
pcb_PTR ready_queue; /*tail pointer to a queue of pcbs that are in the “ready” state*/
pcb_PTR curr_proc; /*pointer to the pcb that is in the “running” state, i.e. the current executing process.*/
int device_sem [49];


int main()
{
    /*Initialize Pass Up Vector*/
    passupvector_t* passupvec = (passupvector_t*) PASSUPVECTOR;
    passupvec->tlb_refll_handler = (memaddr) uTLB_RefillHandler;
    passupvec->tlb_refll_stackPtr = STACKPAGETOP;
    passupvec->execption_handler =(memaddr) generalExceptionHandler;
    passupvec->exception_stackPtr = STACKPAGETOP;

    /*Initialize Level 2 variables*/
    initPcbs();
    initASL();

    /*Initialize Nucleus maintained variables*/
    process_cnt = 0;
    softblock_cnt = 0;
    ready_queue = mkEmptyProcQ();
    curr_proc = NULL;
    int i;
    for(i = 0; i < DEVSEMNO; i++)
        device_sem[i] = 0;

    LDIT(CLOCKINTERVAL);    /*Load the system-wide Interval Timer with 100 milliseconds*/
    curr_proc = allocPcb(); /*Instantiate a single process*/
    insertProcQ(&ready_queue, curr_proc); /*place pcb in Ready Queue*/
    process_cnt++; /*increment process_cnt*/


    /*initializing the processor state */
    /*Local Timer (27) enabled, interrupt mask on (15-8), interrupt (2) enabled, kernel mode on (= 0), previous bits*/
    curr_proc->p_s.s_status = TEBITON | IMON | IEPBITON;
    curr_proc->p_s.s_sp = *((int*) RAMBASEADDR) + *((int*) RAMBASESIZE);  /*set stack pointer to RAMTOP*/
    curr_proc->p_s.s_pc = (memaddr) test;   /*set pc to test*/
    curr_proc->p_s.s_t9 = (memaddr) test; 


    /*set all Proc Tree fields to NULL*/
    curr_proc->p_prnt = NULL;
    curr_proc->p_child = NULL;
    curr_proc->p_sib = NULL;
    curr_proc->p_sib_left = NULL;

    curr_proc->p_time = (cpu_t) 0;
    curr_proc->p_semAdd = NULL;
    curr_proc->p_supportStruct = NULL;


    /*call the Scheduler*/
    scheduler();

    /*do nothing here, scheduler never returns*/ 
    return 1; /*error*/
}
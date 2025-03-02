#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/pcb.h"

cpu_t quantum_start_time; /*records starting time of current process's quantum*/

void scheduler()
{
    /*Remove the pcb from the head of the Ready Queue and store the pointer to the pcb in the Current Process field*/
    
    curr_proc = removeProcQ(&ready_queue); 
    /*debug(8000, process_cnt, softblock_cnt, curr_proc);*/
    /*ready queue empty*/
    if(curr_proc == NULL)
    {
        /*If the Process Count is zero invoke the HALT BIOS service/instruction.*/
        if (process_cnt == 0)
        {
            HALT();
        }

        /*If the Process Count > 0 and the Soft-block Count > 0 enter a Wait State*/
        if (process_cnt > 0 && softblock_cnt > 0)
        {
            /*setSTATUS(getSTATUS() | 0b00000000000000000000000000000001);*/ /*macro: INTRPTENABLED*/    /*enable interrupt*/
            /*setSTATUS(getSTATUS() & 0b11110111111111111111111111111111);*/ /*macro: PLTDISABLED*/   /*disable PLT*/
            /*curr_proc = NULL;*/
            /*debug(18,18,81,81);*/   
            setSTATUS(IECBITON | IMON & TEBITOFF);
            /*debug(81,81,18,18);*/
            /*debug(18,18,process_cnt,softblock_cnt);*/
            WAIT();   /*WAIT might not be executed if after IECBITON an interrupt arrives */
        }

        /*Deadlock for Pandos is defined as when the Process Count > 0 and the Soft-block Count is zero.*/
        if (process_cnt > 0 && softblock_cnt == 0)
        {
            PANIC();
        }
    }

    /*ready queue not empty*/
    else
    {   
        /*debug(1004,curr_proc,1004,1004);*/
        setTIMER(5000); /*local Timer (PLT)*/
        STCK(quantum_start_time);  /*start quantum of current process*/
        /*debug(68, ((state_t*) BIOSDATAPAGE)->s_cause, curr_proc->p_s.s_status ,curr_proc);*/
        LDST(&(curr_proc->p_s));    
    }
}

void switchContext(pcb_PTR to_be_executed)
{
    /*load time*/
    cpu_t quantum_end_time;
    STCK(quantum_end_time);
    curr_proc->p_time += (quantum_end_time - quantum_start_time);

    /*load state of process to be executed, return processor to whatever interrupt state and mode was in 
    effect when the exception occured*/
    LDST(&to_be_executed->p_s); /*put saved state in next_proc's state*/
}
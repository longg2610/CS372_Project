#include "../h/exceptions.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/pcb.h"
#include "../h/asl.h"

/*************************************************/
/* General Exception Handler                      */
/* Purpose: Routes different types of exceptions  */
/* to their specific handlers based on the cause */
/* register value                                */
/*                                               */
/* Exceptions handled:                           */
/* - Interrupts (case 0)                        */
/* - TLB exceptions (cases 1-3)                 */
/* - Program traps (cases 4-7,9-12)             */
/* - System calls (case 8)                      */
/*                                               */
/* Parameters: none                              */
/* Returns: void                                 */
/*************************************************/
void generalExceptionHandler()
{
    unsigned int cause = ((state_t*) BIOSDATAPAGE)->s_cause;

    /* Extract exception code (bits 2-6) from cause register */
    switch ((cause >> GETEXCCODE) & CLEAR26MSB)
    {
    /*Interrupt*/
    case 0:
    {
        /* Handle interrupt exceptions */
        interruptHandler(cause);
        break;
    }

    /* TLB exceptions (cause codes 1-3)*/
    case 1:
    case 2:
    case 3:
    {
        /* Handle TLB exceptions by passing up or terminating */
        passUpOrDie(PGFAULTEXCEPT);
        break;
    }
        
    /*Program traps (cause codes 4-7,9-12)*/
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case 11:
    case 12:
    {
        /* Handle program trap via trap handler */
        trapHandler();
        break; 
    }
        
    /*Syscalls (cause code 8)*/
    case 8:
    {
        /* Handle system calls */
        syscallHandler();
        break;
    }
        
    default:
        /* Handle unknown exceptions */
        break;
    }
}

/*************************************************/
/* Trap Handler                                   */
/* Purpose: Handles program traps by passing them */
/* up to support level or terminating process     */
/*                                               */
/* Program traps include:                         */
/* - Reserved instruction                         */
/* - Overflow                                     */
/* - Invalid memory access                        */ 
/* - Privilege violation                          */
/*                                               */
/* Parameters: none                              */
/* Returns: void                                  */
/* Note: May not return if process terminates     */
/*************************************************/
void trapHandler()
{
    /* Pass trap as general exception to support level
     * or terminate if no support structure exists */
    passUpOrDie(GENERALEXCEPT);
}

/*************************************************/
/* System call handler function                   */
/* Handles 8 types of system calls:              */
/*                                               */
/* SYS1 (CREATETHREAD):                          */
/*   - Params: state_t* procState, void* supStr  */
/*   - Returns: 0 on success, -1 if no resources */
/*                                               */
/* SYS2 (TERMINATETHREAD):                       */
/*   - Params: none                              */
/*   - Returns: never returns                    */
/*                                               */
/* SYS3 (PASSERN):                              */
/*   - Params: int* semaphore                    */
/*   - Returns: control to process when unblocked*/
/*                                               */
/* SYS4 (VERHOGEN):                             */
/*   - Params: int* semaphore                    */
/*   - Returns: control to process               */
/*                                               */
/* SYS5 (WAITIO):                               */
/*   - Params: int line, int dev, int read      */
/*   - Returns: control when I/O complete        */
/*                                               */
/* SYS6 (GETCPUTIME):                           */
/*   - Params: none                             */
/*   - Returns: CPU time used by process        */
/*                                               */
/* SYS7 (WAITCLOCK):                            */
/*   - Params: none                             */
/*   - Returns: control after interval          */
/*                                               */
/* SYS8 (GETSPTPTR):                            */
/*   - Params: none                             */
/*   - Returns: support structure pointer       */
/*                                              */
/* @return void                                 */
/************************************************/
void syscallHandler()
{
    /*check user/kernel mode and call trap handler if necessary*/
    unsigned int status = ((state_t*) BIOSDATAPAGE)->s_status;
    status = (status >> GETKUP) & CLEAR31MSB;
    if (status == 1) /* currently in user-mode, call trap handler*/
    {
        (((state_t*) BIOSDATAPAGE)->s_cause) &= 0b11111111111111111111111110000011;
        (((state_t*) BIOSDATAPAGE)->s_cause) |= 0b00000000000000000000000000101000; /*set Cause.ExcCode to 10 - RI*/
        trapHandler();
    }
        
    int a0 = ((state_t*) BIOSDATAPAGE)->s_a0;
    switch (a0)
    {
    /*Create Process (SYS1)*/
    case CREATETHREAD:
    {
        /* allocate new process control block */
        pcb_PTR new_proc = allocPcb();

        /* new_proc == NULL -> insufficient resources, put error code -1 in v0*/
        if (new_proc == NULL)
            curr_proc->p_s.s_v0 = -1;
        else 
        {
            curr_proc->p_s.s_v0 = 0;
            copyState(&(new_proc->p_s), ((state_t*) BIOSDATAPAGE)->s_a1); /*copy saved exception state into the new process state*/
            new_proc->p_supportStruct = ((state_t*) BIOSDATAPAGE)->s_a2;  /*copy support struct from saved exception state into new process support struct*/
                
            insertProcQ(&ready_queue, new_proc);    /*place new process on ready queue*/
            insertChild(curr_proc, new_proc);       /*make new process a child of current process*/
            process_cnt++;                       

            new_proc->p_time = (cpu_t) 0;  /*new process p_time set to zero*/
            new_proc->p_semAdd = NULL;     /*set new process p_semAdd to NULL*/
        }
        
        /*for non-blocking syscalls, increase PC & LDST from saved exception state*/
        increasePC();
        LDST((state_t*) BIOSDATAPAGE);
        break;
    }

    /*Terminate Process (SYS2)*/
    case TERMINATETHREAD:
    {
        /* terminate the process and all its descendants*/
        killDescendants(curr_proc->p_child);
        killProcess(curr_proc);

        /*current process terminated, call scheduler to dispatch new process*/        
        scheduler();
        break;
    }
        
    /*Passeren (P) (SYS3)*/
    case PASSERN:
    /*P: decrease value of semaphore, if < 0, block the process (sleep)*/
    {
        int* semAdd = ((state_t*) BIOSDATAPAGE)->s_a1;
        
        /*if sem-1 >= 0 -> process is running (not blocked), return control to current process*/
        if ((*semAdd) - 1 >= 0)
        {
            (*semAdd)--;
            increasePC();   
            LDST((state_t*) BIOSDATAPAGE);
        }
            
        /*else: process blocked on ASL call Passeren operation*/
        else
            Passeren(semAdd);
        break;
    }
        
    /*Verhogen (V) (SYS4)*/
    case VERHOGEN:
    {
        /*Call Verhogen() to increment semaphore and wake process*/
        Verhogen(((state_t*) BIOSDATAPAGE)->s_a1);
            
        /*return control to current process*/
        increasePC();
        LDST((state_t*) BIOSDATAPAGE);
        break;
    }
        
    /*Wait for IO Device (SYS5)*/
    case WAITIO:
    {
        /*copy saved exception processor state into current proccess state*/
        copyState(&(curr_proc->p_s), (state_t*) BIOSDATAPAGE);

        int interrupt_line = curr_proc->p_s.s_a1;       /*record interrupt line in a1*/
        int device_number = curr_proc->p_s.s_a2;        /*record device number in a2*/
        int terminal_read = curr_proc->p_s.s_a3;        /*record if waiting for a terminal read operation */
        int* device_semAdd;

        /* Calculate semaphore address based on device type */
        if(interrupt_line != TERMINT)
            /* Non-terminal device: offset = (line-3)*8 + dev# */
            device_semAdd = &(device_sem[(interrupt_line - 3) * DEVPERINT + device_number]);

        else
            /* Terminal device: offset = (line-3)*8 + dev#*2 + r/w */
            device_semAdd = &(device_sem[(interrupt_line - 3) * DEVPERINT + device_number * SUBDEVPERTERM + terminal_read]);
            
        /* block process on device semaphore */
        Passeren(device_semAdd);
        break;
    }
        
    /*Get CPU Time (SYS6)*/
    case GETCPUTIME:
    {
        /* update current process time by adding used quantum */
        updateTime(curr_proc);

        /* store total CPU time in v0 register for return value */
        ((state_t*) BIOSDATAPAGE)->s_v0 = curr_proc->p_time; 

        /*return control to current process*/
        increasePC();
        LDST((state_t*) BIOSDATAPAGE);
        break;
    }
    
    /*Wait For Clock (SYS7)*/
    case WAITCLOCK:
    {
        /*performs P on pseudo-clock semaphore to block the process on the ASL*/
        Passeren(&device_sem[PSEUDOCLK]);
        break;
    }
        
    /*Get SUPPORT Data (SYS8)*/
    case GETSPTPTR:
    {
        /*return p_supportStruct if exists, otherwise return NULL*/
        ((state_t*) BIOSDATAPAGE)->s_v0 = curr_proc->p_supportStruct;      

        /*return control to current process*/
        increasePC();
        LDST((state_t*) BIOSDATAPAGE);
        break;
    }
    
    /*default handler for undefined system calls*/
    default:
    {
        /* move to next instruction */
        increasePC();

        /* pass up undefined syscall up */
        passUpOrDie(GENERALEXCEPT);
        break;
    }
        
    }
}

/**********************************************/
/* Pass Up Or Die Handler                     */
/* Purpose: Either passes exception up to     */
/* support level or terminates process if no  */
/* support structure exists                   */
/*                                           */
/* Parameters:                               */
/* - passup_type: type of exception to pass  */
/*   (TLB, or General )                       */
/*                                           */
/* Returns: void (never returns if process   */
/* is terminated)                            */
/**********************************************/
void passUpOrDie(int passup_type)
{
    /* Check if process has support structure */
    if(curr_proc->p_supportStruct == NULL)
    {

        /* No support structure - terminate process and descendants */
        killDescendants(curr_proc->p_child);
        killProcess(curr_proc);

        /* current process terminated, call scheduler to dispatch a new process*/        
        scheduler();     
    }
        
    else
    /* Has support structure - pass exception up */
    {
        /*copy saved exception state from BIOS Data Page to correct sup_exceptState of current process*/
        state_t* sup_exceptState_p = &(curr_proc->p_supportStruct->sup_exceptState[passup_type]);
        copyState(sup_exceptState_p, (state_t*) BIOSDATAPAGE);

        /*perform LDCXT using field from correct sup_exceptContext field of curr_proc*/
        context_t* sup_exceptContext_p = &(curr_proc->p_supportStruct->sup_exceptContext[passup_type]);
        LDCXT(sup_exceptContext_p->c_stackPtr, sup_exceptContext_p->c_status, sup_exceptContext_p->c_pc);
    }
}

/*************************************************/
/* Kill Process Handler                           */
/* Purpose: Terminates a process, cleans up its   */
/* descendants and updates system counters        */
/*                                               */
/* Parameters:                                    */
/* - proc: pointer to process to terminate        */
/*                                               */
/* Returns: void                                  */
/*************************************************/
void killProcess(pcb_PTR proc)
{
    /* Handle process blocked on semaphore:
     * - For non-device semaphores: increment and remove from blocked list
     * - For device semaphores: leave semaphore unchanged
     */
    if (proc->p_semAdd != NULL)
    {
        if(!(isDeviceSem(proc->p_semAdd)))
        /* Only adjust if it is non-device semaphores */
        {
            (*proc->p_semAdd)++;
            outBlocked(proc);
            softblock_cnt--;
        }
    }
    else {    
        /* Remove from ready queue if not blocked */
        outProcQ(&ready_queue, proc);
    }

    /* Clean up process descendants */
    outChild(proc);
    freePcb(proc);      /* Return PCB to free list */

    /*adjust proccess count*/
    process_cnt--;
}

/*************************************************/
/* Kill Descendants Handler                       */
/* Purpose: Recursively terminates all children   */
/* and siblings of a given process               */
/*                                               */
/* Algorithm:                                     */
/* 1. Base case: NULL child pointer              */
/* 2. Recursively process child's children       */
/* 3. Recursively process child's siblings       */
/* 4. Kill the child process itself              */
/*                                               */
/* Parameters:                                    */
/* - first_child: pointer to first child process */
/*                                               */
/* Returns: void                                  */
/*************************************************/
void killDescendants(pcb_PTR first_child)
{
    /* base case: no children to process */
    if (first_child == NULL)
        return;

    killDescendants(first_child->p_child);
    /*terminate child's children*/

    killDescendants(first_child->p_sib);
    /*terminate child's sibling*/

    /* terminate the current child process */
    killProcess(first_child);
    return;
}

/*************************************************/
/* Passeren (P) Operation Handler                 */
/* Purpose: Performs P operation on semaphore,    */
/* blocking process if necessary                  */
/*                                               */
/* Parameters:                                    */
/* - semAdd: pointer to semaphore to decrement   */
/*                                               */
/* Returns: void                                  */
/* Note: May not return if process blocks        */
/*************************************************/
void Passeren(int* semAdd)
{
    /* Decrement semaphore value */
    (*semAdd)--;

    /* Save process state before potential block*/
    increasePC();   

    /*Copy current state to process PCB*/
    copyState(&(curr_proc->p_s), (state_t*) BIOSDATAPAGE);      
    
    /*(blocking) Update the accumulated CPU time for the Current Process*/
    updateTime(curr_proc);

    if(insertBlocked(semAdd, curr_proc) == FALSE)  /*blocked successfully*/
    {
        softblock_cnt++;        /* Increment blocked process count */
        scheduler();            /* Schedule next process */
    }
    else /* can't block due to lack of available semaphore, stop */
        PANIC();
}

/*************************************************/
/* Verhogen (V) Operation Handler                 */
/* Purpose: Performs V operation on semaphore,    */
/* unblocking process if any are blocked          */
/*                                               */
/* Parameters:                                    */
/* - semAdd: pointer to removed process           */
/*                                               */
/* Returns:                                       */
/* - pcb_PTR: pointer to unblocked process       */
/*   or NULL if no process was blocked           */
/*************************************************/
pcb_PTR Verhogen(int* semAdd)
{
    pcb_PTR removed;

    /* Increment semaphore value */
    (*semAdd)++;

    if ((*semAdd) <= 0)
    /* if processes are blocked on semaphore, remove first blocked process */
    {
         /* Remove first blocked process */
        removed = removeBlocked(semAdd);

         /* If process found, make it ready */
        if(removed != NULL)
        {
            insertProcQ(&ready_queue, removed);     /* Add to ready queue */
            softblock_cnt--;                        /* Decrease blocked count */
        }
    }
    return removed;
}

/*HELPER FUNCTIONS*/

/*************************************************/
/* Increase Program Counter Handler               */
/* Purpose: Advances the program counter to the   */
/* next instruction by incrementing PC by 4       */
/* (MIPS instructions are 4 bytes)               */
/*                                               */
/* Parameters: none                              */
/* Returns: void                                 */
/*************************************************/
void increasePC()  
{
    /* Add 4 to program counter to move to next instruction*/
    ((state_t*) BIOSDATAPAGE)->s_pc += 4;
}

/*************************************************/
/* Update Process Time Handler                    */
/* Purpose: Updates the CPU time used by a        */
/* process by adding it with the time delta between */
/* start and stop timestamps                      */
/*                                               */
/* Parameters:                                    */
/* - proc: pointer to process to update time     */
/*                                               */
/* Returns: void                                  */
/*************************************************/
void updateTime(pcb_PTR proc)
{
    /* Get current CPU time */
    cpu_t time_stop;
    STCK(time_stop);

    /* Add delta between start and stop times to process total time */
    proc->p_time += (time_stop - time_start);
}

/*************************************************/
/* Device Semaphore Check Handler                 */
/* Purpose: Determines if a given semaphore       */
/* belongs to a device by checking if its address */
/* falls within the device semaphore array        */
/*                                               */
/* Parameters:                                    */
/* - semAdd: pointer to semaphore to check       */
/*                                               */
/* Returns:                                       */
/* - TRUE if semaphore is a device semaphore     */
/* - FALSE otherwise                             */
/*************************************************/
int isDeviceSem(int* semAdd)
{
    int i, found = FALSE;

    for (i = 0; i < DEVSEMNO; i++)
     /* Check if semaphore address matches any device semaphore */
        found |= (&device_sem[i] == semAdd);    /* Bitwise OR with comparison result */
    return found;                               /* Return TRUE if match found, FALSE otherwise */
}

/*************************************************/
/* Copy State Handler                            */
/* Purpose: Copies processor state from source    */
/* to destination state structure                 */
/*                                               */
/* Parameters:                                    */
/* - dest: pointer to destination state          */
/* - src: pointer to source state                */
/*                                               */
/* Returns: void                                  */
/*************************************************/
void copyState(state_t* dest, state_t* src)
{
    /* Copy special registers */
    dest->s_entryHI = src->s_entryHI;
    dest->s_cause   = src->s_cause;
    dest->s_status  = src->s_status;
    dest->s_pc      = src->s_pc;
    
    /* Copy general purpose registers */
    int i;
    for (i = 0; i < STATEREGNUM; i++)
        dest->s_reg[i] = src->s_reg[i];
}
/******************************* INTERRUPTS.c *********************************
*
* Module: Interrupt handling and device management for Pandos OS
*
* Brief: A device or timer interrupt occurs when either a previously 
*        initiated I/O request completes or when either a Processor Local Timer (PLT) 
*        or the Interval Timer makes a 0x0000.0000 -> 0xFFFF.FFFF transition.
*
* Components:
* - Interrupt Handler: Routes interrupts by priority
* - Timer Handlers: PLT and Interval timer processing
* - Device Interrupt Handlers: Manage interrupts from Disk, flash, network, printer, terminal
* - Status Management: Device interrupt status and interrupts acknowledgement
*
* Interrupt Priority (highest to lowest):
* 1. PLT Timer (Line 1)
* 2. Interval Timer (Line 2) 
* 3. Disk Devices (Line 3)
* 4. Flash Devices (Line 4)
* 5. Network Devices (Line 5)
* 6. Printer Devices (Line 6)
* 7. Terminal Devices (Line 7)
*
* Dependencies:
* - curr_proc: Currently executing process
* - device_sem: Device semaphore array
* - time_start: CPU time accounting
* @note: For I/O interrupt, CPU time will be updated into newly unblocked process
*        For Interval Timer interrupt: time spent will be charged into current process
*****************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/exceptions.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"


/*************************************************/
/* Interrupt Handler                              */
/* Purpose: Routes interrupts to their specific   */
/* handlers based on the highest priority pending */
/* interrupt line                                 */
/*                                               */
/* Parameters:                                   */
/* - cause: Cause register value containing      */
/*         pending interrupt information         */
/*                                               */
/* Returns: void                                 */
/*                                               */
/* Priority (highest to lowest):                 */
/* 1. PLT Timer (Line 1)                         */
/* 2. Interval Timer (Line 2)                    */
/* 3. Disk Devices (Line 3)                      */
/* 4. Flash Devices (Line 4)                     */
/* 5. Network Devices (Line 5)                   */
/* 6. Printer Devices (Line 6)                   */
/* 7. Terminal Devices (Line 7)                  */
/*************************************************/
void interruptHandler(unsigned int cause)
{
    /*determine highest priority pending interrupt*/
    debug(200, 200, 200, 200);

    /* Check PLT Interrupt - Interrupt Line 1 */
    if      ((cause >> (PLTINT + GETINTLINE)         & CLEAR31MSB) == ON){
        debug(201, 201, 201, 201);
        PLTInterruptHandler();}  

    /* Check Interval Timer - Interrupt Line 2 */
    else if ((cause >> (INTERVALTMRINT + GETINTLINE) & CLEAR31MSB) == ON){
        IntervalTimerInterruptHandler();}  

    /* Check Disk Devices - Interrupt Line 3 */
    else if ((cause >> (DISKINT + GETINTLINE)        & CLEAR31MSB )== ON){nonTimerInterruptHandler(DISKINT, getPendingDevice(INTERRUPTLINE3));} 

    /* Check Flash Drive - Interrupt Line 4 */
    else if ((cause >> (FLASHINT + GETINTLINE)       & CLEAR31MSB) == ON){
        nonTimerInterruptHandler(FLASHINT, getPendingDevice(INTERRUPTLINE4));}

    /* Check Network Devices - Interrupt Line 5 */
    else if ((cause >> (NETWINT + GETINTLINE)        & CLEAR31MSB) == ON){nonTimerInterruptHandler(NETWINT, getPendingDevice(INTERRUPTLINE5));}

    /* Check Printer Devices - Interrupt Line 6 */
    else if ((cause >> (PRNTINT + GETINTLINE)        & CLEAR31MSB) == ON){nonTimerInterruptHandler(PRNTINT, getPendingDevice(INTERRUPTLINE6));} 

    /* Check Terminal Devices - Interrupt Line 7 */
    else if ((cause >> (TERMINT + GETINTLINE)        & CLEAR31MSB) == ON){nonTimerInterruptHandler(TERMINT, getPendingDevice(INTERRUPTLINE7));}  
    /*No Pending Interrupt*/
    else                                                                 {return;}                                                               
}

/*************************************************/
/* Get Pending Device Handler                     */
/* Purpose: Determines which device on an         */
/* interrupt line has a pending interrupt by      */
/* checking the interrupt line's bitmap           */
/*                                               */
/* Parameters:                                    */
/* - int_line_bitmap: int of interrupt line      */
/*                                               */
/* Returns:                                       */
/* - 0-7: device number with pending interrupt   */
/* - -1: no pending device interrupts            */
/*************************************************/
int getPendingDevice(memaddr* int_line_bitmap)
{
    /* Check each bit position (0-7) in bitmap */
    /* Return device number of first bit that is ON */
    if      ((*int_line_bitmap >> 0 & CLEAR31MSB) == ON) return 0;  /* Device 0 */
    else if ((*int_line_bitmap >> 1 & CLEAR31MSB) == ON) return 1;  /* Device 1 */
    else if ((*int_line_bitmap >> 2 & CLEAR31MSB) == ON) return 2;  /* Device 2 */
    else if ((*int_line_bitmap >> 3 & CLEAR31MSB) == ON) return 3;  /* Device 3 */
    else if ((*int_line_bitmap >> 4 & CLEAR31MSB) == ON) return 4;  /* Device 4 */
    else if ((*int_line_bitmap >> 5 & CLEAR31MSB) == ON) return 5;  /* Device 5 */
    else if ((*int_line_bitmap >> 6 & CLEAR31MSB) == ON) return 6;  /* Device 6 */
    else if ((*int_line_bitmap >> 7 & CLEAR31MSB) == ON) return 7;  /* Device 7 */
    else                                                 return -1;  /* No pending interrupts */
}

/*************************************************/
/* Non-Timer Interrupt Handler                    */
/* Purpose: Handles device interrupts by          */
/* acknowledging the interrupt, saving status,    */
/* and unblocking waiting processes              */
/*                                               */
/* Implementation:                                */
/* 1. Record interrupt start time                 */
/* 2. Calculate device register address          */
/* 3. Handle device type:                         */
/*    Terminal: Handle read/write separately      */
/*    Non-terminal: Single operation             */
/* 4. Save device status and acknowledge          */
/* 5. Unblock waiting process if any             */
/* 6. Return control to current process          */
/*                                               */
/* Parameters:                                    */
/* - interrupt_line: interrupt line number (3-7)  */
/* - dev_no: device number on the line (0-7)     */
/*                                               */
/* Device Types:                                  */
/* - Terminal (Line 7): Read/Write operations    */
/* - Non-Terminal:                               */
/*   - Disk (Line 3)                            */
/*   - Flash (Line 4)                           */
/*   - Network (Line 5)                         */
/*   - Printer (Line 6)                         */
/* - Note: CPU time spent will be charged into  */
/*  unblocked process (if there is)             */
/* Returns: void                                  */
/*************************************************/
void nonTimerInterruptHandler(int interrupt_line, int dev_no)
{
    debug(500, 500, 500, 500);
    STCK(time_start);
    /*store time spent in interrupt handling*/

    device_t* device_register = DEVREGBASE + (interrupt_line - 3) * DEVREGINTSCALE + dev_no * DEVREGDEVSCALE;
    /*compute the starting address of the device’s device register*/
    
    unsigned int saved_status;
    int* device_semAdd;

    /* Handle terminal devices (Line 7) */
    if(interrupt_line == TERMINT)
    {
        memaddr* TRANSM_COMMAND = &device_register->t_transm_command;
        memaddr* TRANSM_STATUS = &device_register->t_transm_status;
        memaddr* RECV_COMMAND = &device_register->t_recv_command;
        memaddr* RECV_STATUS = &device_register->t_recv_status;

        /*if transmit not ready (there is transmit interrupt), acknowledge */
        if((*TRANSM_STATUS & TERMSTATMASK) != READY)
        {
            saved_status = *TRANSM_STATUS;      /*save off status code from device register*/
            *TRANSM_COMMAND = ACK;              /*ACK transmit command*/
            device_semAdd = &device_sem[(interrupt_line - 3) * DEVPERINT + dev_no * SUBDEVPERTERM + TERMWRITE]; /*get device semaphore*/
        }
        
        /*else if transmit ready (there is receive interrupt), acknowledge*/
        else if ((*TRANSM_STATUS & TERMSTATMASK) == READY)
        {
            saved_status = *RECV_STATUS;        /*save off status code from device register*/
            *RECV_COMMAND = ACK;                /*ACK receive command*/
            device_semAdd = &device_sem[(interrupt_line - 3) * DEVPERINT + dev_no * SUBDEVPERTERM + TERMREAD];  /*get device semaphore*/
        }
    }

    /* Handle non-terminal devices */
    else
    {
        saved_status = device_register->d_status;  /*save off status code from device register*/
        device_register->d_command = ACK;          /*ACK device command*/
        device_semAdd = &device_sem[(interrupt_line - 3) * DEVPERINT + dev_no];  /*get device semaphore*/
    }
        
    /*perform V on Nucleus maintained semaphore of this device*/
    pcb_PTR unblocked_pcb = Verhogen(device_semAdd);
   
    if(unblocked_pcb != NULL)
    {
        unblocked_pcb->p_s.s_v0 = saved_status;  
        /*place saved off status code into the new unblocked pcb's v0 reg*/

        updateTime(unblocked_pcb);    
        /*charge to new unblocked pcb as it requested the IO*/
    }

    if(curr_proc != NULL)
    /*return to current process*/
    {
        debug(501, 501, 501, 501);
        LDST((state_t *)BIOSDATAPAGE);
    } 
    else{
        scheduler();  /*if there's no current process, scheduler calls WAIT()*/
    }
}

/*************************************************/
/* PLT (Processor Local Timer) Interrupt Handler  */
/* Purpose: Handles PLT interrupts by resetting   */
/* the timer and scheduling the next process      */
/*                                               */
/* Implementation:                                */
/* 1. Reset PLT timer with PLTSTART value        */
/* 2. Save current process state                  */
/* 3. Update CPU time for current process        */
/* 4. Move process to ready queue                */
/* 5. Call scheduler for next process            */
/*                                               */
/* Parameters: none                              */
/* Returns: never returns                        */
/*************************************************/
void PLTInterruptHandler()
{
    setTIMER(PLTSTART); 
    /*acknowledge PLT interrupt by loading timer with new value*/

    /*copy processor state (BIOS Data Page) into process state */
    copyState(&(curr_proc->p_s), (state_t*) BIOSDATAPAGE);

    /*update accumulated CPU time for current process*/
    updateTime(curr_proc);

    insertProcQ(&ready_queue, curr_proc); 
    /*place the Current Process on the Ready Queue*/

    scheduler();    /*call scheduler*/
}


/*************************************************/
/* Interval Timer Interrupt Handler               */
/* Purpose: Handles interval timer interrupts by  */
/* resetting timer and unblocking all processes  */
/* waiting on the pseudo-clock semaphore         */
/*                                               */
/* Implementation:                                */
/* 1. Reset interval timer to 100ms              */
/* 2. Unblock all processes on pseudo-clock      */
/* 3. Reset pseudo-clock semaphore               */
/* 4. Return control to process or scheduler     */
/* - Note: CPU time spent will be charged into    */
/*  current process                              */
/*                                               */
/* Parameters: none                              */
/* Returns: void (never returns directly)        */
/*************************************************/
void IntervalTimerInterruptHandler()
{
    STCK(time_start); 
    /*store time spent in interrupt handling*/

    LDIT(CLOCKINTERVAL);  
    /*acknowledge interrupt by loading interval timer with 100 millisecs*/

    /*unblock all pcbs blocked on pseudo-clock (49) semaphore*/
    pcb_PTR p;
    while ((p = removeBlocked(&device_sem[PSEUDOCLK])) != NULL)
    {
        /* Add each blocked process to ready queue */
        insertProcQ(&ready_queue, p);
        softblock_cnt--;             /* Decrement blocked count */
    }

    /*reset pseudo-clock semaphore to 0*/
    device_sem[PSEUDOCLK] = 0;

    if(curr_proc != NULL)
    {
        /*return control to current process if it's not NULL*/
        updateTime(curr_proc);
        LDST((state_t*) BIOSDATAPAGE);
    }

    else{
        /*call scheduler if there's no current process to return to*/
        scheduler();
    }
} 


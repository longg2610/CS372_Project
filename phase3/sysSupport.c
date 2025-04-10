#include "../h/sysSupport.h"
#include "../h/initProc.h"
#include "../h/const.h"
#define VPNSTART    0x80000
#define VPNTOP      0xBFFFF
HIDDEN void sPtrIncreasePC(support_t* curr_support_struct);
void support_syscall_handler()
{
    support_t * curr_proc_support_struct = SYSCALL(8, 0, 0, 0);
    if ( (curr_proc_support_struct->sup_exceptState[1].s_entryHI >> 12) <  VPNSTART || (curr_proc_support_struct->sup_exceptState[1].s_entryHI >> 12) > VPNTOP){
        terminate(NULL, curr_proc_support_struct);
    };
    int a0;
    a0 = (curr_proc_support_struct->sup_exceptState[1]).s_a0;
    /* debug(9, a0, 9, 9);*/


    switch (a0){
        case 9:
        {
            terminate(NULL, curr_proc_support_struct);
            break;
        }
        case 10:
        {
            get_TOD(curr_proc_support_struct);
            break;
        }
        case 11:
        {
            write_to_printer(curr_proc_support_struct);
            break;
        }
        case 12:
        {
            write_to_terminal(curr_proc_support_struct);
            break;
        }
        case 13:
        {
            read_from_terminal(curr_proc_support_struct);
            break;
        }
        default:
        {
            program_trap(NULL, curr_proc_support_struct);
            break;
        }
    }
}

void terminate(int* semaphore, support_t* curr_support_struct){

    /*Whenever a U-proc terminates, either normally, or abnormally, it should
    first perform a SYS4 (V operation) on the masterSemaphore. */
    SYSCALL(4, (int) &masterSemaphore, 0, 0);

    if (semaphore != NULL)
        SYSCALL(4, semaphore, 0, 0);

    SYSCALL(2, 0, 0, 0);
    sPtrIncreasePC(curr_support_struct);
    LDST(&(curr_support_struct->sup_exceptState[1]));
}

void get_TOD(support_t* curr_support_struct){
    cpu_t time_end;
    STCK(time_end);
    curr_support_struct->sup_exceptState[1].s_v0 = time_end;
    sPtrIncreasePC(curr_support_struct);
    LDST(&(curr_support_struct->sup_exceptState[1]));
}

void write_to_printer(support_t* curr_support_struct){

    unsigned int virtual_addr;
    /*virtual addr of first char in a1*/
    virtual_addr = (memaddr) curr_support_struct->sup_exceptState[1].s_a1;

    /*length of string in a2*/
    int string_len = curr_support_struct->sup_exceptState[1].s_a2;
    

    /*calc printer address*/
    device_t* printer_reg;
    printer_reg = (device_t *) (DEVREGBASE + (FLASHINT-3) * DEVREGINTSCALE + (curr_support_struct->sup_asid -1) * DEVREGDEVSCALE);

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((PRNTINT - 3) * DEVPERINT) + (curr_support_struct->sup_asid - 1);

    /*if length of string > 128 or < 0, terminate the u-proc, check if the address is in range of the logical addr space*/
    if ((string_len > 128) || (string_len <= 0) || virtual_addr < KUSEG){
        terminate(NULL, curr_support_struct);
        /*terminate*/
    }
    
    /*loop and read all characters*/
    int i = 0 ;
    char c;
    int status;
    int errorCond = FALSE;
    char *va = (char *)virtual_addr;

    while (i < string_len && !errorCond)
    {
        c = *va ;

        /*P printer semaphore*/
        SYSCALL(3, (int)&deviceSemaphores[dev_sem_index], 0, 0);
        printer_reg->d_data0 = c;                   /*put character into the data0*/

        disableInt();
        printer_reg->d_command = 2;        /*write command - transmit the char in DATA0 over the line*/

        status = SYSCALL(5, PRNTINT, (curr_support_struct->sup_asid - 1), FALSE);

        enableInt();
        if (status != 1)
        {
            errorCond = TRUE;
        }
        else{
            i++;
        }
        va++;
        SYSCALL(4, (int)&deviceSemaphores[dev_sem_index], 0, 0);
    }

    /*if device not ready, the negative of the device's status val is returned in v0*/
    if (status != 1){
        (&(curr_support_struct->sup_exceptState[1]))->s_v0 = 0 - status;
    }
    else {
        (&(curr_support_struct->sup_exceptState[1]))->s_v0 = i;
    }
    sPtrIncreasePC(curr_support_struct);
    LDST( &(curr_support_struct->sup_exceptState[1]));
}

void write_to_terminal(support_t* curr_support_struct){
    int transmitted_chars;

    /*length of string in a2*/
    int string_len = curr_support_struct->sup_exceptState[1].s_a2;

    /*virtual addr of first char in a1*/
    memaddr virtual_addr = curr_support_struct->sup_exceptState[1].s_a1;
    debug(99, 99, virtual_addr, 99);

    /*calc printer address*/
    device_t* terminal_reg;
    /*(DEVREGBASE + (FLASHINT-3) * DEVREGINTSCALE + (asid-1) * DEVREGDEVSCALE);*/
    terminal_reg = (device_t *) (DEVREGBASE + (TERMINT - 3) * DEVREGINTSCALE + (curr_support_struct->sup_asid - 1) * DEVREGDEVSCALE);

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((TERMINT - 3) * DEVPERINT) + (curr_support_struct->sup_asid - 1) * 2;

    /*if length of string > 128 and < 0, terminate the u-proc, check if the address is in range of the logical addr space*/;
    if ((string_len > 128) || (string_len <= 0) || virtual_addr < KUSEG ){
        terminate(NULL, curr_support_struct);
        /*terminate*/
    }
    
    char c;
    int i=0;
    int status;
    int errorCond = FALSE;
    char *va =  virtual_addr;
    while (i < string_len && !errorCond)
    {
        c = *va;
        SYSCALL(3, (int)&deviceSemaphores[dev_sem_index], 0, 0);

        /*write char to the command field and set COMMAND code*/
        /*terminal_reg->t_transm_command = (terminal_reg->t_transm_command & 0b1111111111111111111111100000000) | 2; */
        disableInt();
        terminal_reg->t_transm_command = (c << 8) | 2;

        /*set transmit command*/
        /*terminal_reg->t_transm_command = (terminal_reg->t_transm_command & 0b1111111111111111111111100000000) | 2; */

        status = SYSCALL(5, TERMINT, (curr_support_struct->sup_asid-1), FALSE);
        enableInt();
        SYSCALL(4, (int)&deviceSemaphores[dev_sem_index], 0, 0);

        if ((status & TERMSTATMASK) != 5){
            errorCond = TRUE;
        }
        else {
            i++;
        }
        va++;
    }

    if ((status & TERMSTATMASK) == 5){
        curr_support_struct->sup_exceptState[1].s_v0 = i;
    }
    else{
        /*when status not 'char transmitted', negative of teh device status value is returned in v0*/
        curr_support_struct->sup_exceptState[1].s_v0 = 0 - (status);
    }
    sPtrIncreasePC(curr_support_struct);
    LDST(&(curr_support_struct->sup_exceptState[1]));
}

void read_from_terminal(support_t * curr_support_struct){
    debug(90, 90, 90, 90);
    int transmitted_char = 0;

    /*support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);*/

    /*virtual addr in a1*/
    unsigned int virtual_addr = (memaddr) curr_support_struct->sup_exceptState[1].s_a1;

    /*check if the address is in range of the logical addr space*/
    if (virtual_addr < KUSEG)
    {
        terminate(NULL, curr_support_struct);
    }

    /*calc printer address*/
    device_t* terminal_reg;
    terminal_reg = (device_t *) (DEVREGBASE + (TERMINT - 3) * DEVREGINTSCALE + (curr_support_struct->sup_asid - 1) * DEVREGDEVSCALE);

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((TERMINT - 3) * DEVPERINT) + (curr_support_struct->sup_asid - 1) * 2 + 1;

    /*read the string until meet the ned of string char '\0'*/
    int status;
    int errorCond = FALSE;
    char * va = (char *)virtual_addr;
    while (*va != 0x0A && !errorCond)
    {
        SYSCALL(3, (int)&deviceSemaphores[dev_sem_index], 0, 0);

        /*set the receive command to 2*/
        disableInt();
        terminal_reg->t_recv_command = 2;
        status = SYSCALL(5, TERMINT, (curr_support_struct->sup_asid) - 1, TRUE);
        enableInt();

        SYSCALL(4, (int)&deviceSemaphores[dev_sem_index], 0, 0);

        /*if the status is Character Received: increment the transmitted count*/
        if (status & TERMSTATMASK != 5){
            errorCond = TRUE;
        }
        else{
            transmitted_char++;
            *va = (status) >> 8;
            debug(88, 88, va, *va);
            break;
        }
        va++;
    }
    if (status & TERMSTATMASK == 5){
        (&(curr_support_struct->sup_exceptState[1]))->s_v0 = transmitted_char;
    }
    else{
        (&(curr_support_struct->sup_exceptState[1]))->s_v0 = 0 - (status);
    }
    sPtrIncreasePC(curr_support_struct);
    LDST(&(curr_support_struct->sup_exceptState[1]));
}

void program_trap(int* semaphore, support_t* curr_support_struct){
    /*
    int devNo = curr_proc_support_struct->sup_asid -1 ;
    int dev_sem_index = 0;
    int checked_mutex= FALSE;
    int i = 0;
    while (!checked_mutex)
    {
        if (i != (TERMINT - 3)){

            if (device_sem[i * DEVPERINT + devNo - 1] <= 0)
                SYSCALL(4, (int)&device_sem[i*8 + (devNo)], 0, 0);
        }
        else {
            if (device_sem[i * 8 + (devNo - 1)*2] <= 0){
                SYSCALL(4, (int)&device_sem[i*8 + (devNo)*2], 0, 0);
            }
            if (device_sem[i * 8 + devNo * 2 + 1] <= 0){
                SYSCALL(4, (int)&device_sem[i*8 + (devNo) *2 + 1], 0, 0);
            }
            checked_mutex= TRUE;
        }
        i++;
    }
    */

    terminate(semaphore, curr_support_struct);
    return;
};

HIDDEN void sPtrIncreasePC(support_t* curr_support_struct)  
{
    /* Add 4 to program counter to move to next instruction*/
    curr_support_struct->sup_exceptState[1].s_pc += 4;
    /*((state_t *)BIOSDATAPAGE)->s_pc += 4;*/
}

/*Similar to what the Nucleus does when 
returning from a successful SYSCALL request [Section 3.5.10], 
the Sup- port Level’s SYSCALL exception handler must also increment 
the PC by 4 in order to return control to the instruction after the SYSCALL instruction.
*/

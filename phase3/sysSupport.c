#include "../h/sysSupport.h"
#include "../h/scheduler.h"
#include "../h/initProc.h"
#include "../h/const.h"

HIDDEN void increasePC();
void support_syscall_handler()
{
    int a0;
    a0 = ((state_t *)BIOSDATAPAGE)->s_a0;

    switch (a0){
        case 9:
        {
            terminate();
        }
        case 10:
        {
            get_TOD();
        }
        case 11:
        {
            write_to_printer();
        }
        case 12:
        {
            write_to_terminal();
        }
        case 13:
        {
            read_from_terminal();
        }
        default:
        {
            program_trap();
        }
    }
}

void terminate(){
    support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);

    /*Whenever a U-proc terminates, either normally, or abnormally, it should
    first perform a SYS4 (V operation) on the masterSemaphore. */
    SYSCALL(4, (int)&masterSemaphore, 0, 0);

    /*Optimization: when a U-proc terminates, mark all of the frames it occupied as unoccupied*/
    /*-> go thru the swap pool and find frame that is occupied by the terminating U-proc*/
    int i;
    for (i = 0; i < 2*UPROCMAX; i++)
    {
        
        if(swap_pool[i].asid == curr_process_support_struct->sup_asid){
            swap_pool[i].asid = -1;
        }
    }
    SYSCALL(2, 0, 0, 0);
    increasePC();
    LDST((state_t *) &(curr_proc_support_struct->sup_exceptState[1]));
}

void get_TOD(){
    cpu_t time_end;
    STCK(time_end);
    ((state_t *)BIOSDATAPAGE)->s_v0 = time_end;
    increasePC();
    LDST((state_t *)BIOSDATAPAGE);
}

void write_to_printer(){

    unsigned int virtual_addr;
    /*virtual addr of first char in a1*/
    virtual_addr = (memaddr) ((state_t *)BIOSDATAPAGE)->s_a1;

    /*length of string in a2*/
    int string_len = ((state_t *)BIOSDATAPAGE)->s_a2;

    support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);

    /*calc printer address*/
    device_t* printer_reg;
    printer_reg = (device_t *) (DEVREGBASE + (DEVREGSIZE * DEVPERINT * (PRNTINT - 3) + (curr_process_support_struct->sup_asid - 1) * DEVREGSIZE));

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((PRNTINT - 3) * DEVPERINT) + (curr_process_support_struct->sup_asid - 1);

    /*if length of string > 128 or < 0, terminate the u-proc*/
    if ((string_len > 128) || (string_len <= 0)){
        terminate();
        /*terminate*/
    }

    /*check if the address is in range of the logical addr space*/
    if ( virtual_addr  < KUSEG)
    {
        terminate();
    }
    
    /*loop and read all characters*/
    int i = 0 ;
    char c;
    int status;
    int errorCond = FALSE;
    char *va = (char *)virtual_addr;

    while (i < string_len || !errorCond)
    {
        c = *va ;

        /*P printer semaphore*/
        SYSCALL(3, (int)&device_sem[dev_sem_index], 0, 0);

        printer_reg->d_data0 = c;                   /*put character into the data0*/
        printer_reg->d_command = 2;        /*write command - transmit the char in DATA0 over the line*/
        status = SYSCALL(5, PRNTINT, (curr_process_support_struct->sup_asid - 1), FALSE);

        if (status != 1)
        {
            errorCond = TRUE;
        }
        else{
            i++;
        }
        va++;

        SYSCALL(4, (int)&device_sem[dev_sem_index], 0, 0);
    }

    /*if device not ready, the negative of the device's status val is returned in v0*/
    if (status != 1){
        ((state_t *)BIOSDATAPAGE)->s_v0 = 0 - status;
    }
    else {
        ((state_t *)BIOSDATAPAGE)->s_v0 = i;
    }
    increasePC();
    LDST((state_t *) &(curr_proc_support_struct->sup_exceptState[1]));
}

void write_to_terminal(){
    int transmitted_chars;
    support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);

    /*length of string in a2*/
    int string_len = ((state_t *)BIOSDATAPAGE)->s_a2;

    /*virtual addr of first char in a1*/
    unsigned int virtual_addr = (memaddr) ((state_t *)BIOSDATAPAGE)->s_a1;

    /*calc printer address*/
    device_t* terminal_reg;
    terminal_reg = (device_t *) (DEVREGBASE + (DEVREGSIZE * DEVPERINT * (TERMINT - 3) + (curr_process_support_struct->sup_asid - 1) * DEVREGSIZE));

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((TERMINT - 3) * DEVPERINT) + (curr_process_support_struct->sup_asid - 1) * 2;

    /*if length of string > 128 and < 0, terminate the u-proc*/;
    if ((string_len > 128) || (string_len < 0)){
        terminate();
        /*terminate*/
    }

    /*check if the address is in range of the logical addr space*/
    if (virtual_addr < KUSEG)
    {
        terminate();
    }
    
    char c;
    int i;
    int status;
    int errorCond = FALSE;
    char *va = (char *) virtual_addr;
    while (i < string_len || !errorCond)
    {
        c = *va;
        SYSCALL(3, (int)&device_sem[dev_sem_index], 0, 0);

        /*write char to the command field and set COMMAND code*/
        /*terminal_reg->t_transm_command = (terminal_reg->t_transm_command & 0b1111111111111111111111100000000) | 2; */
        terminal_reg->t_transm_command = ( c << 8) | 2;

        /*set transmit command*/
        /*terminal_reg->t_transm_command = (terminal_reg->t_transm_command & 0b1111111111111111111111100000000) | 2; */

        status = SYSCALL(5, TERMINT, (curr_process_support_struct->sup_asid-1), FALSE);
        SYSCALL(4, (int)&device_sem[dev_sem_index], 0, 0);

        if ((status & TERMSTATMASK) != 5){
            errorCond = TRUE;
        }
        else {
            i++;
        }
        va++;
    }

    if ((status & TERMSTATMASK) == 5){
        ((state_t *)BIOSDATAPAGE)->s_v0 = i;
    }
    else{
        /*when status not 'char transmitted', negative of teh device status value is returned in v0*/
        ((state_t *)BIOSDATAPAGE)->s_v0 = 0 - (status);
    }
    increasePC();
    LDST((state_t *) &(curr_proc_support_struct->sup_exceptState[1]));
}

void read_from_terminal(){
    int transmitted_char;

    support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);

    /*virtual addr in a1*/
    unsigned int virtual_addr = (memaddr) ((state_t *)BIOSDATAPAGE)->s_a1;

    /*check if the address is in range of the logical addr space*/
    if (virtual_addr < KUSEG)
    {
        terminate();
    }

    /*calc printer address*/
    device_t* terminal_reg;
    terminal_reg = (device_t*) (DEVREGBASE + (DEVREGSIZE * DEVPERINT * (TERMINT - 3) + (curr_process_support_struct->sup_asid - 1) * DEVREGSIZE));

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((TERMINT - 3) * DEVPERINT) + (curr_process_support_struct->sup_asid - 1) * 2;

    /*read the string until meet the ned of string char '\0'*/
    int status;
    int errorCond = FALSE;
    char *va = (char *)virtual_addr;
    while (*va != 0x0A || !errorCond)
    {
        SYSCALL(3, (int)&device_sem[dev_sem_index], 0, 0);

        /*set the receive command to 2*/
        terminal_reg->t_recv_command = 2;
        status = SYSCALL(5, TERMINT, (curr_process_support_struct->sup_asid) - 1, TRUE);

        SYSCALL(4, (int)&device_sem[dev_sem_index], 0, 0);

        /*if the status is Character Received: increment the transmitted count*/
        if (status & TERMSTATMASK != 5){
            errorCond = TRUE;
        }
        else{
            transmitted_char++;
        }
        va++;
    }
    if (status & TERMSTATMASK == 5){
        ((state_t *)BIOSDATAPAGE)->s_v0 = transmitted_char;
    }
    else{
        ((state_t *)BIOSDATAPAGE)->s_v0 = 0 - (status);
    }
    increasePC();
    LDST((state_t *) &(curr_proc_support_struct->sup_exceptState[1]));
}

void program_trap(){
    terminate();
    return;
};

HIDDEN void increasePC()  
{
    /* Add 4 to program counter to move to next instruction*/
    ((state_t*) BIOSDATAPAGE)->s_pc += 4;
}

/*Similar to what the Nucleus does when 
returning from a successful SYSCALL request [Section 3.5.10], 
the Sup- port Level’s SYSCALL exception handler must also increment 
the PC by 4 in order to return control to the instruction after the SYSCALL instruction.
*/

#include "../h/sysSupport.h"
#include "../h/scheduler.h"
#include "../h/initProc.h"

cpu_t time_start;

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
            program_trap();
        }
    }
}

void terminate(){
    support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);
    SYSCALL(4, (int)&masterSemaphore, 0, 0);
    /*SYSCALL(3, (int)&masterSemaphore, 0, 0);*/
    /*when a U-proc terminates, mark all of the frames it occupied as unoccupied*/
    /*-> go thru the swap pool and find frame that is occupied by the terminating U-proc*/
    int i;
    for (i = 0; i < 2*UPROCMAX; i++)
    {
        
        if(swap_pool[i].asid == curr_process_support_struct->sup_asid){
            swap_pool[i].asid = -1;
        }
    }
    SYSCALL(2, 0, 0, 0);
    return;
}

void get_TOD(){
    cpu_t time_end;
    STCK(time_end);
    ((state_t *)BIOSDATAPAGE)->s_v0 = time_end;
    return -1;
}

void write_to_printer(){
    int transmitted_chars;
    support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);

    /*calc printer address*/
    device_t* printer_reg;
    printer_reg = DEVREGBASE + (DEVREGSIZE * DEVPERINT * (PRNTINT - 3) + (curr_process_support_struct->sup_asid - 1) * DEVREGSIZE);

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((PRNTINT - 3) * DEVPERINT) + (curr_process_support_struct->sup_asid - 1);

    /*if length of string > 128, terminate the u-proc*/
    /*length of string in a2*/
    int string_len = ((state_t *)BIOSDATAPAGE)->s_a2;
    if ((string_len > 128) || (string_len < 0)){
        terminate();
        /*terminate*/
    }

    /*virtual addr of first char in a1*/
    int* virtual_addr = ((state_t *)BIOSDATAPAGE)->s_a1;

    /*check if the address is in range of the logical addr space*/
    if ((&virtual_addr) < (curr_process_support_struct->sup_privatePgTbl[0].entryHI >> 12) && (&virtual_addr) > (curr_process_support_struct->sup_privatePgTbl[31].entryHI >> 12))
        terminate();

    
    /*loop and read all characters*/
    int i;
    char c;
    int status;
    for (i = 0; i < string_len; i++)
    {
        
        c = *(virtual_addr + i) ;
        /*P printer semaphore*/
        SYSCALL(3, (int)&device_sem[dev_sem_index], 0, 0);

        printer_reg->d_data0 = c;                   /*put character into the data0*/
        printer_reg->d_command = 0x00000002;        /*write command - transmit the char in DATA0 over the line*/
        status = SYSCALL(5, PRNTINT, (curr_process_support_struct->sup_asid - 1), FALSE);

        if (printer_reg->d_status == 1)
        {
            transmitted_chars++;
        }

        SYSCALL(4, (int)&device_sem[dev_sem_index], 0, 0);
    }

    /*if device not ready, the negative of the device's status val is returned in v0*/
    if (printer_reg->d_status != 1){
        ((state_t *)BIOSDATAPAGE)->s_v0 = -(printer_reg->d_status);
    }
    else {
        ((state_t *)BIOSDATAPAGE)->s_v0 = transmitted_chars;
    }
}

void write_to_terminal(){
    int transmitted_chars;
    support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);

    /*calc printer address*/
    device_t* terminal_reg;
    terminal_reg = DEVREGBASE + (DEVREGSIZE * DEVPERINT * (PRNTINT - 3) + (curr_process_support_struct->sup_asid - 1) * DEVREGSIZE);

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((PRNTINT - 3) * DEVPERINT) + (curr_process_support_struct->sup_asid - 1);

    /*if length of string > 128, terminate the u-proc*/
    /*length of string in a2*/
    int string_len = ((state_t *)BIOSDATAPAGE)->s_a2;
    if ((string_len > 128) || (string_len < 0)){
        terminate();
        /*terminate*/
    }

    /*virtual addr of first char in a1*/
    int* virtual_addr = ((state_t *)BIOSDATAPAGE)->s_a1;

    /*check if the address is in range of the logical addr space*/
    if ((&virtual_addr) < (curr_process_support_struct->sup_privatePgTbl[0].entryHI >> 12) && (&virtual_addr) > (curr_process_support_struct->sup_privatePgTbl[31].entryHI >> 12))
        terminate();

    
    char c;
    int i;
    int status;
    for (i = 0; i < string_len; i++)
    {
        c = *(virtual_addr + i);
        SYSCALL(3, (int)&device_sem[dev_sem_index], 0, 0);

        /*write char to the command field*/
        terminal_reg->t_transm_command = c << 7;

        /*set transmit command*/
        terminal_reg->t_transm_command = (terminal_reg->t_transm_command & 0b1111111111111111111111100000000) | 2;

        status = SYSCALL(5, TERMINT, (curr_process_support_struct->sup_asid-1), FALSE);
        SYSCALL(4, (int)&device_sem[dev_sem_index], 0, 0);

        if ((status & TERMSTATMASK) == 5){
            transmitted_chars++;
        }
    }


    if ((status & TERMSTATMASK) == 5){
        ((state_t *)BIOSDATAPAGE)->s_v0 = transmitted_chars;
    }
    else{
        /*when status not 'char transmitted', negative of teh device status value is returned in v0*/
        ((state_t *)BIOSDATAPAGE)->s_v0 = -(terminal_reg->t_transm_status & 0b0000000000000000000000011111111);
    }

}

void read_from_terminal(){
    int transmitted_char;

    support_t *curr_process_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);

    /*?: is there string length*/
    int string_len = ((state_t *)BIOSDATAPAGE)->s_a2;
    if ((string_len > 128) || (string_len < 0)){
        terminate();
        /*terminate*/
    }

    /*virtual addr in a1*/
    int *virtual_addr = ((state_t *)BIOSDATAPAGE)->s_a1;

    /*check if the address is in range of the logical addr space*/
    if ((&virtual_addr) < (curr_process_support_struct->sup_privatePgTbl[0].entryHI >> 12) && (&virtual_addr) > (curr_process_support_struct->sup_privatePgTbl[31].entryHI >> 12))
        terminate();

    /*calc printer address*/
    device_t* terminal_reg;
    terminal_reg = DEVREGBASE + (DEVREGSIZE * DEVPERINT * (PRNTINT - 3) + (curr_process_support_struct->sup_asid - 1) * DEVREGSIZE);

    /*get the index for semaphore*/
    int dev_sem_index;
    dev_sem_index = ((PRNTINT - 3) * DEVPERINT) + (curr_process_support_struct->sup_asid - 1);

    /*read the string until meet the ned of string char '\0'*/
    int status;
    while ((*virtual_addr) != '\0')
    {
        SYSCALL(3, (int)&device_sem[dev_sem_index], 0, 0);

        /*set the receive command to 2*/
        terminal_reg->t_recv_command = 2;
        status = SYSCALL(5, TERMINT, (curr_process_support_struct->sup_asid) - 1, TRUE);

        /*if the status is Character Received: increment the transmitted count*/
        if (status & TERMSTATMASK == 5){
            transmitted_char++;
        }
        SYSCALL(4, (int)&device_sem[dev_sem_index], 0, 0);
    }
    if (status & TERMSTATMASK == 5){
        ((state_t *)BIOSDATAPAGE)->s_v0 = transmitted_char;
    }
    else{
        ((state_t *)BIOSDATAPAGE)->s_v0 = -(transmitted_char);
    }
    

    return;
}

void program_trap(){
    terminate();
    return;
}
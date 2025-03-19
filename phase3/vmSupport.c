#include "../h/vmSupport.h"
#include "../h/initial.h"

swap_pool_t swap_pool;
/*frame unoccupied has an entry of -1 in ASID in the Swap Pool table -> all frames are unoccupied initially*/
/*init all swap pool entries ASID to -1*/

/*get support_t of currentProcess*/
/*support_t* pFiveSupAddr = (support_t *)SYSCALL(GETSPTPTR, 0, 0, 0);*/
support_t *curr_proc_support_struct;
void getCurrProcessSupportStruct()
{
    /*get pointer to current process's support struct, syscall can't be called outside as a constant expression*/
    support_t* curr_proc_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);
}
void TLB_refill_handler()
{
    /*TLB refill event has access to Phase 2 global variables, current process*/

    /*determine the page number of the missing TLB entry by inspecting entryHI in the saved excpt state in BIOS DataPg*/
    int p = ((state_t *)BIOSDATAPAGE)->s_entryHI >> 12;

    /*get Page Table entry for the page number p*/
    int i;
    for (i = 0; i < 32; i++){
        if ((curr_proc_support_struct->sup_privatePgTbl[i].entryHI) >> 12 == p){
            setENTRYHI(curr_proc_support_struct->sup_privatePgTbl[i].entryHI);
            setENTRYLO(curr_proc_support_struct->sup_privatePgTbl[i].entryLO);
            TLBWR();
            LDST(((state_t *)BIOSDATAPAGE));
        }
    }
}


/*Swap Pool table must be accessed or updated mutex -> swap pool semaphore*/
void pager(){

    /*cause of the TLB exception: in current process support structure: sup_exceptState[0]'s Cause register*/
    unsigned int cause_reg = curr_proc_support_struct->sup_exceptState[0].s_cause;
    int cause_reg_val = (cause_reg >> 2) & 0b000000000000000000000000000011111; /*keep 5 last bits*/

    /*If cause is TLB-modification exception, call program trap*/
    if(cause_reg_val == 1){

        /*if the exception is TLB-Modification, treat this exception as a program trap (4.8 pandos)*/
    }

    /*P swap pool semaphore*/
    SYSCALL(3, (int)&swap_pool_sem, 0, 0);

    /*determine missing page number: in exception state's EntryHI*/


    /*pick a frame i using the page replacement algorithm: FIFO, keep static var mod, increment by Swap Pool size each Page Fault exception*/

    /*determine of frame i is occupied*/

    /*if frame k is occupied: -> execute atomically
    x =process's ASID 
    k = VPN
    mark Page Table entry k as not valid: valid bit V in EntryLO set to 0
    Update the TLB (use macro functions): TLBCLR() for now, when refeactoring, change into first approach
    
    Write contents of frame i to the correct location on preocess x's flash devices
    call progrma Trap if in user-mode
    */

   /*read contents of CUrre Process's backing store/flashdevice logical page VPN into frame i*/

   /*Update Swap Pool table's entry i : ASID, pointer to current processs table entry for page p*/

    /*update current process's Page Table entry for page p -> indicate it's present (V bit) and occupying frame i (PFN)*/

    /*update TLB*/

    /*relaest mutex for swap pool*/

    /* LDST( ((state_t*) BIOSDATAPAGE)->s_status)*/
}
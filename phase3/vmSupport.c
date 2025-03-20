#include "../h/vmSupport.h"
#include "../h/initial.h"

int swap_pool_sem;
swap_pool_t swap_pool[2 * UPROCMAX];

int SWAPPOOL_STARTADDR = 0x20020000;
static frameNum = 0;

/*get support_t of currentProcess*/
/*support_t* pFiveSupAddr = (support_t *)SYSCALL(GETSPTPTR, 0, 0, 0);*/
support_t *curr_proc_support_struct;

void initSwapStructs(){
    /*init both swap pool table and accompanying semaphore*/
    
    /*init swap pool semaphore to 1*/
    swap_pool_sem = 1;

    /*frame unoccupied has an entry of -1 in ASID in the Swap Pool table -> all frames are unoccupied initially*/
    /*init all swap pool entries ASID to -1 (?)*/
    int i;
    for (i = 0; i < 2 * UPROCMAX;i++){
        swap_pool[i].asid = -1;
    }
}

void getCurrProcessSupportStruct()
{
    /*get pointer to current process's support struct, syscall can't be called outside as a constant expression*/
    support_t* curr_proc_support_struct = (support_t *)SYSCALL(8, 0, 0, 0);
}

/*move this function back to phase 2 (?)*/
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
    int missing_page_num = ((state_t *)BIOSDATAPAGE)->s_entryHI >> 12; 

    /*pick a frame i using the page replacement algorithm: FIFO, keep static var mod, increment by Swap Pool size each Page Fault exception*/
    frameNum = (frameNum + 1) % 32; /*mod the swap pool size*/

    /*determine of frame i is occupied*/
    /*take addr of the frame*/
    swap_pool_t *frameAddr = (int*) (SWAPPOOL_STARTADDR + (frameNum * PAGESIZE));

    /*to check if swap pool is occupied, examine the asid*/
    if (frameAddr->asid != -1){
        /*the frame is occupied*/

        /*disable all interrupts before update page table (?) prev, current or old*/
        unsigned int cp0_status = getSTATUS();
        cp0_status = cp0_status & 0b11111111111111111111111111111110;
        setSTATUS(cp0_status);

        /*set the page table k as not valid*/
        frameAddr->pageTable_ptr = 0b1111111111111111111110111111111;

        /*update TLB: */
        TLBP();
        if (getINDEX() >> 31 == 0) {
            /*(newly updated TLB entry is cached in TLB) TLBWI*/
            TLBWI();
        }
        /*cp0_index */

        /*re-enable interrupts : to update atomically (pandos: 4.5.3)*/
        cp0_status = cp0_status | 0b00000000000000000000000000000001;
        setSTATUS(cp0_status);

        writeFlashDevice(frameAddr->asid, frameAddr);
    }

    /*DOUBLE CHECK: read contents of Current Process's backing store/flashdevice logical page VPN into frame i*/
    readFlashDevice(frameAddr->asid, frameAddr);

    /*Update Swap Pool table's entry i : ASID, pointer to current processs table entry for page p*/
    frameAddr->asid = curr_proc_support_struct->sup_asid;
    /*update page tbale pointer for the swap pool*/
    int j;
    for (j = 0; j < 32; j++)
    {
        if (curr_proc_support_struct->sup_privatePgTbl[j].entryHI << 12 == missing_page_num){
            frameAddr->pageTable_ptr = &(curr_proc_support_struct->sup_privatePgTbl[j]);
            /*update valid bit for the curr process table entry for page p*/
            curr_proc_support_struct->sup_privatePgTbl[j].entryHI |= 0b0000000000000000000001000000000;

            /*update physical frame number*/
            curr_proc_support_struct->sup_privatePgTbl[j].entryLO = (curr_proc_support_struct->sup_privatePgTbl[j].entryLO & 0b0000000000000000000111111111111) | (frameNum << 12);
        }
    }

    /*update current process's Page Table entry for page p -> indicate it's present (V bit) and occupying frame i (PFN)*/


    /*update TLB*/
    setSTATUS(getSTATUS()& 0b11111111111111111111111111111110);

    /*set the page table k as not valid*/
    frameAddr->pageTable_ptr = 0b1111111111111111111110111111111;

    /*update TLB: */
    TLBP();
    if (getINDEX() >> 31 == 0) {
        /*(newly updated TLB entry is cached in TLB) TLBWI*/
        TLBWI();
    }
    /*cp0_index */

    /*re-enable interrupts : to update atomically (pandos: 4.5.3)*/
    setSTATUS(getSTATUS() | 0b00000000000000000000000000000001);

    /*releas mutex for swap pool*/
    SYSCALL(4, (int)&swap_pool_sem, 0, 0);

    LDST(((state_t *)BIOSDATAPAGE)->s_status);
}

void readFlashDevice(int asid, swap_pool_t *frameAddr){
    int dev_sem_index;
    /*calculate the device reg address*/
    device_t* dev_reg;
    dev_reg = DEVREGBASE + (DEVREGSIZE * DEVPERINT * (FLASHINT - 3) + (asid - 1) * DEVREGSIZE);
    dev_sem_index = (FLASHINT - 3) * 8 + (asid - 1);

    /*read content of backing store: read from Block Number into frame i*/
    SYSCALL(3, (int)&device_sem[dev_sem_index], 0, 0);

    /*write DATA0 with frame i to read*/
    dev_reg->d_data0 = (memaddr *)frameAddr;

    /*read block at (BLOCKNUMBER) to value at addr in DATA0*/
    /*copy the block at Block Number into RAM at the address in DATA0*/
    frameAddr->pageNum = dev_reg->d_command >> 7;

    /*call sys5*/
    SYSCALL(5, FLASHINT, 0, 0);

    /*V the device semaphore*/
    SYSCALL(4, (int)&device_sem[dev_sem_index], 0, 0);

    return;
};
void writeFlashDevice(int asid, swap_pool_t *frameAddr){
    /*calculate the device reg address*/
    int dev_sem_index;

    device_t *dev_reg;
    dev_reg = DEVREGBASE + (DEVREGSIZE * DEVPERINT * (FLASHINT - 3) + (asid - 1) * DEVREGSIZE);

    dev_sem_index = (FLASHINT - 3) * 8 + (asid - 1);

    /*P device semaphore*/
    SYSCALL(3, (int)&device_sem[dev_sem_index], 0, 0);

    /*write data0 field with the appropriate starting physical addr : the frame's starting addr*/
    dev_reg->d_data0 = (memaddr *) frameAddr;
    unsigned int frame_content = frameAddr->pageNum;

    /* ?: need to shift the reset ACK out - treat any error staus from the write operation as a program trap*/
    if (dev_reg->d_status >> 7 == 6){
        /*call program trap*/
    }

    /*set device busy*/
    dev_reg->d_status = 4 << 7;

    /*(?) Write the device’s COMMAND field. Since a write into a COMMAND field immediately initiates an I/O operation, 
    one must always supply the appropriate parameters in DATA0 before writing the COMMAND field.*/

    /*write flash device's COMMAND field with the device block number and the command to read in lower order byte*/
    /* ? write the content of block number at starting addr frameAddr (frame i)*/
    dev_reg->d_command = ( dev_reg->d_command & (0b0000000000000000000000011111111) )| (frameAddr->pageNum << 7) ;

    /*? update satus*/
    dev_reg->d_status = 1;

    /*followed by a sys5*/
    SYSCALL(5, FLASHINT, 0, 0);

    /*release the device's device reg: V device sem*/
    SYSCALL(4, (int)&device_sem[dev_sem_index], 0, 0);
};

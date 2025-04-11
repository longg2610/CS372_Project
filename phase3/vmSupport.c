#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "../h/const.h"
#include "../h/initProc.h"

int swap_pool_sem;
swap_pool_t swap_pool[2 * UPROCMAX];

unsigned int SWAPPOOL_STARTADDR = 0x20020000;

HIDDEN int getFrameIndex(){
    static int frameNo = 0;
    int res;
    res = frameNo % (UPROCMAX * 2);
    frameNo++;
    return res;
}

/*get support_t of currentProcess*/
/*support_t* pFiveSupAddr = (support_t *)SYSCALL(GETSPTPTR, 0, 0, 0);*/
void initSwapStructs();
void pager();


void enableInt();
void disableInt();

void initSwapStructs(){

    /*init both swap pool table and accompanying semaphore*/
    
    /*init swap pool semaphore to 1*/
    swap_pool_sem = 1;

    /*frame unoccupied has an entry of -1 in ASID in the Swap Pool table -> all frames are unoccupied initially*/
    /*init all swap pool entries ASID to -1; negative means empty*/
    int i;
    for (i = 0; i < 2 * UPROCMAX;i++){
        swap_pool[i].asid = -1;
    }
};

/*Swap Pool table must be accessed or updated mutex -> swap pool semaphore*/
void pager(){
    /*get pointer to current process's support struct, syscall can't be called outside as a constant expression*/
    debug(22, 22, 22, 22);
    support_t *curr_proc_support_struct = (support_t *)SYSCALL(GETSPTPTR, 0, 0, 0);
    if ( (curr_proc_support_struct->sup_exceptState[0].s_entryHI >> 12) <  VPNSTART || (curr_proc_support_struct->sup_exceptState[1].s_entryHI >> 12) > 0xBFFFF){
        program_trap(NULL, curr_proc_support_struct);
    };

    /*cause of the TLB exception: in current process support structure: sup_exceptState[0]'s Cause register*/
    unsigned int cause_reg = curr_proc_support_struct->sup_exceptState[0].s_cause;
    unsigned int cause_reg_val = (cause_reg >> 2) & 0x0000001F; /*keep 5 last bits*/

    /*If cause is TLB-modification exception, call program trap*/
    if(cause_reg_val == 1){
        program_trap(NULL, curr_proc_support_struct);
        /*if the exception is TLB-Modification, treat this exception as a program trap (4.8 pandos)*/
    }


    /*P swap pool semaphore*/
    SYSCALL(3, (int)&swap_pool_sem, 0, 0);

    /*determine missing page number: in saved exception state's EntryHI (The saved exception state responsible for this TLB exception should be found in the Current Process’s
Support Structure for TLB exceptions)*/
    /*is the saved except state in current process*/
    unsigned int missing_page_num = curr_proc_support_struct->sup_exceptState[0].s_entryHI >> 12; 

    /*pick a frame i using the page replacement algorithm: FIFO, keep static var mod, increment by Swap Pool size each Page Fault exception*/

    int frame_index =  getFrameIndex(); /*mod the swap pool size*/

    /*take addr of the frame*/
    memaddr frameAddr;
    frameAddr = (memaddr) (SWAPPOOL_STARTADDR + (frame_index * PAGESIZE));

    /* optimize
    int i;
    for (i=0; i<16; i++){
        tempFrameAddr = (swap_pool_t *)(SWAPPOOL_STARTADDR + (i * PAGESIZE));
        if (frameAddr->asid == -1){
            frameAddr = tempFrameAddr;
            break;
        }
    };
    */

    /*determine of frame i is occupied*/
    /*to check if swap pool is occupied, examine the asid*/
    /*!maybe used swap_pool_table and index*/
    if (swap_pool[frame_index].asid != -1){
        /*the frame is occupied*/

        /*disable all interrupts before update page table ,IEc*/
        disableInt();

        /*set the page table k as not valid*/
        swap_pool[frame_index].pageTable_ptr->entryLO &=  0xFFFFFDFF;

        /*update TLB: */

        /*
        TLBP();
        if (getINDEX() >> 31 == 0) {
            (newly updated TLB entry is cached in TLB) TLBWI
            TLBWI();
        }
        */
        TLBCLR();

        /*re-enable interrupts : to update atomically (pandos: 4.5.3)*/
        enableInt();
        writeFlashDevice(curr_proc_support_struct, frameAddr, swap_pool[frame_index].pageNum);
    }

    
    /*DOUBLE CHECK: read contents of Current Process's backing store/flashdevice logical page VPN into frame i*/
    readFlashDevice(curr_proc_support_struct, frameAddr, missing_page_num);

    /*Update Swap Pool table's entry i : ASID, pointer to current processs table entry for page p*/
    swap_pool[frame_index].asid = curr_proc_support_struct->sup_asid;

    /*DOUBLE CHECK: update page table pointer for the swap pool*/
    int missing_page_index = missing_page_num % 32;
    swap_pool[frame_index].pageNum = missing_page_num;
    swap_pool[frame_index].pageTable_ptr = (page_table_t *) &(curr_proc_support_struct->sup_privatePgTbl[missing_page_index]);

    /*disable interrupts*/
    disableInt();

    /*update valid bit for the curr process table entry for page p*/
    /*curr_proc_support_struct->sup_privatePgTbl[missing_page_index].entryHI |= 0x00000200;*/

    /*update physical frame number*/
    curr_proc_support_struct->sup_privatePgTbl[missing_page_index].entryLO = (frameAddr) | 0x200 | 0x400;

    /*update TLB: */
    /*
    TLBP();
    if (getINDEX() >> 31 == 0) {
        (newly updated TLB entry is cached in TLB) TLBWI
        TLBWI();
    }
    */
    TLBCLR();
    /*cp0_index */

    /*re-enable interrupts : to update atomically (pandos: 4.5.3)*/
    enableInt();

    /*releas mutex for swap pool*/
    SYSCALL(4, (int)&swap_pool_sem, 0, 0);

    LDST( &(curr_proc_support_struct->sup_exceptState[0]) );
}

void readFlashDevice(support_t* curr_support_struct, memaddr frameAddr, int blockNo){
    int status;
    int dev_sem_index;

    /*calculate the device reg address*/
    device_t* dev_reg;
    dev_reg = (device_t*) (DEVREGBASE + (FLASHINT-3) * DEVREGINTSCALE + (curr_support_struct->sup_asid -1) * DEVREGDEVSCALE);
    dev_sem_index = (FLASHINT - 3) * 8 + (curr_support_struct->sup_asid - 1);


    blockNo = blockNo % 32;

    /*read content of backing store: read from Block Number into frame i*/
    /*P device semaphore*/
    SYSCALL(3, (int)&deviceSemaphores[dev_sem_index], 0, 0);

    /*write DATA0 with frame i to read content into*/
    dev_reg->d_data0 = frameAddr;

    /* disable interrupts immediately prior to writing the COMMAND field*/
    disableInt();

    /*put value  at (BLOCKNUMBER) to addr in DATA0*/
    /*copy the block at Block Number into RAM at the address in DATA0*/
    dev_reg->d_command = ALLOFF | (blockNo << 8 ) | 2;

    /*call sys5*/
    status = SYSCALL(5, FLASHINT, curr_support_struct->sup_asid-1, 0);

    /*enable interrupt*/
    enableInt();

    /*V the device semaphore*/
    SYSCALL(4, (int)&deviceSemaphores[dev_sem_index], 0, 0);
    if (status != 1)
    {
        /*call trap will terminate the process*/
        program_trap((int) &swap_pool_sem, curr_support_struct);
    }
    return 0;
};

void writeFlashDevice(support_t* curr_support_struct, memaddr frameAddr, int blockNo){
    int status;

    /*calculate the device reg address*/
    int dev_sem_index;

    device_t *dev_reg;
    dev_reg = (device_t*) (DEVREGBASE + (FLASHINT-3) * DEVREGINTSCALE + (curr_support_struct->sup_asid-1) * DEVREGDEVSCALE);

    dev_sem_index = (FLASHINT - 3) * 8 + (curr_support_struct->sup_asid - 1);

    /*P device semaphore*/
    SYSCALL(3, (int)&deviceSemaphores[dev_sem_index], 0, 0);

    /*write data0 field with the appropriate starting physical addr : the frame's starting addr*/
    dev_reg->d_data0 = frameAddr;

    /*1 page = 4KB, block number is to address each 4KB block => device block number == page number*/
    blockNo = blockNo % 32;
    /* disable interrupts immediately prior to writing the COMMAND field*/
    disableInt();

    /*write flash device COMMAND field with the device block number and the command to write*/
    dev_reg->d_command = ALLOFF | (blockNo << 8) | 3;

    /*followed by a sys5*/
    status = SYSCALL(5, FLASHINT, curr_support_struct->sup_asid - 1, 0);

    /*treat any error staus from the write operation as a program trap*/
    enableInt();

    /*release the device's device reg: V device sem*/
    SYSCALL(4, (int)&deviceSemaphores[dev_sem_index], 0, 0);

    /*treat any status error as program trap*/
    if (status != READY){

        program_trap((int)&swap_pool_sem, curr_support_struct);
        /*call program trap*/
    }
};

void enableInt(){
    setSTATUS( getSTATUS() | IECBITON );
    return 0;
}

void disableInt(){
    setSTATUS(getSTATUS() & (~IECBITON));
    return 0;
}
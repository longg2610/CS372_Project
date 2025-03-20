#include "../h/initProc.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "/usr/include/umps3/umps/libumps.h"

#define VPNSTART 0x80000000

/* init 8 support struct as static array*/
static support_t support_states[UPROCMAX];

/*init peripheral device semaphores*/
int deviceSemaphores[48];

void initProc(){
    /*init semaphore*/
    int j;
    for (j = 0; j < 48;j++){
        deviceSemaphores[j] = 1;
    }
    createProcesses();
    /*set up new process's Page Table --> shows none of the process's pages are present*/
};

void createProcesses()
    {
        int i = 0;

        for (i = 1; i <= UPROCMAX; i++)
        {
            /*/*INITIALIZE PROCESSOR STATE STATE_T OF U-PROC*/
            /*init processor state for each U-proc*/
            state_t initialState;

            /*pc and s_t9 set to 0x800.000B0*/
            initialState.s_t9 = 0x800000B0;

            /*sp set to 0xC000.0000*/
            initialState.s_pc = 0xC0000000;

            /*status set for user-mode: ? current or prev*/
            /*( s_status & 0b11111111111111111111111111111101 (clear mode bit) ) | 1 << 1*/
            initialState.s_status = (initialState.s_status & 0b11111111111111111111111111111101) | 1 << 1;

            /*entryHI.ASID set ot the process's unique ID*/
            /*initialState.s_entryHI = (initialState.s_entryHI & 0b11111111111111111111.000000.111111) | i << 6 */
            initialState.s_entryHI = (initialState.s_entryHI & 0b11111111111111111111000000111111) | i << 6;

            /*INITIALIZE SUPPORT STRUCT*/
            /*set asid in support_t to i */
            support_states[i - 1].sup_asid = i;

            /*initialize context area*/
            /* (? which addr) set 2 pc fields: 0 to addr of Support Level's TLB handler, 1 to the addr of Support Level's general exception*/

            /*set 2 status registers to: kernel-mode + interrupts enabled + PLT enabled*/
            int e;
            for (e = 0; e < 2; e++)
            {
                /*set kernel-mode*/
                support_states[i - 1].sup_exceptContext[e].c_status = (support_states[i - 1].sup_exceptContext->c_status & 0b11111111111111111111111111111101) | 1 << 1;

                /*set interrupts enabled (=1)*/
                support_states[i - 1].sup_exceptContext[e].c_status = (support_states[i - 1].sup_exceptContext[e].c_status & 0b11111111111111111111111111111110) | 1;

                /*set PLT enabled, set to 1*/
                support_states[i - 1].sup_exceptContext[e].c_status = (support_states[i - 1].sup_exceptContext[e].c_status & 0b11111111111111111111111111111111);

                /*set SP field to the end of the stackGen area*/
                support_states[i - 1].sup_exceptContext[e].c_stackPtr = &(support_states[i - 1].sup_stackGen[499]);
            }
            /*init all page table entries in the support_t of the [i-1] process*/
            for (int j = 0; j < 32; j++)
            {
                /*set asid in each page table entry to i - the asid is at bit 6 - bit 11*/
                support_states[i - 1].sup_privatePgTbl[j].entryHI |= i << 6;

                /*VPN field (starts from bit 12 to bit 31) will be set to [0x80000000 ... 0x8001E000] to the first 31 entries*/
                if (j != 32)
                {
                    support_states[i - 1].sup_privatePgTbl[j].entryHI |= (VPNSTART + i) << 12;
                }
                else{
                
                /*VPN of the stack page (page Table entry 31) should be set to 0xBFFFF*/
                support_states[31].sup_privatePgTbl[31].entryHI |= 0xBFFFF << 12;
            }

            /*each page table entry's D field will be set to 1*/
            support_states[i - 1].sup_privatePgTbl[j].entryLO |= 1 << 10;

            /*each page table entry's G field will be set to 0 - off*/
            support_states[i - 1].sup_privatePgTbl[j].entryLO |= 0 << 8;

            /* each page table entry's V bit field will be set to 0 - not valid*/
            support_states[i - 1].sup_privatePgTbl[j].entryLO |= 0 << 9;
        }


        /*call SYS1 to create process*/
        SYSCALL(1, (int)&initialState, (int)&support_states[i - 1], 0);
    }
}

/*set up the new process's backing store on a sencondary storage device - hardware job*/
#include "../h/initProc.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

#define VPNSTART 0x80000000

/*init peripheral device semaphores*/
int deviceSemaphores[48];
int masterSemaphore;

void test()
{
    masterSemaphore = 0;
    /*init semaphore*/
    int j;
    for (j = 0; j < 48;j++){
        deviceSemaphores[j] = 1;
    }
    /* init 8 support struct as static array*/
    static support_t support_states[UPROCMAX];
    
    /*The InstantiatorProcess will : 1.init the swap pool table and swap pool semaphore, 2. devices sem*/
    initSwapStructs();

    /*set up new process's Page Table --> shows none of the process's pages are present*/
    int i;
        for (i = 1; i <= UPROCMAX; i++)
        {
            /*INITIALIZE PROCESSOR STATE STATE_T OF U-PROC*/

            /*init processor state for each U-proc*/
            state_t initialState;

            /*pc and s_t9 set to 0x800.000B0*/
            initialState.s_t9 = 0x800000B0;

            /*sp set to 0xC000.0000*/
            initialState.s_pc = 0xC0000000;

            /*status set for user-mode + IEc on + PLT enabled*/
            /*( s_status & 0b11111111111111111111111111111101 (clear KUc + IEc + TE) ) | 1 << 1*/
            initialState.s_status = (initialState.s_status & 0b11110111111111111111111111111100) | TEBITON | IECBITON | KUCBITON;

            /*entryHI.ASID set ot the process's unique ID*/
            /*clear 11-6 bits then | i << 6 */
            initialState.s_entryHI = (initialState.s_entryHI & 0b11111111111111111111000000111111) | i << 6;

            /*INITIALIZE SUPPORT STRUCT*/
            /*set asid in support_t to i */
            support_states[i - 1].sup_asid = i;

            /*initialize context area*/
            /*set 2 pc fields: 0 to addr of Support Level's TLB handler, 1 to the addr of Support Level's general exception*/
            support_states[i - 1].sup_exceptContext[0].c_pc = (memaddr) pager;
            support_states[i - 1].sup_exceptContext[1].c_pc = (memaddr) support_syscall_handler;

            /*set 2 status registers to: kernel-mode + interrupts enabled + PLT enabled*/
        
            int e;
            for (e = 0; e < 2; e++)
            {
                /*clear KUc(=0) + IEc + TE then OR with IECBITON OR TEBITON*/
                support_states[i - 1].sup_exceptContext[e].c_status = (support_states[i - 1].sup_exceptContext->c_status & 0b11110111111111111111111111111100) | IECBITON | TEBITON;

                /*set SP field to the end of the stackGen area*/
                support_states[i - 1].sup_exceptContext[e].c_stackPtr = &(support_states[i - 1].sup_stackGen[499]);
            }

            /*init all page table entries in the support_t of the [i-1] process*/
            int j;
            for ( j = 0; j < 32; j++)
            {
                /*set asid in each page table entry to i - the asid is at bit 6 - bit 11*/
                support_states[i - 1].sup_privatePgTbl[j].entryHI |= i << 6;

                /*VPN field (starts from bit 12 to bit 31) will be set to [0x80000000 ... 0x8001E000] to the first 31 entries*/
                if (j != 31)
                {
                    support_states[i - 1].sup_privatePgTbl[j].entryHI |= (VPNSTART + i) << 12;
                }
                else{
                    /*VPN of the stack page (page Table entry 31) should be set to 0xBFFFF*/
                    support_states[i-1].sup_privatePgTbl[j].entryHI |= 0xBFFFF << 12;
            }

            /*(?) if Offset is in range 0x0008 + 0x0014, set D's field to 0 */

            /*each page table entry's D field will be set to 1 */
            support_states[i - 1].sup_privatePgTbl[j].entryLO |= 1 << 10;

            /*each page table entry's G field will be set to 0 -pages are private to the asid*/
            support_states[i - 1].sup_privatePgTbl[j].entryLO |= 0 << 8;

            /* each page table entry's V bit field will be set to 0 - not valid*/
            support_states[i - 1].sup_privatePgTbl[j].entryLO |= 0 << 9;
        }

        /*call SYS1 to create process*/
        SYSCALL(1, (int)&initialState, (int)&support_states[i - 1], 0);
        }

        /*test P mastersemaphore 8 times when all U-proc is launched*/
        for (j = 0; j < UPROCMAX;j++){
            SYSCALL(3, (int)&masterSemaphore, 0, 0);
        }
    /*after this loop, `test` issue SYS2, triggering a HALT by the Nucleus*/
    SYSCALL(2, 0, 0, 0);
};

/*After launching all the U-procs, test should repeatedly
issue a SYS3 (V operation) on this semaphore. This loop should iterate
UPROCMAX times: the number of U-proc’s launched: [1..8]*/

/*set up the new process's backing store on a sencondary storage device - hardware job*/
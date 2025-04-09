#include "../h/initProc.h"
#include "../h/const.h"
#include "../h/types.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"



/*init peripheral device semaphores*/
int deviceSemaphores[48];
int masterSemaphore;


void test()
{
    int status;

    masterSemaphore = 0;
    /*init semaphore*/
    int e;
    for (e = 0; e < 48;e++){
        deviceSemaphores[e] = 1;
    }
    /* init 8 support struct as static array*/
    static support_t support_states[UPROCMAX + 1];
    
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
            initialState.s_pc = 0x800000B0;

            initialState.s_sp = 0xC0000000;

            /*status set for user-mode + IEc on + PLT enabled*/
            /*( s_status & 0b11111111111111111111111111111101 (clear KUc + IEc + TE) ) | 1 << 1*/
            initialState.s_status = ALLOFF | TEBITON | IMON | IEPBITON | KUPBITON;

            /*entryHI.ASID set ot the process's unique ID*/
            /*clear 11-6 bits then | i << 6 */
            initialState.s_entryHI = ALLOFF | (i << 6);

            /*INITIALIZE SUPPORT STRUCT*/
            /*set asid in support_t to i */
            support_states[i].sup_asid = i;

            /*initialize context area*/
            /*set 2 pc fields: 0 to addr of Support Level's TLB handler, 1 to the addr of Support Level's general exception*/
            support_states[i].sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) pager;
            support_states[i].sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) support_syscall_handler;

            /*set 2 status registers to: kernel-mode + interrupts enabled + PLT enabled*/

            /*clear KUc(=0) + IEc + TE then OR with IECBITON OR TEBITON*/
            support_states[i].sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF| IMON | IEPBITON | TEBITON;
            support_states[i].sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF| IMON | IEPBITON | TEBITON;

            /*set SP field to the end of the stackGen area*/
            support_states[i].sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(support_states[i].sup_stackTLB[499]);

            /*set SP field to the end of the stackGen area*/
            support_states[i].sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(support_states[i].sup_stackGen[499]);

            

            /*init all page table entries in the support_t of the [i-1] process*/
            int j;
            for ( j = 0; j < 32; j++)
            {
                /*set asid in each page table entry to i - the asid is at bit 6 - bit 11*/
                support_states[i].sup_privatePgTbl[j].entryHI |= (i << 6);

                /*each page table entry's D field will be set to 1 */
                support_states[i].sup_privatePgTbl[j].entryLO = ALLOFF | 0x00000400 | 0x000;

                /*VPN field (starts from bit 12 to bit 31) will be set to [0x80000000 ... 0x8001E000] to the first 31 entries*/
                if (j != 31)
                {
                    support_states[i].sup_privatePgTbl[j].entryHI |= ((VPNSTART + j) << 12);
                }
                else{
                    /*VPN of the stack page (page Table entry 31) should be set to 0xBFFFF*/
                    support_states[i].sup_privatePgTbl[j].entryHI |= (0xBFFFF << 12);
            }

            /*(?) if Offset is in range 0x0008 + 0x0014, set D's field to 0 */
            }

            /*call SYS1 to create process*/
            debug(6,6,6,6);
            status = SYSCALL(1, (int)&initialState, (int)&(support_states[i]), 0);

            debug(7, status, i, 7);
            if (status == ERROR)
            {
                SYSCALL(TERMINATETHREAD, 0, 0, 0);
            }
        }

        /*test P mastersemaphore 8 times when all U-proc is launched*/
        int t;
        for (t = 0; t < UPROCMAX; t++)
        {
            debug(44, 44, 44, masterSemaphore);
            SYSCALL(3, (int)&masterSemaphore, 0, 0);
        }
    /*after this loop, `test` issue SYS2, triggering a HALT by the Nucleus*/
    SYSCALL(2, 0, 0, 0);
};

/*After launching all the U-procs, test should repeatedly
issue a SYS3 (V operation) on this semaphore. This loop should iterate
UPROCMAX times: the number of U-proc’s launched: [1..8]*/

/*set up the new process's backing store on a sencondary storage device - hardware job*/
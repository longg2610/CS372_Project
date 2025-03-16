#include "../h/initProc.h"
#include "../h/pcb.h"
#include "../h/types.h"

#define VPNSTART 0x80000000
/* init 8 U-proc's*/
support_t support_states[UPROCMAX];

/*init swap pool mutex semaphore*/
int swap_pool_sem;

/*set up new process's Page Table --> shows none of the process's pages are present*/

int main(){
    int i = 0;
    swap_pool_sem = 1;

    for (i = 1; i <= UPROCMAX; i++)
    {

        /*set asid in support_t to i*/
        support_states[i - 1].sup_asid = i;

        
        for (int j = 0; j < 32; j++){
            /*set asid in each page table entry to i - the asid is at bit 6 - bit 11*/
            support_states[i - 1].uproc_pageTable[j].entryHI = i << 6; 

            /*VPN field (starts from bit 12 to bit 31) will be set to [0x80000000 ... 0x8001E000] to the first 31 entries*/
            support_states[i - 1].uproc_pageTable[j].entryHI = (VPNSTART + i) << 12;

            /*each page table entry's D field will be set to 1*/
            support_states[i - 1].uproc_pageTable[j].entryLO |= 1 << 10;

            /*each page table entry's G field will be set to 0 - off*/
            support_states[i - 1].uproc_pageTable[j].entryLO |= 0 << 8;

            /* each page table entry's V bit field will be set to 0 - not valid*/
            support_states[i - 1].uproc_pageTable[j].entryLO |= 0 << 9;
        }

    }
}

/*set up the new process's backing store on a sencondary storage device*/
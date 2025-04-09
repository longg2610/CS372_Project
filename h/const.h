#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/*bit manipulation constants*/
#define IEPBITON		0x00000004
#define IECBITON        0x00000001
#define KUPBITON		0x00000008
#define KUCBITON        0x00000002
#define KUPBITOFF		0xFFFFFFF7
#define TEBITON			0x08000000
#define TEBITOFF        0xF7FFFFFF
#define ALLOFF			0x00000000
#define IMON            0x0000FF00
#define CLEAREXCCODE    0xFFFFFF83
#define EXCCODERI       0x00000028

#define TERMSTATMASK	0xFF
#define CAUSEMASK		0xFF

#define CLEAR31MSB 0x00000001
#define CLEAR26MSB 0x0000001F


/*semaphores for sentinel nodes*/
#define ZERO 0
#define MAXINT 2147483647

/*index for pseudo-clock in device semaphores*/
#define DEVSEMNO  49
#define PSEUDOCLK 48

#define MAXPROC 20

/* Hardware & software constants */
#define PAGESIZE		  4096			/* page size in bytes	*/
#define WORDLEN			  4				  /* word size in bytes	*/


/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		0x10000000
#define RAMBASESIZE		0x10000004
#define TODLOADDR		0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024
#define CLOCKINTERVAL	100000UL	/* interval to V clock semaphore */
#define TIMESLICE       5000UL
#define PLTSTART        0x7FFFFFFF



/* utility constants */
#define ERROR               -1
#define SUCCESS             0
#define USER                1
#define ON                  1
#define OFF                 0
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			  static
#define EOS				    '\0'
#define TERMWRITE           0
#define TERMREAD            1
#define SUBDEVPERTERM       2
#define GETEXCCODE          2
#define GETKUP              3
#define GETINTLINE          8
#define UPROCMAX            8
#define VPNSTART            0x80000

#define VON                 0x200
#define GON                 0x100
#define DON                 0x400

#define NULL 			    ((void *)0xFFFFFFFF)

/* device interrupts */
#define INTERPROCINT      0
#define PLTINT            1
#define INTERVALTMRINT    2
#define DISKINT			  3
#define FLASHINT 		  4
#define NETWINT 		  5
#define PRNTINT 		  6
#define TERMINT			  7

#define DEVINTNUM		  5		  /* interrupt lines used by devices */
#define DEVPERINT		  8		  /* devices per interrupt line */
#define DEVREGLEN		  4		  /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	  16 		/* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS			  0
#define COMMAND			  1
#define DATA0			    2
#define DATA1			    3

/* device register field number for terminal devices */
#define RECVSTATUS  	0
#define RECVCOMMAND 	1
#define TRANSTATUS  	2
#define TRANCOMMAND 	3

/* device common STATUS codes */
#define UNINSTALLED		0
#define READY			    1
#define BUSY			    3

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1

/* Memory related constants */
#define KSEG0           0x00000000
#define KSEG1           0x20000000
#define KSEG2           0x40000000
#define KUSEG           0x80000000
#define RAMSTART        0x20000000
#define BIOSDATAPAGE    0x0FFFF000
#define	PASSUPVECTOR	0x0FFFF900
#define STACKPAGETOP    0x20001000
#define DEVREGBASE      0x10000054
#define DEVREGINTSCALE  0x80
#define DEVREGDEVSCALE  0x10

/* system call codes */
#define	CREATETHREAD	1	/* create thread */
#define	TERMINATETHREAD	2	/* terminate thread */
#define	PASSERN			3	/* P a semaphore */
#define	VERHOGEN		4	/* V a semaphore */
#define	WAITIO			5	/* delay on a io semaphore */
#define	GETCPUTIME		6	/* get cpu time used to date */
#define	WAITCLOCK		7	/* delay on the clock semaphore */
#define	GETSPTPTR		8	/* return support structure ptr. */

/*Interrupting Devices Bitmap*/
#define INTERRUPTLINEBASE 0x10000040
#define INTERRUPTLINE3  INTERRUPTLINEBASE + 0x00
#define INTERRUPTLINE4  INTERRUPTLINEBASE + 0x04
#define INTERRUPTLINE5  INTERRUPTLINEBASE + 0x08
#define INTERRUPTLINE6  INTERRUPTLINEBASE + 0x0C
#define INTERRUPTLINE7  INTERRUPTLINEBASE + 0x10


/* Exceptions related constants */
#define	PGFAULTEXCEPT	  0
#define GENERALEXCEPT	  1


/* operations */
#define	MIN(A,B)		((A) < (B) ? A : B)
#define MAX(A,B)		((A) < (B) ? B : A)
#define	ALIGNED(A)		(((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

#endif

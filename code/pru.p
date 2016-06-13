.origin 0
.entrypoint START

#include "pru.hp"

START:
	// Enable OCP master port
	LBCO    r0, CONST_PRUCFG, 4, 4
	CLR     r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
	SBCO    r0, CONST_PRUCFG, 4, 4

	// Configure the programmable pointer register for PRU0 by setting c28_pointer[15:0]
	// field to 0x0120.  This will make C28 point to 0x00012000 (PRU shared RAM + 0x200).
	MOV     r0, 0x00000120
	MOV     r1, PRU0_CTRL + CTPPR_0
	SBBO    r0, r1, 0, 4

	// Enabling cycle counter
	MOV 	r0, PRU0_CTRL + CTRL
	LBBO	r1, r0, 0, 4
	SET     R1, 3
	SBBO	r1, r0, 0, 4

	// clearing encoder counter
	XOR     r5, r5, r5
	// clearing old state register
	XOR     r6, r6, r6
	// clearing pwm generation start register
	XOR     r13, r13, r13
	// setting pwm value to center
	MOV     r14, 1000/5 * 1500      //(1500uS)

MAIN_LOOP:
	//Outputing cycle counter value
	MOV	    r12, PRU0_CTRL + CTRL
	LBBO	r11, r12, 0x0C, 4

	//R11 contains current cycle
	SUB     r15, r11, r13

	QBGT	NO_END_PULSE, r15, r14
END_PULSE:
	CLR	    r30, 0
NO_END_PULSE:
	MOV	    r12, 1000/5 * 20000     //(20000uS)
	QBGT	NO_END_CYCLE, r15, r12
END_CYCLE:
    //Outputting data to shared ram, to be printed to console
	SBCO    r5, CONST_PRUSHAREDRAM, 0, 4
	SBCO    r11, CONST_PRUSHAREDRAM, 4, 4

    XOR     r11, r11, r11
	MOV	    r12, PRU0_CTRL + CTRL
	SBBO	r11, r12, 0x0C, 4

    //Reading in new pulse length
	LBCO    r14, CONST_PRUSHAREDRAM, 12, 4
	SET	    r30, 0
	//SUB     r13, r13, r12
	MOV	    r13, r11
NO_END_CYCLE:

	//Reading in all pins
	MOV     r9, r31

	//Getting changes from last read
	XOR     r6, r6, r9

	//Getting difference in state in current read
	LSR     r7, r9, 1
	AND     r7, r7, 1
	LSR     r8, r9, 5
	AND     r8, r8, 1
	XOR     r7, r7, r8

	QBBS    C_DIFF, r7, 0
C_SAME:
CS_ATEST:
	QBBC    CS_BTEST, r6, 1
	ADD     r5, r5, 1
CS_BTEST:
	QBBC    CS_END, r6, 5
	SUB     r5, r5, 1
CS_END:
	QBA     C_END
C_DIFF:
CD_ATEST:
	QBBC    CD_BTEST, r6, 1
	SUB     r5, r5, 1
CD_BTEST:
	QBBC    CD_END, r6, 5
	ADD     r5, r5, 1
CD_END:
C_END:
	
	//Moving current pin state to register for comparison next cycle
	MOV     r6, r9

	QBA     MAIN_LOOP
	//End loop

	// Halt the processor (shouldn't ever be here)
	HALT

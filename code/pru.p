.origin 0
.entrypoint START

#include "pru.hp"

//Macro for each PWM pin needed
//Macro uses r14 for temp storage
//r11 is the length since cycle start
.macro PIN_CLR
.mparam addr, pin
    //Read source and get pwm length
	LBCO    r14, CONST_PRUSHAREDRAM, addr, 4
    //Clear this specific pwm pin if time reached
    QBGT    NO_CLR_PIN, r11, r14
CLR_PIN:
    CLR     r30, pin
NO_CLR_PIN:
.endm

//Macro is for each quadrature encoder needed
//Macro uses r14 for temp storage
//R13 is difference from last
//R31 is current state
.macro QUAD_READ
.mparam addr, pina, pinb
    //Read out the source first
	LBCO    r14, CONST_PRUSHAREDRAM, addr, 4

    //Handling A channel transitions
    QBBC    A_SAME, r13, pina
A_CHANGE:
    QBBC    A_LOW, r31, pina
A_HIGH:
    QBBC    AH_BL, r31, pinb
AH_BH:
    sub     r14, r14, 1
    QBA     A_END
AH_BL:
    add     r14, r14, 1
    QBA     A_END
A_LOW:
    QBBC    AL_BL, r31, pinb
AL_BH:  
    add     r14, r14, 1
    QBA     A_END
AL_BL:  
    sub     r14, r14, 1
A_SAME:
A_END:

    //Writing back to the source
    SBCO    r14, CONST_PRUSHAREDRAM, addr, 4
.endm
    
//Main program start
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

    //Setting the initial old state register
    MOV     r16, r31

    //Set all pins high before first cycle
    SET     r30, 5
    SET     r30, 3
    SET     r30, 1
    SET     r30, 14

MAIN_LOOP:
	//Getting cycle counter value
	MOV	    r12, PRU0_CTRL + CTRL
	LBBO	r11, r12, 0x0C, 4
    //R11 contains the cycles since the cylce started

//Generating PWM signals
WRITE_OUTPUT:   

    //Check pins to see if time passed
    PIN_CLR 16, 5
    PIN_CLR 20, 3
    PIN_CLR 24, 1
    PIN_CLR 28, 14

    //Set total cycle time
    MOV     r14, 1000/5 * 1000      //1000uS
    //Set all if total cycle time reached
    QBGT    NO_SET_ALL, r11, r14
SET_ALL:
    //Set all pins high in preperation for next cycle
    SET     r30, 5
    SET     r30, 3
    SET     r30, 1
    SET     r30, 14
    //Add on the time elapsed in the prev cycle time register
    //Reset cycle counter
    XOR     r11, r11, r11
	MOV	    r12, PRU0_CTRL + CTRL
	SBBO	r11, r12, 0x0C, 4
NO_SET_ALL:

//Reading the quadrature encoders
READ_INPUT:

    //Resetting the old state register and creating difference register
    XOR     r13, r16, r31
    MOV     r16, r31

    //Reading all encoders now
    QUAD_READ 32, 2, 0
    QUAD_READ 36, 15, 14

	QBA     MAIN_LOOP
	//End loop

	// Halt the processor (shouldn't ever be here)
	HALT

    //End actual code

    /* Old stuff
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

    //Setting previous cycle count from current
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
    */

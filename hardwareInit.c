/*
 * hardwareInit.c
 *
 *  Created on: 12 Nov, 2016
 *      Author: sylam
 */

#include <p32mx250f128d.h>

void hardwareInit(void) {

	SYSKEY = 0;				// ensure OSCCON is locked
	SYSKEY = 0xAA996655;	// unlock sequence
	SYSKEY = 0x556699AA;
	CFGCONbits.IOLOCK = 0;	// allow write

	//PPS CONFIGURATION
	U2RXR = 0b0111;			// RA9 - U2RX
	RPC4R = 0b0010;			// RC4 - U2TX

	SDI2R = 0b0101;			// RC6 - SPI2 (floor sensor)
	T2CKR = 0b0001;			// RB3 - T2CK (wheel counter)
	T4CKR = 0b0100;			// RB2 - T4CK (wheel counter)
	RPB4R = RPA8R = 0b101;		// OC1 AENABL  OC2 BENBL - MOTOR (DRV8834PWP)


	CFGCONbits.IOLOCK = 1;	// forbidden write
	SYSKEY = 0;				// relock

	//GPIO CONFIGURATION
	TRISASET = 2;							// RA1: tact switch
	LATACLR = TRISACLR = 1;					// RA0: LED indicator
	LATCCLR = TRISCCLR = 0b111; // RC2, RC1, RC0 - MOTOR(DRV8834PWP)    									// BPHASE, APHASE, M1
	ANSELBCLR = 0b1100;					// RB3 & RB2 - wheel counters


    //MISCELLANEOUS
	U2BRG = 10;					// baud rate 115200
	U2STASET = 0x1400;			// URXEN & UTXEN

	PR3 = 49999;				//timer3 = 400 Hz (2.5 ms)
	T3CON = 0x8000;				//timer3, no prescaler
	PR5 = 2000;					//timer5 = 100 us
	// floor sensor
	SPI2BRG = 15;				//SPI2 - 625 kHz
	SPI2CON = 0x120;			//SPI2 - CKE, MSTEN

	// wheel counter
	TMR2 = TMR4 = 0;
	PR2 = PR4 = 0xffff;
	T2CON = T4CON = 0x8002;		//External clock, no prescaler

	//ENABLE INTERRUPTS
	IPC9bits.U2IP = 1;			// uart2
	IEC1bits.U2RXIE = 1;

	IPC3bits.T3IP = 1;			// timer3
	IEC0bits.T3IE = 1;
	IPC9bits.SPI2IP = 1;		// floor sensor
	IPC5bits.T5IP = 1;			// IR LED

	//TURN ON PERIPHERAL MODULES
	U2MODEbits.ON = 1;			// turn on uart engine
	SPI2CONbits.ON = 1;			// SPI2 ON
}



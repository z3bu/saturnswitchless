/* ---------------------------------------------------------------
 * Saturn Switchless Mod PIC Code (16F628A)
 * 2016 Adapted from Sebastian Code Maxime Vinzio. 
 * Copyright (c) 2004 Sebastian Kienzl <seb@riot.org>
 * ---------------------------------------------------------------
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <xc.h>
#define _XTAL_FREQ 4000000

//see file:///opt/microchip/xc8/v1.35/docs/chips/16f628a.html
#pragma config WDTE = OFF, PWRTE = OFF, CP = OFF, BOREN = OFF, LVP = OFF, MCLRE = OFF, CPD = OFF, FOSC = INTOSCIO

/*
EEPROM:

Byte
0		current country is saved here		
1		current 50/60-setting is saved here

these can be modified before burning:
2		colour for EU	(1=green, 2=red, 3=orange)
3		colour for USA
4		colour for JAP
*/

__EEPROM_DATA( 0, 0, 1, 3, 2, 0, 0, 0 );
__EEPROM_DATA( ' ','H', 'i', ' ', ';', ')', '!', ' ' );
__IDLOC(42);

/*
 RB0 RESET BUTTON used as an EXT interrupt
 RB1 VF PAL if 0 NTSC if 1
 RB2 RST to the console
 RB4 Green LED
 RB5 Red   LED
  
 RA0 JP6
 RA1 JP10
 RA2 JP12
 */

#define BUTTON	RB0
#define	VF		RB1

#define	RST		RB2
#define RST_T	TRISB2

char currCountry;
#define COUNTRYNUM 3

const char countries_JP[ COUNTRYNUM ] = {
	// JP12/JP10/JP6
	0b110,	// eu
	0b010,	// usa
	0b001	// japan
	//^^^_____ RA2/RA1/RA0 (JP12/JP10/JP6)
};

const char countries_VF[ COUNTRYNUM ] = {
	0,		// eu: 50hz
	1,		// usa: 60hz
	1		// japan: 60hz
};

char countries_COL[ COUNTRYNUM ] = {
	0b01,	// eu: green   (1)
	0b11,	// usa: orange (3)
	0b10	// japan: red  (2)
	//^^______ RB5/RB4 (red/green LED)
};

// approx. delay
void delay( int t )
{
	while( t-- ) {
		__delay_ms( 1 );
	}
}

void setCountry()
{
	PORTA = ( PORTA & 0b110000 ) |
			countries_JP[ currCountry ];
}

void setLeds()
{
	PORTB = ( PORTB & 0b001111 ) |
			( countries_COL[ currCountry ] << 4 );
}

void reset5060()
{
	VF = countries_VF[ currCountry ];
}

void darkenLeds( int msec )
{
	RB4 = 0;
	RB5 = 0;
	delay( msec );
	setLeds();
}

void display5060( char dunkel )
{
	if( !dunkel ) {
		darkenLeds( 200 );
	}

	if( VF == 0 ) {
		// 50Hz: blink once
		delay( 200 );
		darkenLeds( 200 );
	}
	else {
		int i;
		// 60Hz: blink rapidly a few times
		for( i = 0; i < 3; i++ ) {
			delay( 75 );
			darkenLeds( 75 );
		}
	}
}

void load()
{
	char c;
	int i;
	for( i = 0; i < COUNTRYNUM; i++ ) {
		c = eeprom_read( i + 2 );
		if( c >= 1 && c <= 3 ) {
			countries_COL[ i ] = c;
		}
	}

	currCountry = eeprom_read(0 );
	if( currCountry >= COUNTRYNUM )
		currCountry = 0;
	setCountry();
	VF = eeprom_read( 1 );
	setLeds();
	display5060( 1 );
}

void save()
{	
	eeprom_write(0, currCountry);
	eeprom_write(1, VF);
}

void reset()
{
	// put to LOW and output
	RST = 0;
	RST_T = 0;
	delay( 200 );
	// back to HIGH and hi-z
	RST_T = 1;
	RST = 1;
}



void main()
{
	// configure internal osc
	PCON = 1<<3;        // we don't really care about POR/BOD
	CMCON = 0b111;      // comparator mode off

	PORTB = 0;
	TRISB = 0b001101; // /RESET will only be configured to be an output on reset

	// configure wakeup for sleep on RB0/INT external interrupt
	//RBIE = 1;	interrupt for portB 7:4
    //PEIE=1; //
	INTEDG=0; ; //falling bit 
 	GIE = 1; //decides whether or not the processor branches to the interrupt vector following wake-up   
    INTE=1;  //enable RB0 int
    OPTION_REG &= 0x7F;           //RPBU enable individual pullups

	PORTA = 0;
	TRISA = 0b001000;   // JP/LED-outputs

	load(); //load settings
    
	while( 1 ) {
		// port change interrupt flag clear
		//RBIF = 0; for port B 7:4
        INTF = 0; //must be cleared to re-enable it
        INTE = 1;
		// wait until the button is pushed
		SLEEP();

		// debounce
		delay( 5 );

		// we only care about push
		if( BUTTON != 0 )
			continue;

		delay( 250 );

		if( BUTTON ) {
			// already released again -> normal reset
			reset();
		}
		else {
			darkenLeds( 1000 );

			if( BUTTON ) {
				// released only now: 50/60hz toggle
				VF ^= 1;
				save();
				display5060( 1 );
			}
			else {
				// change region?
				char change = 0;

				// display the current region for a while, so there's time to release the button
				delay( 1000 );

				// button still pushed? cycle regions while button is held
				while( !BUTTON ) {
					change = 1;
					if( ++currCountry >= COUNTRYNUM )
						currCountry = 0;
					setLeds();
					delay( 1000 );
				}

				if( change ) {
					// region change!
					setCountry();
					// set 50/60 accordingly and blink
					reset5060();
					save();
					display5060( 0 );
					reset();
				}
			}
		}
	}
}

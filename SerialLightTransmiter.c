/*
===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <stdio.h>

const unsigned int INTERRUPT_PIN = (1<<8);
const unsigned int LED_PIN = (1<<9);
char OUTPUT_STRING[2];
char INPUT_STRING[2];
const unsigned int BITS_IN_MESSAGE = 16;
int bitsSent = 0;
int sending = 0;

/*
 * Sets the designated output pin to on or off.
 * Sets the output pin to high if bit >= 1, 0 if bit is 0.
 */
void setBitToPin(int bit) {
    if (bit) {
   	 LPC_GPIO0->FIOPIN |= LED_PIN;
    } else {
   	 LPC_GPIO0->FIOPIN &= ~LED_PIN;
    }
}

void sendBit(char message[], int position) {
	int index = position/8;
	int bitNum = (position%8);
    int bit = message[index] & (1<<bitNum);
	setBitToPin(bit);
	printf("%d %d %d %d, ", position, index, bit, bitNum);
	//receiveBit(INPUT_STRING,position,bit);
}

void TIMER0_IRQHandler() {
    //setBitToPin(1);
    //LPC_GPIO0->FIOPIN ^= (1<<9);
    LPC_TIM0->IR = 1;
    if (sending == 2) {
    	if (bitsSent < BITS_IN_MESSAGE)
    		sendBit(OUTPUT_STRING,bitsSent);
    	else
    		setBitToPin(0);
    }
    else if (sending == 1) {
   	 setBitToPin(bitsSent%8 >= 4);
   	 if (bitsSent >= 7) {
   		 sending = 2;
   		 bitsSent = -1;
   	 }
    }
    else if (sending == 0) {
   	 setBitToPin(!(bitsSent%2));
   	 if (bitsSent >= 7) {
   		 sending = 1;
   		 bitsSent = -1;
   	 }
    }

    bitsSent++;
    return;
}

int main(void) {
	LPC_GPIO0->FIODIR |= LED_PIN;
	LPC_GPIO0->FIODIR &= ~INTERRUPT_PIN;
	
	printf("Hello World\n");
	
    OUTPUT_STRING[0] = 0xac;
    OUTPUT_STRING[1] = 0x53;


    LPC_TIM0->MR0 = 1200000;
    LPC_TIM0->MCR = 3;   			 /* Interrupt and Reset on MR0 */
    NVIC_EnableIRQ(TIMER0_IRQn);
    LPC_TIM0->TCR = 1;
    sending = 0;
	while(1) {
	}
	return 0 ;
}

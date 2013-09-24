/*
 ==============================================
 Name        : main.c
 Author      :
 Version     :
 Description : Blinking stuff.
 ==============================================
 */

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>
#include <NXP/crp.h>
#include <math.h>

#include "main.h"

// Variable to store CRP value in. Will be placed automatically
// by the linker when "Enable Code Read Protect" selected.
// See crp.h header for more information
__CRP const unsigned int CRP_WORD = CRP_NO_CRP;

uint32_t *PIN_CONTROL = (uint32_t *) 0x2009C000;
uint32_t *PIN_VALUES  = (uint32_t *) 0x2009C014;

#define SREG_CLR   9
#define SREG_CLOCK 8
#define SREG_A     7
#define SREG_B     6

#define CLK_CYCLE_WIDTH 12
#define CLK_CYCLE_HOLD  10000

//////// UTILITY FUNCTIONS ////////
void delay(){
    for (int i=0; i<CLK_CYCLE_WIDTH; i++);
}

void half_delay(){
    for (int i=0; i<CLK_CYCLE_WIDTH/2; i++);
}

void gpio_enable_pin(int pin){
    *PIN_CONTROL |= (1 << pin);
}

void gpio_set_pin(int pin, int val){
    if (val){
        *PIN_VALUES |=  (1 << pin);
    } else {
        *PIN_VALUES &= ~(1 << pin);
    }
}

int main(void) {

  int cycle = 0;
  int current_pin = 0;
  int current_pin_2 = 0;

  gpio_enable_pin(SREG_CLOCK);
  gpio_enable_pin(SREG_CLR);
  gpio_enable_pin(SREG_A);
  gpio_enable_pin(SREG_B);
  
  gpio_set_pin(SREG_CLOCK, 0);
  gpio_set_pin(SREG_CLR, 0);
  gpio_set_pin(SREG_A, 0);
  gpio_set_pin(SREG_B, 0);
  
  int bits[8];
  bits[0] = 1;
  bits[1] = 1;
  bits[2] = 1;
  bits[3] = 1;
  bits[4] = 1;
  bits[5] = 1;
  bits[6] = 1;
  bits[7] = 1;
  
  while(1){

    if (cycle++ % 1000 == 0){
        current_pin = (current_pin + 1) % 8;
    }
    
    if (cycle % 2500 == 0){
        current_pin_2 = current_pin_2 - 1;
        if (current_pin_2 < 0) current_pin_2 = 7;
    }
  
    for (int i=0; i<8; i++){
        if (i == current_pin || i == current_pin_2){
            bits[7-i] = 1;
            if (i == current_pin_2){
                if (cycle%3!=0){
                    bits[7-i] = 0;
                }
            }
        } else {
            bits[7-i] = 0;
        }
    }
      
    // do pin I/O
    gpio_set_pin(SREG_CLR,1);
    gpio_set_pin(SREG_A,1);
    gpio_set_pin(SREG_B,0);
    gpio_set_pin(SREG_CLOCK, 1);
    delay();
    
    for (int i=0; i<8; i++){
        gpio_set_pin(SREG_CLOCK,0);
        half_delay();
        gpio_set_pin(SREG_B,bits[i]);
        gpio_set_pin(SREG_A,bits[i]);
        half_delay();
        gpio_set_pin(SREG_CLOCK,1);
        delay();
    }
    
    // wait a while
    gpio_set_pin(SREG_CLOCK,0);
    for (int i=0; i<CLK_CYCLE_HOLD; i++);
    
    // clear register.
    gpio_set_pin(SREG_CLOCK,0);
    half_delay();
    gpio_set_pin(SREG_CLR,0);
    half_delay();
    gpio_set_pin(SREG_CLOCK, 1);
    half_delay();
    gpio_set_pin(SREG_CLR,1);
    delay();    
  }
  
  return 0;
}

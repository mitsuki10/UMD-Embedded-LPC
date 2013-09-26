/*
 ==============================================
 Name        : main.c
 Author      :
 Version     :
 Description : Blinking stuff.
 ==============================================
 */
 
#include "main.h"

#ifdef __USE_CMSIS
  #include "LPC17xx.h"
#endif

#include <cr_section_macros.h>
#include <NXP/crp.h>
#include <math.h>

#include "SN74HC164N.c" // Support for the SN74HC164N Shift Register
#include "clock_util.c" // Clock Utility
#include "receive.c"    // Receive Utility

// Variable to store CRP value in. Will be placed automatically
// by the linker when "Enable Code Read Protect" selected.
// See crp.h header for more information
__CRP const unsigned int CRP_WORD = CRP_NO_CRP;

// Pins 9, 8, and 7 will control the shift register.
#define SREG_CLR   8
#define SREG_CLOCK 9
#define SREG_A     7

#define UINPUT_RESET  0  // User input on pin 6
#define SIGNAL_INPUT  6

int state = 0;
int systime = 0;
receive_state    sstate;
SN74HC164N_state rstate;

void init_receive(){
    LPC_GPIO0->FIODIR &= ~(1 << SIGNAL_INPUT);
    receive_init(&sstate, &LPC_GPIO0 -> FIOPIN, 1<<SIGNAL_INPUT);
}

void drive_receive(){
    receive_step(&sstate, systime);
}

int receive_done(){
    return sstate.state == SIGNAL_COMPLETE;
}

void init_ui(){
    
  
  // Set P0[0] to Input 
  LPC_GPIO0->FIODIR &= ~(1 << UINPUT_RESET);
  // Enable Rising Edge Interrupt on P0[0]
  LPC_GPIOINT->IO0IntEnR |= (1 << UINPUT_RESET);
  // Enable Falling Edge Interrupt on P0[0] 
  LPC_GPIOINT->IO0IntEnF |= (1 << UINPUT_RESET);
  // Turn on External Interrupt 3 
  NVIC_EnableIRQ(EINT3_IRQn);
  

  // Enable GPIO on pins for shift register, and set output to 0
  LPC_GPIO0 -> FIODIR |= (1 << SREG_CLOCK) | (1 << SREG_CLR) | (1 << SREG_A);
  LPC_GPIO0 -> FIOPIN &= ~((1 << SREG_CLOCK) | (1 << SREG_CLR) | (1 << SREG_A));
  
  // Initialize shift register state
  SN74HC164N_init(&rstate);
  
  // Configure shift register
  rstate.reg_clock = &LPC_GPIO0 -> FIOPIN;
  rstate.reg_clear = &LPC_GPIO0 -> FIOPIN;
  rstate.reg_a     = &LPC_GPIO0 -> FIOPIN;
  rstate.mask_clock = (1 << SREG_CLOCK);
  rstate.mask_clear = (1 << SREG_CLR);
  rstate.mask_a     = (1 << SREG_A);
  rstate.holdtime = 200;
}

void drive_ui(){
    // Calculate bits for shift register
    
    rstate.bits = sstate.state;
    
    if (receive_done()){
        rstate.bits = sstate.bitbuffer[0];
    }

    // Step the shift register
    SN74HC164N_step(&rstate);
}

void init_timer(int numcycles){

  // enable power on Tim0
  LPC_SC->PCONP |= (1<<1);
  
  // Set the PC on Tim0 to 1
  LPC_TIM0->PC = 1;
  LPC_TIM0->TCR |= 1;
  
  // Configure Tim0's counter and interrupt
  LPC_TIM0->MR0 = (uint32_t) numcycles; 
  LPC_TIM0->MCR |= 1 | 2;
  LPC_TIM0->EMR |= 1 | (1<<5);
  
  // Enable Tim0's interrupt
  NVIC_EnableIRQ(TIMER0_IRQn);
}


// Timer 0 interrupt handler
void TIMER0_IRQHandler() {

    // Deal with interrupts TIM0-IR0
    if (LPC_TIM0->IR & 1){
        systime++; // Increment time counter
        
        // Drive receive
        drive_receive();
        
        // Drive UI
        drive_ui();        
        
        // Reset interrupt TIM0-IR0
        LPC_TIM0->IR |= 1;
    }
}

// External interrupt 3 handler
void EINT3_IRQHandler() {

    // If the rising edge interrupt was triggered 
    if((LPC_GPIOINT->IO0IntStatR >> UINPUT_RESET) & 1){
        state = 1;
        if (sstate.state == SIGNAL_COMPLETE){
            init_receive();
        } else {
            sstate.state = SIGNAL_COMPLETE;
        }
    }
    
    // If the falling edge interrupt was triggered
    if((LPC_GPIOINT->IO0IntStatF >> UINPUT_RESET) & 1){ // Turn off P0[UINPUT_1]
        state = 0;
    }
    
    // Clear the Interrupt on P0[UINPUT_1]
    LPC_GPIOINT->IO0IntClr |= (1 << UINPUT_RESET);
}

// Main method
int main(void) {
  
  /*
  // Calculate and apply clock settings
  clock_settings *clkset = calculate_clock_settings(120000000);  
  apply_clock_settings(clkset);
  */
  
  init_ui();
  init_receive();
  init_timer(1199);
  
  // Main loop
  while(1){
    
    // Hang out for a few cycles
    for (int i=0; i<200; i++);
    
    /*
    // Do stuff based on global state
    if (state == 1){
        LPC_TIM0->TCR |= 2;
    } else {
        LPC_TIM0->TCR &= ~2;
    }*/
  }
  
  return 0;
}

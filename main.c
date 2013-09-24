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

// Variable to store CRP value in. Will be placed automatically
// by the linker when "Enable Code Read Protect" selected.
// See crp.h header for more information
__CRP const unsigned int CRP_WORD = CRP_NO_CRP;

// Pins 9, 8, and 7 will control the shift register.
#define SREG_CLR   8
#define SREG_CLOCK 9
#define SREG_A     7

#define UINPUT_1 6 // User input on pin 6

#define SIGNAL_INPUT  0
#define SIGNAL_OUTPUT 1

// Global state values
int state = 0;
int systime = 0;
SN74HC164N_state rstate;

// Timer 0 interrupt handler
void TIMER0_IRQHandler() {

    // Deal with interrupts TIM0-IR0
    if (LPC_TIM0->IR & 1){
        systime++; // Increment time counter
        
        // Calculate bits for shift register
        rstate.bits = systime / 1000;
    
        // Step the shift register
        SN74HC164N_step(&rstate);
        
        // Reset interrupt TIM0-IR0
        LPC_TIM0->IR |= 1;
    }
}

// External interrupt 3 handler
void EINT3_IRQHandler() {

    // If the rising edge interrupt was triggered 
    if((LPC_GPIOINT->IO0IntStatR >> UINPUT_1) & 1){
        state = 1;
    }
    
    // If the falling edge interrupt was triggered
    if((LPC_GPIOINT->IO0IntStatF >> UINPUT_1) & 1){ // Turn off P0[UINPUT_1]
        state = 0;
    }
    
    // Clear the Interrupt on P0[UINPUT_1]
    LPC_GPIOINT->IO0IntClr |= (1 << UINPUT_1);
}

// Enables control of time.
void PLL0_feed_sequence(){
    LPC_SC->PLL0FEED = 0xAA;
    LPC_SC->PLL0FEED = 0x55;
}

// Utility function to apply system clock settings.
// Use carefully to avoid bricking your LPC!
void apply_clock_settings(clock_settings *settings){

    if(LPC_SC->PLL0STAT & (1<<25)) { // If PLL0 is connected 
        LPC_SC->PLL0CON &= ~(1<<1);  // Write disconnect flag 
        PLL0_feed_sequence();        // Commit changes
    }
        
    LPC_SC->PLL0CON &= ~(1<<0);     // Write disable flag 
    PLL0_feed_sequence();           // Commit changes

    // Change CPU Divider
    // For some reason this address is not a member of LPC_SC ?!?
    uint32_t *D_FACTOR_ADDRESS = (uint32_t *) 0x400FC104;
    *D_FACTOR_ADDRESS = settings->val_D_factor;
    
    LPC_SC->CLKSRCSEL &= ~(1|2);                  // Zero out clock selector
    LPC_SC->CLKSRCSEL = settings->val_CLKSRCSEL;  // Set clock selector  
    PLL0_feed_sequence();                         // Commit changes
    
    if (!settings->val_PLL0CON) return;   // If PLL is not needed, we're done.
            
    // Get divider values
    int bits_M = settings->val_M_factor;
    int bits_N = settings->val_N_factor;
    
    // Shift N value to necessary bits
    bits_N = bits_N << 23;
    
    // Zero out divider values
    LPC_SC->PLL0CFG &= ~((1<<15) - 1);
    LPC_SC->PLL0CFG &= ~(((1<<17) - 1) << 23);
    
    // Write divider values
    LPC_SC->PLL0CFG |= bits_M;
    LPC_SC->PLL0CFG |= bits_N;
    PLL0_feed_sequence(); // Commit Changes
    
    // Enable PLL0
    LPC_SC->PLL0CON |= 1;
    PLL0_feed_sequence(); 
    
    // Wait 500 cycles. TODO: calculate correct wait time...
    for (int i=0; i<500; i++);
    LPC_SC->PLL0CON |= 2;      // Set PLL0 Connect Flag 
    PLL0_feed_sequence();      // Commit Changes    
}

// Main method
int main(void) {

  // Set P0[0] to Input 
  LPC_GPIO0->FIODIR &= ~(1 << UINPUT_1);
  // Enable Rising Edge Interrupt on P0[0]
  LPC_GPIOINT->IO0IntEnR |= (1 << UINPUT_1);
  // Enable Falling Edge Interrupt on P0[0] 
  LPC_GPIOINT->IO0IntEnF |= (1 << UINPUT_1);
  // Turn on External Interrupt 3 
  NVIC_EnableIRQ(EINT3_IRQn);
  
  // Calculate and apply clock settings
  clock_settings *clkset = calculate_clock_settings(120000000);  
  apply_clock_settings(clkset);
  
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
    
  // enable power on Tim0
  LPC_SC->PCONP |= (1<<1);
  
  // Set the PC on Tim0 to 1
  LPC_TIM0->PC = 1;
  LPC_TIM0->TCR |= 1;
  
  // Configure Tim0's counter and interrupt
  LPC_TIM0->MR0 = (uint32_t) 1199; 
  LPC_TIM0->MCR |= 1 | 2;
  LPC_TIM0->EMR |= 1 | (1<<5);
  
  // Enable Tim0's interrupt
  NVIC_EnableIRQ(TIMER0_IRQn);
  
  // Main loop
  while(1){
    
    // Hang out for a few cycles
    for (int i=0; i<200; i++);
    
    // Do stuff based on global state
    if (state == 1){
        LPC_TIM0->TCR |= 2;
    } else {
        LPC_TIM0->TCR &= ~2;
    }
  }
  
  return 0;
}

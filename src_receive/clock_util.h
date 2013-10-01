/**
 * LPC Clock Speed Calculator Utility
 *
 * Robert Brownstein
 * University of Maryland, College Park
 * Embedded Systems
 */

// Dependancies
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

// LPC Clocks
#define CLOCK_SPEED_OSC_CLK 12000000 // 12   MHz 
#define CLOCK_SPEED_IRC_OSC  4000000 // 4    MHz
#define CLOCK_SPEED_RTC_CLK    32700 // 32.7 KHz 

// LPC PLL0CLK Min/Max speeds
#define CLOCK_SPEED_PLL0CLK_MIN 275000000 // Min pll0clk speed
#define CLOCK_SPEED_PLL0CLK_MAX 550000000 // Max pll0clk speed

// MAX CPU clock speed
#define CLOCK_SPEED_CCLK_MAX 120000000 // Max cclk speed

// Constraints for C, N, and M variables
#define D_MIN 0
#define D_MAX 255
#define N_MIN 1
#define N_MAX 32
#define M_MIN 6
#define M_MAX 512 

/**
 * Clock settings data type.
 */
typedef struct {
    int frequency;
    int val_CLKSRCSEL;
    int val_N_factor;
    int val_M_factor;
    int val_PLL0CON;
    int val_D_factor;
} clock_settings;

/** 
 * Calculates the frequency of pll0clk given...
 *
 * N: The value of PLL0's N-Divider register
 * M: The value of PLL0's internal clock divider factor register
 * f_sysclk: The frequency of the main connected oscillator
 */
double calc_f_pll0clk(int N, int M, int f_sysclk);

/** 
 * Calculates the frequency of the main CPU clock given...
 *
 * f_pllclk: The output of the CPU PLL selector
 * D: The value of the CPU clock divider's factor register
 */
double calc_f_cclk(double f_pllclk, int D);

/**
 * Calculates optimal clock settings given a desired frequency.
 */
clock_settings * calculate_clock_settings(int desired_frequency);

/**
 * LPC Clock Speed Calculator Utility
 *
 * Robert Brownstein
 * University of Maryland, College Park
 * Embedded Systems
 */

#include "clock_util.h"

/** 
 * Calculates the frequency of pll0clk given...
 *
 * N: The value of PLL0's N-Divider register
 * M: The value of PLL0's internal clock divider factor register
 * f_sysclk: The frequency of the main connected oscillator
 */
double calc_f_pll0clk(int N, int M, int f_sysclk){
    double f = 2.0 * (M+1.0) * f_sysclk;
    f = f / (N+1.0);
    return f;
}


/** 
 * Calculates the frequency of the main CPU clock given...
 *
 * f_pllclk: The output of the CPU PLL selector
 * D: The value of the CPU clock divider's factor register
 */
double calc_f_cclk(double f_pllclk, int D){
    return (f_pllclk) / (D*1.0 + 1.0);
}

clock_settings global_settings;

/**
 * Calculates clock settings to match a desired frequency.
 */
clock_settings * calculate_clock_settings(int desired_frequency){
    int i=0;
    int CLOCK_SEL = -1;
    int D=0;
    int D_SEL=0;
    int N=0;
    int N_SEL=0;
    int M=0;
    int M_SEL=0;
        
    //clock_settings *settings = malloc(sizeof(clock_settings));
    clock_settings *settings = &global_settings;
    memset(settings, 0, sizeof(clock_settings));
    
    int clocks[3];
    clocks[0] = CLOCK_SPEED_IRC_OSC;
    clocks[1] = CLOCK_SPEED_OSC_CLK;
    clocks[2] = CLOCK_SPEED_RTC_CLK;
    
    int needs_pll = 1;
    for (i=0; i < 3; i++){
        if (clocks[i] >= desired_frequency){
            int freq = clocks[i] / desired_frequency;
            if ((freq * desired_frequency == clocks[i]) && (freq <= 255)){
                needs_pll = 0;
                CLOCK_SEL = i;
            }
        }
    }
        
    // If the PLL is unneeded, life is good.
    if (!needs_pll){
        settings->val_CLKSRCSEL = CLOCK_SEL;
        settings->val_PLL0CON = 0;
        settings->val_D_factor = clocks[CLOCK_SEL] / desired_frequency;
        settings->val_D_factor += -1;
        settings->frequency = desired_frequency;
        return settings;
    }
    
    settings->val_CLKSRCSEL = i;
    
    int f_pllclk;
    
    for (D = D_MIN; (D <= D_MAX) && (D_SEL == 0); D++){
        f_pllclk = desired_frequency * (D+1);
        if (f_pllclk >= CLOCK_SPEED_PLL0CLK_MIN &&
                f_pllclk <= CLOCK_SPEED_PLL0CLK_MAX){
            D_SEL = D;
        }
    }
    
    double least_error = DBL_MAX;
    int best_frequency = 0;
    
    for (i=0; i<3; i++){
        for (N = N_MIN; N <= N_MAX; N++){
            for (M = M_MIN; M <= M_MAX; M++){
                double freq = calc_f_pll0clk(N,M,clocks[i]);
                freq = calc_f_cclk(freq, D_SEL);
                double error = (freq-desired_frequency);
                error = error*error;
                if (error < least_error){
                    least_error = error;
                    CLOCK_SEL = i;
                    N_SEL = N;
                    M_SEL = M;
                    best_frequency = (int) freq;
                }
            }
        }
    }
    
    settings->val_CLKSRCSEL = CLOCK_SEL;
    settings->val_N_factor = N_SEL;
    settings->val_M_factor = M_SEL;
    settings->val_D_factor = D_SEL;
    settings->frequency = best_frequency;
    settings->val_PLL0CON = 1;
    
    return settings;        
}

/* // Main function for use when compiled independently.
int main(int argc, char *argv[]){
    if (argc != 2){
        puts("Please specify a frequency via command line.");
        return 0;
    }
    
    int desired_freq = atoi(argv[1]);
    
    clock_settings *settings = calculate_clock_settings(desired_freq);
    
    printf("Closest frequency: %d\n", settings->frequency);
    printf("Clock: %d\n", settings->val_CLKSRCSEL);
    printf("N: %d\n", settings->val_N_factor);
    printf("M: %d\n", settings->val_M_factor);
    printf("PLL0CON: %d\n", settings->val_PLL0CON);
    printf("D: %d\n", settings->val_D_factor);
    
    return 0;
}*/

/*
 ==============================================
 Name        : SN74HC164N.c
 Author      :
 Version     :
 Description : Support for the SN74HC164N shift register
 ==============================================
 */

#include <string.h>

typedef struct {
    int step;
    int bits;
    int holdtime;
    volatile uint32_t  *reg_clock;
    volatile uint32_t  *reg_clear;
    volatile uint32_t  *reg_a;
    int mask_clock;
    int mask_clear;
    int mask_a;
} SN74HC164N_state;

void SN74HC164N_init(SN74HC164N_state *state){
    memset(state, 0, sizeof(SN74HC164N_state));
    state->step = 0;
    state->bits = 2;
    state->holdtime = 1000;
}

void SN74HC164N_step(SN74HC164N_state *state){
    if (state->step <= 31){
        int substep = (state->step) % 4;
        int s, mask;
        switch(substep){
            case 0:
                *state->reg_clock &= ~state->mask_clock;
                break;
            case 1:
                s = 7-(state->step / 4);
                mask = 1 << s;
                if ((state->bits) & mask){
                    *state->reg_a |= state->mask_a;
                } else {
                    *state->reg_a &= ~state->mask_a;
                }
                break;
            case 2:
                *state->reg_clock |= state->mask_clock;
                break;
            default:
                break;
        }
    }
    
    if (state->step >= state->holdtime){
        int clearstep = state->step - state->holdtime;
        switch(clearstep){
            case 1:
                *state->reg_clock &= ~state->mask_clock;
                break;
            case 2:
                *state->reg_clear &= ~state->mask_clear;
                break;
            case 3:
                *state->reg_clock |= state->mask_clock;
                break;
            case 4:
                *state->reg_clear |= state->mask_clear;
                break;
            default:
                break;
        }
        if (clearstep > 5){
            state->step = -1;
        }
    }
    state->step++;
}

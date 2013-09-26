/*
 ==============================================
 Name        : Receive.c
 Author      :
 Version     :
 Description : Allows the LPC to receive bits sent using a primitive format.
 ==============================================
 */

#include <string.h>

#define SIGNAL_DEFAULT     0
#define SIGNAL_WAITING     1
#define SIGNAL_CLOCK_SYNC1 2
#define SIGNAL_CLOCK_SYNC2 3
#define SIGNAL_RECIEVING   4
#define SIGNAL_COMPLETE    5

#define RECEIVE_BUFFER_LEN 1000

typedef struct {

    int bit;

    char *bitbuffer;
    int bitbufferlen;
    int bitbufferpos;
    int bitbufferbit;
    
    int state;
    
    int pulse_width;
    int total_clock_pulse_time;
    int avg_clock_pulse_time;
    int num_clock_pulses;
    
    int systime_next;
    int systime_prev;
    
    volatile uint32_t *input_source;
    int input_mask;
    
} receive_state;

char global_receive_bits[RECEIVE_BUFFER_LEN];

void receive_init( receive_state *state, volatile uint32_t *source, int mask){
    
    memset(&state->bitbuffer, 0, RECEIVE_BUFFER_LEN);
    state->bitbuffer = global_receive_bits;
    state->bitbufferlen = RECEIVE_BUFFER_LEN;
    state->bitbufferpos = 0;
    state->bitbufferbit = 0;
    state->state = SIGNAL_WAITING;
    state->pulse_width = 0;
    state->total_clock_pulse_time = 0;
    state->avg_clock_pulse_time = 0;
    state->num_clock_pulses = 0;
    state->input_source = source;
    state->input_mask   = mask;
    state->systime_next = 0;
    state->systime_prev = 0;
    state->bit = 0;
}

void receive_step(receive_state *state, int systime){

    int bit;
    
    switch(state->state){
    
        case SIGNAL_DEFAULT:
            return;
            break;
    
        case SIGNAL_WAITING:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
            if (bit != state->bit){
                state->systime_prev = systime;
                state->bit = 1;
                state->num_clock_pulses = 0;
                state->state = SIGNAL_CLOCK_SYNC1;
            }
            break;
        
        case SIGNAL_CLOCK_SYNC1:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
            
            if (bit != state->bit){
            
                state->bit = bit;
                int pulse_time = systime - state->systime_prev;
                state->total_clock_pulse_time += pulse_time;
                state->num_clock_pulses++;
                state->systime_prev = systime;
                
                if (state->num_clock_pulses >= 4){
                    state->state = SIGNAL_CLOCK_SYNC2;
                    state->avg_clock_pulse_time = 
                        state->total_clock_pulse_time / state->num_clock_pulses;
                    state->systime_next = systime + state->avg_clock_pulse_time;
                }
                
            }
            
            break;
            
        case SIGNAL_CLOCK_SYNC2:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
            
            // if an expected bit does not appear for at least .5 pulse widths,
            // we probably need to enter receive mode.
            if (systime > state->systime_next + state->avg_clock_pulse_time/2){
                state->state = SIGNAL_RECIEVING;
                state->systime_next = systime + state->avg_clock_pulse_time/2;
                state->systime_prev = systime;
                
                state->bitbuffer[0] = bit;
                state->bitbufferbit = 1;
                state->bitbufferpos = 0;
                state->bit = bit;
                return;
            }
            
            if (bit != state->bit){
                state->bit = bit;
                int pulse_time = systime - state->systime_prev;
                state->total_clock_pulse_time += pulse_time;
                state->num_clock_pulses++;
                state->systime_prev = systime;
                state->avg_clock_pulse_time = 
                        state->total_clock_pulse_time / state->num_clock_pulses;
                    state->systime_next = systime + state->avg_clock_pulse_time;
            }
            
            break;
            
        case SIGNAL_RECIEVING:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
        
            // if we aren't at the sample point, return
            if (systime < state->systime_next){
                return;
            }
            
            state->systime_next += state->avg_clock_pulse_time;
            
            int bitmask = 1 << state->bitbufferbit;
            int bitval  = bit << state->bitbufferbit;
            
            state->bitbuffer[state->bitbufferpos] &= ~bitmask;
            state->bitbuffer[state->bitbufferpos] |=  bitval;
            
            state->bitbufferbit++;
            if (state->bitbufferbit >= 8){
                state->bitbufferpos++;
                state->bitbufferbit = 0;
            }
            
            if (state->bit == bit){
                if (state->systime_prev + state->avg_clock_pulse_time*32 < systime){
                    state->state = SIGNAL_COMPLETE;
                }
            } else {
                state->bit = bit;
                state->systime_prev = systime;
            }
            
            break;
            
        case SIGNAL_COMPLETE:
            
            break;
    }
}

/*
 ==============================================
 Name        : Receive.c
 Author      :
 Version     :
 Description : Allows the LPC to receive bits sent using a primitive format.
 ==============================================
 */

#include <string.h>

// Signal receive states
#define SIGNAL_DEFAULT     0
#define SIGNAL_WAITING     1
#define SIGNAL_CLOCK_SYNC1 2
#define SIGNAL_CLOCK_SYNC2 3
#define SIGNAL_RECEIVING   4
#define SIGNAL_COMPLETE    5
#define SIGNAL_CLOCK_SYNC_AWAIT_MESSAGE_START 6

// Message begin/end flags (TODO: IMPLEMENT)
#define FRAME_FLAG_BEGIN   0xf
#define FRAME_FLAG_END     0x0
#define FRAME_FLAGS_ENABLED 0

// Input buffer length
#define RECEIVE_BUFFER_LEN 1024

// Receive state definition
typedef struct {

    int state;    // Current state of receive sequence
    int bit;      // Current 1/0 on input pin

    char *bitbuffer;   // Buffer to store input
    int bitbufferlen;  // Input buffer length
    int bitbufferpos;  // Write position in bit buffer
    int bitbufferbit;  // Current bit to write to
    
    int systime_next;  // Next system time for sampling
    int systime_prev;  // Last system time sampled
    
    int total_clock_pulse_time; // Total time spent receiving clock pulses
    int avg_clock_pulse_time;   // Average clock pulse duration
    int num_clock_pulses;       // Number of clock pulses received
    
    volatile uint32_t *input_source; // Input source register pointer
    int input_mask;                  // Mask to use when reading input source
    
    char last_eight_bits;
        
} receive_state;

// Define a global receive buffer
char global_receive_bits[RECEIVE_BUFFER_LEN];

// Function to initialize a receive state - sets variables to initial values
void receive_init( receive_state *state, volatile uint32_t *source, int mask){
    
    memset(&state->bitbuffer, 0, RECEIVE_BUFFER_LEN);
    
    state->bitbuffer = global_receive_bits;
    state->bitbufferlen = RECEIVE_BUFFER_LEN;
    state->bitbufferpos = 0;
    state->bitbufferbit = 0;
    
    state->state = SIGNAL_WAITING;
    state->total_clock_pulse_time = 0;
    state->avg_clock_pulse_time = 0;
    state->num_clock_pulses = 0;
    state->input_source = source;
    state->input_mask   = mask;
    
    state->systime_next = 0;
    state->systime_prev = 0;
    
    state->bit = *state->input_source & state->input_mask;
    state->bit = state->bit && state->bit;
    
    state->last_eight_bits = 0;
}

///////////// TODO FINISH ////////////////

void receive_sync_clock(receive_state *state, int systime, int bit){
    
    if (bit != state->bit){
            
        state->bit = bit;
        int time_delta = systime - state->systime_prev;
        
        state->total_clock_pulse_time += time_delta;
        state->num_clock_pulses++;
        state->systime_prev = systime;
        
        if (state->num_clock_pulses >= 4){
            state->state = SIGNAL_CLOCK_SYNC2;
            state->avg_clock_pulse_time = 
                state->total_clock_pulse_time / state->num_clock_pulses;
                
            state->systime_next = systime + state->avg_clock_pulse_time;
        }
        
    }
    
}

void receive_process_bit(receive_state *state, int bit){
    state->last_eight_bits = state->last_eight_bits << 1;
    
    if (bit){
        state->last_eight_bits |= 1;
    }
    
    if (state->last_eight_bits == FRAME_FLAG_BEGIN){
        state->bitbufferpos = 0;
        state->bitbufferbit = 0;
        state->state = SIGNAL_RECEIVING;
        return;
    }
    if (state->last_eight_bits == FRAME_FLAG_END){
        state->state = SIGNAL_COMPLETE;
        return;
    }
    
    if (state->state != SIGNAL_RECEIVING) return;
    
    int bitmask = 1   << state->bitbufferbit;
    int bitval  = bit << state->bitbufferbit;
    
    state->bitbuffer[state->bitbufferpos] &= ~bitmask;
    state->bitbuffer[state->bitbufferpos] |=  bitval;
    
    state->bitbufferbit++;
    if (state->bitbufferbit >= 8){
        state->bitbufferpos++;
        state->bitbufferbit = 0;
    }
}

//////////////// END TODO /////////////////

// Function to drive the receive functionality
void receive_step(receive_state *state, int systime){
    int bit;
    switch(state->state){
    
        case SIGNAL_DEFAULT:
            break;
    
        case SIGNAL_WAITING:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
            if (bit != state->bit){
                state->systime_prev = systime;
                state->bit = bit;
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
                state->state = SIGNAL_RECEIVING;
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
            
        case SIGNAL_CLOCK_SYNC_AWAIT_MESSAGE_START:
            break;
                        
        case SIGNAL_RECEIVING:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
        
            // if we aren't at the sample point, return
            if (systime < state->systime_next){
                return;
            }
            
            state->systime_next += state->avg_clock_pulse_time;
            
            int bitmask = 1   << state->bitbufferbit;
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

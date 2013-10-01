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
#define SIGNAL_CLOCK_SYNC  2
#define SIGNAL_AWAIT_FRAME 4
#define SIGNAL_RECEIVING   5
#define SIGNAL_COMPLETE    6

// Message begin/end flags (TODO: IMPLEMENT)
#define FRAME_FLAG_BEGIN   15
#define FRAME_FLAG_END     0
#define FRAME_FLAGS_ENABLED 0

// Input buffer length
#define RECEIVE_BUFFER_LEN 100

// Receive state definition
typedef struct {

    int state;    // Current state of receive sequence
    int last_bit;      // Current 1/0 on input pin

    char *bit_buffer;   // Buffer to store input
    int   bit_buffer_len;
    int   bit_buffer_pos;
    
    char last_eight_bits;
    int  last_eight_bits_pos;
    
    int pulse_time_total;
    int num_pulses;
    int avg_pulse_time;
    
    int systime_prev_pulse;
    int systime_next_pulse;
    int systime_next_sample;
    
    volatile uint32_t *input_source; // Input source register pointer
    int input_mask;                  // Mask to use when reading input source
        
} receive_state;

// Define a global receive buffer
char global_receive_bits[RECEIVE_BUFFER_LEN];

// Function to initialize a receive state - sets variables to initial values
void receive_init( receive_state *state, volatile uint32_t *source, int mask){
    
    state->state    = SIGNAL_WAITING;
    state->last_bit = 0;
    
    state->bit_buffer = global_receive_bits;
    state->bit_buffer_len = RECEIVE_BUFFER_LEN;
    state->bit_buffer_pos = 0;
    
    memset(state->bit_buffer, 0, RECEIVE_BUFFER_LEN);
    
    state->last_eight_bits = 0x00;
    state->last_eight_bits_pos = 0;
    
    state->pulse_time_total = 0;
    state->num_pulses       = 0;
    state->avg_pulse_time   = 0;
    
    state->systime_prev_pulse  = 0;
    state->systime_next_pulse  = 0;
    state->systime_next_sample = 0;
    
    state->input_source = source;
    state->input_mask   = mask;
}

void receive_match_flags(receive_state *state, int bit){
    state->last_eight_bits = state->last_eight_bits << 1;
    if (bit){ 
        state->last_eight_bits |=  (char) 1; 
    } else {
        state->last_eight_bits &= (char) ~1; 
    }
    if ((int)state->last_eight_bits == (int)FRAME_FLAG_BEGIN){
    
        state->last_eight_bits = 0;
        state->last_eight_bits_pos = 0;

        state->state = SIGNAL_RECEIVING;
        return;
    }
}

void receive_sync_clock(receive_state *state, int systime, int bit){
    
    if (bit != state->last_bit){
    
        state->last_bit = bit;
        
        int time_delta = systime - state->systime_prev_pulse;
        state->pulse_time_total += time_delta;
        state->num_pulses++;
        state->systime_prev_pulse = systime;
        
        if (state->num_pulses >= 4){
            
            if (state->num_pulses % 2 == 0){
                state->avg_pulse_time = 
                state->pulse_time_total / state->num_pulses;
            }
            
            state->systime_next_pulse = systime + state->avg_pulse_time;
            state->systime_next_sample = state->systime_next_pulse + (state->avg_pulse_time/2) + (state->avg_pulse_time/4);
        }
        
    } else {
        if (state->num_pulses >= 4){
            if (systime > state->systime_next_sample){
                state->state = SIGNAL_AWAIT_FRAME;
                receive_match_flags(state, bit);
                return;
            }
        }
    }
}

void receive_process_bit(receive_state *state, int bit){
    
    if (state->state != SIGNAL_RECEIVING) return;
    
    int bitmask = 1   << state->last_eight_bits_pos;
    int bitval  = bit << state->last_eight_bits_pos;
    
    state->last_eight_bits &= ~bitmask;
    state->last_eight_bits |=  bitval;
    
    state->last_eight_bits_pos++;
    
    if (state->last_eight_bits_pos >= 8){
    
        state->last_eight_bits_pos = 0;
        state->bit_buffer[state->bit_buffer_pos] = state->last_eight_bits;        
        state->bit_buffer_pos++;
        
        if (state->bit_buffer_pos >= state->bit_buffer_len){
            state->bit_buffer_pos--;
            state->bit_buffer[state->bit_buffer_pos] = '\0';
            state->state = SIGNAL_COMPLETE;
        }
        
        if ((int) state->last_eight_bits == 0){
            state->bit_buffer[state->bit_buffer_pos] = '\0';
            state->state = SIGNAL_COMPLETE;
        }
        
        state->last_eight_bits = 0;
    }
    
    state->last_bit = bit;
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
            if (bit != state->last_bit){
                state->systime_prev_pulse = systime;
                state->last_bit = bit;
                state->num_pulses = 0;
                state->state = SIGNAL_CLOCK_SYNC;
            }
            break;
        
        case SIGNAL_CLOCK_SYNC:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
            
            receive_sync_clock(state, systime, bit);
            break;
        
        case SIGNAL_AWAIT_FRAME:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
            
            // if we aren't at the sample point, return
            if (systime < state->systime_next_sample){
                return;
            }
            
            state->systime_next_sample += state->avg_pulse_time;
            receive_match_flags(state, bit);
            break;
                        
        case SIGNAL_RECEIVING:
            bit = *state->input_source & state->input_mask;
            bit = bit && bit;
        
            // if we aren't at the sample point, return
            if (systime < state->systime_next_sample){
                return;
            }
            
            state->systime_next_sample += state->avg_pulse_time;
            receive_process_bit(state, bit);
            break;
            
        case SIGNAL_COMPLETE:  
            return;    
            break;
    }
}

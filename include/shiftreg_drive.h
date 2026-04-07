#ifndef SHIFTREG_DRIVE_H
#define  SHIFTREG_DRIVE_H

#include <Arduino.h>

/* Pin definitions */
#define   SR_DATA_IN_PIN    33 // was 12 (changed to enable debugging)
#define   SR_LATCH_PIN      25 // was 13 (changed to enable debugging)
#define   SR_CLK_PIN        32 // was 14 (changed to enable debugging)

/* CD4094 Shift Registers connection diagram */
/*
                    SR#1 (PUMPS)                    --> SR2 (VALVES)                    --> SR3 (LED BAR)
output_pin(Q):      Q8  Q7  Q6  Q5  Q4  Q3  Q2  Q1  --> Q8  Q7  Q6  Q5  Q4  Q3  Q2  Q1  --> Q8  Q7  Q6  Q5  Q4  Q3  Q2  Q1
use for     :       -   -   P6  P5  P4  P3  P2  P1  --> V8  V7  V6  V5  V4  V3  V2  V1  --> L8  L7  L6  L5  L4  L3  L2  L1
bit_identity:       7   6   5   4   3   2   1   0   --> 7   6   5   4   3   2   1   0   --> 7   6   5   4   3   2   1   0
*/

/* Shift Reg ID */
#define   SR1           1
#define   SR2           2
//#define   SR3           3

/* Bit definitions */
#define   SR1_BIT_P1    0
#define   SR1_BIT_P2    1
#define   SR1_BIT_P3    2
#define   SR1_BIT_P4    3
#define   SR1_BIT_P5    4
#define   SR1_BIT_P6    5

#define   SR2_BIT_V1    0
#define   SR2_BIT_V2    1
#define   SR2_BIT_V3    2
#define   SR2_BIT_V4    3
#define   SR2_BIT_V5    4
#define   SR2_BIT_V6    5
#define   SR2_BIT_V7    6
#define   SR2_BIT_V8    7

//Shift register untuk LED tidak jadi terpakai
/*
#define   SR3_BIT_L1    0
#define   SR3_BIT_L2    1
#define   SR3_BIT_L3    2
#define   SR3_BIT_L4    3
#define   SR3_BIT_L5    4
#define   SR3_BIT_L6    5
#define   SR3_BIT_L7    6
#define   SR3_BIT_L8    7
*/

///////////////////////////////////////////////////////////////////////////
/// PROTOTYPES ////
///////////////////////////////////////////////////////////////////////////
void SR_setup_pins(void);
void SR_shift_out(void);
void SR_turn_off_all(void);
//void SR_turn_off_all_pumps_and_valves(void);

void SR_turn_on_P1(void);
void SR_turn_off_P1(void);
void SR_turn_on_P2(void);
void SR_turn_off_P2(void);
void SR_turn_on_P3(void);
void SR_turn_off_P3(void);
void SR_turn_on_P4(void);
void SR_turn_off_P4(void);
void SR_turn_on_P5(void);
void SR_turn_off_P5(void);
void SR_turn_on_P6(void);
void SR_turn_off_P6(void);

/*
void SR_turn_on_L1(void);
void SR_turn_off_L1(void);
void SR_turn_on_L2(void);
void SR_turn_off_L2(void);
void SR_turn_on_L3(void);
void SR_turn_off_L3(void);
void SR_turn_on_L4(void);
void SR_turn_off_L4(void);
void SR_turn_on_L5(void);
void SR_turn_off_L5(void);
void SR_turn_on_L6(void);
void SR_turn_off_L6(void);
void SR_turn_on_L7(void);
void SR_turn_off_L7(void);
void SR_turn_on_L8(void);
void SR_turn_off_L8(void);
void SR_set_ledbar(int num);
*/

void SR_turn_on_V1(void);
void SR_turn_off_V1(void);
void SR_turn_on_V2(void);
void SR_turn_off_V2(void);
void SR_turn_on_V3(void);
void SR_turn_off_V3(void);
void SR_turn_on_V4(void);
void SR_turn_off_V4(void);
void SR_turn_on_V5(void);
void SR_turn_off_V5(void);
void SR_turn_on_V6(void);
void SR_turn_off_V6(void);
void SR_turn_on_V7(void);
void SR_turn_off_V7(void);
void SR_turn_on_V8(void);
void SR_turn_off_V8(void);

void SR_BP_P1_Inflate(void);
void SR_BP_P1_Measure(void);
void SR_BP_P1_Hold(void);
void SR_BP_P2_Inflate(void);
void SR_BP_P2_Measure(void);
void SR_BP_P2_Hold(void);
void SR_BP_P3_Inflate(void);
void SR_BP_P3_Measure(void);
void SR_BP_P4_Inflate(void);
void SR_BP_P4_Measure(void);
void SR_BP_P5_Inflate(void);
void SR_BP_P5_Measure(void);
void SR_BP_P6_Inflate(void);
void SR_BP_P6_Measure(void);

void SR_COMP_LEFT_P1_Measure(void);
void SR_COMP_LEFT_P1_Inflate(void);
void SR_COMP_RIGHT_P2_Measure(void);
void SR_COMP_RIGHT_P2_Inflate(void);
void SR_RHI_P1_Measure(void);
void SR_RHI_P1_Inflate(void);


// === PWM generik P1..P6 ===
// pump_id: 1..6, freq_hz: >=10 (disarankan), duty_percent: 0..100
void PumpPWM_start(uint8_t pump_id, uint32_t freq_hz, uint8_t duty_percent);
void PumpPWM_update(uint8_t pump_id, uint32_t freq_hz, uint8_t duty_percent);
void PumpPWM_stop(uint8_t pump_id);
void PumpPWM_stop_all(void);

// Shortcut: default 45% @ 500 Hz
void PumpPWM_start_default(uint8_t pump_id);


#endif
///////////////////////////////////////////////////////////////////////////
/// END ////
///////////////////////////////////////////////////////////////////////////
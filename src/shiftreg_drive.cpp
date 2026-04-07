#include "shiftreg_drive.h"
// Tambahan di shiftreg_drive.cpp
#include <Arduino.h>
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  //#include "esp32/rom/ets_sys.h" // ets_delay_us
}
// extern shift register last latch (global var)
extern uint8_t SR1_last_latch;
extern uint8_t SR2_last_latch;
//extern uint8_t SR3_last_latch;

const int SR_data_pin = SR_DATA_IN_PIN;
const int SR_latch_pin = SR_LATCH_PIN;
const int SR_clk_pin = SR_CLK_PIN;

// Critical section untuk update latch + shift_out
static portMUX_TYPE sr_mux = portMUX_INITIALIZER_UNLOCKED;

// Helper atomik set/reset bit pompa + shift_out
static inline void pump_bit_write_atomic(uint8_t bit_index, bool on) {
  portENTER_CRITICAL(&sr_mux);
  if (on)  { SR1_last_latch |=  (1 << bit_index); }
  else     { SR1_last_latch &= ~(1 << bit_index); }
  SR_shift_out();
  portEXIT_CRITICAL(&sr_mux);
}

// Struktur kanal PWM per pompa (P1..P6)
typedef struct {
  TaskHandle_t task;
  volatile bool running;
  uint8_t pump_id;     // 1..6
  uint8_t bit_index;   // 0..5
  volatile uint32_t period_us;
  volatile uint32_t high_us;
  volatile uint32_t low_us;
} pwm_ch_t;

static pwm_ch_t pwm_ch[6] = {0};

// Task PWM generik untuk satu pompa
static void pump_pwm_task(void* pv) {
  const int idx = (int)(intptr_t)pv;
  const uint8_t bit = pwm_ch[idx].bit_index;

  // Gunakan mode tick untuk periode >= 2ms, microsecond delay untuk yang lebih cepat
  while (pwm_ch[idx].running) {
    uint32_t high_us = pwm_ch[idx].high_us;
    uint32_t low_us  = pwm_ch[idx].low_us;
    uint32_t period  = pwm_ch[idx].period_us;

    if (high_us > 0) {
      pump_bit_write_atomic(bit, true);
      if (period >= 2000) {
        vTaskDelay(pdMS_TO_TICKS((high_us + 500) / 1000));
      } else {
        ets_delay_us(high_us);
      }
    }

    if (low_us > 0) {
      pump_bit_write_atomic(bit, false);
      if (period >= 2000) {
        vTaskDelay(pdMS_TO_TICKS((low_us + 500) / 1000));
      } else {
        ets_delay_us(low_us);
      }
    }

    // Duty 0% atau 100% tetap loop agar bisa menerima update parameter
    if (high_us == 0 && low_us == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  // Pastikan pompa OFF saat berhenti
  pump_bit_write_atomic(bit, false);
  vTaskDelete(nullptr);
}

// Konversi pump_id (1..6) ke index array (0..5) dan bit (0..5)
static inline bool resolve_pump(uint8_t pump_id, int& idx, uint8_t& bit_index) {
  if (pump_id < 1 || pump_id > 6) return false;
  idx = pump_id - 1;
  bit_index = idx; // P1->bit0, P2->bit1, ..., P6->bit5
  return true;
}

static inline void compute_timings(uint32_t freq_hz, uint8_t duty_percent,
                                   uint32_t& period_us, uint32_t& high_us, uint32_t& low_us) {
  if (freq_hz < 10) freq_hz = 10;
  if (duty_percent > 100) duty_percent = 100;
  period_us = 1000000UL / freq_hz;
  high_us   = (period_us * duty_percent) / 100;
  low_us    = period_us - high_us;
  // Pastikan ada waktu minimal 1 us bila duty > 0 atau < 100 untuk menjaga toggling
  if (duty_percent > 0 && high_us == 0) high_us = 1;
  if (duty_percent < 100 && low_us  == 0) low_us  = 1;
}


/////////////////////////////////////////// Setup shift reg pins
void SR_setup_pins(void)
{
    pinMode(SR_data_pin, OUTPUT);
    pinMode(SR_latch_pin, OUTPUT);
    pinMode(SR_clk_pin, OUTPUT);
}

/////////////////////////////////////////// Shift reg shift out procedure
void SR_shift_out(void)
{
    digitalWrite(SR_latch_pin, LOW);
    shiftOut(SR_data_pin, SR_clk_pin, MSBFIRST, SR2_last_latch);
    shiftOut(SR_data_pin, SR_clk_pin, MSBFIRST, SR1_last_latch);
    digitalWrite(SR_latch_pin, HIGH);
}


/////////////////////////////////////////// Shift reg turn off all outputs
void SR_turn_off_all(void)
{
    memset(&SR1_last_latch, 0, sizeof(SR1_last_latch));
    memset(&SR2_last_latch, 0, sizeof(SR2_last_latch));
    SR_shift_out();
}

/*
/////////////////////////////////////////// Shift reg turn off all Pumps and Valves
void SR_turn_off_all_pumps_and_valves(void)
{
    memset(&SR1_last_latch, 0, sizeof(SR1_last_latch));
    memset(&SR2_last_latch, 0, sizeof(SR2_last_latch));
    SR_shift_out();
}
*/


/////////////////////////////////////////// 
void SR_BP_P1_Measure(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P1);       // SR_turn_off_P1();
    SR2_last_latch |= (1 << SR2_BIT_V1);        // SR_turn_on_V1();
    SR2_last_latch &= ~(1 << SR2_BIT_V2);       // SR_turn_off_V2();
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P1_Inflate(void)
{

     SR1_last_latch |= (1 << SR1_BIT_P1);       //SR_turn_on_P1();            // Pump Channel 1
     SR2_last_latch |= (1 << SR2_BIT_V1);       //SR_turn_on_V1();            // Valve Channel 1
     SR2_last_latch |= (1 << SR2_BIT_V2);       //SR_turn_on_V2();            // Valve Channel 2
     SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P1_Hold(void)
{
     SR1_last_latch &= ~(1 << SR1_BIT_P1);       // SR_turn_off_P1();            // Pump Channel 1
    //  SR2_last_latch |= (1 << SR2_BIT_V1);       //SR_turn_on_V1();            // Valve Channel 1
    //  SR2_last_latch |= (1 << SR2_BIT_V2);       //SR_turn_on_V2();            // Valve Channel 2
     SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P2_Measure(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P2);       //SR_turn_off_P2();          // Pump Channel 2
    SR2_last_latch |= (1 << SR2_BIT_V3);        //SR_turn_on_V3();           // Valve Channel 3
    SR2_last_latch &= ~(1 << SR2_BIT_V4);        //SR_turn_off_V4();          // Valve Channel 4
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P2_Inflate(void)
{
     SR1_last_latch |= (1 << SR1_BIT_P2);       //SR_turn_on_P2();            // Pump Channel 2
     SR2_last_latch |= (1 << SR2_BIT_V3);       //SR_turn_on_V3();            // Valve Channel 3
     SR2_last_latch |= (1 << SR2_BIT_V4);       //SR_turn_on_V4();            // Valve Channel 4
     SR_shift_out();
}

void SR_BP_P2_Hold(void)
{
     SR1_last_latch &= ~(1 << SR1_BIT_P2);       //SR_turn_off_P2();            // Pump Channel 2
    //  SR2_last_latch |= (1 << SR2_BIT_V3);       //SR_turn_on_V3();            // Valve Channel 3
    //  SR2_last_latch |= (1 << SR2_BIT_V4);       //SR_turn_on_V4();            // Valve Channel 4
     SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P3_Measure(void)
{
     SR1_last_latch &= ~(1 << SR1_BIT_P3);       //SR_turn_off_P3();           // Pump Channel 3
     SR2_last_latch |= (1 << SR2_BIT_V5);        //SR_turn_on_V5();            // Valve Channel 3
     SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P3_Inflate(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P3);        //SR_turn_on_P3();   // Pump Channel 3
    SR2_last_latch |= (1 << SR2_BIT_V5);        //SR_turn_on_V5();   // Valve Channel 3
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P4_Measure(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P4);        //SR_turn_off_P4();           // Pump Channel 4
    SR2_last_latch |= (1 << SR2_BIT_V6);        //SR_turn_on_V6();            // Valve Channel 4        
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P4_Inflate(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P4);        //SR_turn_on_P4();            // Pump Channel 4
    SR2_last_latch |= (1 << SR2_BIT_V6);        //SR_turn_on_V6();            // Valve Channel 4
    SR_shift_out();
}

void SR_BP_P5_Measure(void)
{
    PumpPWM_stop(5);
    //SR1_last_latch &= ~(1 << SR1_BIT_P5);       // SR_turn_off_P5();
    SR2_last_latch |= (1 << SR2_BIT_V1);        // SR_turn_on_V1();
    SR2_last_latch &= ~(1 << SR2_BIT_V2);       // SR_turn_off_V2();
    SR2_last_latch |= (1 << SR2_BIT_V7);        // SR_turn_on_V7();            // Valve Channel 2
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P5_Inflate(void)
{
    PumpPWM_start_default(5);
     //SR1_last_latch |= (1 << SR1_BIT_P5);       //SR_turn_on_P5();            // Pump Channel 1
     SR2_last_latch |= (1 << SR2_BIT_V1);       //SR_turn_on_V1();            // Valve Channel 1
     SR2_last_latch |= (1 << SR2_BIT_V2);       //SR_turn_on_V2();            // Valve Channel 2
     SR2_last_latch |= (1 << SR2_BIT_V7);       //SR_turn_on_V7();            // Valve Channel 2
     SR_shift_out();
}

void SR_BP_P6_Measure(void)
{
    PumpPWM_stop(6);
    //SR1_last_latch &= ~(1 << SR1_BIT_P6);       // SR_turn_off_P6();
    SR2_last_latch |= (1 << SR2_BIT_V3);        // SR_turn_on_V3();
    SR2_last_latch &= ~(1 << SR2_BIT_V4);       // SR_turn_off_V4();
    SR2_last_latch |= (1 << SR2_BIT_V8);        // SR_turn_on_V2();            // Valve Channel 2
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_BP_P6_Inflate(void)
{
    
    PumpPWM_start_default(6);
     //SR1_last_latch |= (1 << SR1_BIT_P6);       //SR_turn_on_P6();            // Pump Channel 1
     SR2_last_latch |= (1 << SR2_BIT_V3);       //SR_turn_on_V3();            // Valve Channel 1
     SR2_last_latch |= (1 << SR2_BIT_V4);       //SR_turn_on_V4();            // Valve Channel 2
     SR2_last_latch |= (1 << SR2_BIT_V8);       //SR_turn_on_V8();
     SR_shift_out();
}


/////////////////////////////////////////// 
void SR_COMP_LEFT_P1_Measure(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P1);       //SR_turn_off_P1();       // Pump Channel 1
    SR2_last_latch |= (1 << SR2_BIT_V1);       //SR_turn_on_V1();        // Valve Channel 1
    SR2_last_latch |= (1 << SR2_BIT_V2);       //SR_turn_on_V2();        // Valve Channel 2
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_COMP_LEFT_P1_Inflate(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P1);       //SR_turn_on_P1();        // Pump CH1
    SR2_last_latch |= (1 << SR2_BIT_V1);       //SR_turn_on_V1();        // Valve Channel 1
    SR2_last_latch |= (1 << SR2_BIT_V2);       //SR_turn_on_V2();        // Valve Channel 2
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_COMP_RIGHT_P2_Measure(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V3);       //SR_turn_on_V3();        // Valve Channel 3
    SR2_last_latch |= (1 << SR2_BIT_V4);       //SR_turn_on_V4();        // Valve Channel 4
    SR1_last_latch &= ~(1 << SR1_BIT_P2);      //SR_turn_off_P2();       // Pump Channel 2
    SR_shift_out();
}

/////////////////////////////////////////// 
void SR_COMP_RIGHT_P2_Inflate(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P2);       //SR_turn_on_P2();        // Pump CH2
    SR2_last_latch |= (1 << SR2_BIT_V3);       //SR_turn_on_V3();        // Valve Channel 3
    SR2_last_latch |= (1 << SR2_BIT_V4);       //SR_turn_on_V4();        // Valve Channel 4
    SR_shift_out();
}

///////////////////////////////////////////
void SR_RHI_P1_Measure(void)
{
    //SR2_last_latch |= (1 << SR2_BIT_V1);       //SR_turn_on_V1();        // Valve Channel 1
    //SR2_last_latch |= (1 << SR2_BIT_V2);       //SR_turn_on_V2();        // Valve Channel 2
    SR1_last_latch &= ~(1 << SR1_BIT_P1);       //SR_turn_off_P1();       // Pump Channel 1
    SR_shift_out();
}

///////////////////////////////////////////
void SR_RHI_P1_Inflate(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P1);       //SR_turn_on_P1();        // Pump CH1
    SR2_last_latch |= (1 << SR2_BIT_V1);       //SR_turn_on_V1();        // Valve Channel 1
    SR2_last_latch |= (1 << SR2_BIT_V2);       //SR_turn_on_V2();        // Valve Channel 2
    SR_shift_out();
}




/////////////////////////////////////////// Turn on/off Pump individually
void SR_turn_on_P1(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P1);
    SR_shift_out();
}

void SR_turn_off_P1(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P1);
    SR_shift_out();
}

void SR_turn_on_P2(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P2);
    SR_shift_out();
}

void SR_turn_off_P2(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P2);
    SR_shift_out();
}

void SR_turn_on_P3(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P3);
    SR_shift_out();
}

void SR_turn_off_P3(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P3);
    SR_shift_out();
}

void SR_turn_on_P4(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P4);
    SR_shift_out();
}

void SR_turn_off_P4(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P4);
    SR_shift_out();
}

void SR_turn_on_P5(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P5);
    SR_shift_out();
}

void SR_turn_off_P5(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P5);
    SR_shift_out();
}

void SR_turn_on_P6(void)
{
    SR1_last_latch |= (1 << SR1_BIT_P6);
    SR_shift_out();
}

void SR_turn_off_P6(void)
{
    SR1_last_latch &= ~(1 << SR1_BIT_P6);
    SR_shift_out();
}

/////////////////////////////////////////// Turn on/off Valve individually
void SR_turn_on_V1(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V1);
    SR_shift_out();
}

void SR_turn_off_V1(void)
{
    SR2_last_latch &= ~(1 << SR2_BIT_V1);
    SR_shift_out();
}

void SR_turn_on_V2(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V2);
    SR_shift_out();
}

void SR_turn_off_V2(void)
{
    SR2_last_latch &= ~(1 << SR2_BIT_V2);
    SR_shift_out();
}

void SR_turn_on_V3(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V3);
    SR_shift_out();
}

void SR_turn_off_V3(void)
{
    SR2_last_latch &= ~(1 << SR2_BIT_V3);
    SR_shift_out();
}

void SR_turn_on_V4(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V4);
    SR_shift_out();
}

void SR_turn_off_V4(void)
{
    SR2_last_latch &= ~(1 << SR2_BIT_V4);
    SR_shift_out();
}

void SR_turn_on_V5(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V5);
    SR_shift_out();
}

void SR_turn_off_V5(void)
{
    SR2_last_latch &= ~(1 << SR2_BIT_V5);
    SR_shift_out();
}

void SR_turn_on_V6(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V6);
    SR_shift_out();
}

void SR_turn_off_V6(void)
{
    SR2_last_latch &= ~(1 << SR2_BIT_V6);
    SR_shift_out();
}

void SR_turn_on_V7(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V7);
    SR_shift_out();
}

void SR_turn_off_V7(void)
{
    SR2_last_latch &= ~(1 << SR2_BIT_V7);
    SR_shift_out();
}

void SR_turn_on_V8(void)
{
    SR2_last_latch |= (1 << SR2_BIT_V8);
    SR_shift_out();
}

void SR_turn_off_V8(void)
{
    SR2_last_latch &= ~(1 << SR2_BIT_V8);
    SR_shift_out();
}


void PumpPWM_start(uint8_t pump_id, uint32_t freq_hz, uint8_t duty_percent) {
  int idx; uint8_t bit;
  if (!resolve_pump(pump_id, idx, bit)) return;

  uint32_t period_us, high_us, low_us;
  compute_timings(freq_hz, duty_percent, period_us, high_us, low_us);

  // Jika sudah berjalan, cukup update parameter
  if (pwm_ch[idx].running) {
    pwm_ch[idx].period_us = period_us;
    pwm_ch[idx].high_us   = high_us;
    pwm_ch[idx].low_us    = low_us;
    return;
  }

  pwm_ch[idx].pump_id   = pump_id;
  pwm_ch[idx].bit_index = bit;
  pwm_ch[idx].period_us = period_us;
  pwm_ch[idx].high_us   = high_us;
  pwm_ch[idx].low_us    = low_us;
  pwm_ch[idx].running   = true;

  xTaskCreatePinnedToCore(
      pump_pwm_task, "pump_pwm", 2048, (void*)(intptr_t)idx,
      configMAX_PRIORITIES - 1, &pwm_ch[idx].task, 0);
}

void PumpPWM_update(uint8_t pump_id, uint32_t freq_hz, uint8_t duty_percent) {
  int idx; uint8_t bit;
  if (!resolve_pump(pump_id, idx, bit)) return;

  uint32_t period_us, high_us, low_us;
  compute_timings(freq_hz, duty_percent, period_us, high_us, low_us);

  if (!pwm_ch[idx].running) {
    PumpPWM_start(pump_id, freq_hz, duty_percent);
    return;
  }

  pwm_ch[idx].period_us = period_us;
  pwm_ch[idx].high_us   = high_us;
  pwm_ch[idx].low_us    = low_us;
}

void PumpPWM_stop(uint8_t pump_id) {
  int idx; uint8_t bit;
  if (!resolve_pump(pump_id, idx, bit)) return;
  if (!pwm_ch[idx].running) return;

  pwm_ch[idx].running = false;
  // Beri waktu task keluar
  //vTaskDelay(1);
  pump_bit_write_atomic(bit, false);
  pwm_ch[idx].task = nullptr;
}

void PumpPWM_stop_all(void) {
  for (int i = 0; i < 6; ++i) {
    if (pwm_ch[i].running) {
      PumpPWM_stop(i + 1);
    }
  }
}

void PumpPWM_start_default(uint8_t pump_id) {
  PumpPWM_start(pump_id, 500, 40); // 45% @ 500 Hz
}


/////////////////////////////////////////// Turn on/off LED Bar individually
/*
void SR_turn_on_L1(void)
{
    SR3_last_latch |= (1 << SR3_BIT_L1);
    SR_shift_out();
}

void SR_turn_off_L1(void)
{
    SR3_last_latch &= ~(1 << SR3_BIT_L1);
    SR_shift_out();
}

void SR_turn_on_L2(void)
{
    SR3_last_latch |= (1 << SR3_BIT_L2);
    SR_shift_out();
}

void SR_turn_off_L2(void)
{
    SR3_last_latch &= ~(1 << SR3_BIT_L2);
    SR_shift_out();
}

void SR_turn_on_L3(void)
{
    SR3_last_latch |= (1 << SR3_BIT_L3);
    SR_shift_out();
}

void SR_turn_off_L3(void)
{
    SR3_last_latch &= ~(1 << SR3_BIT_L3);
    SR_shift_out();
}

void SR_turn_on_L4(void)
{
    SR3_last_latch |= (1 << SR3_BIT_L4);
    SR_shift_out();
}

void SR_turn_off_L4(void)
{
    SR3_last_latch &= ~(1 << SR3_BIT_L4);
    SR_shift_out();
}

void SR_turn_on_L5(void)
{
    SR3_last_latch |=  (1 << SR3_BIT_L5);
    SR_shift_out();
}

void SR_turn_off_L5(void)
{
    SR3_last_latch &= ~(1 << SR3_BIT_L5);
    SR_shift_out();
}

void SR_turn_on_L6(void)
{
    SR3_last_latch |= (1 << SR3_BIT_L6);
    SR_shift_out();
}

void SR_turn_off_L6(void)
{
    SR3_last_latch &= ~(1 << SR3_BIT_L6);
    SR_shift_out();
}

void SR_turn_on_L7(void)
{
    SR3_last_latch |= (1 << SR3_BIT_L7);
    SR_shift_out();
}

void SR_turn_off_L7(void)
{
    SR3_last_latch &= ~(1 << SR3_BIT_L7);
    SR_shift_out();
}

void SR_turn_on_L8(void)
{
    SR3_last_latch |= (1 << SR3_BIT_L8);
    SR_shift_out();
}

void SR_turn_off_L8(void)
{
    SR3_last_latch &= ~(1 << SR3_BIT_L8);
    SR_shift_out();
}

/////////////////////////////////////////// Turn on/off LED Bar individually
void SR_set_ledbar(int num)
{
    if     (num == 0) { SR3_last_latch = B00000000; }
    else if(num == 1) { SR3_last_latch = B00000001; }
    else if(num == 2) { SR3_last_latch = B00000011; }
    else if(num == 3) { SR3_last_latch = B00000111; }
    else if(num == 4) { SR3_last_latch = B00001111; }
    else if(num == 5) { SR3_last_latch = B00011111; }
    else if(num == 6) { SR3_last_latch = B00111111; }
    else if(num == 7) { SR3_last_latch = B01111111; }
    else if(num == 8) { SR3_last_latch = B11111111; }

    SR_shift_out();
}
*/
/* =================================================================================================================== */
/* =================================================================================================================== */
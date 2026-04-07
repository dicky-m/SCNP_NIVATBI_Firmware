#include "ppg_sensor.h"


const int SPO2_red_min_pin = SPO2_IR_MIN_PIN;
const int SPO2_ir_min_pin = SPO2_RED_MIN_PIN;

const int SPO2_red_min_pin2 = SPO2_IR_MIN_PIN2;
const int SPO2_ir_min_pin2 = SPO2_RED_MIN_PIN2;


/////////////////////////////////////////// Setup SPO2 sensor pins
void SPO2_setup_pins(void)
{
    pinMode(SPO2_red_min_pin, OUTPUT);
    pinMode(SPO2_ir_min_pin, OUTPUT);
    pinMode(SPO2_red_min_pin2, OUTPUT);
    pinMode(SPO2_ir_min_pin2, OUTPUT);
    set_ch2_non_spo2_measurement();
}

/////////////////////////////////////////// Shift reg shift out procedure
void set_ch2_non_spo2_measurement(void)
{
    digitalWrite(SPO2_red_min_pin, HIGH);
    digitalWrite(SPO2_ir_min_pin, HIGH);
    digitalWrite(SPO2_red_min_pin2, HIGH);
    digitalWrite(SPO2_ir_min_pin2, HIGH);
}

void set_ppg_on(void)
{
    digitalWrite(SPO2_red_min_pin, LOW);
    digitalWrite(SPO2_ir_min_pin, LOW);
    digitalWrite(SPO2_red_min_pin2, LOW);
    digitalWrite(SPO2_ir_min_pin2, LOW);
}

/////////////////////////////////////////// toggle channels
void toggle_spo2_source_ch(int8_t ix)
{
    if(ix == 0) 
    {
        digitalWrite(SPO2_red_min_pin, HIGH);   
        digitalWrite(SPO2_ir_min_pin, LOW);   
        digitalWrite(SPO2_red_min_pin2, HIGH);   
        digitalWrite(SPO2_ir_min_pin2, LOW);  
    }
    else
    {
        digitalWrite(SPO2_red_min_pin, LOW);   
        digitalWrite(SPO2_ir_min_pin, HIGH);   
        digitalWrite(SPO2_red_min_pin2, LOW);   
        digitalWrite(SPO2_ir_min_pin2, HIGH); 
    }
}
/* =================================================================================================================== */
/* =================================================================================================================== */
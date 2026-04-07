#include "batt_monitor.h"


/////////////////////////////////////////// Get battery level and calculate leds to turn on
/*
Battery NS nominal voltage = 13.5V
NIVA min voltage = 11.5V
LED Bar type : 5 bar

Means, we need to maintain between 11.5 - 13.5 V, which is mapped into 2.43 - 2.92 V

< 11.5V     : < 2.43      = 0 bar
11.5V - 12V : 2.43 - 2.55 = 1 bar
12V - 12.5V : 2.55 - 2.67 = 2 bar
12.5V - 13V : 2.67 - 2.79 = 3 bar
13V - 13.5  : 2.79 - 2.92 = 4 bar
> 13.5      : > 2.92      = 5 bar

*/

int calculate_led_bar_to_turn_on(void)
{
    float batt_voltage = (analogRead(BATT_ANALOG_PIN) * MAX_INPUT_VOLTAGE ) / (4095);
    
    int v = 0;

    if      (batt_voltage >= 2.92)                          v = 5; 
    else if (batt_voltage < 2.93 && batt_voltage >= 2.79 )  v = 4; 
    else if (batt_voltage < 2.79 && batt_voltage >= 2.67 )  v = 3; 
    else if (batt_voltage < 2.67 && batt_voltage >= 2.55 )  v = 2; 
    else if (batt_voltage < 2.55 && batt_voltage >= 2.43 )  v = 1; 
    else if (batt_voltage < 2.43)                           v = 1; //0

    return v;
}

/* =================================================================================================================== */
/* =================================================================================================================== */


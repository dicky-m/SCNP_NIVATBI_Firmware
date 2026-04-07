#include <Arduino.h>

#include "main.h"
#include "ads1256.h"
#include "shiftreg_drive.h"
#include "ppg_sensor.h"
#include "batt_monitor.h"
#include "HardwareSerial.h"
#include <Preferences.h>
Preferences pref;

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include <string.h> // untuk memmove
#include <math.h>   // untuk fabsf jika perlu

const char* host = "esp32";
// const char* ssid = "SCNP PRIVATE";
// const char* password = "5cnpPrivate!";

WebServer server(80);

/*
 * Login page
 */

const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>NIVA Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
             "<td>Username:</td>"
             "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='@niva+scnp123')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

/*
 * Server Index Page
 */

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";


void updateFirmware()
{
    if(connected_ota == false){
        WiFi.begin(ssid, password);

        // Wait for connection
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        /*use mdns for host name resolution*/
        if (!MDNS.begin(host)) { //http://esp32.local
            Serial.println("Error setting up MDNS responder!");
            while (1) {
            delay(1000);
            }
        }
        Serial.println("mDNS responder started");
        /*return index page which is stored in serverIndex */
        server.on("/", HTTP_GET, []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/html", loginIndex);
        });
        server.on("/serverIndex", HTTP_GET, []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/html", serverIndex);
        });
        /*handling uploading firmware file */
        server.on("/update", HTTP_POST, []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            ESP.restart();
        }, []() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("Update: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
                Update.printError(Serial);
            }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
            /* flashing firmware to ESP*/
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
            } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
                Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
            }
        });
        server.begin();
        connected_ota = true;
    }
  server.handleClient();
  delay(1);
}

// #define WDT_TIMEOUT 5

/* =================================================================================================================== */
// Timer interrupt ISR for tim_250us
void IRAM_ATTR onTim_250us() 
{
    portENTER_CRITICAL_ISR(&tim_mux_250us);
    xSemaphoreGiveFromISR(sem, NULL);
    portEXIT_CRITICAL_ISR(&tim_mux_250us);
}

/* =================================================================================================================== */
String convertToString(char* a, int size)
{
    int i;
    String s = "";
    for (i = 0; i < size; i++) {
        s = s + a[i];
    }
    return s;
}

void action_prod_enter_sn(void)
{
    // WITH PREFERENCES LIBRARY
    unsigned long start_count = millis();
    Serial.print("[enter_sn]");

    while (millis() < (start_count + 10000))
    {
        if (Serial.available() > 0) 
        {
            StrIn = Serial.readString();
            Serial.print("RX:["+ StrIn + "]");
            //parsing serial data
            niva_sn = StrIn.substring(0,12);
            niva_fw = convertToString(fw_version,sizeof(fw_version));
            Serial.print("niva_sn:[" + niva_sn + "]");
            Serial.print("niva_fw:[" + niva_fw + "]");
            pref.putString("niva_sn", niva_sn);
            pref.putString("niva_fw", niva_fw);
            break;
        }
    }

    niva_sn = pref.getString("niva_sn", "000000000000");
    niva_fw = pref.getString("niva_fw", "0000");
    if(niva_sn[6] != 'N' && niva_sn[7] != 'V') Serial.print("[Timeout]");

}

void action_reset_sn(void)
{
    // WITH PREFERENCES LIBRARY
    unsigned long start_count = millis();
    Serial.print("[???]");
    while (millis() < (start_count + 1000))
    {
        if (Serial.available() > 0) 
        {
            StrIn = Serial.readString();
            Serial.print("RX:["+ StrIn + "]");
            if(StrIn == "nivascnp"){
                pref.putString("niva_sn", "000000000000");
                pref.putString("niva_fw", "0000");
                Serial.println("reset");
                ESP.restart(); 
            }
            else{
                int ssidStart = StrIn.indexOf("ssid[") + 5;  // +5 to skip "ssid["
                int ssidEnd = StrIn.indexOf("]", ssidStart);
                int passwordStart = StrIn.indexOf("password[") + 9; // +9 to skip "password["
                int passwordEnd = StrIn.indexOf("]", passwordStart);

                if (ssidStart >= 5 && ssidEnd != -1 && passwordStart >= 9 && passwordEnd != -1) {
                    String ssidSTR = StrIn.substring(ssidStart, ssidEnd);
                    String passwordSTR = StrIn.substring(passwordStart, passwordEnd);

                    // Print extracted ssid and password
                    Serial.println("SSID: " + ssidSTR);
                    Serial.println("Password: " + passwordSTR);
                    // Convert String to const char*
                    ssid = ssidSTR.c_str();
                    password = passwordSTR.c_str();
                    while (1){
                        updateFirmware();
                    }
                }
            }
            break;
        }
    }
}

/* =================================================================================================================== */
/* =================================================================================================================== */
// UART RX ISR
// NOTE Adi 21.08.2021: >> Arduino framework doesn't have built in functions to use this feature yet.
//                      >> We have to manually modify "HardwareSerial.h/.cpp & esp32-hal-uart.h/.c"
//                      >> learn how to do it with ESP-IDF (future improvement)
void IRAM_ATTR uart_RX_ISR() 
{
    if(Serial.available())
    {
        char incomingData = Serial.read(); 
        if (incomingData == COMMAND_FUNCTION_TEST) {
            action = ACTION_FUNCTION_TEST;
            BP_threshold = BP_THRESHOLD_1;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            seqPress_state.CH5_state = STATE_INFLATE;
            seqPress_state.CH6_state = STATE_INFLATE;
            startStop(true);
        }
        else if (incomingData == 'w'){
            Serial.print("niva_sn:[" + niva_sn + "]");
            Serial.print("niva_fw:[" + niva_fw + "]");
        }
        else if (incomingData == COMMAND_STOP){
            stop_signal_ppg = true;
            action = ACTION_PPG_SIGNAL_CHECK;
        }
        else if (incomingData == COMMAND_ALLSTOP){
            action = ACTION_DEFAULT;
            startStop(false);
        }
        else if (incomingData == COMMAND_RESTART){
            ESP.restart(); 
        }     
        else if (incomingData == COMMAND_OTA_UPDATE){
            action = ACTION_OTA_UPDATE;   
        }
        else if (incomingData == COMMAND_VOLTAGE_TEST) {
            action = ACTION_VOLTAGE_TEST;
            startStop(true);
        }

        // Run PPG Signal check
        else if (incomingData == COMMAND_RUN_PPG_CHECK || incomingData == COMMAND_RUN_PPG_CHECK_ENCRYPT) { 
            stop_signal_ppg = false;
            if (incomingData == COMMAND_RUN_PPG_CHECK_ENCRYPT) encryption = true;
            else encryption = false;
            action = ACTION_PPG_SIGNAL_CHECK;
            startStop(true);
        }

        // TBI Development
        else if (incomingData == COMMAND_RUN_TBI_CHECK || incomingData == COMMAND_RUN_TBI_CHECK) { 
            stop_signal_ppg = false;
            if (incomingData == COMMAND_RUN_PPG_CHECK_ENCRYPT) encryption = true;
            else encryption = false;
            action = ACTION_TBI_SIGNAL_CHECK;
            startStop(true);
        }

        // Send fw version info
        //////////////////////////////////////////////////////////////////////////////////
        else if (incomingData == COMMAND_GET_FW_VERSION) { 
            action = ACTION_SEND_FW_VERSION;
        }
        
        // Send battery status to GUI
        //////////////////////////////////////////////////////////////////////////////////
        else if (incomingData == COMMAND_GET_BATT_LEVEL) { 
            action = ACTION_SEND_BATT_LEVEL;
        }

        // Run Measurement: SPO2
        //////////////////////////////////////////////////////////////////////////////////
        else if (incomingData == COMMAND_RUN_SPO2_MEAS) { 
            action = ACTION_SPO2;
            startStop(true);
        }

        // Run Measurement: BP THRESHOLD 1
        else if (incomingData == COMMAND_RUN_BP_MEAS_T1) { 
            action = ACTION_BP;
            BP_threshold = BP_THRESHOLD_1;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            startStop(true);
        }

        // Run Measurement: BP THRESHOLD 2
        else if (incomingData == COMMAND_RUN_BP_MEAS_T2) { 
            action = ACTION_BP;
            BP_threshold = BP_THRESHOLD_2;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            startStop(true);
        }

        // Run Measurement: BP THRESHOLD 3
        else if (incomingData == COMMAND_RUN_BP_MEAS_T3) { 
            action = ACTION_BP;
            BP_threshold = BP_THRESHOLD_3;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            startStop(true);
        }

        // Run Measurement: BP THRESHOLD 1
        else if (incomingData == COMMAND_RUN_BP_MEAS_T1_XL) { 
            action = ACTION_BP_XL;
            BP_threshold = BP_THRESHOLD_1;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            startStop(true);
        }

        // Run Measurement: BP THRESHOLD 2
        else if (incomingData == COMMAND_RUN_BP_MEAS_T2_XL) { 
            action = ACTION_BP_XL;
            BP_threshold = BP_THRESHOLD_2;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            startStop(true);
        }

        // Run Measurement: BP THRESHOLD 3
        else if (incomingData == COMMAND_RUN_BP_MEAS_T3_XL) { 
            action = ACTION_BP_XL;
            BP_threshold = BP_THRESHOLD_3;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            startStop(true);
        }

            // Run Measurement: BP THRESHOLD 2
        else if (incomingData == COMMAND_RUN_BP_MEAS_T2_LITE) { 
            action = ACTION_BP_LITE;
            BP_threshold = BP_THRESHOLD_2;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            startStop(true);
        }

        // Run Measurement: BP THRESHOLD 3
        else if (incomingData == COMMAND_RUN_BP_MEAS_T3_LITE) { 
            action = ACTION_BP_LITE;
            BP_threshold = BP_THRESHOLD_3;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            seqPress_state.CH3_state = STATE_INFLATE;
            seqPress_state.CH4_state = STATE_INFLATE;
            startStop(true);
        }

        // Run Measurement: PPG 
        //////////////////////////////////////////////////////////////////////////////////

        else if (incomingData == COMMAND_RUN_PPG_MEAS_SHORT) { 
            action = ACTION_PPG_SHORT;
            startStop(true);
        }

        else if (incomingData == COMMAND_RUN_COMP_MEAS_SHORT) { 
            set_ch2_non_spo2_measurement();
            action = ACTION_COMP_SHORT;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            startStop(true);

        }

        else if (incomingData == COMMAND_RUN_COMP_SHORT_XL) { 
            set_ch2_non_spo2_measurement();
            action = ACTION_COMP_XL;
            seqPress_state.CH1_state = STATE_INFLATE;
            seqPress_state.CH2_state = STATE_INFLATE;
            startStop(true);
        }

        // Run Measurement: COMPLIANCE
        //////////////////////////////////////////////////////////////////////////////////
        else if (incomingData == COMMAND_RUN_COMP_MEAS_LEFT) { 
            action = ACTION_COMP_LEFT;
            seqPress_state.CH1_state = STATE_INFLATE;
            startStop(true);
        }

        //coba Compliance Kanan
        //////////////////////////////////////////////////////////////
        else if (incomingData == COMMAND_RUN_COMP_MEAS_RIGHT) { 
            action = ACTION_COMP_RIGHT;
            seqPress_state.CH2_state = STATE_INFLATE;
            startStop(true);
        }

        
        //////////////////////////////////////////////////////////////////////////////////////////
        // DEBUG COMMANDS
        //////////////////////////////////////////////////////////////////////////////////////////
        // Pump toggle command
        else if (incomingData == COMMAND_TOGGLE_P1) { action = ACTION_TOGGLE_PUMP; debug_id = 1;} 
        else if (incomingData == COMMAND_TOGGLE_P2) { action = ACTION_TOGGLE_PUMP; debug_id = 2;} 
        else if (incomingData == COMMAND_TOGGLE_P3) { action = ACTION_TOGGLE_PUMP; debug_id = 3;} 
        else if (incomingData == COMMAND_TOGGLE_P4) { action = ACTION_TOGGLE_PUMP; debug_id = 4;} 
        else if (incomingData == COMMAND_TOGGLE_P5) { action = ACTION_TOGGLE_PUMP; debug_id = 5;} 
        else if (incomingData == COMMAND_TOGGLE_P6) { action = ACTION_TOGGLE_PUMP; debug_id = 6;} 

        // Valve toggle command
        else if (incomingData == COMMAND_TOGGLE_V1) { action = ACTION_TOGGLE_VALVE; debug_id = 1;} 
        else if (incomingData == COMMAND_TOGGLE_V2) { action = ACTION_TOGGLE_VALVE; debug_id = 2;} 
        else if (incomingData == COMMAND_TOGGLE_V3) { action = ACTION_TOGGLE_VALVE; debug_id = 3;} 
        else if (incomingData == COMMAND_TOGGLE_V4) { action = ACTION_TOGGLE_VALVE; debug_id = 4;} 
        else if (incomingData == COMMAND_TOGGLE_V5) { action = ACTION_TOGGLE_VALVE; debug_id = 5;} 
        else if (incomingData == COMMAND_TOGGLE_V6) { action = ACTION_TOGGLE_VALVE; debug_id = 6;} 
        else if (incomingData == COMMAND_TOGGLE_V7) { action = ACTION_TOGGLE_VALVE; debug_id = 7;} 
        else if (incomingData == COMMAND_TOGGLE_V8) { action = ACTION_TOGGLE_VALVE; debug_id = 8;} 

        // SR_print_last_latch 
        else if (incomingData == COMMAND_SR_PRINT_LAST_LATCH) { 
            action = ACTION_PRINT_LAST_LATCH;
        }   

        // SR_turn off all 
        else if (incomingData == COMMAND_SR_TURN_OFF_ALL) { 
            action = ACTION_SR_TURN_OFF_ALL;
        } 

        // TBI Development
        else if (incomingData == COMMAND_TBI){
            action = ACTION_TBI;
            seqPress_state.CH5_state = STATE_INFLATE;  // Pompa3
            seqPress_state.CH6_state = STATE_INFLATE;  // Pompa4
            startStop(true);
        }
        //xSemaphoreGive(sem); //aktifkan paksa semaphore RTOS // CSR Dicky 21-02-25 
    }
}

/* =================================================================================================================== */
/* =================================================================================================================== */
void setup() 
{

    ///////////////////////////// Serial setup
    Serial.begin(250000);
    Serial.flush();
    vTaskDelay(100 / portTICK_PERIOD_MS); //delay(100);
    Serial.setInterrupt(&uart_RX_ISR);
    vTaskDelay(100 / portTICK_PERIOD_MS); //delay(100);
    
    SR_setup_pins();

    reset_pump_valve_state_vars();
    SR_turn_on_P1();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    SR_turn_off_all();

    ////////////////////////////////////////////////////////////////////////////////////
    // With PREFERENCES LIBRARY
    pref.begin("niva_data", false);
    niva_fw = convertToString(fw_version,sizeof(fw_version));
    pref.putString("niva_fw", niva_fw);

    niva_sn = pref.getString("niva_sn", "000000000000");
    niva_fw = pref.getString("niva_fw", "0000");
    Serial.print("niva_sn:[" + niva_sn + "]");
    Serial.print("niva_fw:[" + niva_fw + "]");

    if(niva_sn[6] != 'N' && niva_sn[7] != 'V') // Format : 270916NV1234NIVA
    {
        action_prod_enter_sn();
    }

    if(niva_sn[6] == 'N' && niva_sn[7] == 'V') // Format : 270916NV1234NIVA
    {
    action_reset_sn();
    ///////////////////////////// Pins setup
    ADS_setup_pins();
    SPO2_setup_pins();
    pinMode(LED_BUILTIN, OUTPUT);
    /*
    ///////////////////////////// timer Interrupt_250us setup
    MCU clk = 80Mhz = 12.5ns period
    timerAttachInterrupt(tim_250us, &onTim_250us, true);
    timerAlarmWrite(tim_250us, 250, true); // 250 * 1 us = 250 us (timer clk)    
    */
    tim_250us = NULL;

    sem = xSemaphoreCreateBinary();
    // Diubah menjadi counting semaphore dengan kapasitas maksimum 10 (Coba handle bug ESP32 tidak respons command action)
    // sem = xSemaphoreCreateCounting(10, 0);
    ///////////////////////////// SPI and ADC setup
    SPI.begin();
    initADS();      //initADS harus dilakukan setelah akses ke flash agar tidak membuat error

    ///////////////////////////// initialize seqPress_states to default
    memset(&seqPress_state, 0, sizeof(seqPress_state)); 
    toggle_spo2_source_ch(1);
    //////////////////////////// Membaca nilai initial masing2 sensor untuk fitur auto zero
    startStop(true);                      //Start timer
    initial = read_ADC(72);
    vTaskDelay(100 / portTICK_PERIOD_MS); //delay(100);
    initial1 = read_ADC(88);              //Menyimpan ADC initial sensor Left Brachial (0 mmhg)
    vTaskDelay(100 / portTICK_PERIOD_MS); //delay(100);
    initial2 = read_ADC(104);             //Menyimpan ADC initial sensor Right Brachial (0 mmhg)
    vTaskDelay(100 / portTICK_PERIOD_MS); //delay(100);
    initial3 = read_ADC(120);             ////Menyimpan ADC initial sensor Left Ancle (0 mmhg)
    vTaskDelay(100 / portTICK_PERIOD_MS); //delay(100);
    initial4 = read_ADC(72);              //Menyimpan ADC initial sensor Right Ancle (0 mmhg)
    startStop(false);    
    Serial.println("[Welcome NIVA]");   
        //P5_PWM_start_45();  
    //SR_BP_P5_Inflate();
    //SR_BP_P6_Inflate();

        
    }   
    else
    {
        Serial.println("Fail to verify serial number. Please reset.");
        while(1);
    }       
}


// TBI Development
void seqTBI(int thres) 
{
    //ts = millis()%40;
    // Inflasi jika tekanan masih di bawah dan tbi belum tercapai
    if (adc_read3 < thres && tbi1 == 0 && seqPress_state.CH5_state == STATE_INFLATE) {
            SR_BP_P5_Inflate();
            seqPress_state.CH5_state = STATE_MEASURE;
    }
    // Cek kondisi: tekanan sudah melebihi batas
    else if (adc_read3 > thres && seqPress_state.CH5_state == STATE_MEASURE) {
            tbi1 = 1;
            SR_BP_P5_Measure();
            pressure1 = TBI_THRESHOLD;
            seqPress_state.CH5_state = STATE_DEFAULT;
    }
    else if (tbi1 == 1 && seqPress_state.CH5_state == STATE_MEASURE) {
            SR_BP_P5_Measure();
            seqPress_state.CH5_state = STATE_DEFAULT;
    }
    // else if (seqPress_state.CH5_state == STATE_MEASURE) {
    //     SR_BP_P5_Measure(); 
    //     seqPress_state.CH5_state = STATE_DEFAULT;
    // }



    // cuff 2

    if (adc_read4 < thres && tbi2 == 0 && seqPress_state.CH6_state == STATE_INFLATE) {
        SR_BP_P6_Inflate();
        seqPress_state.CH6_state = STATE_MEASURE;
    }
    else if (adc_read4 > thres && seqPress_state.CH6_state == STATE_MEASURE) {
        tbi2 = 1;
        SR_BP_P6_Measure(); 
        pressure2 = TBI_THRESHOLD;
        seqPress_state.CH6_state = STATE_DEFAULT;
    } 
    else if(tbi2 == 1 && seqPress_state.CH6_state == STATE_MEASURE) {
        SR_BP_P6_Measure(); 
        seqPress_state.CH6_state = STATE_DEFAULT;
    }
    // else if (seqPress_state.CH6_state == STATE_MEASURE) {
    //     SR_BP_P6_Measure(); 
    //     seqPress_state.CH6_state = STATE_DEFAULT;
    // }
}
// void seqTBI(int thres) 
// {
//     ts = millis()%40;
//     // Inflasi jika tekanan masih di bawah dan tbi belum tercapai
//     if (adc_read3 < thres && tbi1 == 0 && seqPress_state.CH5_state == STATE_INFLATE) {
//             if (ts >= 22) SR_BP_P5_Inflate();
//             else SR_turn_off_P5();
//             // seqPress_state.CH5_state = STATE_MEASURE;
//     }
//     // Cek kondisi: tekanan sudah melebihi batas
//     else if (adc_read3 > thres && seqPress_state.CH5_state == STATE_INFLATE) {
//             tbi1 = 1;
//             SR_BP_P5_Measure();
//             pressure1 = TBI_THRESHOLD;
//             seqPress_state.CH5_state = STATE_DEFAULT;
//     }
//     else if (tbi1 == 1 && seqPress_state.CH5_state == STATE_INFLATE) {
//             SR_BP_P5_Measure();
//             seqPress_state.CH5_state = STATE_DEFAULT;
//     }
//     // else if (seqPress_state.CH5_state == STATE_MEASURE) {
//     //     SR_BP_P5_Measure(); 
//     //     seqPress_state.CH5_state = STATE_DEFAULT;
//     // }



//     // cuff 2

//     if (adc_read4 < thres && tbi2 == 0 && seqPress_state.CH6_state == STATE_INFLATE) {
//         if (ts >= 22) SR_BP_P6_Inflate();
//         else SR_turn_off_P6();
//     }
//     else if (adc_read4 > thres && seqPress_state.CH6_state == STATE_INFLATE) {
//         tbi2 = 1;
//         SR_BP_P6_Measure(); 
//         pressure2 = TBI_THRESHOLD;
//         seqPress_state.CH6_state = STATE_DEFAULT;
//     } 
//     else if(tbi2 == 1 && seqPress_state.CH6_state == STATE_INFLATE) {
//         SR_BP_P6_Measure(); 
//         seqPress_state.CH6_state = STATE_DEFAULT;
//     }
//     // else if (seqPress_state.CH6_state == STATE_MEASURE) {
//     //     SR_BP_P6_Measure(); 
//     //     seqPress_state.CH6_state = STATE_DEFAULT;
//     // }
// }

/* =================================================================================================================== */
/* =================================================================================================================== */

// TBI Development
void processTBI(uint8_t temp_ch[]) {   
    ch_index = (ix + 1) % 4; //Jadi tidak perlu ix+1
    ch_addr = temp_ch[ch_index];

    switch (ch_addr) {
        case 120 :  //channel 3
        samples[ix] = read_ADC(120)-initial3;
        adc_read3 = samples[ix]; 
        break;
        case 40 :   //channel 4
        samples[ix] = read_ADC(40)-initial4;
        adc_read4 = samples[ix]; 
        break;

        case 56 :
        for (int i = IIR_ORDER; i > 0; i--) {
            iir_x[i] = iir_x[i - 1];
            iir_y[i] = iir_y[i - 1];
        }
        iir_x[0] = (float)read_ADC(56);
        samples[ix] = iir_x[0];
        //pernah coba jadi 'samples[ix] = (float)read_ADC(8); iir_x[0] = samples[ix];' tapi alatnya error

        y = b[0]*iir_x[0];
        for (int i = 1; i <= IIR_ORDER; i++) {
            y += b[i]*iir_x[i];
            y -= a[i]*iir_y[i];
        }
        y /= a[0];
        iir_y[0] = y;       
        //samples[ix] = (int32_t)y; //kalau mau liat hasil filter di serialplot

        // Validasi terhadap threshold
        if (fabs(y) < float(PULSE_THRESHOLD)) {
            sample_counter++;

            if (sample_counter >= TIMEOUT_SAMPLE_COUNT && tbi1 == 0) {
                pressure1 = adc_read3;
                tbi1 = 1;
            }
        } else {
            sample_counter = 0; // Reset jika melanggar threshold
        } 
        break;

        case 104 :
        for (int i = IIR_ORDER; i > 0; i--) {
            iir2_x[i] = iir2_x[i - 1];
            iir2_y[i] = iir2_y[i - 1];
        }
         
        samples[ix] = (float)read_ADC(104);
        iir2_x[0] = samples[ix];

        y2 = b[0]*iir2_x[0];
        for (int i = 1; i <= IIR_ORDER; i++) {
            y2 += b[i]*iir2_x[i];
            y2 -= a[i]*iir2_y[i];
        }
        y2 /= a[0];
        iir2_y[0] = y2;
        
        // samples[ix] = (int32_t)y2; //kalau mau liat hasil filter di serialplot

        // Validasi terhadap threshold
        if (fabs(y2) < float(PULSE_THRESHOLD)) {
            sample_counter2++;

            if (sample_counter2 >= TIMEOUT_SAMPLE_COUNT && tbi2 == 0) {
                pressure2 = adc_read4;
                tbi2 = 1;
            }
        } else {
            sample_counter2 = 0; // Reset jika melanggar threshold
        } 
        break;

        default:
        samples[ix] = read_ADC(ch_addr);
        break;
    }

    if (samples[ix]<=0){
        samples[ix]=500;        
    }

    if(release){
        samples[ix]=20133;
    }

    adc_read = samples[ix];
    ix++; 
    /*
    if (ix >= 32) {        
        ix = 0;
        count_hold++;
        if(count_hold >= 2){
            Serial.write((uint8_t *)samples, 128);
            count_hold = 2;
        }
    }
    */
   
        // Jika buffer sudah terisi 32 nilai (128 byte)
    if (ix >= 32) {
        ix = 0;
        count_hold++;
        if (count_hold >= 2) {
            // Buat framing dengan header dan footer
            uint8_t header[] = {0xAA, 0x55};  // misalnya: header = AA55
            uint8_t footer[] = {0x55, 0xAA};  // footer = 55AA
            const int dataLength = 32 * sizeof(int32_t);  // 32 nilai, masing-masing 4 byte (128 byte total)
            uint8_t packet[2 + dataLength + 2];  // total 132 byte
            
            // Salin header
            memcpy(packet, header, 2);
            // Salin data sensor (buffer samples)
            memcpy(packet + 2, samples, dataLength);
            // Salin footer
            memcpy(packet + 2 + dataLength, footer, 2);
            
            Serial.write(packet, sizeof(packet));  // Kirim paket melalui serial USB
            count_hold = 2;  // Reset count_hold agar pengiriman terjadi secara periodik
        }
    }
}

/* =================================================================================================================== */
/* =================================================================================================================== */

void loop() 
{
    //////////////////////////////////////////////// Default
    if(action == ACTION_DEFAULT)
    {   
        // do nothing, just wait for UART ISR
    }

    //////////////////////////////////////////////// Send FW version information to GUI
    if(action == ACTION_SEND_FW_VERSION)
    {   
        print_fw_version();
        action = ACTION_DEFAULT;
    }

    //////////////////////////////////////////////// Get Battery status and send info to GUI
    if(action == ACTION_SEND_BATT_LEVEL)
    {  
        LEDBAR_led_to_turn_on = calculate_led_bar_to_turn_on(); 
        Serial.print(LEDBAR_led_to_turn_on);
        action = ACTION_DEFAULT;
    } 
        // Start sequence
        //if (xSemaphoreTake(sem, pdMS_TO_TICKS(10)) == pdTRUE) untuk coba handle ESP32 tidak respon action 20-02-25 GAGAL
        if (xSemaphoreTake(sem, 0) == pdTRUE) 
        {   
            //////////////////////////////////////////////// Run PPG Signal Check only 
            if(action == ACTION_PPG_SIGNAL_CHECK) 
            {
                if (stop_signal_ppg) startStop(false);
                if (encryption) processNiva_encrypt(source_ch_PPG);
                else if(!encryption) processNiva(source_ch_PPG);
                toggle_spo2_source_ch(ix);
            }

            // TBI Development
            if(action == ACTION_TBI_SIGNAL_CHECK) 
            {   
                SR_turn_on_V1();
                SR_turn_on_V2();
                SR_turn_on_V3();
                SR_turn_on_V4();
                SR_turn_on_V7();
                SR_turn_on_V8();
                if (stop_signal_ppg) startStop(false);
                if (encryption) processNiva_encrypt(source_ch_TBI);
                else if(!encryption) processNiva(source_ch_TBI);
            }

            else if(action == ACTION_VOLTAGE_TEST) 
            {
                if (millis() - starts >= 1500) {
                    starts = millis();
                    if(state_out == 0){
                        state_out = 1;
                        SR_turn_on_P1();
                        SR_turn_on_P2();
                        SR_turn_on_P3();
                        SR_turn_on_P4();
                        SR_turn_on_P5();
                        SR_turn_on_P6();
                        SR_turn_on_V1();
                        SR_turn_on_V2();
                        SR_turn_on_V3();
                        SR_turn_on_V4();
                        SR_turn_on_V5();
                        SR_turn_on_V6();
                        SR_turn_on_V7();
                        SR_turn_on_V8();
                    }
                    else if (state_out == 1){
                        state_out = 0;
                        SR_turn_off_all();
                    }
                    
                }
                
            }
            else if(action == ACTION_OTA_UPDATE)
            {   
                updateFirmware();
            }

            else if(action == ACTION_FUNCTION_TEST) 
            {

                // Pembuangan Udara
                if (millis() - starts <= 500) {                
                    processNiva(source_ch_BP);
                    deflation();
                }

                // Sequence: BP Measurement
                else if (millis() - starts > 500 && millis() - starts <= 13000) {
                    processNiva(source_ch_BP);
                    SeqPress(BP_threshold);
                }

                // Pembuangan Udara
                else if (millis() - starts > 13000 && millis() - starts <= 15000) {
                    processNiva(source_ch_BP);
                    deflation();
                }

                else if (millis() - starts > 15000 && millis() - starts <= 20000) {
                    if(millis() - starts <= 19999){
                        processNiva(source_ch_PPG);
                        //toggle_spo2_source_ch(ix);
                    }
                    else{
                        startStop(false);
                    }
                }
            }

            //////////////////////////////////////////////// Run BP
            else if(action == ACTION_BP) 
            {
                // Pembuangan Udara
                if (millis() - starts <= 2000) {                
                    processNiva(source_ch_BP);

                    deflation();
                }

                // Sequence: BP Measurement
                else if (millis() - starts > 2000 && millis() - starts <= 72000) {                
                    processNiva(source_ch_BP);
                    SeqPress(BP_threshold);
                }

                // Pembuangan Udara
                else if (millis() - starts > 72000 && millis() - starts <= 82000) {                
                    processNiva(source_ch_BP);
                    deflation();
                }
                
                // Stop measurement
                else if (millis() - starts > 82000 && millis() - starts <= 82001) {
                    startStop(false);
                }
            }

            //////////////////////////////////////////////// Run BP XL
            else if(action == ACTION_BP_XL) 
            {
                // Pembuangan Udara
                if (millis() - starts <= 2000) {                
                    processNiva_encrypt(source_ch_BP);
                    deflation();
                }

                // Sequence: BP Measurement
                else if (millis() - starts > 2000 && !release) {                
                    processNiva_encrypt(source_ch_BP);
                    SeqPress(BP_threshold);
                }

                // Pembuangan Udara
                else if (release || millis() - starts > 320000 && millis() - starts <= 330001) {          
                    if(millis() - starts >= 330000) startStop(false);
                    else{
                        processNiva_encrypt(source_ch_BP);
                        deflation();
                    }
                }
            }

            //////////////////////////////////////////////// Run BP LITE Adaptif Time
            else if(action == ACTION_BP_LITE) 
            {
                // Pembuangan Udara
                if (millis() - starts <= 2000) {                
                    processNiva_encrypt(source_ch_BP);
                    deflation();
                }
                // Sequence: BP Measurement
                else if (millis() - starts > 2000 && !release) {                
                    processNiva_encrypt(source_ch_BP);
                    SeqPress(BP_threshold);
                }

                // Pembuangan Udara
                else if (millis() - starts > 47000 && release || millis() - starts > 320000 && millis() - starts <= 330001) {  
                        processNiva_encrypt(source_ch_BP);
                        deflation();
                        if(timeRelease == 0) timeRelease = millis();
                        else if (millis() - timeRelease >= 5000) startStop(false);                   
                }
            }

            // TBI Development
            else if(action == ACTION_TBI)
            {
                // Tahap 1: 2 detik awal
                if (millis() - starts <= 2000) {
                    processTBI(source_ch_TBI);
                    deflation(); // pelepasan awal
                    tbi1 = 0;
                    tbi2 = 0;
                    pressure1 = -1;
                    pressure2 = -1;
                }

                // Tahap 2: Inflasi sampai threshold tercapai
                else if (millis() - starts > 2000 && (tbi1 == 0 || tbi2 == 0)) {   
                    processTBI(source_ch_TBI);
                    seqTBI(TBI_THRESHOLD);  // threshold 180mmHg
                } 

                // Tahap 3 : Deflasi pelan sampai 1/2 threshold
                else if (tbi1 == 1 && tbi2 == 1 && (adc_read3 > pressure1/2 || adc_read4 > pressure2/2)) {
                    processTBI(source_ch_TBI);
                    SeqComp(TBI_THRESHOLD);  // pastikan valve terbuka penuh
                }
                // Tahap 4: Tekanan sudah di bawah thresholhold deflation
                else if (release && tbi1 == 1 && tbi2 == 1 && adc_read3 < pressure1/2 && adc_read4 < pressure2/2) {
                    processTBI(source_ch_TBI);
                    deflation();  // pastikan valve terbuka penuh  
                    if(timeRelease == 0) timeRelease = millis();
                    else if (millis() - timeRelease >= 1000) startStop(false); // hentikan pengiriman data
                                    
                }
            }

            else if(action == ACTION_PPG_SHORT) 
            {
                // Sequence: PPG Measurement
                if (millis() - starts <= 30000) {            
                    processNiva_encrypt(source_ch_PPG);
                    toggle_spo2_source_ch(ix);
                }

                // Stop measurement
                else if (millis() - starts > 30000 && millis() - starts <= 30001) {
                    startStop(false);
                    
                }
            }

            else if(action == ACTION_COMP_SHORT) 
            {
                // Pembuangan Udara
                if (millis() - starts <= 2000) {                
                    processNiva_encrypt(source_ch_COMP);
                    deflation();
                }

                // Sequence: Compliance Left Arm (Channel 1)
                else if (millis() - starts > 2000 && millis() - starts <= 43000) {                
                    processNiva_encrypt(source_ch_COMP);
                    SeqComp(COMP_THRESHOLD);
                }

                // Pembuangan Udara
                else if (millis() - starts > 43000 && millis() - starts <= 45000) {              
                    processNiva_encrypt(source_ch_COMP);
                    deflation();
                }

                // Stop measurement
                else if (millis() - starts > 45000 && millis() - starts <= 45001) {
                    startStop(false);	
                    ESP.restart();              //Restart per siklus
                }
            }

            else if(action == ACTION_COMP_XL) 
            {
                unsigned long a = 58000;
                unsigned long b = 60000;
                unsigned long c = 10;
                // Pembuangan Udara
                if (millis() - starts <= 2000) {                
                    processNiva_encrypt(source_ch_COMP);
                    deflation();
                }

                // Sequence: Compliance Left Arm (Channel 1)
                else if (millis() - starts > 2000 && millis() - starts <= a) {
                    processNiva_encrypt(source_ch_COMP);
                    SeqComp(COMP_THRESHOLD_XL);
                }

                // Pembuangan Udara
                else if (millis() - starts > a && millis() - starts <= b) {
                    processNiva_encrypt(source_ch_COMP);
                    deflation();
                }

                // Stop measurement
                else if (millis() - starts > b && millis() - starts <= b+1) {
                    startStop(false);	
                    //ESP.restart();              //Restart per siklus tidak boleh pada manset besar
                }
                
            }
        }

    //////////////////////////////////////////////// [DEBUG] Print SR last latch
    if(action == ACTION_PRINT_LAST_LATCH) 
    {
        print_SR_data();
        action = ACTION_DEFAULT;
    }

    //////////////////////////////////////////////// [DEBUG] Force SR to reset all output values
    if(action == ACTION_SR_TURN_OFF_ALL) 
    {
        SR_turn_off_all();
        action = ACTION_DEFAULT;
    }

    //////////////////////////////////////////////// [DEBUG] Toggle Pump manually
    if(action == ACTION_TOGGLE_PUMP) 
    {
        pump_toggle(debug_id);
        debug_id = 0;
        action = ACTION_DEFAULT;
    }

    //////////////////////////////////////////////// [DEBUG] Toggle Valve manually
    if(action == ACTION_TOGGLE_VALVE) 
    {
        valve_toggle(debug_id);
        debug_id = 0;
        action = ACTION_DEFAULT;
    }
    // esp_task_wdt_reset(); // reset wdt
}

/* =================================================================================================================== */
/* =================================================================================================================== */

void processNiva(uint8_t temp_ch[]) 
{
    ch_index = (ix + 1) % 4;
    ch_addr = temp_ch[ch_index];
    
    ///////////////////// Aktual value sensor masing2 dikurangi dengan nilai initial sehingga semua nilai menjadi start dari sekitar 0
    switch (ch_addr) {
        case 88 :   //channel 1
        samples[ix] = read_ADC(88) - initial2; 
        adc_read1 = samples[ix]; 
        break;
        case 104 :  //channel 2
        samples[ix] = read_ADC(104) - initial3;
        adc_read2 = samples[ix]; 
        break;
        case 120 :  //channel 3
        samples[ix] = read_ADC(120) - initial4;
        adc_read3 = samples[ix]; 
        break;
        case 72 :   //channel 4
        samples[ix] = read_ADC(72) - initial1;
        adc_read4 = samples[ix]; 
        break;
        default:
        samples[ix] = read_ADC(ch_addr);
        break;
    }

    if (samples[ix]<=0){
        samples[ix]=500;        
    }

    if(release){
        if(action == ACTION_BP_LITE || action == ACTION_BP_XL) samples[ix]=20133;
    }

    adc_read = samples[ix];
    ix++; 

    if (ix >= 32) {        
        ix = 0;
        count_hold++;
        if(count_hold >= 2){
            Serial.write((uint8_t *)samples, 128);
            count_hold = 2;
        }
    }
}


void processNiva_encrypt(uint8_t temp_ch[]) 
{
    ch_index = (ix + 1) % 4;
    ch_addr = temp_ch[ch_index];
    
    // Baca sensor dan kalibrasi (nilai awal dikurangkan)
    switch (ch_addr) {
        case 88:   // channel 1
            samples[ix] = read_ADC(88) - initial2; 
            adc_read1 = samples[ix];
            break;
        case 104:  // channel 2
            samples[ix] = read_ADC(104) - initial3;
            adc_read2 = samples[ix];
            break;
        case 120:  // channel 3
            samples[ix] = read_ADC(120) - initial4;
            adc_read3 = samples[ix];
            break;
        case 72:   // channel 4
            samples[ix] = read_ADC(72) - initial1;
            adc_read4 = samples[ix];
            break;
        default:
            samples[ix] = read_ADC(ch_addr);
            break;
    }
    
    // Jika hasil pembacaan ≤ 0, set nilai default
    if (samples[ix] <= 0) {
        samples[ix] = 500;
    }
    
    // Mode release: ganti nilai sensor dengan nilai preset
    if (release) {
        if (action == ACTION_BP_LITE || action == ACTION_BP_XL)
            samples[ix] = 20133;
    }
    
    adc_read = samples[ix];
    ix++;
    
    // Jika buffer sudah terisi 32 nilai (128 byte)
    if (ix >= 32) {
        ix = 0;
        count_hold++;
        if (count_hold >= 2) {
            // Buat framing dengan header dan footer
            uint8_t header[] = {0xAA, 0x55};  // misalnya: header = AA55
            uint8_t footer[] = {0x55, 0xAA};  // footer = 55AA
            const int dataLength = 32 * sizeof(int32_t);  // 32 nilai, masing-masing 4 byte (128 byte total)
            uint8_t packet[2 + dataLength + 2];  // total 132 byte
            
            // Salin header
            memcpy(packet, header, 2);
            // Salin data sensor (buffer samples)
            memcpy(packet + 2, samples, dataLength);
            // Salin footer
            memcpy(packet + 2 + dataLength, footer, 2);
            
            Serial.write(packet, sizeof(packet));  // Kirim paket melalui serial USB
            count_hold = 2;  // Reset count_hold agar pengiriman terjadi secara periodik
        }
    }
}


/* =================================================================================================================== */
/* =================================================================================================================== */

void deflation(void) 
{
    SR_turn_off_all();
}

void SeqPress(int thres) 
{
    //////////////////////////////////////////// Steady Status
    if(la_stat == STEADY && ra_stat == STEADY && ll_stat == STEADY && rl_stat == STEADY){
        all_stat = STEADY;
        if(adc_read1 < MMHG50 && adc_read2 < MMHG50 && adc_read3 < MMHG50 && adc_read4 < MMHG50){
            release = true;
        }
    }
    else{
        all_stat = NOT_STEADY;
    }

    //////////////////////////////////////////// Handle: [CHANNEL 1] Left Hand
    if (adc_read1 > thres) {
        seqPress_state.CH1_state = STATE_MEASURE;
        la_stat = STEADY;
        P1_BP = 1;
         if(seqPress_state.CH1_state == STATE_MEASURE) {
            if(all_stat == STEADY){
                SR_BP_P1_Measure();
                seqPress_state.CH1_state = STATE_DEFAULT;
            }
            else{
                SR_turn_off_P1();
            }
         }   
   }

    else if (adc_read1 < thres && P1_BP == 0) {
        if(seqPress_state.CH1_state == STATE_INFLATE) {
            la_stat = NOT_STEADY;
            SR_BP_P1_Inflate();
        }
    }

    else if(all_stat == STEADY){
        SR_BP_P1_Measure();
        P1_BP = 1;
        seqPress_state.CH1_state = STATE_DEFAULT;
    }
    
    //////////////////////////////////////////// Handle: [CHANNEL 2] Right Hand
    if (adc_read2 > thres) {
        seqPress_state.CH2_state = STATE_MEASURE;
        ra_stat = STEADY;
        P2_BP = 1;
        if(seqPress_state.CH2_state == STATE_MEASURE) {
            if(all_stat == STEADY){
                SR_BP_P2_Measure();
                seqPress_state.CH2_state = STATE_DEFAULT;
            }
            else{
                SR_turn_off_P2();
            }
        }

    } 
    else if (adc_read2 < thres && P2_BP == 0) {
        if(seqPress_state.CH2_state == STATE_INFLATE) {
            ra_stat = NOT_STEADY;
            SR_BP_P2_Inflate();
            seqPress_state.CH2_state = STATE_MEASURE;
        }
    }
    else if(all_stat == STEADY){
        SR_BP_P2_Measure();
        P2_BP = 1;
        seqPress_state.CH2_state = STATE_DEFAULT;
    }

    //////////////////////////////////////////// Handle: [CHANNEL 3] Left Leg
    if (adc_read3 > thres) {
        seqPress_state.CH3_state = STATE_MEASURE;
        ll_stat = STEADY;
        P3_BP = 1;
        if(seqPress_state.CH3_state == STATE_MEASURE) {
            SR_BP_P3_Measure();
            seqPress_state.CH3_state = STATE_DEFAULT;
        }
    } 
    else if (adc_read3 < thres && P3_BP == 0) {
        if(seqPress_state.CH3_state == STATE_INFLATE) {
            ll_stat = NOT_STEADY;
            SR_BP_P3_Inflate();
            seqPress_state.CH3_state = STATE_MEASURE;
        }
    }

    //////////////////////////////////////////// Handle: [CHANNEL 4] Right Leg
    if (adc_read4 > thres) {
        seqPress_state.CH4_state = STATE_MEASURE;
        rl_stat = STEADY;
        P4_BP = 1;
        if(seqPress_state.CH4_state == STATE_MEASURE) {
            SR_BP_P4_Measure();    
            seqPress_state.CH4_state = STATE_DEFAULT;
        }
    } 
    else if (adc_read4 < thres && P4_BP == 0) {
        if(seqPress_state.CH4_state == STATE_INFLATE) {
            rl_stat = NOT_STEADY;
            SR_BP_P4_Inflate();
            seqPress_state.CH4_state = STATE_MEASURE;
        }
    }
}

void SeqComp(int thres) 
{
    if (adc_read1 > thres) {
        if(seqPress_state.CH1_state == STATE_MEASURE) {
            SR_COMP_LEFT_P1_Measure();
            P1_COMP_LA = 1;
            seqPress_state.CH1_state = STATE_DEFAULT;
        }
    } 
    else if (adc_read1 < thres && P1_COMP_LA == 0) {
        if(seqPress_state.CH1_state == STATE_INFLATE) {
            SR_COMP_LEFT_P1_Inflate();
            seqPress_state.CH1_state = STATE_MEASURE;
        }
    }

    if (adc_read2 > thres) {
        if(seqPress_state.CH2_state == STATE_MEASURE) {
            SR_COMP_RIGHT_P2_Measure();
            P2_COMP_RA = 1;
            seqPress_state.CH2_state = STATE_DEFAULT;
        }
    } 
    else if (adc_read2 < thres && P2_COMP_RA == 0) {
        if(seqPress_state.CH2_state == STATE_INFLATE) {
            SR_COMP_RIGHT_P2_Inflate();
            seqPress_state.CH2_state = STATE_MEASURE;
        }
    }

     if (tbi1 == 1 && tbi2 == 1 && adc_read3 < pressure1/2 && adc_read4 < pressure2/2) {
                    release = true;
     }
}

void SeqCompLeft(int thres) 
{
    if ((ix + 1) % 4 == 2 && adc_read > thres) {
        if(seqPress_state.CH1_state == STATE_MEASURE) {
            SR_COMP_LEFT_P1_Measure();
            P1_COMP_LA = 1;
            seqPress_state.CH1_state = STATE_DEFAULT;
        }
    } 
    else if ( (ix + 1) % 4 == 2 && adc_read < thres && P1_COMP_LA == 0) {
        if(seqPress_state.CH1_state == STATE_INFLATE) {
            SR_COMP_LEFT_P1_Inflate();
            seqPress_state.CH1_state = STATE_MEASURE;
        }
    }
}

void SeqCompRight(int thres) 
{
    if ((ix + 1) % 4 == 3 && adc_read > thres) {
        if(seqPress_state.CH2_state == STATE_MEASURE) {
            SR_COMP_RIGHT_P2_Measure();
            P2_COMP_RA = 1;
            seqPress_state.CH2_state = STATE_DEFAULT;
        }
    } 
    else if ( (ix + 1) % 4 == 3 && adc_read < thres && P2_COMP_RA == 0) {
        if(seqPress_state.CH2_state == STATE_INFLATE) {
            SR_COMP_RIGHT_P2_Inflate();
            seqPress_state.CH2_state = STATE_MEASURE;
        }
    }
}

void startStop(bool isStop) 
{ 
    // Start
    if (isStop && tim_250us == NULL) {
        SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE1));
        digitalWrite(ADS_CS_PIN, LOW);

        tim_250us = timerBegin(0, 80, true);
        timerAttachInterrupt(tim_250us, &onTim_250us, true);
        timerAlarmWrite(tim_250us, 250, true);
        timerAlarmEnable(tim_250us);
        starts = millis(); // Reset the timing 
    }

    // Stop
    else {
        
        timerAlarmDisable(tim_250us);
        timerDetachInterrupt(tim_250us);
        timerEnd(tim_250us);
        tim_250us = NULL;
        digitalWrite(ADS_CS_PIN, HIGH);
        SPI.endTransaction();

        PumpPWM_stop_all();
        deflation();
        //set_ch2_non_spo2_measurement(); //off Led PPG
        action = ACTION_DEFAULT;
        reset_pump_valve_state_vars();
        memset(samples, 0, sizeof(samples));
        memset(&seqPress_state, STATE_DEFAULT, sizeof(seqPress_state));
        vTaskDelay(500 / portTICK_PERIOD_MS);  
    }
}

void pump_toggle(int pump_id) 
{
    // Pump 1
    if (pump_id == 1) {
        if(!(SR1_last_latch & 0b00000001)) { SR_turn_on_P1(); }
        else                               { SR_turn_off_P1(); }
    }

    // Pump 2
    else if (pump_id == 2) {
        if(!(SR1_last_latch & 0b00000010)) { SR_turn_on_P2(); }
        else                               { SR_turn_off_P2(); }
    }
    
    // Pump 3
    else if (pump_id == 3) {
        if(!(SR1_last_latch & 0b00000100)) { SR_turn_on_P3(); }
        else                               { SR_turn_off_P3(); }
    }

    // Pump 4
    else if (pump_id == 4) {
        if(!(SR1_last_latch & 0b00001000)) { SR_turn_on_P4(); }
        else                               { SR_turn_off_P4(); }
    }

    // Pump 5
    else if (pump_id == 5) {
        if(!(SR1_last_latch & 0b00010000)) { SR_turn_on_P5(); }
        else                               { SR_turn_off_P5(); }
    }

    // Pump 6
    else if (pump_id == 6) {
        if(!(SR1_last_latch & 0b00100000)) { SR_turn_on_P6(); }
        else                               { SR_turn_off_P6(); }
    }
}

void valve_toggle(int valve_id) 
{
    // Valve 1
    if (valve_id == 1) {
        if(!(SR2_last_latch & 0b00000001)) { SR_turn_on_V1(); }
        else                               { SR_turn_off_V1(); }
    }

    // Valve 2
    else if (valve_id == 2) {
        if(!(SR2_last_latch & 0b00000010)) { SR_turn_on_V2(); }
        else                               { SR_turn_off_V2(); }
    }

    // Valve 3
    else if (valve_id == 3) {
        if(!(SR2_last_latch & 0b00000100)) { SR_turn_on_V3(); }
        else                               { SR_turn_off_V3();}
    }

    // Valve 4
    else if (valve_id == 4) {
        if(!(SR2_last_latch & 0b00001000)) { SR_turn_on_V4();}
        else                               { SR_turn_off_V4(); }
    }

    // Valve 5
    else if (valve_id == 5) {
        if(!(SR2_last_latch & 0b00010000)) { SR_turn_on_V5(); }
        else                               { SR_turn_off_V5(); }
    }

    // Valve 6
    else if (valve_id == 6) {
        if(!(SR2_last_latch & 0b00100000)) { SR_turn_on_V6(); }
        else                               { SR_turn_off_V6(); }
    }

    // Valve 7
    else if (valve_id == 7) {
        if(!(SR2_last_latch & 0b01000000)) { SR_turn_on_V7(); }
        else                               { SR_turn_off_V7(); }
    }

    // Valve 8
    else if (valve_id == 8) {
        if(!(SR2_last_latch & 0b10000000)) { SR_turn_on_V8(); }
        else                               { SR_turn_off_V8(); }
    }
}

void print_SR_data(void)
{
    Serial.print("SR1|SR2: ");
    Serial.print(SR1_last_latch, BIN); 
    Serial.print(" | ");
    Serial.println(SR2_last_latch, BIN); 
}

void reset_pump_valve_state_vars(void)
{
    adc_read = 0;
    ix = 0;
    count_hold = 0;
    P1_BP = 0;
    P2_BP = 0;
    P3_BP = 0;
    P4_BP = 0;
    P1_COMP_LA = 0;
    P2_COMP_RA = 0;
    P1_RHI = 0;
    la_stat = NOT_STEADY;
    ra_stat = NOT_STEADY;
    ll_stat = NOT_STEADY;
    rl_stat = NOT_STEADY;
    all_stat = NOT_STEADY;
    release = false;
    timeRelease = 0;
}

void print_fw_version(void)
{
    Serial.print(fw_version);
}

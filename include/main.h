  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /*
  PRODUCT: NIVA
  VERSION: 3.2

  CHANGES:
  1) [07.12.2021] Menghilangkan satu Shift Register untuk LED Baterai.
  2) [31.12.2021] StartStop : add memset(samples, 0, sizeof(samples));
  3) [31.12.2021] Hardware : remove CP2102 power from USB, connect VCC 5V from battery supply line. 
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  VERSION: 3.3
  Editor : Dicky Mardiansyah
  4) [13.07.2022] Menambahkan void SR_BP_P1_Hold(void) dan void SR_BP_P2_Hold(void) untuk menunggu manset kaki terisi penuh
  5) [15.07.2022] Perubahan pada timing RHI (mengikuti update 3.2 laptop Pak Adi)
              else if(action == ACTION_RHI) 
              {
                  // Sequence: RHI 1
                  if (millis() - starts <= 300000) {          // awalnya adalah 60000 
  6) [15.07.2022] Menambahkan status NOT_STEADY pada void reset_pump_valve_state_vars(void) agar pengukuran tekanan tangan menunggu treshold kaki terpenuhi
  7) [18.07.2022] Menambahkan count hold pada pengiriman serial data adc selama 2 count agar menghilangkan nilai 0 pada pembacaan PPG (count_hold++; pada void processNiva)
  8) [22.07.2022] Mengubah array uint8_t source_ch_RHI[] = {ch_AIN4_MPX1, ch_AIN4_MPX1, ch_AIN4_MPX1, ch_AIN4_MPX1}; untuk memastikan pengiriman data compliance kiri sesuai
                  Mengganti processNiva(source_ch_BP); menjadi processNiva(source_ch_RHI); pada else if(action == ACTION_COMP_LEFT) {
  9) [26.07.2022] Menghapus uint8_t source_ch_RHI[] = {ch_AIN4_MPX1, ch_AIN4_MPX1, ch_AIN4_MPX1, ch_AIN4_MPX1}; pada main.h
                  Menambahkan uint8_t source_ch_RHI[] = {ch_AIN4_MPX1, ch_AIN4_MPX1, ch_AIN4_MPX1, ch_AIN4_MPX1}; pada else if(action == ACTION_COMP_LEFT) {
                  Menambahkan uint8_t source_ch_RHI[] = {ch_AIN5_MPX2, ch_AIN5_MPX2, ch_AIN5_MPX2, ch_AIN5_MPX2}; pada else if(action == ACTION_COMP_RIGHT) {    
                  [cancel] Menambahkan WDT (watchdog timmer selama 3 detik) = jika esp stuck/hang selama 3 detik maka esp akan reboot.
  10) [31.08.2022] Reactive WDT karena ketika endurance test NIVA pernah mengalami crash dan perlu direstart (kemungkinan crash sekitar 1:50)
  11) [01.09.2022] Cancel WDT karena ketika endurance test mengalami lebih banyak masalah seperti manset tidak terdek, sinyal sensor bermasalah dll.
  12) [09.09.2022] Menambahkan instruksi restart per siklus pengujian, karena berdasarkan data endurance test kebanyakan error terjadi ketika siklus pengujian ke lebih dari 10

  VERSION: 3.4
  13) [27.09.2022] Membaca nilai initial masing2 sensor (lihat void setup)
                  Nilai aktual masing2 sensor dikurangi dengan nilai initial agar nilai sensor dimulai dari 0 (lihat void processNiva)
                  Nilai masing2 treshold disesuaikan dengan perhitungan nilai xgzp 40kPa
  14) [03.10.2022] Menonaktifkan buffer untuk meningkatkan range tegangan pembacaan sensor dari 3V menjadi 5V, hal ini diperlukan karena kebutuhan sensor xgzp 40kPa (lihat SetRegisterValue(STATUS,B00110000), lihat datasheet sensor dan ads1256) 
                  Mengaktifkan kembali buffer karena ketika dinonaktifkan pembacaan sensor jadi tidak linear
  15) [26.10.2022] Nilai masing2 treshold disesuaikan dengan perhitungan nilai xgzp 50kPa
  16) [01.11.2022] Mengupdate pembacaan battery dari sensing 6V menjadi 13.5V lihat battery_monitor.cpp
                  Hardware : Mengganti resistor pembagi tegangan R73 dari 10k menjadi 2.7k
  17) [02.11.2022] Prosedur hold pada pump brachial tidak berfungsi karena kemungkinan ada glitch pada pembacaan sensor (nilai adc_read)
                  Menambahkan variabel adc_read1 - adc_read4 untuk memisahkan hasil pembacaan masing2 pressure sensor (lihat void SeqPress(int thres))
  18) [04.11.2022] Menyalakan pump 1 selama 1 detik pada void setup() sebagai indikasi dalam menentukan firmware sudah berhasil diupload
  19) [29.12.2022] Mengubah FW Version jadi 1.0 untuk Produksi
  20) [13.02.2023] Menambahkan command dan routine untuk melakukan function test

  VERSION: 3.5
  21) [17.11.2023] Menambah command dan routine ACTION_PPG_SHORT dan ACTION_COMP_SHORT (PPG dan Compliance menjadi 30 detik)
                  Compliance menjadi kedua tangan dipompa dan ditahan secara bersamaan, tidak salah satunya lagi.
                  Firmware ini hanya untuk software versi 2.1S
  22) [15.05.2024] Menambah ACTION_BP_XL untuk pengukuran BP manset besar
                  
  */
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  #ifndef MAIN_H
  #define  MAIN_H

  #include <Arduino.h>

  ////////////////////////////////////////////////////////////////////////////////////
  // FW VERSION
  #define FW_VERSION  "V4.0" //NIVA TBI 2026
  char fw_version[5]  = FW_VERSION;

  ////////////////////////////////////////////////////////////////////////////////////
  // FLASH WRITE READ
  const uint32_t NVM_Offset = 0x290000;
  uint8_t FLASH_Address = 0;
  
  String StrIn = ""; // buffer to read sn from uart
  //String StrOut;
  String f_read;//[16] = {0}; //full read from flash: "270916NV1234SCNP"
  String niva_sn;// = "000000000000";//[12] = {0}; //parse to: "270916NV1234";
  String niva_fw;// = "0000";//[4] = {0}; //parse to: "SCNP";

  
  template<typename T>
  void FlashWrite(uint32_t address, const T& value) {
    ESP.flashEraseSector((NVM_Offset+address)/4096);
    ESP.flashWrite(NVM_Offset+address, (uint32_t*)&value, sizeof(value));
  }
  
  template<typename T>
  void FlashRead(uint32_t address, T& value) {
    ESP.flashRead(NVM_Offset+address, (uint32_t*)&value, sizeof(value));
  }

  ////////////////////////////////////////////////////////////////////////////////////
  // Measurement source channels - reference to ADS output pins
  // Make sure the connections are tally with hardware, i.e ch_AIN0_PPG1: AIN0 connected to PPG1.
  #define ch_AIN0_PPG1    0x08    // dec 8
  #define ch_AIN1_PPG2    0x18    // dec 24
  #define ch_AIN2_PPG3    0x28    // dec 40
  #define ch_AIN3_PPG4    0x38    // dec 56

  #define ch_AIN4_MPX1    0x48    // dec 72
  #define ch_AIN5_MPX2    0x58    // dec 88
  #define ch_AIN6_MPX3    0x68    // dec 104
  #define ch_AIN7_MPX4    0x78    // dec 120

  // ix       =====>      = {     3               0              1            2    };
  // ch_index =====>      = {     0               1              2            3    };
  uint8_t source_ch_PPG[] = {ch_AIN0_PPG1, ch_AIN1_PPG2, ch_AIN2_PPG3, ch_AIN3_PPG4}; 
  uint8_t source_ch_BP[]  = {ch_AIN4_MPX1, ch_AIN5_MPX2, ch_AIN6_MPX3, ch_AIN7_MPX4};
  uint8_t source_ch_RG[]  = {ch_AIN4_MPX1, ch_AIN5_MPX2, ch_AIN6_MPX3, ch_AIN7_MPX4};
  uint8_t source_ch_COMP[] = {ch_AIN4_MPX1, ch_AIN5_MPX2, ch_AIN6_MPX3, ch_AIN7_MPX4};  
  // uint8_t source_ch_RHI[] = {ch_AIN4_MPX1, ch_AIN4_MPX1, ch_AIN4_MPX1, ch_AIN4_MPX1}; //{ch_AIN4_MPX1, ch_AIN5_MPX2, ch_AIN0_PPG1, ch_AIN1_PPG2}; 
  // uint8_t source_ch_LF[]  = {ch_AIN4_MPX1, ch_AIN5_MPX2, ch_AIN6_MPX3, ch_AIN7_MPX4}; 

// TBI Development
uint8_t source_ch_TBI[] = {ch_AIN6_MPX3, ch_AIN7_MPX4, ch_AIN2_PPG3, ch_AIN3_PPG4};
// uint8_t source_ch_TBI[] = {ch_AIN6_MPX3, ch_AIN7_MPX4, ch_AIN2_PPG3, ch_AIN3_PPG4};

  ////////////////////////////////////////////////////////////////////////////////////
  // Timer for measurement: set tick every 250us --> x4 channels = send data every 1000 us = 1 ms = 1kHz
  #define TIMER_ID_250US   0
  hw_timer_t * tim_250us = NULL;
  portMUX_TYPE tim_mux_250us = portMUX_INITIALIZER_UNLOCKED;
  volatile SemaphoreHandle_t sem;

  ////////////////////////////////////////////////////////////////////////////////////
  // Current millis temp variable
  unsigned long starts = 0;
  unsigned long timeRelease = 0;
  unsigned long ts = 0;
  ////////////////////////////////////////////////////////////////////////////////////
  // Variables for processNiva()
  volatile int32_t adc_read;                    // Temp current sensor data
  int32_t adc_read1;
  int32_t adc_read2;
  int32_t adc_read3;
  int32_t adc_read4;
  volatile int ix;
  int32_t samples[32];
  volatile int32_t average;
  int32_t initial;
  int32_t initial1;
  int32_t initial2;
  int32_t initial3;
  int32_t initial4;
  bool auto_zero=true;

  int8_t ch_index;
  byte ch_addr;
  int count_hold = 0;

  // Time
  unsigned long second = 0;

  /////////////////////////////////////////////////////////////////////
// TBI Development
int threshold_reached1 = -1;
int threshold_reached2 = -1;

// Adaptive Threshold
const float b[] = {1.0, 0.0, -1.0};
const float a[] = {1.0, -1.763275744689875246251631324412301182747,  0.763706196732711117824976554402383044362};
#define IIR_ORDER 2
float y;

float iir_x[IIR_ORDER + 1] = {0}; // IIR_ORDER+1
float iir_y[IIR_ORDER + 1] = {0};

////////////////////////
float y2;
float iir2_x[IIR_ORDER + 1] = {0}; // IIR_ORDER+1
float iir2_y[IIR_ORDER + 1] = {0};

// Pulse treshold dengan timeout bisa diubah ubah nanti kalau udah implementasi real
#define PULSE_THRESHOLD 100000  // Atur sesuai skala hasil filter
#define TIMEOUT_SAMPLE_COUNT 2000  // 2 detik × 900 Hz

int sample_counter = 0; 
int sample_counter2 = 0;      // Counter untuk sampel dalam 2 detik
int32_t pressure1 = -1;       // Nilai tekanan saat pulse hilang (sekali saja)
int32_t pressure2 = -1;       // Nilai tekanan saat pulse terdeteksi (sekali saja)

int32_t tbi1, tbi2; // flag threshold reached untuk TBI
/////////////////////////////////////////////////////////////////////////////

  
  ////////////////////////////////////////////////////////////////////////////////////
  // Motor and Valve state vars
  int32_t P1_BP, P2_BP, P3_BP, P4_BP, P1_COMP_LA, P2_COMP_RA, P1_RHI;

  ////////////////////////////////////////////////////////////////////////////////////
  // Define serial incomingData
  #define COMMAND_RUN_PPG_CHECK       'a'
  #define COMMAND_RUN_BP_MEAS_T1      'b'
  #define COMMAND_RUN_BP_MEAS_T2      'c'
  #define COMMAND_RUN_BP_MEAS_T3      'd'

  #define COMMAND_RUN_COMP_MEAS_LEFT  'f' //MENAMBAHKAN ACTION SERIAL INCOMING DATA UNTUK COMPLIANCE TANGAN KIRI
  #define COMMAND_RUN_RHI_MEAS        'g'
  #define COMMAND_RUN_SPO2_MEAS       'h'
  #define COMMAND_GET_BATT_LEVEL      'i'
  #define COMMAND_GET_FW_VERSION      'j'
  #define COMMAND_RUN_COMP_MEAS_RIGHT 'k' //MENAMBAHKAN ACTION SERIAL INCOMING DATA UNTUK COMPLIANCE TANGAN KANAN

  #define COMMAND_RUN_BP_MEAS_T1_XL   'o'
  #define COMMAND_RUN_BP_MEAS_T2_XL   'm'
  #define COMMAND_RUN_BP_MEAS_T3_XL   'n'
  #define COMMAND_RUN_COMP_SHORT_XL   'l'

  #define COMMAND_RUN_BP_MEAS_T2_LITE   'p'
  #define COMMAND_RUN_BP_MEAS_T3_LITE   'q'

  #define COMMAND_RUN_PPG_MEAS_SHORT    '3'
  #define COMMAND_RUN_COMP_MEAS_SHORT   '4'
  #define COMMAND_RUN_PPG_CHECK_ENCRYPT 't'

  #define COMMAND_TOGGLE_P1           'A'
  #define COMMAND_TOGGLE_P2           'B'
  #define COMMAND_TOGGLE_P3           'C'
  #define COMMAND_TOGGLE_P4           'D'
  #define COMMAND_TOGGLE_P5           'E'
  #define COMMAND_TOGGLE_P6           'F'
  #define COMMAND_TOGGLE_V1           'G'
  #define COMMAND_TOGGLE_V2           'H'
  #define COMMAND_TOGGLE_V3           'I'
  #define COMMAND_TOGGLE_V4           'J'
  #define COMMAND_TOGGLE_V5           'K'
  #define COMMAND_TOGGLE_V6           'L'
  #define COMMAND_TOGGLE_V7           'M'
  #define COMMAND_TOGGLE_V8           'N'

  #define COMMAND_SR_PRINT_LAST_LATCH '8'
  #define COMMAND_SR_TURN_OFF_ALL     '9'

  #define COMMAND_FUNCTION_TEST       'z'
  #define COMMAND_VOLTAGE_TEST        'x'

  #define COMMAND_SN                  'S'

  #define COMMAND_OTA_UPDATE          'OTA'
  #define COMMAND_RESTART             'r'
  #define COMMAND_STOP                'U'
  #define COMMAND_ALLSTOP             'V'

  // TBI Development
  #define COMMAND_RUN_TBI_CHECK       'Z'
  #define COMMAND_TBI                 'e' // Jangan 3 karakter nanti error alatnya langsung mompa


  ////////////////////////////////////////////////////////////////////////////////////
  // Set BP threshold level                 mpxv 50kpa       xgzp 50kPa         xgzp 40kPa     mpxv (voltage)
  #define BP_THRESHOLD_1              (int) 3450000          //2865000          //3600000     //4000000 // 160 mmHg jadi 170 mmhg
  #define BP_THRESHOLD_2              (int) 4250000          //3760000          //4700000     //5100000 // 210 mmHg
  #define BP_THRESHOLD_3              (int) 4850000          //4650000          //5400000     //6200000 // 260 mmHg   5250000 = 260mmhg diubah jadi 240mmhg
  #define COMP_THRESHOLD              (int) 1210000          //1075000          //1350000     //1600000 // 37 mmHg // 60 mmHg
  #define COMP_THRESHOLD_XL           (int) 1500000 // 75mmhg
  //#define RHI_THRESHOLD               (int) 3200000 // mmHg
  #define MMHG50                      (int) 800000 //600000 //1000000  

  int BP_threshold = 0;

  // TBI Development
  #define TBI_THRESHOLD               (int) 3623940 // 180 mmHg

  ////////////////////////////////////////////////////////////////////////////////////
  // Set measurement action type
  #define ACTION_DEFAULT              0
  #define ACTION_BP                   1

  #define ACTION_COMP_LEFT            3 //MENAMBAHKAN ACTION TYPE UNTUK COMPLIANCE TANGAN KIRI
  #define ACTION_RHI                  4
  #define ACTION_SPO2                 5

  #define ACTION_BP_XL                16
  #define ACTION_BP_LITE              18
  #define ACTION_COMP_XL              35

  #define ACTION_PPG_SIGNAL_CHECK     6
  #define ACTION_PRINT_LAST_LATCH     7
  #define ACTION_SR_TURN_OFF_ALL      8
  #define ACTION_TOGGLE_PUMP          9
  #define ACTION_TOGGLE_VALVE         10
  #define ACTION_SEND_FW_VERSION      11
  #define ACTION_SEND_BATT_LEVEL      12
  #define ACTION_COMP_RIGHT           13 //MENAMBAHKAN ACTION TYPE UNTUK COMPLIANCE TANGAN KANAN
  #define ACTION_FUNCTION_TEST        14
  #define ACTION_VOLTAGE_TEST         15

  #define ACTION_PPG_SHORT            30
  #define ACTION_COMP_SHORT           31

  #define ACTION_OTA_UPDATE           50


  // TBI Development
  #define ACTION_TBI_SIGNAL_CHECK     98
  #define ACTION_TBI                  99

  const char* ssid = "";
  const char* password = "";

  bool connected_ota = false;

  int8_t debug_id = 0;
  int8_t action = ACTION_DEFAULT;
  int8_t state_out = 0;


  ////////////////////////////////////////////////////////////////////////////////////
  // shift register last latch
  volatile uint8_t SR1_last_latch = B00000000;
  volatile uint8_t SR2_last_latch = B00000000;

  ////////////////////////////////////////////////////////////////////////////////////
  // battery voltage level
  int LEDBAR_led_to_turn_on = 0;

  ////////////////////////////////////////////////////////////////////////////////////
  // Variables to monitor states during BP, Compliance, RHI
  #define STATE_DEFAULT   0
  #define STATE_INFLATE   1
  #define STATE_MEASURE   2
  //#define STATE_DEFLATE   1

  typedef struct {
      //uint8_t deflate;            // 0=default ; 1=deflate 
      volatile uint8_t CH1_state;          // 0=default ; 1=inflate ; 2=measure
      volatile uint8_t CH2_state;          // 0=default ; 1=inflate ; 2=measure
      volatile uint8_t CH3_state;          // 0=default ; 1=inflate ; 2=measure
      volatile uint8_t CH4_state;          // 0=default ; 1=inflate ; 2=measure
      volatile uint8_t CH5_state;          // 0=default ; 1=inflate ; 2=measure
      volatile uint8_t CH6_state;          // 0=default ; 1=inflate ; 2=measure
      volatile uint8_t RHI2_state;         // 0=default ; 1=inflate ; 2=measure
  } SeqPress_states;

  SeqPress_states seqPress_state;




  ////////////////////////////////////////////////////////////////////////////////////
  // steady thres status
  #define NOT_STEADY   0
  #define STEADY   1
  bool la_stat = NOT_STEADY;
  bool ra_stat = NOT_STEADY;
  bool ll_stat = NOT_STEADY;
  bool rl_stat = NOT_STEADY;
  bool all_stat = NOT_STEADY;
  bool release = false;
  bool stop_signal_ppg = false;
  bool encryption = false;

  ///////////////////////////////////////////////////////////////////////////
  /// PROTOTYPES ////
  ///////////////////////////////////////////////////////////////////////////
  void processNiva(uint8_t temp_ch[]);
  void processNiva_encrypt(uint8_t temp_ch[]);
  void deflation(void);
  void SeqPress(int thres);
  void SeqComp(int thres);
  void SeqCompLeft(int thres);
  void SeqCompRight(int thres);
  void SeqRHI(int thres);
  void startStop(bool isStop);
  void pump_toggle(int pump_id);
  void valve_toggle(int valve_id);
  void print_SR_data(void);
  void reset_pump_valve_state_vars(void);
  void print_fw_version(void);
  //void seqRelease(int thres);


  // Ini perlu?
  void processTBI(uint8_t temp_ch[]);
  void processNivaFiltered(uint8_t temp_ch[]);
  void seqTBI(int thres);


  #endif
  ///////////////////////////////////////////////////////////////////////////
  /// END ////
  ///////////////////////////////////////////////////////////////////////////
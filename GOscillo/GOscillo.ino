/*
 * ESP32 Oscilloscope using a 320x240 TFT Version 1.09
 * The max software loop sampling rates are 10ksps with 2 channels and 20ksps with a channel.
 * In the I2S DMA mode, it can be set up to 250ksps.
 * + Pulse Generator
 * + PWM DDS Function Generator (23 waveforms)
 * Copyright (c) 2023, Siliconvalley4066
 */
/*
 * Arduino Oscilloscope using a graphic LCD
 * The max sampling rates are 4.3ksps with 2 channels and 8.6ksps with a channel.
 * Copyright (c) 2009, Noriaki Mitsunaga
 */

#include <WebServer.h>
#include "User_Setup.h"
#include <Adafruit_ST7789.h> // Import the Adafruit_ST7789 library
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#include "driver/adc.h"
#include <EEPROM.h>
#define EEPROM_START 0
#include "arduinoFFT.h"
#define FFT_N 256
double vReal[FFT_N]; // Real part array, actually float type
double vImag[FFT_N]; // Imaginary part array
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_N, 1.0);  // Create FFT object

float waveFreq[2];             // frequency (Hz)
float waveDuty[2];             // duty ratio (%)
int dataMin[2];                // buffer minimum value (smallest=0)
int dataMax[2];                //        maximum value (largest=4095)
int dataAve[2];                // 10 x average value (use 10x value to keep accuracy. so, max=40950)
int saveTimer;                 // remaining time for saving EEPROM
int timeExec;                  // approx. execution time of current range setting (ms)
extern byte duty;
extern byte p_range;
extern unsigned short count;
extern long ifreq;
extern byte wave_id;

const int ad_ch0 = ADC1_CHANNEL_6;  // Analog 34 pin for channel 0 ADC1_CHANNEL_6
const int ad_ch1 = ADC1_CHANNEL_7;  // Analog 35 pin for channel 1 ADC1_CHANNEL_7
const long VREF[] = {50, 99, 248, 495, 990}; // reference voltage 3.3V ->  50dots : LCD (3.3V * 15dots/div / 1V/div)
                                        //                        -> 99  : 0.5V/div
                                        //                        -> 248 : 0.2V/div
                                        //                        -> 495 : 100mV/div
                                        //                        -> 990 : 50mV/div
//const int MILLIVOL_per_dot[] = {100, 50, 20, 10, 5}; // mV/dot
//const int ac_offset[] PROGMEM = {1792, -128, -1297, -1679, -1860}; // for OLED
const int ac_offset[] PROGMEM = {3072, 512, -1043, -1552, -1804}; // for Web
const int MODE_ON = 0;
const int MODE_INV = 1;
const int MODE_OFF = 2;
const char Modes[3][4] PROGMEM = {"ON", "INV", "OFF"};
const int TRIG_AUTO = 0;
const int TRIG_NORM = 1;
const int TRIG_SCAN = 2;
const int TRIG_ONE  = 3;
const char TRIG_Modes[4][5] PROGMEM = {"Auto", "Norm", "Scan", "One "};
const int TRIG_E_UP = 0;
const int TRIG_E_DN = 1;
#define RATE_MIN 0
#define RATE_MAX 18
#define RATE_NUM 19
#define RATE_DMA 5
#define RATE_DUAL 7
#define RATE_ROLL 15
#define RATE_MAG 1
#define ITEM_MAX 28
const char Rates[RATE_NUM][5] PROGMEM = {"10us", "20us", "100u", "200u", "500u", " 1ms", "1.3m", " 2ms", " 5ms", "10ms", "20ms", "50ms", "100m", "200m", "0.5s", " 1s ", " 2s ", " 5s ", " 10s"};
const unsigned long US_DIV[] PROGMEM = {40, 40, 40, 80, 200, 1000, 1300, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000};
const unsigned long HREF[] PROGMEM = {40, 40, 40, 80, 200, 400, 500, 1333, 3333, 6667, 13333, 33333, 40000, 80000, 200000, 400000, 800000, 2000000, 4000000};
//const unsigned long HREF[] PROGMEM = {40, 40, 40, 80, 200, 400, 500, 800, 2000, 4000, 8000, 20000, 40000, 80000, 200000, 400000, 800000, 2000000, 4000000};
#define RANGE_MIN 0
#define RANGE_MAX 4
#define VRF 3.3
const char Ranges[5][5] PROGMEM = {" 1V ", "0.5V", "0.2V", "0.1V", "50mV"};
byte range0 = RANGE_MIN;
byte range1 = RANGE_MIN;
byte ch0_mode = MODE_ON, ch1_mode = MODE_ON, rate = 0, orate, wrate = 0;
byte trig_mode = TRIG_AUTO, trig_lv = 10, trig_edge = TRIG_E_UP, trig_ch = ad_ch0;
bool Start = true;  // Start sampling
byte item = 0;      // Default item
short ch0_off = 0, ch1_off = 400;
bool ch0_active = false;
bool ch1_active = false;
byte data[4][SAMPLES];                  // keep twice of the number of channels to make it a double buffer
uint16_t cap_buf[NSAMP], cap_buf1[NSAMP];
uint16_t payload[SAMPLES*2+8];
byte odat00, odat01, odat10, odat11;    // old data buffer for erase
byte sample=0;                          // index for double buffer
bool fft_mode = false, pulse_mode = false, dds_mode = false, fcount_mode = false;
byte info_mode = 3; // Text information display mode
bool dac_cw_mode = false;
int trigger_ad;
volatile bool wfft, wdds;

#define LEFTPIN   12  // LEFT
#define RIGHTPIN  13  // RIGHT
#define UPPIN     14  // UP
#define DOWNPIN   27  // DOWN
#define CH0DCSW   36  // DC/AC switch ch0
#define CH1DCSW   39  // DC/AC switch ch1

#define BGCOLOR   TFT_BLACK
#define GRIDCOLOR TFT_DARKGREY
#define CH1COLOR  TFT_GREEN
#define CH2COLOR  TFT_YELLOW
#define FRMCOLOR  TFT_LIGHTGREY
#define TXTCOLOR  TFT_WHITE
#define HIGHCOLOR TFT_CYAN
#define OFFCOLOR  TFT_DARKGREY
#define REDCOLOR  TFT_RED
#define LED_ON    HIGH
#define LED_OFF   LOW
// Bits in info_mode
#define INFO_OFF  0x20
#define INFO_BIG  0x10
#define INFO_VOL2 0x08
#define INFO_VOL1 0x04
#define INFO_FRQ2 0x02
#define INFO_FRQ1 0x01

TaskHandle_t taskHandle;

void setup(){
#define DEBUG
#ifdef DEBUG
  Serial.begin(115200);
  delay(1000); // Give serial port some time to initialize.
  Serial.println("ESP32 Oscilloscope");
  Serial.printf("CORE1 = %d\n", xPortGetCoreID());
#endif

  xTaskCreatePinnedToCore(setup1, "WebProcess", 4096, NULL, 1, &taskHandle, PRO_CPU_NUM); //Core 0でタスク開始
  pinMode(CH0DCSW, INPUT_PULLUP);   // CH1 DC/AC
  pinMode(CH1DCSW, INPUT_PULLUP);   // CH2 DC/AC
  pinMode(UPPIN, INPUT_PULLUP);     // up
  pinMode(DOWNPIN, INPUT_PULLUP);   // down
  pinMode(RIGHTPIN, INPUT_PULLUP);  // right
  pinMode(LEFTPIN, INPUT_PULLUP);   // left
  pinMode(34, ANALOG);              // Analog 34 pin for channel 0 ADC1_CHANNEL_6
  pinMode(35, ANALOG);              // Analog 35 pin for channel 1 ADC1_CHANNEL_7
  display.init(LCD_WIDTH, LCD_HEIGHT);                    // initialise the library
  display.setRotation(1);
  uint16_t calData[5] = { 368, 3538, 256, 3459, 7 };
  display.fillScreen(BGCOLOR);

  EEPROM.begin(32);                     // set EEPROM size. Necessary for ESP32
  loadEEPROM();                         // read last settings from EEPROM
  wfft = fft_mode;
  wdds = dds_mode;

  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
  adc1_config_width(ADC_WIDTH_BIT_12);
  if (pulse_mode)
    pulse_init();                       // calibration pulse output
  if (dds_mode)
    dds_setup_init();
  orate = RATE_DMA + 1;                 // old rate befor change
  rate_i2s_mode_config();
}

void DrawGrid() {
  display.drawFastVLine(XOFF, YOFF, LCD_YMAX, FRMCOLOR);          // left vertical line
  display.drawFastVLine(XOFF+SAMPLES, YOFF, LCD_YMAX, FRMCOLOR);  // right vertical line
  display.drawFastHLine(XOFF, YOFF, SAMPLES, FRMCOLOR);           // top horizontal line
  display.drawFastHLine(XOFF, YOFF+LCD_YMAX, SAMPLES, FRMCOLOR);  // bottom horizontal line
  for (int y = 0; y < LCD_YMAX; y += DOTS_DIV) {
    if (y > 0){
      display.drawFastHLine(XOFF+1, YOFF+y, SAMPLES - 1, GRIDCOLOR);  // Draw 9 horizontal lines
    }
    for (int i = 5; i < DOTS_DIV; i += 5) {
      display.drawFastHLine(XOFF+SAMPLES / 2 - 3, YOFF+y + i, 7, GRIDCOLOR);  // Draw the vertical center line ticks
    }
  }
  for (int x = 0; x < SAMPLES; x += DOTS_DIV) {
    if (x > 0){
      display.drawFastVLine(XOFF+x, YOFF+1, LCD_YMAX - 1, GRIDCOLOR); // Draw 11 vertical lines
    }
    for (int i = 5; i < DOTS_DIV; i += 5) {
      display.drawFastVLine(XOFF+x + i, YOFF+LCD_YMAX / 2 -3, 7, GRIDCOLOR);  // Draw the horizontal center line ticks
    }
  }
}

void DrawText() {
  if (info_mode & INFO_OFF)
    return;
  display.setTextSize(1); // Small
  display.setCursor(170, 1);
  display.print("FRQ1");
  display.setCursor(260, 1);
  display.print("DTY1");

  disp_ch0(1, 1);         // CH1
  display_ac_inv(1, CH0DCSW, ch0_mode);
  display.setCursor(30, 1);   // CH1 range
  disp_ch0_range();
  display.setCursor(60, 1);  // Rate
  disp_sweep_rate();

  display.setCursor(140, 1);  // Function
  display.setTextColor(HIGHCOLOR, BGCOLOR);
  if (Start == false) {
    display.setTextColor(REDCOLOR, BGCOLOR);
    display.print("HOLD");
  } else {
    display.print("RUN");
  }
  if (ch0_mode == MODE_ON) {
    dataAnalize(0);
    if (info_mode & INFO_FRQ1)
      freqDuty(0);
    if (info_mode & INFO_VOL1)
      measure_voltage(0);
  } else {
    waveFreq[0] = 0;
    waveDuty[0] = 0;
  }
  display_freqduty(0);

  if (fft_mode) return;
  display.setTextColor(TXTCOLOR, BGCOLOR);
  display.setCursor(MENU, YOFF); // Temporary debugging info
  display.print("info_mode = ");
  display.print(info_mode);
  display.setCursor(MENU, YOFF + 8);
  display.print("trig_mode = ");
  display.print(trig_mode);
  display.setCursor(MENU, YOFF + 16);
  display.print("rate = ");
  display.print(rate);

  display.setCursor(170, BOTTOM_LINE);
  display.print("FRQ2");
  display.setCursor(260, BOTTOM_LINE);
  display.print("DTY2");
  disp_ch1(1, BOTTOM_LINE);         // CH2
  display_ac_inv(BOTTOM_LINE, CH1DCSW, ch1_mode);
  display.setCursor(30, BOTTOM_LINE);   // CH2 range
  disp_ch1_range();
  set_pos_color(60, BOTTOM_LINE, TXTCOLOR); // Trigger souce
  disp_trig_source(); 
  display.setCursor(100, BOTTOM_LINE);  // Trigger edge
  disp_trig_edge();
  display.setCursor(140, BOTTOM_LINE);  // Trigger mode
  disp_trig_mode();

  if (ch1_mode == MODE_ON) {
    dataAnalize(1);
    if (info_mode & INFO_FRQ2)
      freqDuty(1);
    if (info_mode & INFO_VOL2)
      measure_voltage(1);
  } else {
    waveFreq[1] = 0;
    waveDuty[1] = 0;
  }
  display_freqduty(1);

  draw_trig_level(GRIDCOLOR); // draw trig_lv mark
}

void draw_trig_level(int color) { // draw trig_lv mark
  int x, y;

  clear_trig_level();
  x = XOFF+DISPLNG+1; y = YOFF+LCD_YMAX - trig_lv;
  display.drawLine(x, y, x+8, y+4, color);
  display.drawLine(x+8, y+4, x+8, y-4, color);
  display.drawLine(x+8, y-4, x, y, color);
}

void clear_trig_level() {
  int x, y;

  x = XOFF+DISPLNG+1;
  y = YOFF;
  display.fillRect(x, y, 9, LCD_YMAX, BGCOLOR);   // clear trig_lv mark area
}

void display_range(byte rng) {
  display.print(Ranges[rng]);
}

void display_rate(void) {
  display.print(Rates[rate]);
}

void display_mode(byte chmode) {
  display.print(Modes[chmode]); 
}

void display_trig_mode(void) {
  display.print(TRIG_Modes[trig_mode]); 
}

void display_ac(byte pin) {
  if (digitalRead(pin) == LOW) display.print('~');
}

void DrawGrid(int x) {
  if ((x % DOTS_DIV) == 0) {
    for (int y=0; y<=LCD_YMAX; y += 5)
      display.drawPixel(XOFF+x, YOFF+y, GRIDCOLOR);
  } else if ((x % 5) == 0)
    for (int y=0; y<=LCD_YMAX; y += DOTS_DIV)
      display.drawPixel(XOFF+x, YOFF+y, GRIDCOLOR);
}

void ClearAndDrawGraph() {
  int clear;
  byte *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
  int disp_leng;
  disp_leng = DISPLNG-1;
  if (sample == 0)
    clear = 2;
  else
    clear = 0;
  p1 = data[clear+0];
  p2 = p1 + 1;
  p3 = data[sample+0];
  p4 = p3 + 1;
  p5 = data[clear+1];
  p6 = p5 + 1;
  p7 = data[sample+1];
  p8 = p7 + 1;

  ch0_active = (ch0_mode == MODE_ON);
  ch1_active = (ch1_mode == MODE_ON);

  for (int x=0; x<disp_leng; x++) {
    if (ch0_active) {
      display.drawLine(XOFF+x, YOFF+LCD_YMAX-*p1++, XOFF+x+1, YOFF+LCD_YMAX-*p2++, BGCOLOR);
      display.drawLine(XOFF+x, YOFF+LCD_YMAX-*p3++, XOFF+x+1, YOFF+LCD_YMAX-*p4++, CH1COLOR);
    }
    if (ch1_active) {
      display.drawLine(XOFF+x, YOFF+LCD_YMAX-*p5++, XOFF+x+1, YOFF+LCD_YMAX-*p6++, BGCOLOR);
      display.drawLine(XOFF+x, YOFF+LCD_YMAX-*p7++, XOFF+x+1, YOFF+LCD_YMAX-*p8++, CH2COLOR);
    }
  }
}

void ClearAndDrawDot(int i) {
  if (i < 1) {
    DrawGrid(i);
    return;
  }
  if (ch0_mode != MODE_OFF) {
    display.drawLine(XOFF+i-1, YOFF+LCD_YMAX-odat00,   XOFF+i, YOFF+LCD_YMAX-odat01, BGCOLOR);
    display.drawLine(XOFF+i-1, YOFF+LCD_YMAX-data[0][i-1], XOFF+i, YOFF+LCD_YMAX-data[0][i], CH1COLOR);
  }
  if (ch1_mode != MODE_OFF) {
    display.drawLine(XOFF+i-1, YOFF+LCD_YMAX-odat10,   XOFF+i, YOFF+LCD_YMAX-odat11, BGCOLOR);
    display.drawLine(XOFF+i-1, YOFF+LCD_YMAX-data[1][i-1], XOFF+i, YOFF+LCD_YMAX-data[1][i], CH2COLOR);
  }
  DrawGrid(i);
}

#define BENDX 3480  // 85% of 4096
#define BENDY 3072  // 75% of 4096

int16_t adc_linearlize(int16_t level) {
  if (level < BENDY) {
    level = map(level, 0, BENDY, 0, BENDX);
  } else {
    level = map(level, BENDY, 4095, BENDX, 4095);
  }
  return level;
}

void scaleDataArray(byte ad_ch, int trig_point)
{
  byte *pdata, ch_mode, range;
  short ch_off;
  uint16_t *idata, *qdata, *rdata;
  long a, b;
  int ch;

  if (ad_ch == ad_ch1) {
    ch_off = ch1_off;
    ch_mode = ch1_mode;
    range = range1;
    pdata = data[sample+1];
    idata = &cap_buf1[trig_point];
    qdata = rdata = payload+SAMPLES;
    ch = 1;
  } else {
    ch_off = ch0_off;
    ch_mode = ch0_mode;
    range = range0;
    pdata = data[sample+0];
    idata = &cap_buf[trig_point];
    qdata = rdata = payload;
    ch = 0;
  }
  for (int i = 0; i < SAMPLES; i++) {
    *idata = adc_linearlize(*idata);
    a = ((*idata + ch_off) * VREF[range]) / 4095;
    if (a > LCD_YMAX) a = LCD_YMAX;
    else if (a < 0) a = 0;
    if (ch_mode == MODE_INV)
      a = LCD_YMAX - a;
    *pdata++ = (byte) a;
    b = ((*idata++ + ch_off) * VREF[range]) / 120;
    if (b > 4095) b = 4095;
    else if (b < 0) b = 0;
    if (ch_mode == MODE_INV)
      b = 4095 - b;
    *qdata++ = (int16_t) b;
  }
  if (rate == 0) {
    mag(data[sample+ch], 10); // x10 magnification for display
  } else if (rate == 1) {
    mag(data[sample+ch], 5);  // x5 magnification for display
  }
  if (rate == 0) {
    mag(rdata, 10);           // x10 magnification for WEB
  } else if (rate == 1) {
    mag(rdata, 5);            // x5 magnification for WEB
  }
}

byte adRead(byte ch, byte mode, int off, int i)
{
  int16_t aa = adc1_get_raw((adc1_channel_t) ch);  // ADC read and save approx 46us
  aa = adc_linearlize(aa);
  long a = (((long)aa+off)*VREF[ch == ad_ch0 ? range0 : range1]+2048) >> 12;
  if (a > LCD_YMAX) a = LCD_YMAX;
  else if (a < 0) a = 0;
  if (mode == MODE_INV)
    a = LCD_YMAX - a;
  long b = (((long)aa+off)*VREF[ch == ad_ch0 ? range0 : range1] + 101) / 201;
  if (b > 4095) b = 4095;
  else if (b < 0) b = 0;
  if (mode == MODE_INV)
    b = 4095 - b;
  if (ch == ad_ch1) {
    cap_buf1[i] = aa;
    payload[i+SAMPLES] = b;
  } else {
    cap_buf[i] = aa;
    payload[i] = b;
  }
  return a;
}

int advalue(int value, long vref, byte mode, int offs) {
  if (mode == MODE_INV)
    value = LCD_YMAX - value;
  return ((long)value << 12) / vref - offs;
}

void set_trigger_ad() {
  if (trig_ch == ad_ch0) {
    trigger_ad = advalue(trig_lv, VREF[range0], ch0_mode, ch0_off);
  } else {
    trigger_ad = advalue(trig_lv, VREF[range1], ch1_mode, ch1_off);
  }
}

void loop() {
  int oad, ad;
  unsigned long auto_time;

  timeExec = 100;
  if (rate > RATE_DMA) {
    set_trigger_ad();
    auto_time = pow(10, rate / 3) + 5;
    if (trig_mode != TRIG_SCAN) {
      unsigned long st = millis();
      oad = adc1_get_raw((adc1_channel_t)trig_ch)&0xfffc;
      for (;;) {
        ad = adc1_get_raw((adc1_channel_t)trig_ch)&0xfffc;

        if (trig_edge == TRIG_E_UP) {
          if (ad > trigger_ad && trigger_ad > oad)
            break;
        } else {
          if (ad < trigger_ad && trigger_ad < oad)
            break;
        }
        oad = ad;

        if (rate > 9)
          CheckSW();      // no need for fast sampling
        if (trig_mode == TRIG_SCAN)
          break;
        if (trig_mode == TRIG_AUTO && (millis() - st) > auto_time)
          break; 
      }
    }
  }
  
  // sample and draw depending on the sampling rate
  if (rate < RATE_ROLL && Start) {
    // change the index for the double buffer
    if (sample == 0)
      sample = 2;
    else
      sample = 0;

    if (rate <= RATE_DMA) { // channel 0 only I2S DMA sampling (Max 500ksps)
      sample_i2s();
    } else if (rate == 6) { // channel 0 only 50us sampling
      sample_200us(50);
//    } else if (rate >= 7 && rate <= 8) {  // dual channel 100us, 200us sampling
//      sample_dual_us(HREF[rate] / 10);
    } else {                // dual channel .5ms, 1ms, 2ms, 5ms, 10ms, 20ms sampling
      sample_dual_us(US_DIV[rate] / DOTS_DIV);
//      sample_dual_ms(HREF[rate] / 10);
    }
    draw_screen();
  } else if (Start) { // 40ms - 400ms sampling
    timeExec = 5000;
    static const unsigned long r_[] PROGMEM = {40000, 80000, 200000, 400000};
    unsigned long r;
    int disp_leng;
    disp_leng = DISPLNG;
//    unsigned long st0 = millis();
    unsigned long st = micros();
    for (int i=0; i<disp_leng; i ++) {
      r = r_[rate - RATE_ROLL];  // rate may be changed in loop
      while((st - micros())<r) {
        CheckSW();
        if (rate<RATE_ROLL)
          break;
      }
      if (rate<RATE_ROLL) { // sampling rate has been changed
        display.fillScreen(BGCOLOR);
        break;
      }
      st += r;
      if (st - micros()>r)
          st = micros(); // sampling rate has been changed to shorter interval
      if (!Start) {
         i --;
         continue;
      }
      odat00 = odat01;      // save next previous data ch0
      odat10 = odat11;      // save next previous data ch1
      odat01 = data[0][i];  // save previous data ch0
      odat11 = data[1][i];  // save previous data ch1
      if (ch0_mode != MODE_OFF) data[0][i] = adRead(ad_ch0, ch0_mode, ch0_off, i);
      if (ch1_mode != MODE_OFF) data[1][i] = adRead(ad_ch1, ch1_mode, ch1_off, i);
      if (ch0_mode == MODE_OFF) payload[0] = -1;
      if (ch1_mode == MODE_OFF) payload[SAMPLES] = -1;
      xTaskNotify(taskHandle, 0, eNoAction);  // notify Websocket server task
      ClearAndDrawDot(i);
    }
    DrawGrid(disp_leng);  // right side grid   
    DrawText();
  }
  if (trig_mode == TRIG_ONE)
    Start = false;
  CheckSW();
  saveEEPROM();                         // save settings to EEPROM if necessary
  if (wdds != dds_mode) {
    if (wdds) {
      dds_setup();
    } else {
      dds_close();
    }
    dds_mode = wdds;
  }
}

void draw_screen() {
//  display.fillScreen(BGCOLOR);
  if (wfft != fft_mode) {
    fft_mode = wfft;
    display.fillScreen(BGCOLOR);
  }
  if (fft_mode) {
    DrawGrid();
    DrawText();
    plotFFT();
  } else {
    if ((ch0_active && (ch0_mode != MODE_ON)) || (ch1_active && (ch1_mode != MODE_ON))) {
      display.fillScreen(BGCOLOR); // Clear display if either channel turned off
    }
    DrawGrid();
    ClearAndDrawGraph();
    DrawText();
    if (ch0_mode == MODE_OFF) payload[0] = -1;
    if (ch1_mode == MODE_OFF) payload[SAMPLES] = -1;
  }
  xTaskNotify(taskHandle, 0, eNoAction);  // notify Websocket server task
  delay(10);    // wait Web task to send it (adhoc fix)
}

void display_freqduty(int ch) {
  int x;
  byte y, yduty;
  if (ch == 0) {
    yduty = 1;
    display.setTextColor(CH1COLOR, BGCOLOR);
  } else {
    yduty = YOFF + LCD_YMAX +10;
    display.setTextColor(CH2COLOR, BGCOLOR);
  }
  x = 200;
  y = yduty;
  TextBG(&y, x, 8);
  float freq = waveFreq[ch];
  if (freq < 999.5)
    display.print(freq);
  else if (freq < 999999.5)
    display.print(freq, 0);
  else {
    display.print(freq/1000.0, 0);
    display.print('k');
  }
  display.print("Hz");
//  if (fft_mode) return;
  x = 290;
  y = yduty;
  TextBG(&y, x, 6);
  display.print(waveDuty[ch], 1);  display.print('%');
}

void measure_voltage(int ch) {
  int x;
  byte y;
  if (fft_mode) return;
  float vavr = VRF * dataAve[ch] / 40950.0;
  float vmax = VRF * dataMax[ch] / 4095.0;
  float vmin = VRF * dataMin[ch] / 4095.0;
  x = MENU + 48, y = 42;  // Small
  if (ch == 0) {
    display.setTextColor(CH1COLOR, BGCOLOR);
  } else {
    y += 100;
    display.setTextColor(CH2COLOR, BGCOLOR);
  }
  TextBG(&y, x, 8);
  display.print("max");  display.print(vmax); if (vmax >= 0.0) display.print('V');
  TextBG(&y, x, 8);
  display.print("avr");  display.print(vavr); if (vavr >= 0.0) display.print('V');
  TextBG(&y, x, 8);
  display.print("min");  display.print(vmin); if (vmin >= 0.0) display.print('V');
}

void sample_dual_us(unsigned int r) { // dual channel. r > 67
  if (ch0_mode != MODE_OFF && ch1_mode == MODE_OFF) {
    unsigned long st = micros();
    for (int i=0; i<SAMPLES; i ++) {
      while(micros() - st < r) ;
      cap_buf[i] = adc1_get_raw((adc1_channel_t) ad_ch0);
      st += r;
    }
    scaleDataArray(ad_ch0, 0);
    memset(data[1], 0, SAMPLES);
  } else if (ch0_mode == MODE_OFF && ch1_mode != MODE_OFF) {
    unsigned long st = micros();
    for (int i=0; i<SAMPLES; i ++) {
      while(micros() - st < r) ;
      cap_buf1[i] = adc1_get_raw((adc1_channel_t) ad_ch1);
      st += r;
    }
    scaleDataArray(ad_ch1, 0);
    memset(data[0], 0, SAMPLES);
  } else {
    unsigned long st = micros();
    for (int i=0; i<SAMPLES; i ++) {
      while(micros() - st < r) ;
      cap_buf[i] = adc1_get_raw((adc1_channel_t) ad_ch0);
      cap_buf1[i] = adc1_get_raw((adc1_channel_t) ad_ch1);
      st += r;
    }
    scaleDataArray(ad_ch0, 0);
    scaleDataArray(ad_ch1, 0);
  }
}

void sample_dual_ms(unsigned int r) { // dual channel. r > 500
// .5ms, 1ms or 2ms sampling
  unsigned long st = micros();
  for (int i=0; i<SAMPLES; i ++) {
    while(micros() - st < r) ;
    st += r;
    if (ch0_mode != MODE_OFF) {
      cap_buf[i] = adc1_get_raw((adc1_channel_t) ad_ch0);
    }
    if (ch1_mode != MODE_OFF) {
      cap_buf1[i] = adc1_get_raw((adc1_channel_t) ad_ch1);
    }
  }
//  if (ch0_mode == MODE_OFF) memset(data[0], 0, SAMPLES);
//  if (ch1_mode == MODE_OFF) memset(data[1], 0, SAMPLES);
  scaleDataArray(ad_ch0, 0);
  scaleDataArray(ad_ch1, 0);
}

void sample_200us(unsigned int r) { // adc1_get_raw() with timing, channel 0 or 1. 1250us/div 20ksps
  uint16_t *idata;
  int ad_ch;
  if (ch0_mode == MODE_OFF && ch1_mode != MODE_OFF) {
    ad_ch = ad_ch1;
    idata = cap_buf1;
  } else {
    ad_ch = ad_ch0;
    idata = cap_buf;
  }
  unsigned long st = micros();
//  disableCore1WDT();
  for (int i=0; i<SAMPLES; i ++) {
    while(micros() - st < r) ;
    *idata++ = adc1_get_raw((adc1_channel_t) ad_ch);
    st += r;
//    yield();
//    esp_task_wdt_reset();
  }
//  enableCore1WDT();
  delay(1);
  scaleDataArray(ad_ch, 0);
  delay(1);
}

void plotFFT() {
  byte *lastplot, *newplot;
  int y = YOFF + LCD_YMAX;

  int clear = (sample == 0) ? 2 : 0;
  for (int i = 0; i < FFT_N; i++) {
    vReal[i] = cap_buf[i];
    vImag[i] = 0.0;
  }
  FFT.dcRemoval();
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);  // Weigh data
  FFT.compute(FFTDirection::Forward);                     // Compute FFT
  FFT.complexToMagnitude();                               // Compute magnitudes
  newplot = data[sample];
  lastplot = data[clear];
  payload[0] = 0;
  for (int i = 1; i < FFT_N/2; i++) {
    float mag = vReal[i];
    payload[i] = constrain((int)(mag / 50.0), 0, 4095); // Normalize magnitude (/2048) & scale to web canvas (*4096/100)
    int dat = constrain((int)(mag / 1706.7), 0, LCD_YMAX); // Normalize magnitude (/2048) & scale to LCD (*LCD_YMAX/100)
    display.drawFastVLine(i, y - lastplot[i], lastplot[i], BGCOLOR); // erase old
    display.drawFastVLine(i, y - dat, dat, CH1COLOR);
    newplot[i] = dat;
  }
  draw_scale();
}

void draw_scale() {
  int ylim = BOTTOM_LINE + 4;
  float fhref, nyquist, binfreqbase, binfreq;
  display.setTextColor(TXTCOLOR);
  fhref = freqhref();
  nyquist = 5.0e6 / fhref; // Nyquist frequency
  long inyquist = nyquist;
  payload[FFT_N/2] = (short) (inyquist / 1000);
  payload[FFT_N/2+1] = (short) (inyquist % 1000);
  clear_bottom_text();
  binfreqbase = DOTS_DIV * 1000000.0 / FFT_N / US_DIV[rate];
  for (int i=0; i<FFT_N/2; i+=30) {
    binfreq = i * binfreqbase;
    if (binfreq < 1000.0) {
      display.setCursor(XOFF + i, ylim); display.print(binfreq,0);
    } else {
      binfreq = binfreq / 1000.0;
      display.setCursor(XOFF + i, ylim); display.print(binfreq,1); display.print("k");
    }
  }
  for (int j = 0; j <= 100; j += 25) {
    display.setCursor(XOFF + FFT_N/2, YOFF + (j * LCD_YMAX / 100));
    display.print(100 - j); display.print("%");
  }
}

float freqhref() {
  return (float) HREF[rate];
}

#ifdef EEPROM_START
void saveEEPROM() {                   // Save the setting value in EEPROM after waiting a while after the button operation.
  int p = EEPROM_START;
  if (saveTimer > 0) {                // If the timer value is positive
    saveTimer = saveTimer - timeExec; // Timer subtraction
    if (saveTimer <= 0) {             // if time up
      EEPROM.write(p++, range0);      // save current status to EEPROM
      EEPROM.write(p++, ch0_mode);
      EEPROM.write(p++, lowByte(ch0_off));  // save as Little endian
      EEPROM.write(p++, highByte(ch0_off));
      EEPROM.write(p++, range1);
      EEPROM.write(p++, ch1_mode);
      EEPROM.write(p++, lowByte(ch1_off));  // save as Little endian
      EEPROM.write(p++, highByte(ch1_off));
      EEPROM.write(p++, rate);
      EEPROM.write(p++, trig_mode);
      EEPROM.write(p++, trig_lv);
      EEPROM.write(p++, trig_edge);
      EEPROM.write(p++, trig_ch);
      EEPROM.write(p++, fft_mode);
      EEPROM.write(p++, info_mode);
      EEPROM.write(p++, item);
      EEPROM.write(p++, pulse_mode);
      EEPROM.write(p++, duty);
      EEPROM.write(p++, p_range);
      EEPROM.write(p++, lowByte(count));  // save as Little endian
      EEPROM.write(p++, highByte(count));
      EEPROM.write(p++, dds_mode);
      EEPROM.write(p++, wave_id);
      EEPROM.write(p++, ifreq & 0xff);
      EEPROM.write(p++, (ifreq >> 8) & 0xff);
      EEPROM.write(p++, (ifreq >> 16) & 0xff);
      EEPROM.write(p++, (ifreq >> 24) & 0xff);
      EEPROM.write(p++, dac_cw_mode);
      EEPROM.commit();    // actually write EEPROM. Necessary for ESP32
    }
  }
}
#endif

void set_default() {
  range0 = RANGE_MIN;
  ch0_mode = MODE_ON;
  ch0_off = 0;
  range1 = RANGE_MIN;
  ch1_mode = MODE_ON;
  ch1_off = 2048;
  rate = 7;
  trig_mode = TRIG_AUTO;
  trig_lv = 20;
  trig_edge = TRIG_E_UP;
  trig_ch = ad_ch0;
  fft_mode = false;
  info_mode = 3;  // display frequency and duty.  Voltage display is off
  item = 0;       // menu item
  pulse_mode = true;
  duty = 128;     // PWM 50%
  p_range = 16;   // PWM range
  count = 256;    // PWM 1220Hz
  dds_mode = false;
  wave_id = 0;    // sine wave
  ifreq = 23841;  // 238.41Hz
  dac_cw_mode = false;
}

extern const byte wave_num;

#ifdef EEPROM_START
void loadEEPROM() { // Read setting values from EEPROM (abnormal values will be corrected to default)
  int p = EEPROM_START, error = 0;

  range0 = EEPROM.read(p++);                // range0
  if ((range0 < RANGE_MIN) || (range0 > RANGE_MAX)) ++error;
  ch0_mode = EEPROM.read(p++);              // ch0_mode
  if (ch0_mode > 2) ++error;
  *((byte *)&ch0_off) = EEPROM.read(p++);     // ch0_off low
  *((byte *)&ch0_off + 1) = EEPROM.read(p++); // ch0_off high
  if ((ch0_off < -8192) || (ch0_off > 8191)) ++error;

  range1 = EEPROM.read(p++);                // range1
  if ((range1 < RANGE_MIN) || (range1 > RANGE_MAX)) ++error;
  ch1_mode = EEPROM.read(p++);              // ch1_mode
  if (ch1_mode > 2) ++error;
  *((byte *)&ch1_off) = EEPROM.read(p++);     // ch1_off low
  *((byte *)&ch1_off + 1) = EEPROM.read(p++); // ch1_off high
  if ((ch1_off < -8192) || (ch1_off > 8191)) ++error;

  rate = EEPROM.read(p++);                  // rate
  if ((rate < RATE_MIN) || (rate > RATE_MAX)) ++error;
//  if (ch0_mode == MODE_OFF && rate < 5) ++error;  // correct ch0_mode
  trig_mode = EEPROM.read(p++);             // trig_mode
  if (trig_mode > TRIG_SCAN) ++error;
  trig_lv = EEPROM.read(p++);               // trig_lv
  if (trig_lv > LCD_YMAX) ++error;
  trig_edge = EEPROM.read(p++);             // trig_edge
  if (trig_edge > 1) ++error;
  trig_ch = EEPROM.read(p++);               // trig_ch
  if (trig_ch != ad_ch0 && trig_ch != ad_ch1) ++error;
  fft_mode = EEPROM.read(p++);              // fft_mode
  info_mode = EEPROM.read(p++);             // info_mode
  if (info_mode > 63) ++error;
  item = EEPROM.read(p++);                  // item
  if (item > ITEM_MAX) ++error;
  pulse_mode = EEPROM.read(p++);            // pulse_mode
  duty = EEPROM.read(p++);                  // duty
  p_range = EEPROM.read(p++);               // p_range
  if (p_range > 16) ++error;
  *((byte *)&count) = EEPROM.read(p++);     // count low
  *((byte *)&count + 1) = EEPROM.read(p++); // count high
  if (count > 256) ++error;
  dds_mode = EEPROM.read(p++);              // DDS wave id
  wave_id = EEPROM.read(p++);               // DDS wave id
  if (wave_id >= wave_num) ++error;
  *((byte *)&ifreq) = EEPROM.read(p++);     // ifreq low
  *((byte *)&ifreq + 1) = EEPROM.read(p++); // ifreq
  *((byte *)&ifreq + 2) = EEPROM.read(p++); // ifreq
  *((byte *)&ifreq + 3) = EEPROM.read(p++); // ifreq high
  if (ifreq > 99999L) ++error;
  dac_cw_mode = EEPROM.read(p++);              // DDS wave id
  if (error > 0)
    set_default();
}
#endif

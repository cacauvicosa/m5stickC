/* ESP8266/32 Audio Spectrum Analyser on an SSD1306/SH1106 Display
 * The MIT License (MIT) Copyright (c) 2017 by David Bird. 
 * The formulation and display of an AUdio Spectrum using an ESp8266 or ESP32 and SSD1306 or SH1106 OLED Display using a Fast Fourier Transform
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files 
 * (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, 
 * publish, distribute, but not to use it commercially for profit making or to sub-license and/or to sell copies of the Software or to 
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:  
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 * See more at http://dsbird.org.uk 
*/
// M5StickC Audio Spectrum             : 2019.06.01 : macsbug
//  https://macsbug.wordpress.com/2019/06/01/
// Audio Spectrum Display with M5STACK : 2017.12.31 : macsbug
//  https://macsbug.wordpress.com/2017/12/31/audio-spectrum-display-with-m5stack/
// https://github.com/tobozo/ESP32-8-Octave-Audio-Spectrum-Display/tree/wrover-kit
// https://github.com/G6EJD/ESP32-8266-Audio-Spectrum-Display
// https://github.com/kosme/arduinoFFT
 
#include "arduinoFFT.h"
arduinoFFT FFT = arduinoFFT();  
#include <M5StickC.h>
#include <driver/i2s.h>
#pragma GCC optimize ("O3")
#define PIN_CLK  0
#define PIN_DATA 34
#define READ_LEN (2 * 1024)
uint8_t BUFFER[READ_LEN] = {0};
uint16_t oldx[160];
uint16_t oldy[160];
uint16_t *adcBuffer = NULL; // uint16_t *adcBuffer = NULL;
#define SAMPLES 512         // Must be a power of 2
#define SAMPLING_FREQUENCY 40000
struct eqBand {
  const char *freqname;
  uint16_t amplitude;
  int peak;
  int lastpeak;
  uint16_t lastval;
  unsigned long lastmeasured;
};
 
eqBand audiospectrum[8] = {
  // Adjust the amplitude values to fit your microphone
  // freqname,amplitude,peak,lastpeak,lastval,lastmeasured
  {  ".1",1000, 0, 0, 0, 0},
  {  ".2", 500, 0, 0, 0, 0},
  {  ".5", 300, 0, 0, 0, 0},
  {  "1" , 250, 0, 0, 0, 0},
  {  "2" , 200, 0, 0, 0, 0},
  {  "4" , 100, 0, 0, 0, 0},
  {  "8" ,  50, 0, 0, 0, 0},
  { "16" ,  50, 0, 0, 0, 0}
};
 
unsigned int sampling_period_us;
unsigned long microseconds;
double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned long newTime, oldTime;
uint16_t tft_width  = 160;
uint16_t tft_height =  80;
uint8_t bands = 8;
uint8_t bands_width = floor( tft_width / bands );
uint8_t bands_pad = bands_width - 10;
uint16_t colormap[255];//color palette for the band meter(pre-fill in setup)
 
void i2sInit(){
   i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate =  44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, //is fixed at 12bit,stereo,MSB
    .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 2,
    .dma_buf_len = 128,
   };
   i2s_pin_config_t pin_config;
   pin_config.bck_io_num   = I2S_PIN_NO_CHANGE;
   pin_config.ws_io_num    = PIN_CLK;
   pin_config.data_out_num = I2S_PIN_NO_CHANGE;
   pin_config.data_in_num  = PIN_DATA; 
   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
   i2s_set_pin(I2S_NUM_0, &pin_config);
   i2s_set_clk(I2S_NUM_0, 44100,I2S_BITS_PER_SAMPLE_16BIT,I2S_CHANNEL_MONO);
}
 
void mic_record_task (void* arg){      
  while(1){
    i2s_read_bytes(I2S_NUM_0,(char*)BUFFER,READ_LEN,(100/portTICK_RATE_MS));
    adcBuffer = (uint16_t *)BUFFER;
    showSignal();
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}
 
void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.setTextSize(1);
   
  i2sInit();
  xTaskCreatePinnedToCore(mic_record_task,"mic_record_task",2048,NULL,1,NULL,1);
 
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQUENCY));
  delay(2000);
  for(uint8_t i=0;i<tft_height;i++) {
  //colormap[i] = M5.Lcd.color565(tft_height-i*.5,i*1.1,0); //RGB 
    colormap[i] = M5.Lcd.color565(tft_height-i*4.4,i*2.5,0);//RGB:rev macsbug
  }
  for (byte band = 0; band <= 7; band++) {
    M5.Lcd.setCursor(bands_width*band + 2, 0);
    M5.Lcd.print(audiospectrum[band].freqname);
  }
}
 
void showSignal(){
  for (int i = 0; i < SAMPLES; i++) {
    newTime  = micros() - oldTime;
    oldTime  = newTime;
    vReal[i] = adcBuffer[i];
    vImag[i] = 0;
    while (micros() < (newTime + sampling_period_us)){//do nothing to wait
    }
  }
   
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
 
  for (int i = 2; i < (SAMPLES/2); i++){ 
    // Don't use sample 0 and only first SAMPLES/2 are usable. 
    // Each array eleement represents a frequency and its value the amplitude.
    if (vReal[i] > 1500) { // Add a crude noise filter, 10 x amplitude or more
      byte bandNum = getBand(i);
      if(bandNum!=8) {
        displayBand(bandNum, (int)vReal[i]/audiospectrum[bandNum].amplitude);
      }
    }
  }
  long vnow = millis();
  for (byte band = 0; band <= 7; band++) {
    // auto decay every 50ms on low activity bands
    if(vnow - audiospectrum[band].lastmeasured > 50) {
      displayBand(band, audiospectrum[band].lastval>4 ? audiospectrum[band].lastval-4 : 0);
    }
    if (audiospectrum[band].peak > 0) {
      audiospectrum[band].peak -= 2;
      if(audiospectrum[band].peak<=0) {
        audiospectrum[band].peak = 0;
      }
    }
    // only draw if peak changed
    if(audiospectrum[band].lastpeak != audiospectrum[band].peak) {
      // delete last peak
     M5.Lcd.drawFastHLine(bands_width*band,tft_height-audiospectrum[band].lastpeak,bands_pad,BLACK);
     audiospectrum[band].lastpeak = audiospectrum[band].peak;
     M5.Lcd.drawFastHLine(bands_width*band, tft_height-audiospectrum[band].peak,
                          bands_pad, colormap[tft_height-audiospectrum[band].peak]);
    }
  } 
}
 
void displayBand(int band, int dsize){
  uint16_t hpos = bands_width*band;
  int dmax = 200;
  if(dsize>tft_height-10) {
    dsize = tft_height-10; // leave some hspace for text
  }
  if(dsize < audiospectrum[band].lastval) {
    // lower value, delete some lines
    M5.Lcd.fillRect(hpos, tft_height-audiospectrum[band].lastval,
                    bands_pad, audiospectrum[band].lastval - dsize,BLACK);
  }
  if (dsize > dmax) dsize = dmax;
  for (int s = 0; s <= dsize; s=s+4){
    M5.Lcd.drawFastHLine(hpos, tft_height-s, bands_pad, colormap[tft_height-s]);
  }
  if (dsize > audiospectrum[band].peak){audiospectrum[band].peak = dsize;}
  audiospectrum[band].lastval = dsize;
  audiospectrum[band].lastmeasured = millis();
}
 
byte getBand(int i) {
  if (i<=2 )             return 0;  // 125Hz
  if (i >3   && i<=5 )   return 1;  // 250Hz
  if (i >5   && i<=7 )   return 2;  // 500Hz
  if (i >7   && i<=15 )  return 3;  // 1000Hz
  if (i >15  && i<=30 )  return 4;  // 2000Hz
  if (i >30  && i<=53 )  return 5;  // 4000Hz
  if (i >53  && i<=200 ) return 6;  // 8000Hz
  if (i >200           ) return 7;  // 16000Hz
  return 8;
}
 
void loop() {}

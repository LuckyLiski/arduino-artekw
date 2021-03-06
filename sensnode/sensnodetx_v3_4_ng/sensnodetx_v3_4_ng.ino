/*
SensnodeTX v3.4-dev
Written by Artur Wronowski <artur.wronowski@digi-led.pl>
Works with optiboot too.
Need Arduino 1.0 to compile

TODO:
- pomiar napiecia bateri, skalowanie czasu pomiedzy pomiarami w zaleznosci od panujacego napiecia
- srednia z pomiarow
- wiartomierz //#define ObwAnem 0.25434 // meters
- http://hacking.majenko.co.uk/node/57
*/

// libs for I2C and DS18B20
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// lisb for SHT21 and BMP085
#include <BMP085.h>
#include <SHT2x.h>
//#include <Adafruit_BMP085.h>
// lib for RFM12B from https://github.com/jcw/jeelib
#include <JeeLib.h>
// avr sleep instructions
#include <avr/sleep.h>
// DHT11
#include "DHT.h"

#if DHT11
  DHT dht(4, DHT11);
#endif

/*************************************************************/

#define myNodeID            15
#define network             210
#define NEW_REV             1 // 3.0 or 3.4

// Settings
//#define SMOOTH              3 // smoothing factor used for running averages
#define PERIOD              1 // minutes
#define RADIO_SYNC_MODE     2 // http://jeelabs.net/pub/docs/jeelib/RF12_8h.html#a6843bbc70df373dbffa0b3d1f33ef0ae

// Used devices or buses (1 on 0)
#define LDR                 0 // use LDR sensor
#define DS18B20             1 // use 1WIE DS18B20
#define I2C                 0 // use i2c bus for BMP085 and SHT21
#define DEBUG               1 // debug mode - serial output
#define LED_ON              0 // use act led for transmission
#define DHT11               1 // DHT11

/*************************************************************/

// Input/Output definition
// Analog
#if NEW_REV //3.4
  #define LDR               2
  #define BAT_VOL           3
#else //3.0
  #define LDR               0
  #define BAT_VOL           1
  #define CustomA3          3
#endif

// Digital
#if NEW_REV //3.4
  #define MOSFET            7
  #define ONEWIRE_DATA      8
  #define ACT_LED           9
#else //3.0
  #define CustomD3          3
  #define CustomD4          4
  #define MOSFET            5
  #define ONEWIRE_DATA      8
  #define ACT_LED           9
#endif

/************************************************************/

// structure of received data

typedef struct {
  int light;
  int humi;
  int temp;
  int pressure;
  byte lobat      :1;
  int battvol;
} Payload;
Payload measure;

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

#if NEW_REV
  //Port p1 (1); // JeeLabs Port P1
  Port p2 (2); // JeeLabs Port P2
  Port ldr (3);  // Analog pin 2
  Port batvol (4); // Analog pin 3
#endif



byte count = 0;
int tempReading;

#if DS18B20
    OneWire oneWire(ONEWIRE_DATA);
    DallasTemperature sensors(&oneWire);
#endif

void setup()
{
    rf12_initialize(myNodeID, RF12_433MHZ, network);
    rf12_control(0xC040); // 2.2v low

#if I2C
    Wire.begin();
#endif

#if DEBUG
    Serial.begin(115200);
#endif

#if ONEWIRE
    sensors.begin();
#endif

}

void loop()
{
  doMeasure(); // mierz
  #if DEBUG
     transmissionRS();
  #endif
  #if LED_ON
    activityLed(1);
  #endif
  doReport(); // wyslij
  #if LED_ON
    activityLed(0);
  #endif

  for (byte t = 0; t < PERIOD; t++)  // spij
    Sleepy::loseSomeTime(60000);
}

static void doReport()
{
  rf12_sleep(RF12_SLEEP);
  while (!rf12_canSend())
    rf12_recvDone();
  rf12_sendStart(0, &measure, sizeof measure);
  rf12_sendWait(RADIO_SYNC_MODE);
  rf12_sleep(RF12_SLEEP);
}

static void transmissionRS()
{
  activityLed(1);
  Serial.println(' ');
  Serial.print("LIGHT ");
  Serial.println(measure.light);
  Serial.print("HUMI ");
  Serial.println(measure.humi);
  Serial.print("TEMP ");
  Serial.println(measure.temp);
  Serial.print("PRES ");
  Serial.println(measure.pressure);
  Serial.print("LOBAT " );
  Serial.println(measure.lobat, DEC);
  Serial.print("BATVOL ");
  Serial.println(measure.battvol);
  /*
  Serial.print("VCCREF ");
  Serial.println(readVcc());
  */
  activityLed(0);
}

static void activityLed (byte on) {
  pinMode(ACT_LED, OUTPUT);
  digitalWrite(ACT_LED, on);
  delay(150);
}
/*
 long readVcc() {
   long result;
   // Read 1.1V reference against Vcc
   ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
   delay(2); // Wait for Vref to settle
   ADCSRA |= _BV(ADSC); // Convert
   while (bit_is_set(ADCSRA,ADSC));
   result = ADCL;
   result |= ADCH<<8;
   result = 1126400L / result; // Back-calculate Vcc in mV
   return result;
   ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
}
*/
static void doMeasure() {
  count++;
  tempReading = 0;

#if NEW_REV
  for (byte t = 0;t < 3; t++) {
    tempReading += batvol.anaRead();
    Sleepy::loseSomeTime(50);
  }
  tempReading = tempReading / 3;
  measure.battvol = map(tempReading,0,1023,0,6600);
#else
  for (byte t = 0;t < 3; t++) {
    tempReading += analogRead(BAT_VOL);
    Sleepy::loseSomeTime(50);
  }
  tempReading = tempReading / 3;
  measure.battvol = map(tempReading,0,1023,0,6600);
#endif

//  measure.battvol = readVcc();
  Serial.print(".");
  //measure.nodeid = myNodeID;
  measure.lobat = rf12_lowbat();

#if LDR
  if ((count % 2) == 0) {
     measure.light = ldr.anaRead();
  }
  /*
    def get_light_level(self):
    result = self.adc.readADC(self.adcPin) + 1
    vout = float(result)/1023 * 3.3
    rs = ((3.3 - vout) / vout) * 5.6
    return abs(rs)
    */
#endif

#if I2C
  float shthumi = SHT2x.GetHumidity();
  float shttemp = SHT2x.GetTemperature();
  measure.humi = shthumi * 10;
  measure.temp = shttemp * 10;
  Sleepy::loseSomeTime(250);
  BMP085.getCalData();
  BMP085.readSensor();
  measure.pressure = (BMP085.press*10*10) + 16;
#endif

#if DS18B20
  sensors.requestTemperatures();
  Sleepy::loseSomeTime(750);
  float tmp = sensors.getTempCByIndex(0);
  measure.temp = tmp * 10;
#endif

#if DHT11
  float h = dht.readHumidity();
  measure.humi = h;
#endif
}


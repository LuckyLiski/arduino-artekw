static void doMeasure() {
  solar(getLDR());
#if I2C
    SHT21.readSensor();
    delay(50);
    BMP085.readSensor();
#endif
#if I2C
    getSHT21('h');
    getSHT21('t');
    getBMP085();
#endif
  pomiar.lobat = rf12_lowbat();
  getLDR();
  getBatVol();
  getSolVol();
  ++pomiar.seq;
}

int getBatVol()
{
  byte state = pinState(MOSFET_SOL);
  if (!pinState(MOSFET_SOL)) 
  {
     mosfet(MOSFET_SOL, 0);
  }
  int BatteryVal = analogRead(BatteryPin);
  pomiar.battvol = map((BatteryVal), 0, 1023, 0, 660);
  mosfet(MOSFET_SOL, state);
  return pomiar.battvol;
}

int getSolVol()
{
  byte state = pinState(MOSFET_BAT);
  if (!pinState(MOSFET_BAT)) 
  {
     mosfet(MOSFET_BAT, 0);
  }

  int SolarVal = analogRead(SolarPin);
  pomiar.solvol = map((SolarVal), 0, 1023, 0, 660);
  mosfet(MOSFET_BAT, state);
  return pomiar.solvol;
}

float getSHT21(char opt)
{
  if (opt == 'h' )
  {
    pomiar.humi = SHT21.humi;
  }
  else if (opt == 't')
  {
    pomiar.temp = SHT21.temp;
  }  
  return pomiar.humi || pomiar.temp;
}

float getBMP085()
{
  pomiar.pressure = (BMP085.press*10) + 16;
  return pomiar.pressure;
}

int getLDR()
{
  int LDRVal = analogRead(LDRPin);
  LDRVal = 1023 - LDRVal;
  pomiar.light = map((LDRVal), 0, 1023, 0, 255);
  return pomiar.light;
}  

float getWind()
{
  //
}

float getTemp()
{
  //
}

static void doReport()
{
  for (byte i = 0; i < RETRY_ACK; ++i) {
    rf12_sleep(-1);
    while (!rf12_canSend())
      rf12_recvDone();
    rf12_sendStart(RF12_HDR_ACK, &pomiar, sizeof pomiar);
    rf12_sendWait(RADIO_SYNC_MODE);
    byte acked = waitForACK();
    rf12_sleep(0);
    
    if (acked) {
      #if DEBUG
        Serial.print("ACK! ");
        Serial.println((int) i);
        Serial.println("-------");
        delay(2);
      #endif
      scheduler.timer(MEASURE, MEASURE_PERIOD);
      return;
    }
    Sleepy::loseSomeTime(RETRY_PERIOD * 100);
  }
  scheduler.timer(MEASURE, MEASURE_PERIOD);
  #if DEBUG
    Serial.print("no ACK! ");
    Serial.println("-------");
    delay(2);
  #endif
}

static void doReport2()
{
  rf12_sleep(-1);
  while (!rf12_canSend())
    rf12_recvDone();
  rf12_sendStart(0, &pomiar, sizeof pomiar);
  rf12_sendWait(RADIO_SYNC_MODE);
  rf12_sleep(0);
}

static byte waitForACK() {
  MilliTimer t;
  while (!t.poll(ACK_TIME)) {
    if (rf12_recvDone() && rf12_crc == 0
        && rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | NODEID))
      return 1;
     set_sleep_mode(SLEEP_MODE_IDLE);
     sleep_mode();
  }
  return 0;
}
      
static void transmissionRS()
{
  activityLed(1);
  Serial.print("LIGHT ");
  Serial.println(pomiar.light);
  Serial.print("HUMI ");
  Serial.println(pomiar.humi);
  Serial.print("TEMP ");
  Serial.println(pomiar.temp);
  Serial.print("PRES ");
  Serial.println(pomiar.pressure);
  Serial.print("SPD ");
  Serial.println(pomiar.wind);
  Serial.print("LOBAT " );
  Serial.println(pomiar.lobat, DEC);
  Serial.print("BATVOL ");
  Serial.println(pomiar.battvol);
  Serial.print("SOLVOL ");
  Serial.println(pomiar.solvol);
  Serial.print("MSOL ");
  Serial.println(pinState(MOSFET_SOL), DEC);
  Serial.print("MBAT ");
  Serial.println(pinState(MOSFET_BAT), DEC);
  activityLed(0);
}

static void activityLed (byte on) {
  pinMode(ACT_LED, OUTPUT);
  digitalWrite(ACT_LED, on);
  delay(150);
}

static void mosfet(byte fet, byte on) {
  pinMode(fet, OUTPUT);
  digitalWrite(fet, !on);
  delay(100);
}

static byte solar(int ldr)
{
  byte tmp;
  if (ldr > SOLAR) {
    mosfet(MOSFET_SOL, 1);
    mosfet(MOSFET_BAT, 1);
    tmp = 1;
  }
  else
  {
    mosfet(MOSFET_SOL, 0);
    mosfet(MOSFET_BAT, 0);
    tmp = 0;
  }
  return (pomiar.solar = tmp, pomiar.bat = tmp);
}

static byte pinState(int pin)
{
  return digitalRead(pin) ? 1 : 0;
}

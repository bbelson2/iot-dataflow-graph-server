/*
   IoT Dataflow Graph Server
   Copyright (c) 2015, Adam Rehn, Jason Holdsworth
   
   Clock display sink
   Copyright (c) 2018, Bruce Belson

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

/*
The following videos are useful wrt the 7735 display.

1) https://www.youtube.com/watch?v=2xsL6JSwlS0
2) https://www.youtube.com/watch?v=HKW_tlUD-kw

refs:
https://wiki.wemos.cc/products:d1:d1_mini
https://en.wikipedia.org/wiki/Serial_Peripheral_Interface

Wiring:

ST7735    ESP8266   NodeMCU 12E
Reset     D0        GPIO16
CS        D1        GPIO5
DC RS     D2        GPIO4
DIN SDI   HMOSI/D7  GPIO13
CLK       HCLK/D5   GPIO14
VCC       Vin       Vin (4.5v)
LED       3V3       V3V
GND       GND       0in
*/

/*
Software

https://github.com/adafruit/Adafruit-ST7735-Library
https://learn.adafruit.com/1-8-tft-display/graphics-library
https://learn.adafruit.com/adafruit-gfx-graphics-library
*/

#define USE_NETWORK

#ifdef USE_NETWORK
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <ESP8266MulticastUDP.h>

//The identifier for this node
const String& NODE_IDENTIFIER = "clock-with-display-sink";

//The interval (in milliseconds) at which input is read
#define READ_INTERVAL 100

// Setup the ESP8266 Multicast UDP object as a sink
ESP8266MulticastUDP multicast("iot-dataflow", "it-at-jcu",
  IPAddress(224, 0, 0, 115), 9090);

#define ERROR_PIN D8

void initComms()
{
  // Use the Red LED to say 'hello'
  for (int i = 0; i < 3; i++) {
    digitalWrite(ERROR_PIN, HIGH);
    delay(500);
    digitalWrite(ERROR_PIN, LOW);
    delay(500);
  }
  
  // Attempt to connect to the WiFi network
  Serial.println("");
  Serial.println("Beginning multicast");
  multicast.begin();
  Serial.print(NODE_IDENTIFIER);
  if (multicast.isConnected()) {
    multicast.join();
    Serial.println(" connected to WiFi and joined Multicast group");
  } else {
    Serial.println(" error: failed to connect to WiFi network!");
    digitalWrite(ERROR_PIN, HIGH);
  }  
}
void  processComms() {
  if (multicast.available()) {
    Serial.println("Reading from WiFi network!");
    DataPacket packet = multicast.read();
    String message = packet.data;
    Serial.println("Got message from WiFi network!");

    if (message.startsWith(NODE_IDENTIFIER)) {
      performTask(message.substring(NODE_IDENTIFIER.length() + 1));
    }
  }
}
#else
void initComms() {}
void  processComms() {}
#endif // USE_NETWORK

// To do

// 1) Encapsulate the timer
// 2) Radically simplify the logic of the timer-ticks conversion 
//    (convert hhmmss => ticks and memorise offset)
// 3) Use a better font at this size

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
//#include <Fonts\FreeMono24pt7b.h>

#define TFT_RST D0
#define TFT_CS  D1
#define TFT_DC  D2

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// These are the reverse of the constants at Adafruit's documentation 
// because of big- vs small-endian defaults
const uint16_t grey = 0x5AEB;
const uint16_t white = 0xFFFF;
const uint16_t blue = 0xF800; 

/*
 * Clock display constants and helpers
 */

class DigitalClockDisplay {
public:
  DigitalClockDisplay(Adafruit_ST7735) : m_tft(tft) {
    // Default property values
    m_foreColor = grey;
    m_backColor = white;
    m_xClock = 8;
    m_yClock = 50;
    m_font = 0; //&FreeMono24pt7b;

    // Cached values
    m_first = true;
    m_hh1 = 0;
    m_mm1 = 0;
    m_ss1 = 0;
    m_uiMode1 = 4; // out of range
  }

  void begin(uint16_t x, uint16_t y, uint16_t foreColor, uint16_t backColor) {
    m_xClock = x;
    m_yClock = y;
    m_foreColor = foreColor;
    m_backColor = backColor;
  }
  
  // Properties
  uint16_t getBackColor() const { return m_backColor; }
  void setBackColor(uint16_t color) { m_backColor = color; }
  uint16_t getForeColor() const { return m_foreColor; }
  void setForeColor(uint16_t color) { m_foreColor = color; }
  uint16_t getClockX() const { return m_xClock; }
  void setClockX(uint16_t x) { m_xClock = x; }
  uint16_t getClockY() const { return m_yClock; }
  void setClockY(uint16_t y) { m_yClock = y; }

  uint16_t getClockCY() const { return mc_cyClock; }

  // Operations
  void updateTime(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t uiMode) {

    m_tft.setFont(m_font);
    m_tft.setTextSize(mc_clockTextSize);
    m_tft.setTextColor(m_foreColor);
    
    // 1st time only
    if (m_first) {
      printColon(getPartX(1), m_yClock, mc_clockTextSize, m_foreColor);
      printColon(getPartX(3), m_yClock, mc_clockTextSize, m_foreColor);
    }
    // If part has changed, print it
    if (m_first || (hh != m_hh1)) { printTimePart(hh, 0); }
    if (m_first || (mm != m_mm1)) { printTimePart(mm, 2); }
    if (m_first || (ss != m_ss1)) { printTimePart(ss, 4); }
    
    // Update cursor
    if (uiMode != m_uiMode1) {
      if (m_uiMode1 < 4 && m_uiMode1 > 0) {
        printUnderline((m_uiMode1 - 1) * 2, false);
      }
      if (uiMode < 4 && uiMode > 0) {
        printUnderline((uiMode - 1) * 2, true);
      }
    }

    // Retain cached values
    m_hh1 = hh; m_mm1 = mm; m_ss1 = ss; m_uiMode1 = uiMode; m_first = false;
    
    // Restore default font
    m_tft.setFont();
  }

public:
  uint16_t getPartX(uint8_t part) const {
    //assert(part >= 0 && part <= 4);
    return m_xClock + ((part / 2) * (mc_cxSep + mc_cxCharPair)) + ((part % 2) * mc_cxCharPair);
  }
  uint16_t getPartCX(uint8_t part) {
    //assert(part >= 0 && part <= 4);
    return ((part % 2) == 0) ? mc_cxCharPair : mc_cxSep;
  }
  static void intToCharPair(uint32_t n, char* pch) {
    pch[0] = '0' + (n / 10);
    pch[1] = '0' + (n % 10);
  }
  
public:
  void printColon(uint8_t x, uint8_t y, uint8_t sz, uint16_t color) {
    m_tft.fillRect(x - (sz/2), y + (2*sz), sz, sz, color);
    m_tft.fillRect(x - (sz/2), y + (4*sz), sz, sz, color);
  }
  void printTimePart(uint8_t value, uint8_t partNum) {
    char buf[3];
    buf[2] = 0;
    intToCharPair(value, buf);
    
    uint8_t x = getPartX(partNum);
    uint8_t cx = getPartCX(partNum);
    m_tft.fillRect(x, m_yClock, cx, mc_cyClock, m_backColor); 
    m_tft.setCursor(x, m_yClock);
    m_tft.print(buf);
  }
  
  void printUnderline(uint8_t partNum, bool paint) {
    uint16_t color = paint ? m_foreColor : m_backColor;
    uint8_t x = getPartX(partNum);
    uint8_t cx = getPartCX(partNum) - mc_cxSep;
    uint8_t y = m_yClock + mc_cyClock + 2;
    m_tft.drawLine(x, y, x + cx, y, color);
  }
 
protected:
  const uint8_t mc_clockTextSize = 3;
  const uint16_t mc_cyClock = 22;
  const uint16_t mc_cxCharPair = 36;
  const uint16_t mc_cxSep = 5;
  
private:
  Adafruit_ST7735 m_tft;
  
  // Properties
  uint16_t m_xClock, m_yClock;
  uint16_t m_foreColor, m_backColor;
  const GFXfont* m_font; 

  // Cached values, to minimise display updates
  bool m_first = true;
  uint8_t m_hh1 = 0, m_mm1 = 0, m_ss1 = 0;
  uint8_t m_uiMode1 = 4; // out of range
};

DigitalClockDisplay digitalClock = DigitalClockDisplay(tft);

void initScreen() {
  tft.initR(INITR_BLACKTAB);

  tft.setTextWrap(false);
  tft.fillScreen(white);
  tft.setTextColor(grey);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("-IT @ JCU-");
  tft.setTextSize(1);
  tft.setCursor(10, 30);
  tft.println("-- Welcome to JCU --");
}

#define SWITCH1_PIN D3  // Only D3 and D4 are pullups on the Wemos mini
#define SWITCH2_PIN D4

void setup() {
  Serial.begin(115200);

  pinMode(ERROR_PIN, OUTPUT);
  pinMode(SWITCH1_PIN, INPUT_PULLUP);
  pinMode(SWITCH2_PIN, INPUT_PULLUP);

  initScreen();
  initTime();
  digitalClock.begin(8, 50, grey, white);
  initComms();
}

uint8_t mode = 0;

// Global time when mode => 0
uint8_t ss0, mm0, hh0;
unsigned long us0;

void calcTimeNow(unsigned long us, uint8_t& hh, uint8_t& mm, uint8_t& ss)
{
  unsigned long ssAll;
  // Secs since t0
  ssAll = (us - us0) / 1000;
  // Time now (without carry)
  ss = ss0 + (ssAll % 60);
  mm = mm0 + (ssAll / 60);
  hh = hh0 + ((ssAll / 3600) % 24);
  // Carry
  mm += (ss / 60);
  ss %= 60;
  hh += (mm / 60);
  mm %= 60;
  hh %= 24;
}

void initTime()
{
  ss0 = 0;
  mm0 = 0;
  hh0 = 0;
  us0 = millis();
}

void loop() {
  uint8_t hh, mm, ss;
  static uint8_t ssLast = (uint8_t)-1;
 
  static uint8_t uiMode = 0, uiModeLast = (uint8_t)-1;

  int sw1, sw2;
  static int sw1Last = 1, sw2Last = 1; 
  
  bool modeChanged = false;
  bool hasTimeInput = false;

  static float value = 10.0;

  // Switch mode?
  sw1 = digitalRead(SWITCH1_PIN);
  if (!sw1Last && sw1)
  {
    uiMode++;
    uiMode %= 4;
    if (uiMode == 0) {
      us0 = millis();
      ssLast = (uint8_t)-1; // Force an update
    }
    modeChanged = true;
    //Serial.print("modeChanged => ");
    //Serial.println(uiMode);
  }
  sw1Last = sw1;

  sw2 = digitalRead(SWITCH2_PIN);
  if (!modeChanged && !sw2Last && sw2) {
    hasTimeInput = true;
    //Serial.println("hasTimeInput");
  }
  sw2Last = sw2;

  // Depending on mode...
  if (uiMode == 0)
  {
    calcTimeNow(millis(), hh, mm, ss);
    if (ss != ssLast) {
      digitalClock.updateTime(hh, mm, ss, uiMode);
      ssLast = ss;
      /*
      // Heartbeat to test ERROR_PIN
      static int pinValue = HIGH;
      digitalWrite(ERROR_PIN, pinValue);
      pinValue = (pinValue == HIGH) ? LOW : HIGH;
      */
    }
  }
  else
  {
    // Grab time as of now for editing
    if (modeChanged && (uiMode == 1)) {
      calcTimeNow(millis(), hh, mm, ss);
      hh0 = hh; mm0 = mm; ss0 = ss;
    }
    
    // Use input from button 2 to set clock parts
    if (hasTimeInput) {
      switch (uiMode) {
        case 1: hh0 = (hh + 1) % 24; break;
        case 2: mm0 = (mm + 1) % 60; break;
        case 3: ss0 = (ss + 1) % 60; break;
      }
    }
    if (hasTimeInput || (uiMode != uiModeLast)) {
      digitalClock.updateTime(hh0, mm0, ss0, uiMode);
      uiModeLast = uiMode;
    }
  }

  // Now display the input value
  processComms();

  // Test version - use a click on switch 2 as input
  if ((uiMode == 0) && hasTimeInput) {
    value += 0.1;
    if (value >= 100.0)
      value = 10.0;
    char buf[10];
    dtostrf(value, 2, 2, buf);
    performTask(buf);
  }
}

void performTask(String data) {
  tft.setTextColor(blue);
  
  char buf[20];
  float theValue = data.toFloat();
  dtostrf(theValue, 2, 1, buf);
  //strcat(buf, "c");
  uint8_t x = 7;
  uint8_t y = digitalClock.getClockY() + digitalClock.getClockCY() + 14;
  tft.fillRect(x, y, 128 - x, 128 - y, white); 
  tft.setCursor(x, y);
  tft.setTextSize(4);
  tft.print(buf);
  tft.drawCircle(104, y + 4, 4, blue);
  tft.setCursor(110, y);
  tft.print("c");
}

/*
#define __ASSERT_USE_STDERR
#include <assert.h>

// handle diagnostic informations given by assertion and abort program execution:
void __assert(const char *__func, const char *__file, int __lineno, const char *__sexp) {
    // transmit diagnostic informations through serial link. 
    Serial.println(__func);
    Serial.println(__file);
    Serial.println(__lineno, DEC);
    Serial.println(__sexp);
    Serial.flush();
    // abort program execution.
    abort();
}
*/

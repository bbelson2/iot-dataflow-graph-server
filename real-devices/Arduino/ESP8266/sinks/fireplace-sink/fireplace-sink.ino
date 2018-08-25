/*
   IoT Dataflow Graph Server
   Copyright (c) 2015, Adam Rehn, Jason Holdsworth
   
   Fireplace sink
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

/* UDP multicast group */

#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <ESP8266MulticastUDP.h>

//The identifier for this node
const String& NODE_IDENTIFIER = "fireplace-sink";

//The interval (in milliseconds) at which input is read
#define READ_INTERVAL 100

// Setup the ESP8266 Multicast UDP object as a sink
ESP8266MulticastUDP multicast("iot-dataflow", "it-at-jcu",
  IPAddress(224, 0, 0, 115), 9090);

#define ERROR_PIN 15

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
  DataPacket packet;
  if (multicast.readWithoutBlock(packet)) {
    //Serial.println("Reading from WiFi network!");
    String message = packet.data;
    //Serial.println("Got message:" + message);

    if (message.startsWith(NODE_IDENTIFIER)) {
      performTask(message.substring(NODE_IDENTIFIER.length() + 1));
    }
  }
}

static int changeStateRequest = 0;

/* OLED header info */
#include <SPI.h>
#include <FS.h>
/* Important: Make sure to include SPI.h and SD.h above FTOLED.h if you want to load BMPs from SD */
#include <FTOLED.h>
#include <fonts/SystemFont5x7.h>

const byte pin_cs = D1;
const byte pin_dc = D2;
const byte pin_reset = D0;

OLED oled(pin_cs, pin_dc, pin_reset);

// Text box is used to display error messages if any frames
// fail to load
OLED_TextBox text(oled);

const int FRAME_COUNT = 64;

#define MSG_NOSD F("FS memory not found")
#define MSG_SKIP F("Skipping missing frame ")

/* Servo header info */

#include <Servo.h>

#define SPILL_PIN D4
#define SERVO_PIN D6
#define TRIGGER_PIN D8

#define DOWN_ANGLE 0
#define UP_ANGLE   90

Servo servo;

bool gotRisingEdge() {
  static int lastValue = HIGH;
  int newValue = digitalRead(TRIGGER_PIN);
  bool result = (lastValue == HIGH) && (newValue == LOW);
  lastValue = newValue;
  return result;
}

class Fire {
public:
  enum FireStates {
    STATE_OFF = 0,
    STATE_LIGHTING,
    STATE_ON               
  };

  void begin() {
    setServoAngle(START_ANGLE);
    digitalWrite(SPILL_PIN, LOW);
  }
  
  void run() {
    // Get inputs
    bool go = gotRisingEdge();
    if (go) {
      Serial.println("got rising edge");
    }
    
    unsigned long now = millis();
    FireStates nextState = m_state;

    // Is there a state change?
    switch (m_state) {
    case STATE_OFF:
      if (go || (changeStateRequest == 1)) {
        nextState = STATE_LIGHTING;
      }
      break;
    case STATE_LIGHTING:
      if (go) {
        nextState = STATE_OFF;
      }
      else if (changeStateRequest == -1) {
        nextState = STATE_OFF;
      }
      else if (m_lightingActionsCompleted) {
        nextState = STATE_ON;
      }
      break;
    case STATE_ON:
      if (go || (changeStateRequest == -1)) {
        nextState = STATE_OFF;
      }
      break;
    }

    // Is there a state change?
    if (nextState != m_state) {
      switch (nextState) {
        case STATE_OFF:
          setServoAngle(START_ANGLE);
          digitalWrite(SPILL_PIN, LOW);
          oled.clearScreen();
          break;
        case STATE_LIGHTING:
          break;
        case STATE_ON:
          setServoAngle(START_ANGLE);
          digitalWrite(SPILL_PIN, LOW);
          break;
      }
      m_state = nextState;
      m_msStateStarted = now;
    }

    // Deal with intermediate state
    if (m_state == STATE_LIGHTING) {
      showLightingState(now);
    }

    // Is the fire lit?
    if (m_state == STATE_ON) {
      m_fireIsLit = true;
    } else if (m_state == STATE_OFF) {
      m_fireIsLit = false;
    }
    // else it was set in showLightingState()

    // Update the fie
    if (m_fireIsLit) {
      if (++m_frameIndex >= FRAME_COUNT) {
        m_frameIndex = 0;
      }
      char filename[20];
      snprintf(filename, sizeof(filename), "/%03d.bmp", m_frameIndex);
      File frame = SPIFFS.open(filename, "r");
      if (frame) {
        oled.displayBMP(frame, 0, 0);
        frame.close();
      } 
      else {
        Serial.print(MSG_SKIP);
        Serial.println(filename);
        text.clear();
        text.print(MSG_SKIP);
        text.println(filename);   
      }
    }
  }

  Fire() {
    m_state = STATE_OFF;
    m_frameIndex = -1;
    //m_msNextStateChange = 0;
    //m_msMoveStarted = 0;
    //m_msUpDuration = 2000;
    //m_msDownDuration = 2000;
    //mc_msPerDegree = 10;
    m_lightingActionsCompleted = false;
    m_fireIsLit = false;
  }
protected:
  void showLightingState(unsigned long now) {
    //    0 - 999 : raise lit spill
    // 1000 - 1499: hold lit spill at unlit fire
    // 1500 - 1999: lower unlit spill from lit fire
    // 2000 +     : done
    
    static unsigned long MS_SPILL_RAISED  = 1000;
    static unsigned long MS_FIRE_LIT      = 1500;
    static unsigned long MS_SPILL_LOWERED = 2000;

    //static int RAISE_DIRECTION = (LIT_ANGLE > START_ANGLE) ? 1 : -1;
    
    unsigned long progress = now - m_msStateStarted;

    m_lightingActionsCompleted = false;
    
    // Light spill
    // TODO - flicker
    bool spillLit = (progress < MS_FIRE_LIT);
    
    // Set arm position
    int servoAngle = START_ANGLE; 
    if (progress < MS_SPILL_RAISED) {
      servoAngle = START_ANGLE + 
                   (int)(((float)progress / (float)(MS_SPILL_RAISED - 0))
                        * (float)(LIT_ANGLE - START_ANGLE));
    }
    else if (progress < MS_FIRE_LIT) {
      servoAngle = LIT_ANGLE;
    }
    else if (progress < MS_SPILL_LOWERED) {
      servoAngle = LIT_ANGLE - 
                   (int)(((float)(progress - MS_FIRE_LIT) / (float)(MS_SPILL_LOWERED - MS_FIRE_LIT))
                        * (float)(START_ANGLE - LIT_ANGLE));
    }
    else {
      servoAngle = START_ANGLE;
      m_lightingActionsCompleted = true;
    }

    // Is the fire lit
    bool fireLit = (progress >= MS_FIRE_LIT);
    
    // Take immediate actions
    digitalWrite(SPILL_PIN, spillLit ? HIGH : LOW);
    setServoAngle(servoAngle); 

    // Set up deferred action
    m_fireIsLit = fireLit;
  }
  void setServoAngle(int angle) {
    if (m_servoAngle == angle) {
      return;
    }
    servo.write(angle);
    m_servoAngle = angle;
  }
public:
  static const int START_ANGLE = 0;
  static const int LIT_ANGLE = 90;
private:
  FireStates    m_state;
  int           m_frameIndex;
  int           m_servoAngle;
  unsigned long m_msStateStarted;
  bool          m_lightingActionsCompleted,
                m_fireIsLit;
};

Fire fire;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  oled.begin();
  oled.selectFont(SystemFont5x7);
  text.setForegroundColour(RED);
  if(!SPIFFS.begin()) {
    Serial.println(MSG_NOSD);
    text.println(MSG_NOSD);
    delay(500);
  }
  oled.begin(); // Calling begin() again so we get faster SPI, see https://github.com/freetronics/FTOLED/wiki/Displaying-BMPs
  Serial.println("oled setup() complete");
  
  pinMode(SPILL_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
  servo.attach(SERVO_PIN);
  
  fire.begin();
}

void loop() {
  fire.run();
  processComms();  
}

// Request change of state
// -1 = switch off
//  1 = switch on
//  0 = no change

void performTask(String data) {
  static bool lastState = false;
  bool state = true;
  if (data.equals("true")) {
    state = true;
  }
  else if (data.equals("false")) {
    state = false;
  }
  else {
    state = (data.toInt() != 0); 
  }
  
  if (state == lastState) {
    changeStateRequest = 0;
  }
  else {
    changeStateRequest = state ? 1 : -1;
  }
  lastState = state;
}


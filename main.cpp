#include <tinyNeoPixel_static.h>
#include <Wire.h>

#include "draw.h"
#include "scan.h"

#define I2C_ADDRESS 0x15
#define STATUS_LED_PIN 5
#define SWITCH_PIN 15

#define MATRIX_HEIGHT NUM_ROWS
#define MATRIX_WIDTH NUM_COLS

#define MAX_MESSAGE_SIZE 140
#define MIN_UPDATE_INTERVAL 5
#define MAX_UPDATE_INTERVAL 500

// i2c
bool statusLed = false;
bool statusInitial = false;
uint8_t statusBlinks = 0; // number of extra short blinks after long "ACK" blink

// message
char message[MAX_MESSAGE_SIZE];
int16_t messageWidth;
int16_t x = NUM_COLS;
int16_t y = NUM_ROWS;

// both need to be on for scan display to be on
volatile bool display = true;
bool drawMode = true;

// updates/timing
unsigned long drawUpdateInterval = 100;
unsigned long lastDrawUpdate = 0;
unsigned long statusUpdateInterval = 1000;
unsigned long lastStatusUpdate = 0;

bool setDisplay()
{
  scanSetDisplayState(display);
  return display;
  // switchState = digitalRead(SWITCH_PIN);
  // bool scanDisplayState = display && switchState;
  // scanDisplay(scanDisplayState);
  // return scanDisplayState;
}

void setMessage(const char *newMessage)
{
  strncpy(message, newMessage, MAX_MESSAGE_SIZE - 1);
  message[MAX_MESSAGE_SIZE - 1] = '\0';
  messageWidth = getTextWidth(message);
  x = MATRIX_WIDTH;
}

void receiveEvent(int bytesReceived)
{
  statusLed = true;
  digitalWrite(STATUS_LED_PIN, !statusLed);
  lastStatusUpdate = millis();
  statusBlinks = 0;
  statusInitial = true;

  if (bytesReceived < 2)
  {
    return;
  }

  static char buffer[MAX_MESSAGE_SIZE];
  static int bufferIndex = 0;

  uint8_t command = Wire.read();
  statusBlinks = command + 1; // use value to blink status LED

  // setDisplay
  if (command == 0x00)
  {
    display = Wire.read();
    setDisplay();
  }
  // setMessage
  else if (command == 0x01)
  {
    // read chunk into buffer, discard extra bytes if past buffer size
    while (Wire.available())
    {
      uint8_t byte = Wire.read();
      if (bufferIndex < MAX_MESSAGE_SIZE - 1)
      {
        buffer[bufferIndex++] = byte;
      }
    }
    buffer[bufferIndex] = '\0';

    // last chunk (or buffer overflow)
    if (bufferIndex > 0 && (buffer[bufferIndex - 1] == '\n' || bufferIndex >= MAX_MESSAGE_SIZE - 1))
    {
      if (buffer[bufferIndex - 1] == '\n')
      {
        buffer[--bufferIndex] = '\0';
      }

      setMessage(buffer);
      bufferIndex = 0;
    }
  }
  // setScrollSpeed
  else if (command == 0x02)
  {
    uint8_t scrollSpeed = Wire.read();
    drawUpdateInterval = map(constrain(scrollSpeed, 0, 100), 100, 0, MIN_UPDATE_INTERVAL, MAX_UPDATE_INTERVAL);
  }
  // getState
  else if (command == 0x03)
  {
    // TODO: when more read command implement lastCommand for requestEvent filtering
  }
  else
  {
    statusBlinks = 10;
  }
}

void requestEvent()
{
  bool switchState = digitalRead(SWITCH_PIN);
  Wire.write((uint8_t)switchState);
}

void setup()
{
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, false);

  pinMode(SWITCH_PIN, INPUT_PULLUP);

  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  if (drawMode)
  {
    scanSetDrawMode(drawMode);
    setMessage("From a wild weird clime that lieth, sublime; out of Space, out of Time.");
  }

  scanInit();

  delay(500);
  digitalWrite(STATUS_LED_PIN, true);
}

void loop()
{
  // status LED indicates when I2C message is received, long blink first, then short blink count indicates status
  if ((statusBlinks > 0 || statusLed) && millis() - lastStatusUpdate > statusUpdateInterval)
  {
    statusLed = !statusLed;
    digitalWrite(STATUS_LED_PIN, !statusLed);
    lastStatusUpdate = millis();

    if (statusBlinks > 0)
    {
      statusBlinks -= statusLed ? 1 : 0;
      statusUpdateInterval = statusLed ? 100 : 50;

      // longer gap between long and short blinks
      if (statusInitial)
      {
        statusInitial = false;
        statusUpdateInterval = 300;
      }
    }
    else
    {
      statusUpdateInterval = 500;
    }
  }

  if (millis() - lastDrawUpdate < drawUpdateInterval || !setDisplay())
  {
    return;
  }

  if (drawMode)
  {
    scanClear();
    drawString(x, MATRIX_HEIGHT - 2, MATRIX_WIDTH, MATRIX_HEIGHT, message, true);
    scanShow();
    lastDrawUpdate = millis();

    if (--x < -messageWidth)
    {
      x = MATRIX_WIDTH;
    }
  }
  else
  {
    bool switchState = digitalRead(SWITCH_PIN);
    if (switchState)
    {
      scroll();
    }
    scanShow();
  }
}
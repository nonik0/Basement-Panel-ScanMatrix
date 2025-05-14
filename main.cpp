#include <tinyNeoPixel_static.h>
#include <Wire.h>

#include "draw.h"
#include "scan.h"

#define I2C_ADDRESS 0x15
#define STATUS_LED_PIN 5
#define SWITCH_PIN 15

#define MATRIX_HEIGHT NUM_ROWS
#define MATRIX_WIDTH NUM_COLS

#define MAX_MESSAGE_SIZE 160
#define MIN_UPDATE_INTERVAL 5
#define MAX_UPDATE_INTERVAL 500

// i2c
char buffer[MAX_MESSAGE_SIZE];
size_t bufferIndex = 0;
bool statusLed = false;

// message
char message[MAX_MESSAGE_SIZE];
int16_t messageWidth;
int16_t x = NUM_COLS;
int16_t y = NUM_ROWS;

// both need to be on for scan display to be on
volatile bool display = true;
volatile bool switchState;

// updates/timing
unsigned long drawUpdateInterval = 100;
unsigned long lastDrawUpdate = 0;
unsigned long lastI2cMessage = 0;


bool setDisplay(bool display)
{
  switchState = digitalRead(SWITCH_PIN);
  bool scanDisplayState = display && switchState;
  scanDisplay(scanDisplayState);
  return scanDisplayState;
}

bool setDisplay()
{
  return setDisplay(display);
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
  lastI2cMessage = millis();

  if (bytesReceived < 2)
    return;

  uint8_t command = Wire.read();
  
  // setDisplay
  if (command == 0x00)
  {
    bool i2cDisplay = Wire.read();
    setDisplay(i2cDisplay);
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
    Wire.write(switchState);
  }
}

void setup()
{
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, false);

  pinMode(SWITCH_PIN, INPUT_PULLUP);

  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);

  setMessage("From a wild weird clime that lieth, sublime; out of Space, out of Time.");

  scanInit();

  delay(500);
  digitalWrite(STATUS_LED_PIN, true);
}

void loop()
{
  if (millis() - lastDrawUpdate < drawUpdateInterval)
  {
    return;
  }

  // status LED turns off one second after receiving I2C message
  if (statusLed && millis() - lastI2cMessage > 1000)
  {
    statusLed = false;
    digitalWrite(STATUS_LED_PIN, !statusLed);
  }

  if (!setDisplay())
  {
    delay(100);
    return;
  }
  
  scanClear();
  drawString(x, MATRIX_HEIGHT - 2, MATRIX_WIDTH, MATRIX_HEIGHT, message, true);
  scanShow();
  lastDrawUpdate = millis();

  if (--x < -messageWidth)
  {
    x = MATRIX_WIDTH;
  }
}
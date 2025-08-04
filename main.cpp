#include <tinyNeoPixel_static.h>
#include <Wire.h>

#include "drawText.h"
#include "scrollAnim.h"
#include "scrollText.h"
#include "scanMatrix.h"

#define I2C_ADDRESS 0x15
#define STATUS_LED_PIN 5
#define SWITCH_PIN 15

#define MIN_UPDATE_INTERVAL 5
#define MAX_UPDATE_INTERVAL 500
#define STATUS_UPDATE_INTERVAL 500
#define TEMP_MESSAGE_DURATION 5000

#if defined(MATRIX_16X16)
#define DEFAULT_DRAW_UPDATE_INTERVAL 100
#elif defined(MATRIX_8X8)
#define DEFAULT_DRAW_UPDATE_INTERVAL 120
#endif

enum Mode
{
  ScrollAnim,
  ScrollText
};

// i2c
bool statusLedState = false;
bool statusLedFirstBlink = false;
uint8_t statusLedBlinks = 0; // number of extra short blinks after long "ACK" blink

// display state
volatile bool display = true;
Mode mode = Mode::ScrollAnim;
unsigned long lastDrawUpdate = 0;
unsigned long lastStatusLedUpdate = 0;
unsigned long drawUpdateInterval = DEFAULT_DRAW_UPDATE_INTERVAL;
unsigned long statusLedUpdateInterval = STATUS_UPDATE_INTERVAL;

void handleOnReceive(int bytesReceived)
{
  statusLedState = true;
  digitalWrite(STATUS_LED_PIN, !statusLedState);
  lastStatusLedUpdate = millis();
  statusLedBlinks = 0;
  statusLedFirstBlink = true;

  if (bytesReceived < 2)
  {
    return;
  }

  static char buffer[MAX_MESSAGE_SIZE];
  static int bufferIndex = 0;

  uint8_t command = Wire.read();
  statusLedBlinks = command + 1; // use value to blink status LED

  // setDisplay
  if (command == 0x00)
  {
    display = Wire.read();
    scanDisplay(display);
  }
  // setMessage and showTempMessage
  else if (command == 0x01 || command == 0x04)
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

      if (command == 0x01)
      {
        scrollTextSetMessage(buffer);
      }
      else
      {
        // show temporary message
        scanClear();
        drawString(MESSAGE_X_OFFSET, MATRIX_HEIGHT - MESSAGE_Y_OFFSET, MATRIX_WIDTH, MATRIX_HEIGHT, buffer, true);
        scanShow();

        lastTempMessage = millis();
      }
      bufferIndex = 0;
    }
  }
  // setScrollSpeed
  else if (command == 0x02)
  {
    uint8_t scrollSpeed = Wire.read();
    drawUpdateInterval = map(constrain(scrollSpeed, 0, 100), 100, 0, MIN_UPDATE_INTERVAL, MAX_UPDATE_INTERVAL);
  }
  // setDisplayMode
  else if (command == 0x03)
  {
    mode = (Mode)Wire.read();
  }
  else
  {
    statusLedBlinks = 10;
  }
}

void handleOnRequest()
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
  Wire.onReceive(handleOnReceive);
  Wire.onRequest(handleOnRequest);

  if (mode == Mode::ScrollText)
  {
    scrollTextSetMessage("From a wild weird clime that lieth, sublime; out of Space, out of Time.");
  }
  scanInit();
  scanDisplay(display);

  delay(500);
  digitalWrite(STATUS_LED_PIN, true);
}

void loop()
{
  // status LED indicates when I2C message is received, long blink first, then short blink count indicates status
  if ((statusLedBlinks > 0 || statusLedState) && millis() - lastStatusLedUpdate > statusLedUpdateInterval)
  {
    statusLedState = !statusLedState;
    digitalWrite(STATUS_LED_PIN, !statusLedState);
    lastStatusLedUpdate = millis();

    if (statusLedBlinks > 0)
    {
      statusLedBlinks -= statusLedState ? 1 : 0;
      statusLedUpdateInterval = statusLedState ? (STATUS_UPDATE_INTERVAL >> 2) : (STATUS_UPDATE_INTERVAL >> 3);

      // longer gap between first long blink and later short blinks
      if (statusLedFirstBlink)
      {
        statusLedFirstBlink = false;
        statusLedUpdateInterval = STATUS_UPDATE_INTERVAL >> 1;
      }
    }
    else
    {
      statusLedUpdateInterval = STATUS_UPDATE_INTERVAL;
    }
  }

  // check if ready to draw again
  bool switchState = digitalRead(SWITCH_PIN);
  if (millis() - lastDrawUpdate < drawUpdateInterval || !display || !switchState || (lastTempMessage != 0 && millis() - lastTempMessage < TEMP_MESSAGE_DURATION))
  {
    yield();
    return;
  }
  lastDrawUpdate = millis();
  lastTempMessage = 0;

  // draw for current mode
  switch (mode)
  {
  case ScrollAnim:
    scrollAnim();
    break;
  case ScrollText:
    scrollText();
    break;
  }
  delay(10);
}

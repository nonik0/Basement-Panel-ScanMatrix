#include <tinyNeoPixel_static.h>
#include <Wire.h>

#include "draw.h"
#include "scan.h"

#define I2C_ADDRESS 0x15
#define STATUS_LED_PIN 5
#define SWITCH_PIN 15

#define MATRIX_HEIGHT NUM_ROWS
#define MATRIX_WIDTH NUM_COLS

#define MIN_UPDATE_INTERVAL 5
#define MAX_UPDATE_INTERVAL 500
#define STATUS_UPDATE_INTERVAL 500
#define TEMP_MESSAGE_DURATION 5000

#if defined(MATRIX_16X16)
#define DEFAULT_DRAW_UPDATE_INTERVAL 100
#define MAX_MESSAGE_SIZE 10
#define TEMP_MESSAGE_Y_OFFSET 5
#elif defined(MATRIX_8X8)
#define DEFAULT_DRAW_UPDATE_INTERVAL 120
#define MAX_MESSAGE_SIZE 140
#define TEMP_MESSAGE_Y_OFFSET 2
#endif
#define TEMP_MESSAGE_X_OFFSET 3

// i2c
bool statusLedState = false;
bool statusLedFirstBlink = false;
uint8_t statusLedBlinks = 0; // number of extra short blinks after long "ACK" blink

// message
char message[MAX_MESSAGE_SIZE];
int16_t messageWidth;
int16_t x = NUM_COLS;
int16_t y = NUM_ROWS;
unsigned long lastTempMessage = 0; // time left for temporary message display

// display state
volatile bool display = true;
bool scrollingTextMode = false;
unsigned long lastDrawUpdate = 0;
unsigned long lastStatusLedUpdate = 0;
unsigned long drawUpdateInterval = DEFAULT_DRAW_UPDATE_INTERVAL;
unsigned long statusLedUpdateInterval = STATUS_UPDATE_INTERVAL;

bool setDisplayState()
{
  scanSetDisplayState(display);
  return display;
}

void setMessage(const char *newMessage)
{
  strncpy(message, newMessage, MAX_MESSAGE_SIZE - 1);
  message[MAX_MESSAGE_SIZE - 1] = '\0';
  messageWidth = getTextWidth(message);
  x = MATRIX_WIDTH;
}

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
    setDisplayState();
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
        setMessage(buffer);
      }
      else
      {
        // show temporary message
        scanClear();
        drawString(TEMP_MESSAGE_X_OFFSET, MATRIX_HEIGHT - TEMP_MESSAGE_Y_OFFSET, MATRIX_WIDTH, MATRIX_HEIGHT, buffer, true);
        //drawString(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT, buffer, true);
        //drawString(0, MATRIX_HEIGHT, MATRIX_WIDTH, MATRIX_HEIGHT, buffer, true);
        scanShow();

        scanSetDisplayMode(true);
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
    scrollingTextMode = Wire.read();
    scanSetDisplayMode(scrollingTextMode);
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

  if (scrollingTextMode)
  {
    scanSetDisplayMode(scrollingTextMode);
    setMessage("From a wild weird clime that lieth, sublime; out of Space, out of Time.");
  }

  scanInit();

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

  // handle either scrolling message or scrolling animation
  if ((lastTempMessage != 0 && millis() - lastTempMessage < TEMP_MESSAGE_DURATION) ||
      millis() - lastDrawUpdate < drawUpdateInterval || !setDisplayState())
  {
    yield();
    return;
  }
  lastDrawUpdate = millis();

  // restore previous mode if temporary message is done
  if (lastTempMessage > 0) {
    scanSetDisplayMode(scrollingTextMode);
    lastTempMessage = 0;
  }

  // handle scrolling text or animation
  if (scrollingTextMode)
  {
    scanClear();
    drawString(x, MATRIX_HEIGHT - TEMP_MESSAGE_Y_OFFSET, MATRIX_WIDTH, MATRIX_HEIGHT, message, true);
    scanShow();

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
  }
}
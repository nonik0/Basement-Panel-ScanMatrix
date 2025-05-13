// TODO: below TODO needs SPI, do that first
// TODO: try this lib https://github.com/michaelkamprath/ShiftRegisterLEDMatrixLib

#include <tinyNeoPixel_static.h>
#include <Wire.h>

#include "draw.h"
#include "scan.h"

#define I2C_ADDRESS 0x13

#define MATRIX_HEIGHT NUM_ROWS
#define MATRIX_WIDTH NUM_COLS

#define MAX_MESSAGE_SIZE 160
#define MIN_UPDATE_INTERVAL 5
#define MAX_UPDATE_INTERVAL 500

// message
char message[MAX_MESSAGE_SIZE];
int messageWidth;
int x = NUM_COLS;
int y = NUM_ROWS;

volatile bool display = true;

// updates/timing
unsigned long drawUpdateInterval = 300;
unsigned long lastDrawUpdate = 0;

void setMessage(const char *newMessage)
{
  strncpy(message, newMessage, MAX_MESSAGE_SIZE - 1);
  message[MAX_MESSAGE_SIZE - 1] = '\0';
  messageWidth = getTextWidth(message);
  x = MATRIX_WIDTH;
}

void receiveEvent(int bytesReceived)
{
  if (bytesReceived < 2)
    return;

  static char buffer[MAX_MESSAGE_SIZE];
  static int bufferIndex = 0;

  uint8_t command = Wire.read();
  if (command == 0x00)
  {
    bool state = Wire.read();
    setScanDisplay(state);
    display = state;
  }
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
  else if (command == 0x02)
  {
    uint8_t scrollSpeed = Wire.read();
    drawUpdateInterval = map(constrain(scrollSpeed, 0, 100), 100, 0, MIN_UPDATE_INTERVAL, MAX_UPDATE_INTERVAL);
  }
}

void drawPixel(uint16_t x, uint16_t y, uint32_t color)
{
  if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT)
  {
    // uint16_t flippedX = MATRIX_WIDTH - 1 - x;
    // uint16_t flippedY = MATRIX_HEIGHT - 1 - y;
    // matrix.setPixelColor(flippedY * MATRIX_WIDTH + flippedX, color);
    //displayBuffer[y] |= (1 << x);
  }
}

void setup()
{
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  setMessage("test");

  initScan();
  setScanDisplay(true);

  // clear();
  // delay(1000);
  // drawString(x, MATRIX_HEIGHT - 2, MATRIX_WIDTH, MATRIX_HEIGHT, "test", true);
}

void loop()
{
  if (millis() - lastDrawUpdate < drawUpdateInterval)
  {
    return;
  }

  setScanDisplay(display);
  if (!display)
  {
    delay(100);
    return;
  }
  
  // TODO: Update draw buffer
  // drawString(x, MATRIX_HEIGHT - 2, MATRIX_WIDTH, MATRIX_HEIGHT, "test", true);
  scroll();
  lastDrawUpdate = millis();

  if (--x < -messageWidth)
  {
    x = MATRIX_WIDTH;
  }
}
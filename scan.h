#pragma once

#include <Arduino.h>
#include <SPI.h>

#define NUM_ROWS 8
#define NUM_COLS 8
#define NUM_LEDS 64
#define NUM_BLANK_CYCLES 2 // number of cycles between each row drawn

// #define SS_PIN          // unused
#define LATCH_PIN 16 // RCLK/STB
#define OE_PIN 17    // !OE, can be tied to GND if need to save pin
// #define DATA_PIN 18       // MOSI/IN
// #define MISO_PIN 19     // unused
// #define CLOCK_PIN 20      // SCLK/CLK

// scroll mode consts/variables
const int ScrollWidth = 32;
volatile int scrollIndex = 0;
uint32_t scrollBuffer[NUM_ROWS] = {
    0b00011000000011110000000111100000,
    0b00011000000111111000001100110000,
    0b11111111001111111100001000010000,
    0b01111110001111111100001100110000,
    0b00111100001111111100000111100000,
    0b01111110000100001000010011001000,
    0b01100110000100001000011111111000,
    0b11000011000011110000000111100000,
};

// draw mode variables
volatile bool drawModeActive = false;   // I2C updates => flag for mode: scroll=false, draw=true
volatile bool drawBufferActive = false; // ISR updates => changes when frame is complete
uint8_t drawUpdateBuffer[NUM_ROWS];     // draw updates go here
volatile uint8_t drawBuffer[NUM_ROWS];  // ISR shifts out data from this, copies new data from drawBuffer

// ISR state variables
volatile bool bufferUpdate = false; // flag to signal ISR that buffer needs to change/be updated
volatile int curLine = 0;
volatile int blankCycles = 0; // off cycles between each line write
bool scanDisplay;

void scanClear()
{
  for (int i = 0; i < NUM_ROWS; i++)
  {
    drawUpdateBuffer[i] = 0;
  }
}

void scanSetDisplayState(bool displayState)
{
  scanDisplay = displayState;
  digitalWrite(OE_PIN, !scanDisplay);
}

void scanDrawPixel(int x, int y, bool on)
{
  if (x < 0 || x >= NUM_COLS || y < 0 || y >= NUM_ROWS)
    return;

  if (on)
    drawUpdateBuffer[y] |= (1 << x);
  else
    drawUpdateBuffer[y] &= ~(1 << x);
}

void scanSetDrawMode(bool isActive)
{
  if (drawModeActive != isActive)
  {
    drawModeActive = isActive;
    bufferUpdate = true;
  }
}

void scanInit()
{
  pinMode(OE_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  digitalWrite(OE_PIN, LOW);
  digitalWrite(LATCH_PIN, LOW);

  SPI.begin();

  // Configure Timer B (TCA0) for CTC mode at 8kHz from 10MHz
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV2_gc;
  TCB0.CTRLB = TCB_CNTMODE_INT_gc; // CTC mode
  TCB0.CCMP = 1249 * 2;            // (20Mhz / 2) / 1250 = 8kHz
  TCB0.INTCTRL = TCB_CAPT_bm;      // Enable interrupt on capture
}

void scanShow()
{
  bufferUpdate = true;
}

void scroll()
{
  scrollIndex = (scrollIndex + 1) % ScrollWidth;
}

ISR(TCB0_INT_vect)
{
  // clear interrupt flag
  TCB0.INTFLAGS = TCB_CAPT_bm;

  // calculate row data and select
  uint8_t rowData, rowSelect;
  if (blankCycles > 0 || !scanDisplay)
  {
    rowData = 0xFF;
    rowSelect = 0xFF;
  }
  else
  {
    if (drawBufferActive)
    {
      rowData = ~drawBuffer[curLine];
    }
    else
    {
      uint32_t fullRowData = ~scrollBuffer[curLine];
      rowData = ((fullRowData << scrollIndex) | (fullRowData >> (ScrollWidth - scrollIndex))) & 0xFF;
    }
    rowSelect = ~(0x01 << curLine);
  }

  // shift out row data
  SPI.transfer(rowData);
  SPI.transfer(rowSelect);
  digitalWrite(LATCH_PIN, LOW);
  digitalWrite(LATCH_PIN, HIGH);

  // update the current line and blank cycles
  if (--blankCycles < 0)
  {
    curLine = (curLine + 1) % NUM_ROWS;
    blankCycles = NUM_BLANK_CYCLES;
  }

  // swap in new frame if available
  if (bufferUpdate && curLine == 0 && blankCycles == NUM_BLANK_CYCLES)
  {
    // update active buffer
    drawBufferActive = drawModeActive;

    if (drawBufferActive)
    {
      for (int i = 0; i < NUM_ROWS; i++)
      {
        drawBuffer[i] = drawUpdateBuffer[i];
      }
    }

    bufferUpdate = false;
  }
}

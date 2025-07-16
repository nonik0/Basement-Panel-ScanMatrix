#pragma once

#include <Arduino.h>
#include <SPI.h>

// Matrix size configuration based on build flags
#if defined(MATRIX_16X16)
#define NUM_ROWS 16
#define NUM_COLS 16
#define NUM_LEDS 256
#define NUM_BLANK_CYCLES 0
#define SCROLL_WIDTH 64
#define SCROLL_INDEX_INITIAL 0
#define BLANK_DATA 0xFFFF
#elif defined(MATRIX_8X8)
#define NUM_ROWS 8
#define NUM_COLS 8
#define NUM_LEDS 64
#define NUM_BLANK_CYCLES 2
#define SCROLL_WIDTH 32
#define SCROLL_INDEX_INITIAL 29
#define BLANK_DATA 0xFF
#else
#error "No matrix size defined. Use -DMATRIX_8X8 or -DMATRIX_16X16"
#endif

// Pin definitions
// #define SS_PIN        // unused
#define LATCH_PIN 16 // RCLK/STB
#define OE_PIN 17    // !OE, can be tied to GND if need to save pin
// #define DATA_PIN 18   // MOSI/IN
// #define MISO_PIN 19   // unused
// #define CLOCK_PIN 20  // SCLK/CLK

// scroll mode consts/variables
const int ScrollWidth = SCROLL_WIDTH;
volatile int scrollIndex = SCROLL_INDEX_INITIAL;

#if defined(MATRIX_16X16)
typedef uint16_t rowdata_t;
typedef uint64_t scrolldata_t;
volatile scrolldata_t scrollBuffer[NUM_ROWS] = {
    0b0000000110000000000000000011111100000000000000111111110000000000,
    0b0000001111000000000000001111110011000000000011111111111100000000,
    0b0000001111000000000000010011110000100000000111100000011110000000,
    0b0000011111100000000000100111111100010000001110001001000111000000,
    0b1111111111111111000000101100001110010000001110001001000111000000,
    0b0111110110111110000001111000000111111000001111000000001111000000,
    0b0011110110111100000001111000000110011000000111111111111110000000,
    0b0001110110111000000001011000000100001000000011111111111100000000,
    0b0001111111111000000001001100001100001000000000111111110000000000,
    0b0001111111111000000001001111111110011000000110001001000110000000,
    0b0011111111111100000001011111111111111000001001101001011001000000,
    0b0011111111111100000000111001001001110000001000111001100001000000,
    0b0111111111111110000000010001001000100000001000001001000001000000,
    0b0111111001111110000000010000000000100000000100000000000010000000,
    0b1111100000011111000000001000000001000000000011000000001100000000,
    0b1110000000000111000000000111111110000000000000111111110000000000,
};

#elif defined(MATRIX_8X8)
typedef uint8_t rowdata_t;
typedef uint32_t scrolldata_t;
scrolldata_t scrollBuffer[NUM_ROWS] = {
    0b00011000000011110000001111110000,
    0b00011000000111111000011000011000,
    0b11111111001111111100011000011000,
    0b01111110001111111100001111110000,
    0b00111100001111111100000011000000,
    0b01111110000100001000011011011000,
    0b01100110000100001000011111111000,
    0b11000011000011110000000111100000,
};
#endif

// draw mode variables
volatile bool drawModeActive = false;    // I2C updates => flag for mode: scroll=false, draw=true
volatile bool drawBufferActive = false;  // ISR updates => changes when frame is complete
rowdata_t drawUpdateBuffer[NUM_ROWS];    // draw updates go here
volatile rowdata_t drawBuffer[NUM_ROWS]; // ISR shifts out data from this, copies new data from drawBuffer

// ISR state variables
volatile bool bufferUpdate = false; // flag to signal ISR that buffer needs to change/be updated
volatile int curLine = 0;
volatile int blankCycles = 0; // off cycles between each line write
bool displayEnabled;

void scanClear()
{
  for (int i = 0; i < NUM_ROWS; i++)
  {
    drawUpdateBuffer[i] = 0;
  }
}

void scanSetDisplayMode(bool setDrawModeActive)
{
  if (drawModeActive != setDrawModeActive)
  {
    drawModeActive = setDrawModeActive;
    bufferUpdate = true;
  }
}

void scanSetDisplayState(bool setDisplayEnabled)
{
  displayEnabled = setDisplayEnabled;
  digitalWrite(OE_PIN, !displayEnabled);
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
  rowdata_t rowData, rowSelect;
  if (blankCycles > 0 || !displayEnabled)
  {
    rowData = BLANK_DATA;
    rowSelect = BLANK_DATA;
  }
  else
  {
    if (drawBufferActive)
    {
      rowData = ~drawBuffer[curLine];
    }
    else
    {
      scrolldata_t fullRowData = ~scrollBuffer[curLine];
      rowData = (fullRowData << scrollIndex) | (fullRowData >> (ScrollWidth - scrollIndex));
    }
    rowSelect = ~(0x01 << curLine);
  }

  // shift out row data
#if defined(MATRIX_16X16)
  SPI.transfer16(rowData);
  SPI.transfer16(rowSelect);
#elif defined(MATRIX_8X8)
  SPI.transfer(rowData);
  SPI.transfer(rowSelect);
#endif
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

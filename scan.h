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

uint8_t drawBuffer[NUM_ROWS]; // draw updates go here
volatile uint8_t scanBuffer[NUM_ROWS]; // ISR shifts out data from this, copies new data from drawBuffer

bool scanOutputEnable;
volatile int curLine = 0;
volatile int blankCycles = 0; // cycles to shift out off data between each row drawn
volatile bool drawUpdate = false; // flag to signal ISR to copy new data after cur frame is drawn

inline void scanClear()
{
  for (int i = 0; i < NUM_ROWS; i++)
  {
    drawBuffer[i] = 0;
  }
}

inline void scanDisplay(bool displayState)
{
  scanOutputEnable = displayState; // update the display state
  digitalWrite(OE_PIN, !scanOutputEnable);
}

inline void scanDrawPixel(int x, int y, bool on)
{
  if (x < 0 || x >= NUM_COLS || y < 0 || y >= NUM_ROWS)
    return;

  if (on)
    drawBuffer[y] |= (1 << x);
  else
    drawBuffer[y] &= ~(1 << x);
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

inline void scanShow()
{
  drawUpdate = true;
}

ISR(TCB0_INT_vect)
{
  // clear interrupt flag
  TCB0.INTFLAGS = TCB_CAPT_bm;

  // shift out row data
  bool blankFrame = blankCycles > 0 || !scanOutputEnable;
  uint8_t rowData = blankFrame ? 0xFF : ~scanBuffer[curLine];
  uint8_t rowSelect = blankFrame ? 0xFF : ~(0x01 << curLine);
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
  if (drawUpdate && curLine == 0 && blankCycles == NUM_BLANK_CYCLES)
  {
    for (int i = 0; i < NUM_ROWS; i++)
    {
      scanBuffer[i] = drawBuffer[i];
    }
    drawUpdate = false;
  }
}

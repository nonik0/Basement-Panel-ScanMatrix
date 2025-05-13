#pragma once

#include <Arduino.h>
#include <SPI.h>

#define NUM_ROWS 8
#define NUM_COLS 8
#define NUM_LEDS ROWS *COLS
#define NUM_BLANK_CYCLES 0

#define OE_PIN 19    // !OE, can be tied to GND if need to save pin
#define DATA_PIN 18  // MOSI
#define LATCH_PIN 17 // RCLK
#define CLOCK_PIN 16 // SCLK

volatile uint8_t displayBuffer[NUM_ROWS] = {
    0b10000001,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00011000,
    0b00100100,
    0b01000010,
    0b10000001};

volatile int curLine = 0;
volatile int blankCycles = 0; // between each line cycle
bool scanDisplay;
volatile int scrollIndex;

void initScan()
{
  pinMode(OE_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  digitalWrite(OE_PIN, LOW);
  digitalWrite(LATCH_PIN, LOW);

  SPI.pins(DATA_PIN, 0xFF, CLOCK_PIN);
  SPI.begin();

  cli();

  // Configure Timer B (TCA0) for CTC mode at 8kHz from 20MHz
  TCA0.SINGLE.CTRLA = 0;                         // Disable timer during setup
  TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_FRQ_gc;  // CTC (Frequency) mode
  TCA0.SINGLE.PER = 311;                         // 20 MHz / (8 × (311 + 1)) = ~8.02 kHz
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV8_gc; // Prescaler = 8
  TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;       // Enable overflow interrupt
  TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;     // Enable timer

  sei();
}

inline void setScanDisplay(bool displayState)
{
  scanDisplay = displayState; // update the display state
  digitalWrite(OE_PIN, !scanDisplay);
}

inline void scroll()
{
  scrollIndex = (scrollIndex + 1) % NUM_COLS;
}

ISR(TCA0_OVF_vect)
{
  uint8_t rowData, rowSelect;

  if (blankCycles > 0 || !scanDisplay)
  {
    rowData = 0xFF;
    rowSelect = 0xFF;
  }
  else
  {
    rowData = ~displayBuffer[curLine];
    rowSelect = ~(0x01 << curLine);
  }

  SPI.transfer(rowData);
  SPI.transfer(rowSelect);
  digitalWrite(LATCH_PIN, LOW);
  digitalWrite(LATCH_PIN, HIGH);

  blankCycles--;
  if (blankCycles < 0)
  {
    curLine = (curLine + 1) % NUM_ROWS;
    blankCycles = 2;
  }

  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

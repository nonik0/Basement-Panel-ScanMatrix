#pragma once

#include <Arduino.h>
#include <SPI.h>

#define NUM_ROWS 8
#define NUM_COLS 8
#define NUM_LEDS 64
#define NUM_BLANK_CYCLES 0

//#define SS_PIN          // unused
#define STATUS_LED_PIN 5
#define LATCH_PIN 16      // RCLK/STB
#define OE_PIN 17         // !OE, can be tied to GND if need to save pin
#define DATA_PIN 18       // MOSI/IN
//#define MISO_PIN 19     // unused
#define CLOCK_PIN 20      // SCLK/CLK

void busyDelay(unsigned long delayCount);
void shiftOutWithDelay(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val);

volatile uint8_t displayBuffer[NUM_ROWS] = {
  0b00000000,
  0b00000000,
  0b00100100,
  0b00000000,
  0b00000000,
  0b00100100,
  0b00011000,
  0b00000000};

volatile int curLine = 0;
volatile int blankCycles = 0; // between each line cycle
bool scanDisplay;
volatile int scrollIndex;

volatile int statusCycleCount = 0;
volatile int statusCycleReset = 100;

void initScan()
{
  pinMode(OE_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  digitalWrite(OE_PIN, LOW);
  digitalWrite(DATA_PIN, LOW);
  digitalWrite(LATCH_PIN, LOW);
  digitalWrite(CLOCK_PIN, LOW);

  // SPI.begin();

  // Configure Timer B (TCA0) for CTC mode at 8kHz from 10MHz
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV2_gc;
  TCB0.CTRLB = TCB_CNTMODE_INT_gc; // CTC mode
  TCB0.CCMP = 1249;                // (20Mhz / 2) / 1250 = 8kHz
  TCB0.INTCTRL = TCB_CAPT_bm;      // Enable interrupt on capture
}

void clear() {
  for (int i = 0; i < NUM_ROWS; i++)
  {
    displayBuffer[i] = 0;
  }
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

inline void busyDelay(unsigned long delayCount)
{
  for (volatile unsigned long i = 0; i < delayCount; i++)
  {
    __asm__ __volatile__("nop");
  }
}

inline void shiftOutWithDelay(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
  for (uint8_t i = 0; i != 8; i++)
  {
    if (bitOrder == LSBFIRST)
    {
      digitalWrite(dataPin, val & 0x01), val >>= 1;
    }
    else
    {
      digitalWrite(dataPin, !!(val & 0x80)), val <<= 1;
    }
    digitalWrite(clockPin, HIGH);
    digitalWrite(clockPin, LOW);
    busyDelay(3);
  }
}

ISR(TCB0_INT_vect)
{
  // clear the interrupt flag
  TCB0.INTFLAGS = TCB_CAPT_bm;

  // blink status to indicate the scan interrupt is running
  if (statusCycleCount++ >= statusCycleReset)
  {
    bool statusLedState = digitalRead(STATUS_LED_PIN);
    digitalWrite(STATUS_LED_PIN, !statusLedState);

    statusCycleCount = 0;
    statusCycleReset = statusLedState ? 4000 : 2000;
  }

  // reduce frequency by factor of 8
  if (statusCycleCount % 4 != 0)
  {
    return;
  }

  uint8_t rowData, rowSelect;
  if (blankCycles > 0 || !scanDisplay)
  {
    rowData = 0xFF;
    rowSelect = 0xFF;
  }
  else
  {
    rowData = ~displayBuffer[curLine];
    rowData = (rowData << scrollIndex) | (rowData >> (NUM_COLS - scrollIndex));
    rowSelect = ~(0x01 << (NUM_ROWS - curLine));
  }

  //SPI.transfer(rowData);
  //SPI.transfer(rowSelect);
  shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, rowData);
  shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, rowSelect);
  //shiftOutWithDelay(DATA_PIN, CLOCK_PIN, LSBFIRST, rowData);
  //shiftOutWithDelay(DATA_PIN, CLOCK_PIN, LSBFIRST, rowSelect);
  digitalWrite(LATCH_PIN, LOW);
  digitalWrite(LATCH_PIN, HIGH);

  blankCycles--;
  if (blankCycles < 0)
  {
    curLine = (curLine + 1) % NUM_ROWS;
    blankCycles = NUM_BLANK_CYCLES;
  }
}

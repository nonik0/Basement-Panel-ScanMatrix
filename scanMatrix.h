#pragma once

#include <Arduino.h>
#include <SPI.h>

// Pin definitions
#define LATCH_PIN 16 // RCLK/STB
#define OE_PIN 17    // !OE, can be tied to GND if need to save pin
// #define DATA_PIN 18   // MOSI/IN
// #define CLOCK_PIN 20  // SCLK/CLK

// Matrix size and type configuration based on build flags
#if defined(MATRIX_16X16)
#define NUM_ROWS 16
#define NUM_COLS 16
#define NUM_LEDS 256
#define NUM_BLANK_CYCLES 0
#define BLANK_DATA 0xFFFF
typedef uint16_t rowdata_t;
#elif defined(MATRIX_8X8)
#define NUM_ROWS 8
#define NUM_COLS 8
#define NUM_LEDS 64
#define NUM_BLANK_CYCLES 2
#define BLANK_DATA 0xFF
typedef uint8_t rowdata_t;
#else
#error "No matrix size defined. Use -DMATRIX_8X8 or -DMATRIX_16X16"
#endif

#define MATRIX_HEIGHT NUM_ROWS
#define MATRIX_WIDTH NUM_COLS

// draw variables
rowdata_t drawBuffer[NUM_ROWS];             // draw updates go here
volatile rowdata_t displayBuffer[NUM_ROWS]; // ISR shifts out data from this, copies new data from drawBuffer

// ISR state variables
volatile bool bufferUpdate = false; // flag to signal ISR that buffer needs to change/be updated
volatile uint8_t curLine = 0;
volatile uint8_t blankCycles = 0; // off cycles between each line write
bool displayEnabled;

void scanClear()
{
    for (int i = 0; i < NUM_ROWS; i++)
    {
        drawBuffer[i] = 0;
    }
}

void scanDisplay(bool enabled)
{
    displayEnabled = enabled;
    digitalWrite(OE_PIN, !displayEnabled);
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

void scanSetPixel(int x, int y, bool on)
{
    if (x < 0 || x >= NUM_COLS || y < 0 || y >= NUM_ROWS)
        return;

    if (on)
        drawBuffer[y] |= (1 << x);
    else
        drawBuffer[y] &= ~(1 << x);
}

void scanSetRow(uint8_t row, rowdata_t rowData) {
    drawBuffer[row] = rowData;
}

void scanShow()
{
    bufferUpdate = true;
}

ISR(TCB0_INT_vect)
{
    // clear interrupt flag
    TCB0.INTFLAGS = TCB_CAPT_bm;

    // calculate row data and select
    rowdata_t rowData = BLANK_DATA;
    rowdata_t rowSelect = BLANK_DATA;
    if (displayEnabled && blankCycles == 0)
    {
        rowData = ~displayBuffer[curLine];
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
    if (blankCycles == 0)
    {
        curLine = (curLine + 1) % NUM_ROWS;
        blankCycles = NUM_BLANK_CYCLES;
    }
    else {
        blankCycles--;
    }

    // swap in new frame if available after finishing last frame
    if (bufferUpdate && curLine == 0 && blankCycles == NUM_BLANK_CYCLES)
    {
        for (int i = 0; i < NUM_ROWS; i++)
        {
            displayBuffer[i] = drawBuffer[i];
        }

        bufferUpdate = false;
    }
}
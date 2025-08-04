#pragma once

#include "drawText.h"
#include "scanMatrix.h"

#define MAX_MESSAGE_SIZE 140
#define MESSAGE_X_OFFSET 3
#if defined(MATRIX_16X16)
#define MESSAGE_Y_OFFSET 5
#elif defined(MATRIX_8X8)
#define MESSAGE_Y_OFFSET 2
#endif

char scrollMessage[MAX_MESSAGE_SIZE];
int16_t scrollMessageWidth;
int16_t scrollMessageX = NUM_COLS;
int16_t scrollMessageY = NUM_ROWS;
unsigned long lastTempMessage = 0; // time left for temporary message display

void scrollTextSetMessage(const char *newMessage)
{
  strncpy(scrollMessage, newMessage, MAX_MESSAGE_SIZE - 1);
  scrollMessage[MAX_MESSAGE_SIZE - 1] = '\0';
  scrollMessageWidth = getTextWidth(scrollMessage);
  scrollMessageX = MATRIX_WIDTH;
}

void scrollText() {
  scanClear();
  drawString(scrollMessageX, MATRIX_HEIGHT - MESSAGE_Y_OFFSET, MATRIX_WIDTH, MATRIX_HEIGHT, scrollMessage, true);
  scanShow();

  if (--scrollMessageX < -scrollMessageWidth)
  {
    scrollMessageX = MATRIX_WIDTH;
  }
}
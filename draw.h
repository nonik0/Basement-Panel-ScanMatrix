#pragma once

#include "Picopixel.h"

extern void drawPixel(uint16_t x, uint16_t y, uint32_t color);

uint8_t getCharWidth(unsigned char c)
{
  uint16_t first = pgm_read_byte(&Picopixel.first);
  uint16_t last = pgm_read_byte(&Picopixel.last);
  uint8_t charWidth = 0;

  if ((c >= first) && (c <= last)) // Char present in this font?
  {
    GFXglyph *glyph = &(((GFXglyph *)pgm_read_ptr(&Picopixel.glyph))[c - first]);
    charWidth = pgm_read_byte(&glyph->xAdvance);
  }

  return charWidth;
}

int getTextWidth(const char *str)
{
  char c;
  uint16_t width = 0;
  while ((c = *str++))
  {
    width += getCharWidth(c);
  }
  return width;
}

void drawChar(uint16_t x, uint16_t y, char c, uint32_t color, uint8_t &glyphWidth)
{
  uint8_t first = pgm_read_byte(&Picopixel.first);
  uint8_t last = pgm_read_byte(&Picopixel.last);
  if ((c < first) || (c > last)) // Char present in this font?
  {
    glyphWidth = 0;
    return;
  }

  GFXglyph *glyph = &(((GFXglyph *)pgm_read_ptr(&Picopixel.glyph))[c - first]);
  uint8_t *bitmap = (uint8_t *)pgm_read_ptr(&Picopixel.bitmap);
  int count = 0;

  uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
  uint8_t w = pgm_read_byte(&glyph->width);
  uint8_t h = pgm_read_byte(&glyph->height);
  int8_t xo = pgm_read_byte(&glyph->xOffset);
  int8_t yo = pgm_read_byte(&glyph->yOffset);
  uint8_t xx, yy, bits = 0, bit = 0;

  for (yy = 0; yy < h; yy++)
  {
    for (xx = 0; xx < w; xx++)
    {
      if (!(bit++ & 7))
      {
        bits = pgm_read_byte(&bitmap[bo++]);
      }
      if (bits & 0x80)
      {
        drawPixel(x + xo + xx, y + yo + yy, color);
        count++;
      }
      bits <<= 1;
    }
  }

  glyphWidth = (uint8_t)pgm_read_byte(&glyph->xAdvance);
}

void drawString(int16_t x, int16_t y, int16_t max_x, int16_t max_y, const char *str, uint32_t color)
{
  if (y < 0 || y >= max_y)
  {
    return;
  }

  while (*str && x < max_x)
  {
    if (x < 0)
    {
      str++;
      continue;
    }

    uint8_t charWidth = 0;
    drawChar(x, y, *str, color, charWidth);
    x += charWidth;
    str++;
  }
}

/*
 * Troy's 8-bit computer - Emulator
 *
 * Copyright (c) 2020 Troy Schrapel
 *
 * This code is licensed under the MIT license
 *
 * https://github.com/visrealm/vrcpu
 *
 */

#include "lcd.h"
#include <stdlib.h>
#include <memory.h>

#define CHAR_WIDTH  5
#define CHAR_HEIGHT 8
#define DATA_WIDTH 40
#define DATA_HEIGHT 4

struct LCD_s
{
	int width;
	int height;

	byte entryModeFlags;
	byte displayFlags;

	char* data;
	char* ptr;
	char* pixels;
	int scrollOffset;

	int pixelsWidth;
	int pixelsHeight;
	int numPixels;
};


DLLEXPORT LCD* newLCD(int width, int height)
{
	LCD* lcd = (LCD*)malloc(sizeof(LCD));
	if (lcd != NULL)
	{
		if (height > 4) height = 4;

		lcd->width = width;
		lcd->height = height;

		size_t datalen = LINE_WIDTH * height;
		lcd->data = (char*)malloc(datalen);
		if (lcd->data != NULL)
		{
			memset(lcd->data, 0, datalen);
		}
		lcd->ptr = lcd->data;
		lcd->scrollOffset = 0;
		lcd->pixelsWidth  = lcd->width * (CHAR_WIDTH + 1) - 1;
		lcd->pixelsHeight = lcd->height * (CHAR_HEIGHT + 1) - 1;
		lcd->numPixels = lcd->pixelsWidth * lcd->pixelsHeight;
		lcd->pixels = (char*)malloc(lcd->numPixels);
		memset(lcd->pixels, -1, lcd->numPixels);
		updatePixels(lcd);
	}
	return lcd;
}

DLLEXPORT void destroyLCD(LCD* lcd)
{
	if (lcd)
	{
		free(lcd->data);
		free(lcd->pixels);
		memset(lcd, 0, sizeof(lcd));
		free(lcd);
	}
}

DLLEXPORT void sendCommand(LCD* lcd, byte command)
{
	if (command & LCD_CMD_SET_DRAM_ADDR)
	{
		int offset = (command & 0x7f);
		if (offset < 0)
		{
			offset = 0;
		}
		else if (offset >= LINE_WIDTH * lcd->height)
		{
			offset = LINE_WIDTH * lcd->height - 1;
		}
		lcd->ptr = lcd->data + offset;
	}
	else if (command & LCD_CMD_SET_CGRAM_ADDR)
	{

	}
	else if (command & LCD_CMD_SHIFT)
	{
		if (command & LCD_CMD_SHIFT_CURSOR)
		{

		}
	}
	else if (command & LCD_CMD_DISPLAY)
	{
		lcd->displayFlags = command;
	}
	else if (command & LCD_CMD_ENTRY_MODE)
	{
		lcd->entryModeFlags = command;
	}
	else if (command & LCD_CMD_HOME)
	{
		lcd->ptr = lcd->data;
	}
	else if (command & LCD_CMD_CLEAR)
	{
		if (lcd->data != NULL)
		{
			size_t datalen = ((size_t)lcd->width + 1) * lcd->height;
			memset(lcd->data, 0, datalen);
		}
		lcd->ptr = lcd->data;
	}
}

void increment(LCD* lcd)
{
	++lcd->ptr;
	size_t offset = lcd->ptr - lcd->data;
	if ((offset + 1) % (LINE_WIDTH) == 0)
	{
		++lcd->ptr;
	}
	if (lcd->ptr >= lcd->data + (lcd->width + 1) * lcd->height)
	{
		lcd->ptr = lcd->data;
	}
}

DLLEXPORT void writeByte(LCD* lcd, byte data)
{
	*lcd->ptr = data;
	if (lcd->entryModeFlags & LCD_CMD_ENTRY_MODE_INCREMENT)
	{
		increment(lcd);
	}
}

DLLEXPORT void writeString(LCD* lcd, const char* str)
{
	const char* ptr = str;
	while (*ptr != '\0')
	{
		writeByte(lcd, *ptr++);
	}
}


DLLEXPORT byte readByte(LCD* lcd)
{
	return *lcd->ptr;
}

DLLEXPORT const char* readLine(LCD* lcd, int row)
{
	return lcd->data + (row * (lcd->width + 1));
}

static const byte lcd_font[128][CHAR_WIDTH];
DLLEXPORT const byte* charBits(byte c)
{
	if (c < 128)
	{
		return lcd_font[c];
	}
	return lcd_font[0];
}


DLLEXPORT void updatePixels(LCD* lcd)
{
	char *pixel = lcd->pixels;

	//printf("pixelsWidth: %d\n", lcd->pixelsWidth);
	//printf("pixelsHeight: %d\n", lcd->pixelsHeight);

	for (int row = 0; row < lcd->height; ++row)
	{
		for (int col = 0; col < lcd->width; ++col)
		{
			char *charTopLeft = lcd->pixels + (row * (CHAR_HEIGHT + 1) * lcd->pixelsWidth) + col * (CHAR_WIDTH + 1);
			char c = *(lcd->data + (row * (lcd->width + 1) + col));

			const byte* bits = charBits(c);

			for (int y = 0; y < CHAR_HEIGHT; ++y)
			{
				pixel = charTopLeft + y * lcd->pixelsWidth;
				for (int x = 0; x < CHAR_WIDTH; ++x)
				{
					*pixel = (bits[x] & (0x80 >> y)) ? 1 : 0;
					++pixel;
				}
			}
		}
	}
}

DLLEXPORT void numPixels(LCD* lcd, int* width, int* height)
{
	if (width) *width = lcd->pixelsWidth;
	if (height) *height = lcd->pixelsHeight;
}

DLLEXPORT char pixelState(LCD* lcd, int x, int y)
{
	int offset = y * lcd->pixelsWidth + x;
	if (offset >= 0 && offset < lcd->numPixels)
		return lcd->pixels[offset];
	return -1;
}


static const byte lcd_font[128][5] = {
		{0x00, 0x00, 0x00, 0x00, 0x00}, //   0 - space
		{0x7c, 0xa2, 0x8a, 0xa2, 0x7c}, //   1 - light smiley face
		{0x7c, 0xd6, 0xf6, 0xd6, 0x7c}, //   2 - dark smiley face
		{0x38, 0x7c, 0x3e, 0x7c, 0x38}, //   3 - full heart
		{0x00, 0x38, 0x1c, 0x38, 0x00}, //   4 - small heart
		{0x0c, 0x6c, 0xfe, 0x6c, 0x0c}, //   5 - club
		{0x18, 0x3a, 0x7e, 0x3a, 0x18}, //   6 - spade
		{0x00, 0x18, 0x18, 0x00, 0x00}, //   7 - bullet
		{0xff, 0xe7, 0xe7, 0xff, 0xff}, //   8 - big rectangle
		{0x3c, 0x24, 0x24, 0x3c, 0x00}, //   9 - small rectangle
		{0xc3, 0xdb, 0xdb, 0xc3, 0xff}, //  10 - filled rectangle
		{0x0c, 0x12, 0x52, 0x6c, 0x70}, //  11 - man symbol
		{0x60, 0x94, 0x9e, 0x94, 0x60}, //  12 - woman symbol
		{0x06, 0x0e, 0xfc, 0x40, 0x20}, //  13 - musical note
		{0x06, 0x7e, 0x50, 0xac, 0xfc}, //  14 - double music note
		{0x18, 0x24, 0x24, 0x24, 0x18}, //  15 - record
		{0x00, 0xfe, 0x7c, 0x38, 0x10}, //  16 - play
		{0x10, 0x38, 0x7c, 0xfe, 0x00}, //  17 - play backwards
		{0x7e, 0x7e, 0x00, 0x7e, 0x7e}, //  18 - pause
		{0x3c, 0x3c, 0x3c, 0x3c, 0x00}, //  19 - stop
		{0x0a, 0x3a, 0xfa, 0x3a, 0x0a}, //  20 - eject
		{0xfe, 0x7c, 0x38, 0x10, 0xfe}, //  21 - fwd
		{0xfe, 0x10, 0x38, 0x7c, 0xfe}, //  22 - rev
		{0x01, 0x01, 0x01, 0x01, 0x01}, //  23 - lower 1/8 block (full)
		{0x03, 0x03, 0x03, 0x03, 0x03}, //  24 - lower 1/4 block (full)
		{0x07, 0x07, 0x07, 0x07, 0x07}, //  25 - lower 3/8 block (full)
		{0x0f, 0x0f, 0x0f, 0x0f, 0x0f}, //  26 - lower 1/2 block (full)
		{0x1f, 0x1f, 0x1f, 0x1f, 0x1f}, //  27 - lower 5/8 block (full)
		{0x3f, 0x3f, 0x3f, 0x3f, 0x3f}, //  28 - lower 3/4 block (full)
		{0x7f, 0x7f, 0x7f, 0x7f, 0x7f}, //  29 - lower 7/8 block (full)
		{0x00, 0x00, 0x00, 0x00, 0x00}, //  30
		{0x00, 0x00, 0x00, 0x00, 0x00}, //  31
		{0x00, 0x00, 0x00, 0x00, 0x00}, //  32 - space
		{0x00, 0x60, 0xfa, 0x60, 0x00}, //  33 - !
		{0xe0, 0xc0, 0x00, 0xe0, 0xc0}, //  34 - "
		{0x24, 0x7e, 0x24, 0x7e, 0x24}, //  35 - #
		{0x24, 0x54, 0xd6, 0x48, 0x00}, //  36 - $
		{0xc6, 0xc8, 0x10, 0x26, 0xc6}, //  37 - %
		{0x6c, 0x92, 0x6a, 0x04, 0x0a}, //  38 - &
		{0x00, 0xe0, 0xc0, 0x00, 0x00}, //  39 - '
		{0x00, 0x7c, 0x82, 0x00, 0x00}, //  40 - (
		{0x00, 0x82, 0x7c, 0x00, 0x00}, //  41 - )
		{0x10, 0x7c, 0x38, 0x7c, 0x10}, //  42 - *
		{0x10, 0x10, 0x7c, 0x10, 0x10}, //  43 - +
		{0x00, 0x07, 0x06, 0x00, 0x00}, //  44 - ,
		{0x10, 0x10, 0x10, 0x10, 0x10}, //  45 - -
		{0x00, 0x06, 0x06, 0x00, 0x00}, //  46 - .
		{0x04, 0x08, 0x10, 0x20, 0x40}, //  47 - /
		{0x7c, 0x8a, 0x92, 0xa2, 0x7c}, //  48 - 0
		{0x00, 0x42, 0xfe, 0x02, 0x00}, //  49 - 1
		{0x46, 0x8a, 0x92, 0x92, 0x62}, //  50 - 2
		{0x44, 0x92, 0x92, 0x92, 0x6c}, //  51 - 3
		{0x18, 0x28, 0x48, 0xfe, 0x08}, //  52 - 4
		{0xf4, 0x92, 0x92, 0x92, 0x8c}, //  53 - 5
		{0x3c, 0x52, 0x92, 0x92, 0x0c}, //  54 - 6
		{0x80, 0x8e, 0x90, 0xa0, 0xc0}, //  55 - 7
		{0x6c, 0x92, 0x92, 0x92, 0x6c}, //  56 - 8
		{0x60, 0x92, 0x92, 0x94, 0x78}, //  57 - 9
		{0x00, 0x6c, 0x6c, 0x00, 0x00}, //  58 - :
		{0x00, 0x37, 0x36, 0x00, 0x00}, //  59 - ;
		{0x10, 0x28, 0x44, 0x82, 0x00}, //  60 - <
		{0x24, 0x24, 0x24, 0x24, 0x24}, //  61 - =
		{0x00, 0x82, 0x44, 0x28, 0x10}, //  62 - >
		{0x40, 0x80, 0x9a, 0x90, 0x60}, //  63 - ?
		{0x7c, 0x82, 0xba, 0xaa, 0x78}, //  64 - @
		{0x7e, 0x88, 0x88, 0x88, 0x7e}, //  65 - A
		{0xfe, 0x92, 0x92, 0x92, 0x6c}, //  66 - B
		{0x7c, 0x82, 0x82, 0x82, 0x44}, //  67 - C
		{0xfe, 0x82, 0x82, 0x82, 0x7c}, //  68 - D
		{0xfe, 0x92, 0x92, 0x92, 0x82}, //  69 - E
		{0xfe, 0x90, 0x90, 0x90, 0x80}, //  70 - F
		{0x7c, 0x82, 0x92, 0x92, 0x5e}, //  71 - G
		{0xfe, 0x10, 0x10, 0x10, 0xfe}, //  72 - H
		{0x00, 0x82, 0xfe, 0x82, 0x00}, //  73 - I
		{0x0c, 0x02, 0x02, 0x02, 0xfc}, //  74 - J
		{0xfe, 0x10, 0x28, 0x44, 0x82}, //  75 - K
		{0xfe, 0x02, 0x02, 0x02, 0x02}, //  76 - L
		{0xfe, 0x40, 0x20, 0x40, 0xfe}, //  77 - M
		{0xfe, 0x40, 0x20, 0x10, 0xfe}, //  78 - N
		{0x7c, 0x82, 0x82, 0x82, 0x7c}, //  79 - O
		{0xfe, 0x90, 0x90, 0x90, 0x60}, //  80 - P
		{0x7c, 0x82, 0x8a, 0x84, 0x7a}, //  81 - Q
		{0xfe, 0x90, 0x90, 0x98, 0x66}, //  82 - R
		{0x64, 0x92, 0x92, 0x92, 0x4c}, //  83 - S
		{0x80, 0x80, 0xfe, 0x80, 0x80}, //  84 - T
		{0xfc, 0x02, 0x02, 0x02, 0xfc}, //  85 - U
		{0xf8, 0x04, 0x02, 0x04, 0xf8}, //  86 - V
		{0xfc, 0x02, 0x3c, 0x02, 0xfc}, //  87 - W
		{0xc6, 0x28, 0x10, 0x28, 0xc6}, //  88 - X
		{0xe0, 0x10, 0x0e, 0x10, 0xe0}, //  89 - Y
		{0x8e, 0x92, 0xa2, 0xc2, 0x00}, //  90 - Z
		{0x00, 0xfe, 0x82, 0x82, 0x00}, //  91 - [
		{0x40, 0x20, 0x10, 0x08, 0x04}, //  92 - slash
		{0x00, 0x82, 0x82, 0xfe, 0x00}, //  93 - ]
		{0x20, 0x40, 0x80, 0x40, 0x20}, //  94 - ^
		{0x01, 0x01, 0x01, 0x01, 0x01}, //  95 - _
		{0x00, 0xc0, 0xe0, 0x00, 0x00}, //  96 - `
		{0x04, 0x2a, 0x2a, 0x2a, 0x1e}, //  97 - a
		{0xfe, 0x22, 0x22, 0x22, 0x1c}, //  98 - b
		{0x1c, 0x22, 0x22, 0x22, 0x14}, //  99 - c
		{0x1c, 0x22, 0x22, 0x22, 0xfe}, // 100 - d
		{0x1c, 0x2a, 0x2a, 0x2a, 0x10}, // 101 - e
		{0x10, 0x7e, 0x90, 0x90, 0x00}, // 102 - f
		{0x18, 0x25, 0x25, 0x25, 0x3e}, // 103 - g
		{0xfe, 0x20, 0x20, 0x1e, 0x00}, // 104 - h
		{0x00, 0x00, 0xbe, 0x02, 0x00}, // 105 - i
		{0x02, 0x01, 0x21, 0xbe, 0x00}, // 106 - j
		{0xfe, 0x08, 0x14, 0x22, 0x00}, // 107 - k
		{0x00, 0x00, 0xfe, 0x02, 0x00}, // 108 - l
		{0x3e, 0x20, 0x18, 0x20, 0x1e}, // 109 - m
		{0x3e, 0x20, 0x20, 0x1e, 0x00}, // 110 - n
		{0x1c, 0x22, 0x22, 0x22, 0x1c}, // 111 - o
		{0x3f, 0x22, 0x22, 0x22, 0x1c}, // 112 - p
		{0x1c, 0x22, 0x22, 0x22, 0x3f}, // 113 - q
		{0x22, 0x1e, 0x22, 0x20, 0x10}, // 114 - r
		{0x10, 0x2a, 0x2a, 0x2a, 0x04}, // 115 - s
		{0x20, 0x7c, 0x22, 0x24, 0x00}, // 116 - t
		{0x3c, 0x02, 0x04, 0x3e, 0x00}, // 117 - u
		{0x38, 0x04, 0x02, 0x04, 0x38}, // 118 - v
		{0x3c, 0x06, 0x0c, 0x06, 0x3c}, // 119 - w
		{0x36, 0x08, 0x08, 0x36, 0x00}, // 120 - x
		{0x39, 0x05, 0x06, 0x3c, 0x00}, // 121 - y
		{0x26, 0x2a, 0x2a, 0x32, 0x00}, // 122 - z
		{0x10, 0x7c, 0x82, 0x82, 0x00}, // 123 - {
		{0x00, 0x00, 0xff, 0x00, 0x00}, // 124 - |
		{0x00, 0x82, 0x82, 0x7c, 0x10}, // 125 - }
		{0x40, 0x80, 0x40, 0x80, 0x00}, // 126 - ~
		{0x01, 0x01, 0x01, 0x01, 0x00} };

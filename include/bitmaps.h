#pragma once
#include <Arduino.h>

const unsigned char bmpWiFi []  = PROGMEM {
	0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x1f, 0xf8, 0x7f, 0xfe, 0xf8, 0x1f, 0x63, 0xc6, 0x0f, 0xf0, 
	0x1f, 0xf8, 0x08, 0x10, 0x03, 0xc0, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char bmpNoWiFi [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x03, 0x8c, 0x1f, 0x98, 0x30, 0x3c, 0x4f, 0x72, 0x18, 0x98, 
	0x01, 0xc0, 0x03, 0x20, 0x07, 0x80, 0x0b, 0xc0, 0x11, 0x80, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char bmpSettings [] PROGMEM = {
	0x00, 0x00, 0x01, 0x80, 0x03, 0xc0, 0x06, 0x60, 0x3c, 0x3c, 0x21, 0x84, 0x33, 0xcc, 0x16, 0x68, 
	0x16, 0x68, 0x33, 0xcc, 0x21, 0x84, 0x3c, 0x3c, 0x06, 0x60, 0x03, 0xc0, 0x01, 0x80, 0x00, 0x00
};

const unsigned char bmpManager [] PROGMEM = {
	0x03, 0x02, 0x04, 0x04, 0x05, 0x08, 0x20, 0x30, 0x50, 0x50, 0x48, 0xa0, 0x45, 0x40, 0x42, 0x8a, 
	0x41, 0x02, 0x40, 0x8c, 0x20, 0x40, 0x10, 0x20, 0x38, 0x10, 0x6f, 0xe0, 0xc4, 0x00, 0xfc, 0x00
};
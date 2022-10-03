#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <Adafruit_SH110X.h>
#include <stdio.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, OLED_RESET);


#endif /* DISPLAY_H_ */
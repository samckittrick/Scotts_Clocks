/*-----------------------------------------------------
   Monochron Advanced Features
   By: Scott McKittrick
   
   This file contains advanced features for the 
   Monochron clock kit.
   
------------------------------------------------------*/
#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <string.h>
#include "util.h"
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++
   Autodimming Backlight
   -Allows the backlight to be automatically dimmed 
    at specified times.
    
   To Activate uncomment "#define AUTODIM" in ratt.h
   
   Required resources:
      .text: 
      .data: 
+++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifdef AUTODIM

void autoDim(uint8_t hour, uint8_t minute)
{
   if((hour == 6) && (minute == 0))
   {
      OCR2B = 10;
   }
   
   if((hour == 11) && (minute == 0))
   {
      OCR2B = 1;
   }
}

setBacklightAutoDim()
{
   uint8_t mode = SET_BRIGHTNESS;
}

#endif
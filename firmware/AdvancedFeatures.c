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
//the time variables hold the time of day in minutes
// so 22:15 would be 22*60+15 = 1335
volatile uint16_t autodim_day_time = 360;
volatile uint16_t autodim_night_time = 1380;

volatile uint8_t autodim_day_bright = 11;
volatile uint8_t autodim_night_bright = 1;

void autoDim(uint8_t hour, uint8_t minute)
{
   //start by shifting the time scale to the left using autodim_night_time as the zero value. This simplifies comparisons.
   int16_t temp;
   uint16_t dayTimeStandard;
   temp = autodim_day_time - autodim_night_time;
   if(temp < 0)
   {
      dayTimeStandard = 1440 - (autodim_night_time - autodim_day_time);
   }
   else
   {
      dayTimeStandard = temp;
   }
   
   //find current time
   uint16_t curTimeStandard = generateTimeCombo(hour, minute);
   //standardize it
   temp = curTimeStandard - autodim_night_time;
   if(temp < 0)
   {
      curTimeStandard = 1440 - (autodim_night_time - curTimeStandard);
   }
   else
   {
      curTimeStandard = temp;
   }
   
   //determine what brightness setting to use
   //since the night time is now the zero value, anything less than the standardized is night time
   //and anything more than or equal to the standardized day time is daytime.
   if(curTimeStandard < dayTimeStandard)
   {
      OCR2B = autodim_night_bright;
   }
   else
   {
      OCR2B = autodim_day_bright;
   }
}

uint16_t generateTimeCombo(uint8_t hour, uint8_t minute)
{
   uint16_t timeCombo = (uint16_t)hour*60;
   return timeCombo + minute;
}

void setBacklightAutoDim()
{
   uint8_t mode = SET_BRIGHTNESS;
}

#endif
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

//Some variables used in multiple features
extern volatile uint8_t screenmutex;
extern volatile uint8_t just_pressed, pressed;
extern volatile uint8_t timeoutcounter;
extern volatile uint8_t time_format;
extern volatile uint8_t time_h, time_m, time_s;
extern volatile uint8_t date_d, date_m, date_y;


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++
   Autodimming Backlight
   -Allows the backlight to be automatically dimmed 
    at specified times.
    
   To Activate uncomment "#define AUTODIM" in ratt.h
   
   Required resources:
      .text: 58 bytes
      .data: 19924
+++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifdef AUTODIM
//enums   
#define AUTODIM_SET_DAY_BRIGHT 1
#define AUTODIM_SET_DAY_H 2
#define AUTODIM_SET_DAY_M 3
#define AUTODIM_SET_NIGHT_BRIGHT 4
#define AUTODIM_SET_NIGHT_H 5
#define AUTODIM_SET_NIGHT_M 6
#define AUTODIM_DAY_BRIGHT 7
#define AUTODIM_DAY_TIME 8
#define AUTODIM_NIGHT_BRIGHT 9
#define AUTODIM_NIGHT_TIME 10

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
   uint16_t curTimeStandard = (hour*60)+minute;
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

void setBacklightAutoDim()
{
   uint8_t mode = AUTODIM_NIGHT_TIME;
   uint8_t hour;
   uint8_t minute;
   screenmutex++; //exclude all other writes to the screen
   glcdClearScreen();
   
   //title
   glcdSetAddress(28, 0);
   glcdPutStr("AutoDim Menu", NORMAL);
   //bottom instructions
   glcdSetAddress(0, 7);
   glcdPutStr("Press Menu to Advance", NORMAL);
   glcdSetAddress(0, 6);
   glcdPutStr("Press Set to set ", NORMAL);
   
   //time 1
   glcdSetAddress(MENU_INDENT, 1);
   glcdPutStr("Set Time 1:", NORMAL);
   glcdSetAddress(GLCD_XPIXELS - 36, 1);
   hour = autodim_night_time/60;
   minute = autodim_night_time%60;
   print_timehour(hour, NORMAL);
   glcdWriteChar(':', NORMAL);
   printnumber(minute, NORMAL);
   
   if(time_format == TIME_12H)
   {
      if(hour > 12)
      {
         glcdWriteChar('P', NORMAL);
      }
      else
      {
         glcdWriteChar('A', NORMAL);
      }
   }  
   
   //brightness 1
   glcdSetAddress(MENU_INDENT, 2);
   glcdPutStr("Set Brightness: ", NORMAL);
   glcdSetAddress(GLCD_XPIXELS-12, 2);
   printnumber(autodim_night_bright>>OCR2B_BITSHIFT, NORMAL);
   
   //time 2
   glcdSetAddress(MENU_INDENT, 4);
   glcdPutStr("Set Time 2: ", NORMAL);
   glcdSetAddress(GLCD_XPIXELS - 36, 4);
   hour = autodim_day_time/60;
   minute = autodim_day_time%60;
   print_timehour(hour, NORMAL);
   glcdWriteChar(':', NORMAL);
   printnumber(minute, NORMAL);
   
   if(time_format == TIME_12H)
   {
      if(hour > 12)
      {
         glcdWriteChar('P', NORMAL);
      }
      else
      {
         glcdWriteChar('A', NORMAL);
      }
   }
   
   //brightness 2
   glcdSetAddress(MENU_INDENT,5);
   glcdPutStr("Set Brightness: ", NORMAL);
   glcdSetAddress(GLCD_XPIXELS - 12, 5);
   printnumber(autodim_day_bright>>OCR2B_BITSHIFT, NORMAL);
   
   drawArrow(0, 11, MENU_INDENT -1);
   
   uint8_t exit = 0;
   while(!exit)
   {
      
      if(just_pressed & 0x1)
      {
         just_pressed = 0;
         switch(mode)
         {
            case AUTODIM_NIGHT_TIME:
               mode = AUTODIM_NIGHT_BRIGHT;
               glcdFillRectangle(0, 0, MENU_INDENT -1, 48, NORMAL);
               drawArrow(0, 19, MENU_INDENT -1);
               break;
            
            case AUTODIM_NIGHT_BRIGHT:
               mode = AUTODIM_DAY_TIME;
               glcdFillRectangle(0, 0, MENU_INDENT -1, 48, NORMAL);
               drawArrow(0, 35, MENU_INDENT -1);
               break;
            
            case AUTODIM_DAY_TIME:
               mode = AUTODIM_DAY_BRIGHT;
               glcdFillRectangle(0, 0, MENU_INDENT -1, 48, NORMAL);
               drawArrow(0, 43, MENU_INDENT - 1);
               glcdSetAddress(0, 7);
               glcdPutStr("Press Menu to Exit    ", NORMAL);
               break;
               
            case AUTODIM_SET_NIGHT_H:
            case AUTODIM_SET_NIGHT_M:
            case AUTODIM_SET_DAY_H:
            case AUTODIM_SET_DAY_M:
            case AUTODIM_SET_NIGHT_BRIGHT:
            case AUTODIM_SET_DAY_BRIGHT:
               break;
               
            default:
               exit = 1;
               break;
         }
      }
      if(just_pressed || pressed)
      {
         timeoutcounter = INACTIVITYTIMEOUT;
      }
      else if(!timeoutcounter)
      {
         exit = 1;
         continue;
      }
      
      if(just_pressed & 0x2)
      {
         just_pressed = 0;
         switch(mode)
         {
            //Nighttime Brightness
            case AUTODIM_NIGHT_BRIGHT:
               mode = AUTODIM_SET_NIGHT_BRIGHT;
               OCR2B = autodim_night_bright;
               glcdSetAddress(GLCD_XPIXELS -12, 2);
               printnumber(autodim_night_bright>>OCR2B_BITSHIFT, INVERTED);
               glcdSetAddress(0, 6);
               glcdPutStr("Press + to change", NORMAL);
               break;
            case AUTODIM_SET_NIGHT_BRIGHT:
               mode = AUTODIM_NIGHT_BRIGHT;
               glcdSetAddress(GLCD_XPIXELS -12, 2);
               printnumber(OCR2B>>OCR2B_BITSHIFT, NORMAL);
               autodim_night_bright = OCR2B;
               #ifdef AUTODIM_EEPROM
               if(autodim_night_bright != eeprom_read_byte((uint8_t *)EE_AUTODIM_NIGHT_BRIGHT))
                  eeprom_write_byte((uint8_t *)EE_AUTODIM_NIGHT_BRIGHT, autodim_night_bright);
               #endif
               autoDim(time_h, time_m);
               glcdSetAddress(0, 6);
               glcdPutStr("Press Set to set ", NORMAL);
               break;            
               
            //Daytime Brightness
            case AUTODIM_DAY_BRIGHT:
               mode = AUTODIM_SET_DAY_BRIGHT;
               OCR2B = autodim_day_bright;
               glcdSetAddress(GLCD_XPIXELS -12, 5);
               printnumber(autodim_day_bright>>OCR2B_BITSHIFT, INVERTED);
               glcdSetAddress(0, 6);
               glcdPutStr("Press + to change", NORMAL);
               break;
            case AUTODIM_SET_DAY_BRIGHT:
               mode = AUTODIM_DAY_BRIGHT;
               glcdSetAddress(GLCD_XPIXELS -12, 5);
               printnumber(OCR2B>>OCR2B_BITSHIFT, NORMAL);
               autodim_day_bright = OCR2B;
               #ifdef AUTODIM_EEPROM
               if(autodim_day_bright != eeprom_read_byte((uint8_t *)EE_AUTODIM_DAY_BRIGHT))
                  eeprom_write_byte((uint8_t *)EE_AUTODIM_DAY_BRIGHT, autodim_day_bright);
               #endif
               autoDim(time_h, time_m);
               glcdSetAddress(0, 6);
               glcdPutStr("Press Set to set ", NORMAL);
               break;
            

            //Day time
            case AUTODIM_DAY_TIME:
               mode = AUTODIM_SET_DAY_H;
               glcdSetAddress(GLCD_XPIXELS - 36, 4);
               print_timehour(autodim_day_time/60, INVERTED);
               autoDim(time_h, time_m);
               glcdSetAddress(0, 6);
               glcdPutStr("Press + to change", NORMAL);
               break;
            case AUTODIM_SET_DAY_H:
               mode = AUTODIM_SET_DAY_M;
               glcdSetAddress(GLCD_XPIXELS - 36, 4);
               print_timehour(autodim_day_time/60, NORMAL);
               glcdSetAddress(GLCD_XPIXELS - 18, 4);
               printnumber(autodim_day_time%60, INVERTED);
               break;
            case AUTODIM_SET_DAY_M:
               mode = AUTODIM_DAY_TIME;
               glcdSetAddress(GLCD_XPIXELS - 18, 4);
               printnumber(autodim_day_time%60, NORMAL);
               glcdSetAddress(0, 6);
               glcdPutStr("Press Set to set ", NORMAL);
               #ifdef AUTODIM_EEPROM
               if(autodim_day_time != eeprom_read_word((uint16_t *)EE_AUTODIM_DAY_TIME))
                  eeprom_write_word((uint16_t *)EE_AUTODIM_DAY_TIME, autodim_day_time);
               #endif
               break;

            //Night Time
            case AUTODIM_NIGHT_TIME:
               mode = AUTODIM_SET_NIGHT_H;
               glcdSetAddress(GLCD_XPIXELS - 36, 1);
               print_timehour(autodim_night_time/60, INVERTED);
               autoDim(time_h, time_m);
               glcdSetAddress(0, 6);
               glcdPutStr("Press + to change", NORMAL);
               break;
            case AUTODIM_SET_NIGHT_H:
               mode = AUTODIM_SET_NIGHT_M;
               glcdSetAddress(GLCD_XPIXELS - 36, 1);
               print_timehour(autodim_night_time/60, NORMAL);
               glcdSetAddress(GLCD_XPIXELS - 18, 1);
               printnumber(autodim_night_time%60, INVERTED);
               break;
            case AUTODIM_SET_NIGHT_M:
               mode = AUTODIM_NIGHT_TIME;
               glcdSetAddress(GLCD_XPIXELS - 18, 1);
               printnumber(autodim_night_time%60, NORMAL);
               glcdSetAddress(0, 6);
               glcdPutStr("Press Set to set ", NORMAL);
               #ifdef AUTODIM_EEPROM
               if(autodim_night_time != eeprom_read_word((uint16_t *)EE_AUTODIM_NIGHT_TIME))
                  eeprom_write_word((uint16_t *)EE_AUTODIM_NIGHT_TIME, autodim_night_time);
               #endif
               break;
            
            default:
               break;
         }
         
      }
      
      if((just_pressed & 0x4) || (pressed & 0x4))
      {
         just_pressed = 0;
         
         //night brightness
         if(mode == AUTODIM_SET_NIGHT_BRIGHT)
         {
            OCR2B += OCR2B_PLUS;
            if(OCR2B > OCR2A_VALUE)
            {
               OCR2B = 0;
            }
            
            glcdSetAddress(GLCD_XPIXELS - 12, 2);
            printnumber(OCR2B>>OCR2B_BITSHIFT, INVERTED);
         }  

         //day brightness
         if(mode == AUTODIM_SET_DAY_BRIGHT)
         {
            OCR2B += OCR2B_PLUS;
            if(OCR2B > OCR2A_VALUE)
            {
               OCR2B = 0;
            }
            
            glcdSetAddress(GLCD_XPIXELS - 12, 5);
            printnumber(OCR2B>>OCR2B_BITSHIFT, INVERTED);
         }
         
         //day hour
         if(mode == AUTODIM_SET_DAY_H)
         {
            autodim_day_time += 60; //add an hour
            if(autodim_day_time/60 >= 24)
            {
               autodim_day_time = 0 + autodim_day_time%60;
            }
               
            glcdSetAddress(GLCD_XPIXELS-36, 4);
            print_timehour(autodim_day_time/60, INVERTED);
            
            if(time_format == TIME_12H)
            {
               glcdSetAddress(GLCD_XPIXELS -6, 4);
               if((autodim_day_time/60) >= 12)
               {
                  glcdWriteChar('P', NORMAL);
               }
               else
               {
                  glcdWriteChar('A', NORMAL);
               }
            }
            if(pressed)
               _delay_ms(200);
         }

         //night hour
         if(mode == AUTODIM_SET_NIGHT_H)
         {
            autodim_night_time += 60; //add an hour
            if(autodim_night_time/60 >= 24)
            {
               autodim_night_time = 0 + autodim_night_time%60;
            }
            
            glcdSetAddress(GLCD_XPIXELS-36, 1);
            print_timehour(autodim_night_time/60, INVERTED);
            
            if(time_format == TIME_12H)
            {
               glcdSetAddress(GLCD_XPIXELS -6, 1);
               if(autodim_night_time/60 >= 12)
               {
                  glcdWriteChar('P', NORMAL);
               }
               else
               {
                  glcdWriteChar('A', NORMAL);
               }
            }
            if(pressed)
               _delay_ms(200);
         }
         
         //day minute
         if(mode == AUTODIM_SET_DAY_M)
         {
            if(((autodim_day_time%60)+1) > 60)
            {
               autodim_day_time = autodim_day_time/60;
            }
            autodim_day_time++;
            
            glcdSetAddress(GLCD_XPIXELS-18, 4);
            printnumber(autodim_day_time%60, INVERTED);
            if(pressed)
               _delay_ms(200);
         }     
         
         //night minute
         if(mode == AUTODIM_SET_NIGHT_M)
         {
            if(((autodim_night_time%60)+1) > 60)
            {
               autodim_night_time = autodim_night_time/60;
            }
            autodim_night_time++;
            
            glcdSetAddress(GLCD_XPIXELS-18, 1);
            printnumber(autodim_night_time%60, INVERTED);
            if(pressed)
               _delay_ms(200);
         }
      }
   }
   
   screenmutex--; //return the screen
   return;
}

#ifdef AUTODIM_EEPROM
void init_autodim_eeprom()
{
   uint16_t day_time, night_time;
   uint8_t day_bright, night_bright;
   day_time = eeprom_read_word((uint16_t *)EE_AUTODIM_DAY_TIME);
   night_time = eeprom_read_word((uint16_t *)EE_AUTODIM_NIGHT_TIME);
   day_bright = eeprom_read_byte((uint8_t *)EE_AUTODIM_DAY_BRIGHT);
   night_bright = eeprom_read_byte((uint8_t *)EE_AUTODIM_NIGHT_BRIGHT);
   
   if((day_time >= 0) && (day_time <= 1440))
   {
      autodim_day_time = day_time;
   }
   else
   {
       eeprom_write_word((uint16_t *)EE_AUTODIM_DAY_TIME, autodim_day_time);
   }
   
   if((night_time >= 0) && (night_time <= 1440))
   {
      autodim_night_time = night_time;
   }
   else
   {
      eeprom_write_word((uint16_t *)EE_AUTODIM_NIGHT_TIME, autodim_night_time);
   }
      
   if((day_bright >= 0) && (day_bright <= OCR2A_VALUE))
   {
      autodim_day_bright = day_bright;
   }
   else
   {
      eeprom_write_byte((uint8_t *)EE_AUTODIM_DAY_BRIGHT, autodim_day_bright);
   }
      
   if((night_bright >= 0) && (night_bright <= OCR2A_VALUE))
   {
      autodim_night_bright = night_bright;
   }
   else
   {
      eeprom_write_byte((uint8_t *)EE_AUTODIM_NIGHT_BRIGHT, autodim_night_bright);
   }
}
#endif //#ifdef AUTODIM_EEPROM
#endif //#ifdef AUTODIM


/*++++++++++++++++++++++++++++++++++++++++++++++++++++
   Automatic Adjustment for DayLight Savings Time
   
   To Activate uncomment "#define AUTODST" in ratt.h
   
   Required resources:
      .text: 
      .data: 
+++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifdef AUTODST
uint8_t autodst_isDST = 0;
uint8_t autodst_changedToday = 0;
void init_autodst_eeprom(void)
{
   uint8_t temp = eeprom_read_byte((uint8_t *)EE_AUTODST);
   if((temp == 0)||(temp == 1))
      autodst_isDST = temp;
}

void update_autodst_eeprom(uint8_t value)
{
   if(eeprom_read_byte((uint8_t *)EE_AUTODST) != value)
   {
      eeprom_write_byte((uint8_t *)EE_AUTODST, value);
   }
}

uint32_t secondsIntoYear(uint8_t day, uint8_t month, uint8_t year)
{
   //how many days into the year we are
   static const uint16_t monthDays[]={0,31,59,90,120,151,181,212,243,273,304,334};
   uint32_t daysIntoYear = monthDays[month - 1] + day - 1;
   
   //is it a leap year?
   if((year%4 == 0) && (month > 2))
   {  
      if(year%100 == 0)
      {
         if(year%400 == 0)
         {
            daysIntoYear++;
         }
      }
      else
      {
         daysIntoYear++;
      }
   }
   
   //convert total to seconds
   return daysIntoYear * 86400;
}

uint32_t dstCalculate(uint8_t hour, uint8_t dotw_target, uint8_t n, uint8_t month, uint8_t year)
{
   uint8_t dstDay;
   uint8_t firstDow =  dotw(month, 1, year); //determine what the first day is
   if(firstDow <= dotw_target) //if it's earlier than the target dow
   {
      //the first target dow is at this date
      dstDay = dotw_target - firstDow + 1;
   }
   else //if its later than the target dow
   {
      //the first target dow is in the next week
      dstDay = 8 - (firstDow - dotw_target);
   }
   //allow for nth occurance of target dow
   dstDay += 7*(n-1);
   uint32_t dstSeconds = secondsIntoYear(dstDay, month, year); //where in the year is it?
   return dstSeconds += 3600*hour; //consider the time not just the day
}

void autodst(uint8_t* rule)
{
   //reset autodst_changedToday
   if((time_h == 00)&&(time_m == 00))
      autodst_changedToday = 0;

      
   //calculate where in the year the dst days are
   uint32_t startSeconds = dstCalculate(rule[0], rule[1], rule[2], rule[3], date_y);
   uint32_t endSeconds = dstCalculate(rule[4], rule[5], rule[6], rule[7], date_y);
   
   //calculate how far into the year we are.
   uint32_t nowSeconds = secondsIntoYear(date_d, date_m, date_y);
   nowSeconds = nowSeconds + time_h*3600 + time_m*60 + time_s;
   
   if(((nowSeconds < startSeconds)||(nowSeconds >= endSeconds))&& autodst_isDST && !autodst_changedToday)
   {
      autodst_isDST = 0;
      update_autodst_eeprom(0);
      autodst_changedToday = 1;
      uint8_t hour = time_h - 1;
      writei2ctime(time_s, time_m, hour, 0, date_d, date_m, date_y);
   }
   
   if(((nowSeconds >= startSeconds) && (nowSeconds < endSeconds)) && !autodst_isDST && !autodst_changedToday)
   {
      autodst_isDST = 1;
      update_autodst_eeprom(1);
      autodst_changedToday = 1;
      uint8_t hour = time_h + 1;
      writei2ctime(time_s, time_m, hour, 0, date_d, date_m, date_y);
   }    
}
#endif //#ifdef AUTODST

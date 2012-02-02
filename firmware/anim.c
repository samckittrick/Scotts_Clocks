/* ***************************************************************************
// anim.c - the main animation and drawing code for MONOCHRON
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
**************************************************************************** */

#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "util.h"
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"
#include "font5x7.h"
#include "WorldMap.c"

extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t old_m, old_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t alarming, alarm_h, alarm_m, alarm_on;
extern volatile uint8_t time_format;
extern volatile uint8_t region;
extern volatile uint8_t score_mode;
extern volatile uint8_t baseInverted;
extern volatile uint8_t minute_changed, hour_changed;
#ifdef AUTODST
extern volatile uint8_t autodst_isDST;
#endif

uint8_t hour12;
uint8_t lastTimeFormat;
uint8_t last_score_mode;
uint8_t timePM;
uint8_t showColon;
uint8_t needPopUp, havePopUp;
char dateString[11];
uint8_t dayotw, dayotw_old;
char dayText[10];

//Day of the week strings
char monday[] PROGMEM = "Monday";
char tuesday[] PROGMEM = "Tuesday";
char wednesday[] PROGMEM = "Wednesday";
char thursday[] PROGMEM = "Thursday";
char friday[] PROGMEM = "Friday";
char saturday[] PROGMEM = "Saturday";
char sunday[] PROGMEM = "Sunday";
const char *dayTable[] PROGMEM = { sunday, monday, tuesday, wednesday, thursday, friday, saturday };

//month strings
char jan[] PROGMEM = "January";
char feb[] PROGMEM = "February";
char mar[] PROGMEM = "March";
char apr[] PROGMEM = "April";
char may[] PROGMEM = "May";
char jun[] PROGMEM = "June";
char jul[] PROGMEM = "July";
char aug[] PROGMEM = "August";
char sep[] PROGMEM = "September";
char oct[] PROGMEM = "October";
char nov[] PROGMEM = "November";
char dec[] PROGMEM = "December";
PGM_P monthTable[] PROGMEM = { jan, feb, mar, apr, may, jun, jul, aug, sep, oct, nov, dec};

//alarm icon image
//7x7 icon. See images.ods
uint8_t alarmIcon[] = { 0x00, 0x3A, 0x44, 0xD4, 0x44, 0x3A, 0x00 }; 
uint8_t haveAlarmIcon;

//Space Station Icon
//3x5 icon. See images.ods
uint8_t spaceStation[] = { 0x00, 0x6C, 0x10, 0x6C, 0x00 };
uint8_t stationLocation, oldStationLocation, lastStationTime;

#ifdef AUTODST
//DST Icon
//11x7 icon. See images.ods
uint8_t dstIcon[] = { 0xF8, 0x88, 0xF8, 0x00, 0x74, 0x54, 0x5C, 0x00, 0x20, 0x3F, 0x20 };
#endif //#ifdef AUTODST

//Is it time to redraw the screen?
uint8_t redraw_time = 0;
uint8_t redraw_station = 0;

//Calculating groundtrack constants
#define RADIANS_PER_PIXEL 0.0490873852 //2Pi spread across a 128 pixel screen
#define PIXELS_PER_SECOND 0.0237037037 //128 pixels spread across a 90 minute orbit
#define SECONDS_PER_PIXEL 21 //It takes 42 seconds for the station to move one pixel based on a 90 minute orbit

uint32_t rval[2]={0,0};
uint32_t key[4];

void encipher(void) {  // Using 32 rounds of XTea encryption as a PRNG.
  unsigned int i;
  uint32_t v0=rval[0], v1=rval[1], sum=0, delta=0x9E3779B9;
  for (i=0; i < 32; i++) {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
  }
  rval[0]=v0; rval[1]=v1;
}

void init_crand(void) {
  uint32_t temp;
  key[0]=0x2DE9716E;  //Initial XTEA key. Grabbed from the first 16 bytes
  key[1]=0x993FDDD1;  //of grc.com/password.  1 in 2^128 chance of seeing
  key[2]=0x2A77FB57;  //that key again there.
  key[3]=0xB172E6B0;
  rval[0]=0;
  rval[1]=0;
  encipher();
  temp = alarm_h;
  temp<<=8;
  temp|=time_h;
  temp<<=8;
  temp|=time_m;
  temp<<=8;
  temp|=time_s;
  key[0]^=rval[1]<<1;
  encipher();
  key[1]^=temp<<1;
  encipher();
  key[2]^=temp>>1;
  encipher();
  key[3]^=rval[1]>>1;
  encipher();
  temp = alarm_m;
  temp<<=8;
  temp|=date_m;
  temp<<=8;
  temp|=date_d;
  temp<<=8;
  temp|=date_y;
  key[0]^=temp<<1;
  encipher();
  key[1]^=rval[0]<<1;
  encipher();
  key[2]^=rval[0]>>1;
  encipher();
  key[3]^=temp>>1;
  rval[0]=0;
  rval[1]=0;
  encipher();	//And at this point, the PRNG is now seeded, based on power on/date/time reset.
}

uint16_t crand(uint8_t type) {
  if((type==0)||(type>2))
  {
    wdt_reset();
    encipher();
    return (rval[0]^rval[1])&RAND_MAX;
  } else if (type==1) {
  	return ((rval[0]^rval[1])>>15)&3;
  } else if (type==2) {
  	return ((rval[0]^rval[1])>>17)&1;
  }
}

void setscore(void) //Identify what information needs to be shown
{

  /* The score_mode variable indicates what kind of time should be displayed by the clock. I.e. time, date, year, the time the alarm is set to, etc.
     The setscore function is called from ratt.c every time the score mode is changed (when the user presses the '+' button on the clock) and allows
     the animation control to decide how to display that information. */
  if(score_mode != last_score_mode) {
    redraw_time = 1;
    last_score_mode = score_mode;
  }
  switch(score_mode) {
  	case SCORE_MODE_DOW:
      needPopUp = 1;
  	  break;
  	case SCORE_MODE_DATELONG:
  	  break;
    case SCORE_MODE_TIME:
      needPopUp = 0;
      break;
    case SCORE_MODE_DATE:
      needPopUp = 1;
      break;
    case SCORE_MODE_YEAR:
      break;
    case SCORE_MODE_ALARM:
      needPopUp = 1;
      break;
  }
}

//initialise the animation. This function is called once before the clock loop begins.
void initanim(void) {
  DEBUG(putstring("screen width: "));
  DEBUG(uart_putw_dec(GLCD_XPIXELS));
  DEBUG(putstring("\n\rscreen height: "));
  DEBUG(uart_putw_dec(GLCD_YPIXELS));
  DEBUG(putstring_nl(""));
  
  if(time_format == TIME_12H)
  {
     if((time_h > 12))
     {
        hour12 = time_h-12;
        timePM = 1;
     }
     else
     {
        hour12 = time_h;
        //special case
        if(time_h == 0)
        {
            hour12 = 12;
        }
        timePM = 0;
     }
  }
  else
  {
      hour12 = time_h;
  }
  
  dayotw = dayotw_old = dotw(date_m, date_d, date_y);
  strcpy_P(dayText, (char*)pgm_read_dword(&(dayTable[dayotw])));
  
  needPopUp = havePopUp = 0;
  haveAlarmIcon = alarm_on;
  last_score_mode = score_mode;
  lastTimeFormat = time_format;
  stationLocation = oldStationLocation = 3;
  lastStationTime = time_s;
  redraw_station = 5;

}

//initialise the display. This function is called at least once, and may be called several times after.
// This function is called once when the clock starts and then every time a menu is shown and cleared.
void initdisplay(uint8_t inverted) {
   //clear the screen
   glcdFillRectangle(0,0,GLCD_XPIXELS, GLCD_YPIXELS, inverted);
   drawMap(inverted);
   
   //Clear bottom bar
   glcdFillRectangle(0, GLCD_YPIXELS - 10, GLCD_XPIXELS, 10, inverted);
   glcdFillRectangle(0, GLCD_YPIXELS-10, GLCD_XPIXELS, 1, !inverted);
   glcdSetAddress(1, GLCD_TEXT_LINES-1);
   glcdPutStr("ISS", inverted);
   redraw_time = 1;
}

//advance the animation by one step. This function is called from ratt.c every ANIM_TICK miliseconds.
void step(void) {
   
   //we might need to redraw the time
   if((minute_changed || hour_changed) || (time_format != lastTimeFormat))
   {
      redraw_time = 1;
      minute_changed = 0;
      hour_changed = 0;
      if(time_format == TIME_12H)
      {
         if(time_h > 12)
         {
            hour12 = time_h-12;
            timePM = 1;
         }
         else
         {
            hour12 = time_h;
            //special case
            if(time_h == 0)
            {
               hour12 = 12;
            }
            timePM = 0;
         }
      }
      else
      { 
         hour12 = time_h;
      }
      
      lastTimeFormat = time_format;
   }
   
         
   dayotw = dotw(date_m, date_d, date_y);
   if(dayotw != dayotw_old)
   {
      strcpy_P(dayText, (PGM_P)pgm_read_dword(&(dayTable[dayotw])));
      dayotw_old = dayotw;
   }
   
   if(time_s != lastStationTime)
   {
      /*lastStationTime = time_s;
      redraw_station = 1;
      oldStationLocation = stationLocation;
      stationLocation++;
      if(stationLocation > GLCD_XPIXELS - 4)
         stationLocation = 3;*/
      uint8_t offset = 0;
      if(lastStationTime > time_s) //deal with the fact that time loops around
      {
         offset = (time_s + 59) - lastStationTime;
      }
      else
      {
         offset = time_s - lastStationTime;
      }
      
      if(offset >= SECONDS_PER_PIXEL)
      {      
         oldStationLocation = stationLocation;
         stationLocation++;
         //if the station moves off the right of the screen, we must move wrap it over to the left.
         //if we don't, it causes the screen to flash like crazy
         if(stationLocation > GLCD_XPIXELS -4)
            stationLocation = 3;
         //setting the time this way ensures that the station moves 1 pixel every 42 seconds even if we don't move it right on time.
         lastStationTime = time_s; 
         redraw_station = 1;
      }
   }
   
   //if the alarm status has changed
   if(haveAlarmIcon != alarm_on)
   {
      redraw_time = 1;
      haveAlarmIcon = alarm_on;
   }
   
   if(needPopUp && !havePopUp)
   {
      if((region == REGION_US) || (region == DOW_REGION_US))
      { 
         sprintf(dateString, "%02d/%02d/20%2d", date_m, date_d, date_y);
      }
      else
      {
         sprintf(dateString, "%02d/%02d/20%2d", date_d, date_m, date_y);
      }
   }
   
   //Flasher for the Colon in the time
   if(time_s & 0x1)
   {
      showColon = 1;
   }
   else
   {
      showColon = 0;
   } 
}

//draw everything to the screen
// After step() updates everything necessary for the animation, draw() is called to actually draw the frame on the screen.
//draw() is called every ANIM_TICK miliseconds.
void draw(uint8_t inverted) {
   
   //if we need to show the date
   if(needPopUp && !havePopUp)
   {
      glcdFillRectangle(14, 6, 100, 44, inverted);
      glcdRectangle(14,6,100,44);
      glcdRectangle(16,8,96,40);
      havePopUp = 1;
      
      if(score_mode == SCORE_MODE_DATE)
      {
         //the below formula is (displayAreaWidth - (wordLength*characterWidth))/2
         // it centers the day of the week in the box
         uint8_t offset = (96 - (strlen(dayText)*6))>>1;
         glcdSetAddress(18 + offset, 2);
         glcdPutStr(dayText, inverted);
         glcdSetAddress(36, 4);
         glcdPutStr(dateString, inverted);
      }
   }
   
   //if we have the popup and still need it
   if(needPopUp && havePopUp)
   {
      //if we are showing the alarm
      if(score_mode == SCORE_MODE_ALARM)
      {
         //flash the word alarm
         if(time_s & 0x1)
         {
            glcdSetAddress(51, 2);
            glcdPutStr("     ", inverted);
         }
         else
         {
            glcdSetAddress(51, 2);
            glcdPutStr("Alarm", inverted);
         }
         
         //print alarm time
         glcdSetAddress(49, 4);
         uint8_t alarmHDisplay = alarm_h;
         uint8_t alarmPM = 0;
         if((time_format == TIME_12H) && (alarm_h > 12))
         {
            alarmHDisplay = alarm_h -12;
            alarmPM = 1;
         }
         if(((alarmHDisplay/10) == 0) && (time_format == TIME_12H))
         {
            glcdWriteChar(32, inverted);
         }
         else
         {
            glcdWriteChar(48 + alarmHDisplay/10, inverted);
         }
         glcdWriteChar(48 + alarmHDisplay%10, inverted);
         glcdWriteChar(58, inverted);
         glcdWriteChar(48 + alarm_m/10, inverted);
         glcdWriteChar(48 + alarm_m%10, inverted);
         if(time_format == TIME_12H)
         {
            if(alarmPM)
            {
               glcdWriteChar(80, inverted);
            }
            else
            {
               glcdWriteChar(65, inverted);
            }
         }
      }
   }
   
   //if we were showing the date and it times out
   //refresh the screen
   if(!needPopUp && havePopUp)
   {
      drawMap(inverted);
      havePopUp = 0;
      redraw_time = 1;
      redraw_station = 1;
   }
   
   if((redraw_station) && !(havePopUp && (stationLocation >= 13) && (stationLocation <= 113)))
   {
      redraw_station = 0;
      //Clear old station from screen. Must put pixels back the way they were, not just erase them.
      uint8_t oldStationY = groundTrack(oldStationLocation);
      for(uint8_t i = 0; i < 5; i++)
         for(uint8_t j = 0; j < 8; j++)
            if(getMapPixelValue((oldStationLocation-1)+i, (oldStationY-4)+j))
            {
               glcdFillRectangle((oldStationLocation-1)+i, (oldStationY-4)+j, 1, 1, !inverted);
            }
            else
            {
               glcdFillRectangle((oldStationLocation -1) + i, (oldStationY -4) + j, 1, 1, inverted);
            }
      
      //draw new station
      uint8_t stationY = groundTrack(stationLocation);
      for(uint8_t i = 0; i < 5; i++)
         for(uint8_t j = 0; j < 8; j++)
            if(spaceStation[i] & ((uint8_t)1 << (8-j)))
            {
               glcdFillRectangle((stationLocation - 1)+i, (stationY - 4)+j, 1,1, !inverted);
            }
            else
            {
               glcdFillRectangle((stationLocation - 1)+i, (stationY - 4)+j, 1,1, inverted);
            }
   }
   
   if(redraw_time)
   {
      redraw_time = 0;
      
      //clear time bar
      glcdFillRectangle(19, GLCD_YPIXELS-9, GLCD_XPIXELS-19, 9, inverted);
      
       //draw alarm icon
      if(alarm_on)
      {
         for(uint8_t i = 0; i < 7; i++)
            for(uint8_t j = 0; j < 7; j++)
               if(alarmIcon[i] & ((uint8_t)1 << (7-j)))
                  glcdFillRectangle(43 + i, (GLCD_YPIXELS - 8) + j, 1, 1, !inverted);
      }
      
      #ifdef AUTODST
      //draw dst icon
      if(autodst_isDST)
         for(uint8_t i = 0; i < 11; i++)
            for(uint8_t j = 0; j < 8; j++)
               if(dstIcon[i] & ((uint8_t)1 << (8 - j)))
                  glcdFillRectangle(52 + i, (GLCD_YPIXELS - 9) + j, 1, 1, !inverted);
      #endif //#ifdef AUTODST
      
      //write time
      glcdSetAddress(GLCD_XPIXELS - 37, GLCD_TEXT_LINES-1);
      if((hour12/10) == 0)
      {
         glcdWriteChar(32, inverted);
      }
      else
      {
         glcdWriteChar(48 + hour12/10, inverted);
      }
      glcdWriteChar(48 + hour12%10, inverted);
      glcdWriteChar(58, inverted);
      glcdWriteChar(48 + time_m/10, inverted);
      glcdWriteChar(48 + time_m%10, inverted);
      if(time_format == TIME_12H)
      {
         if(timePM)
         {
            glcdWriteChar(80, inverted);
         }
         else
         {
            glcdWriteChar(65, inverted);
         }
      }
      

   }
   
   glcdSetAddress(GLCD_XPIXELS - 25, GLCD_TEXT_LINES - 1); //Place Colon
   if(showColon)
   {
      glcdWriteChar(58, inverted);
   }
   else
   {
      glcdWriteChar(32, inverted);
   }
   
}

//Draw the map on the screen
void drawMap(uint8_t inverted)
{
   glcdFillRectangle(0,0, 128, 54, inverted);
   //loop through the worldMap array
   for(uint16_t i = 0; i < 256; i +=2)
   {
      //extract top and bottom halves of image
      uint32_t colTop = pgm_read_dword_near(worldMap+i);
      uint32_t colBot = pgm_read_dword_near(worldMap+i+1);
      for(uint8_t j = 0; j < 32; j++)
      {
         //print top and bottom halves.
         // the 32-j is to put the image right side up, since the position is measured from the 
         // top left instead of the bottom left.
         if(colTop & ((uint32_t)1 << (32 - j)))
            glcdFillRectangle(i>>1, j,1,1,!inverted);
         if((colBot & ((uint32_t)1 << (32 - j))) && (32+j <= 54))
            glcdFillRectangle(i>>1, j+31,1,1,!inverted);
      }
   }
   
   glcdFillRectangle(0,0,GLCD_XPIXELS, 1, !inverted);
   
   for(uint8_t i = 0; i < 128; i++)
   {
      glcdFillRectangle(i, groundTrack(i), 1, 1, !inverted);
   }
   
}

uint8_t groundTrack(uint8_t x)
{
   return 27 + floor(20*sin(RADIANS_PER_PIXEL*(x+36)));
}

uint8_t getMapPixelValue(uint8_t x, uint8_t y)
{
   uint8_t mapDot = 0;
   uint8_t gcDot = 0;
   if((y < 32) && (y >= 0))
   {
      uint32_t column = pgm_read_dword_near(worldMap + (x << 1));
      if(column & ((uint32_t)1 << (32 - y)))
         mapDot = 1;
   }
   else if((y >= 32) && (y < 54))
   {
      uint32_t column = pgm_read_dword_near(worldMap + (x << 1) + 1);
      if(column & ((uint32_t)1 << (32 - (y - 32))))
         mapDot = 1;
   }
   
   if(groundTrack(x) == y)
      gcDot = 1;
   
   return mapDot || gcDot;
}

uint8_t dotw(uint8_t mon, uint8_t day, uint8_t yr)
{
  uint16_t month, year;

    // Calculate day of the week
    
    month = mon;
    year = 2000 + yr;
    if (mon < 3)  {
      month += 12;
      year -= 1;
    }
    return (day + (2 * month) + (6 * (month+1)/10) + year + (year/4) - (year/100) + (year/400) + 1) % 7;
}

/*void fillPopUp(uint8_t score_mode)
{
      if((score_mode == SCORE_MODE_DATE)||*/
      
            /*fillPopUp(score_mode);
      dayotw = dotw(date_m, date_d, date_y);
      strcpy_P(PopUp1, (PGM_P)pgm_read_word(&(dayTable[dayotw-1])));*/

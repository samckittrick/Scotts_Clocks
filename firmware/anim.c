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
extern volatile uint8_t alarming, alarm_h, alarm_m;
extern volatile uint8_t time_format;
extern volatile uint8_t region;
extern volatile uint8_t score_mode;
extern volatile uint8_t baseInverted;
extern volatile uint8_t minute_changed, hour_changed;

uint8_t hour12;
uint8_t timePM;
uint8_t showColon;
//Is it time to redraw the screen?
uint8_t redraw_time = 0;

uint8_t last_score_mode = 0;

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

void init_crand() {
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
  	  break;
  	case SCORE_MODE_DATELONG:
  	  break;
    case SCORE_MODE_TIME:
      break;
    case SCORE_MODE_DATE:
      break;
    case SCORE_MODE_YEAR:
      break;
    case SCORE_MODE_ALARM:
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
  
  if(time_h > 12)
  {
      hour12 = time_h-12;
      timePM = 1;
   }
   else
   {
      hour12 = time_h;
      timePM = 0;
   }
}

//initialise the display. This function is called at least once, and may be called several times after.
// It is possible that this function is called every ANIM_TICK miliseconds, but I am not sure yet.
void initdisplay(uint8_t inverted) {
   //clear the screen
   glcdFillRectangle(0,0,GLCD_XPIXELS, GLCD_YPIXELS, inverted);
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
            glcdFillRectangle(i/2, j,1,1,!inverted);
         if(colBot & ((uint32_t)1 << (32 - j)))
            glcdFillRectangle(i/2, j+31,1,1,!inverted);
      }
   }
   
   //Clear bottom bar
   glcdFillRectangle(0, GLCD_YPIXELS - 10, GLCD_XPIXELS, 10, inverted);
   glcdFillRectangle(0, GLCD_YPIXELS-10, GLCD_XPIXELS, 1, !inverted);
   glcdSetAddress(1, GLCD_TEXT_LINES-1);
   glcdPutStr("ISS", inverted);
   redraw_time = 1;
   draw(inverted);
}

//advance the animation by one step. This function is called from ratt.c every ANIM_TICK miliseconds.
void step(void) {
   
   if(minute_changed || hour_changed)
   {
      redraw_time = 1;
      minute_changed = 0;
      hour_changed = 0;
      
      if(time_h > 12)
      {
         hour12 = time_h-12;
         timePM = 1;
      }
      else
      {
         hour12 = time_h;
         timePM = 0;
      }
   }
   
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
   if(redraw_time)
   {
      redraw_time = 0;
      glcdFillRectangle(20, GLCD_YPIXELS-8, GLCD_XPIXELS-20, 8, inverted);
      glcdSetAddress(GLCD_XPIXELS - 37, GLCD_TEXT_LINES-1);
      glcdWriteChar(48 + hour12/10, inverted);
      glcdWriteChar(48 + hour12%10, inverted);
      glcdWriteChar(58, inverted);
      glcdWriteChar(48 + time_m/10, inverted);
      glcdWriteChar(48 + time_m%10, inverted);
      if(timePM)
      {
         glcdWriteChar(80, inverted);
      }
      else
      {
         glcdWriteChar(65, inverted);
      }
   }
   
   glcdSetAddress(GLCD_XPIXELS - 24, GLCD_TEXT_LINES - 1); //Place Colon
   if(showColon)
   {
      glcdWriteChar(58, inverted);
   }
   else
   {
      glcdWriteChar(32, inverted);
   }
}

// 8 pixels high
static unsigned char __attribute__ ((progmem)) BigFont[] = {
	0xFF, 0x81, 0x81, 0xFF,// 0
	0x00, 0x00, 0x00, 0xFF,// 1
	0x9F, 0x91, 0x91, 0xF1,// 2
	0x91, 0x91, 0x91, 0xFF,// 3
	0xF0, 0x10, 0x10, 0xFF,// 4
	0xF1, 0x91, 0x91, 0x9F,// 5
	0xFF, 0x91, 0x91, 0x9F,// 6
	0x80, 0x80, 0x80, 0xFF,// 7
	0xFF, 0x91, 0x91, 0xFF,// 8 
	0xF1, 0x91, 0x91, 0xFF,// 9
	0x00, 0x00, 0x00, 0x00,// SPACE
};

static unsigned char __attribute__ ((progmem)) MonthText[] = {
	0,0,0,
	'J','A','N',
	'F','E','B',
	'M','A','R',
	'A','P','R',
	'M','A','Y',
	'J','U','N',
	'J','U','L',
	'A','U','G',
	'S','E','P',
	'O','C','T',
	'N','O','V',
	'D','E','C',
};

static unsigned char __attribute__ ((progmem)) DOWText[] = {
	'S','U','N',
	'M','O','N',
	'T','U','E',
	'W','E','D',
	'T','H','U',
	'F','R','I',
	'S','A','T',
};

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

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
#include <math.h>

#include "util.h"
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"
#include "font5x7.h"

extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t old_m, old_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t alarming, alarm_h, alarm_m;
extern volatile uint8_t time_format;
extern volatile uint8_t region;
extern volatile uint8_t score_mode;
extern volatile uint8_t baseInverted;

//Variables needed for the AutoDim feature
#ifdef AUTODIM
extern volatile uint8_t autodim_day_time_h;
extern volatile uint8_t autodim_day_time_m;
extern volatile uint8_t autodim_day_bright;
extern volatile uint8_t autodim_night_time_h;
extern volatile uint8_t autodim_night_time_m;
extern volatile uint8_t autodim_night_bright;
#endif

//A couple variables to print some basic message on the screen as a place holder for a new animation.
uint8_t xpos, ypos;
char msg[22];

extern volatile uint8_t minute_changed, hour_changed;

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
  xpos = 0;
  ypos = 0;
  
   minute_changed = 0;
   hour_changed = 0;
   if(ypos >= GLCD_TEXT_LINES)
      ypos = 0;
      strcpy(msg, "Hello World");
   
   if(time_m & 0x1)
   {
      baseInverted = 1;
   }
   else
   {
      baseInverted = 0;
   }
}

//initialise the display. This function is called at least once, and may be called several times after.
// It is possible that this function is called every ANIM_TICK miliseconds, but I am not sure yet.
void initdisplay(uint8_t inverted) {
   glcdFillRectangle(0,0,GLCD_XPIXELS, GLCD_YPIXELS, inverted);
   glcdSetAddress(xpos, ypos);
   glcdPutStr(msg, inverted);
}

//advance the animation by one step. This function is called from ratt.c every ANIM_TICK miliseconds.
void step(void) {
   
   if(minute_changed || hour_changed)
   {
      redraw_time = 1;
      minute_changed = 0;
      hour_changed = 0;
      ypos++;
      if(ypos >= GLCD_TEXT_LINES)
         ypos = 0;
         strcpy(msg, "Hello World");
      
      if(time_m & 0x1)
      {
         baseInverted = 1;
      }
      else
      {
         baseInverted = 0;
      }
      
      #ifdef AUTODIM
         autoDim(time_h, time_m);
      #endif
   }
   
   
 
}

//draw everything to the screen
// After step() updates everything necessary for the animation, draw() is called to actually draw the frame on the screen.
//draw() is called every ANIM_TICK miliseconds.
void draw(uint8_t inverted) {
   if(redraw_time)
   {
      redraw_time = 0;
      glcdFillRectangle(0,0,GLCD_XPIXELS, GLCD_YPIXELS, inverted);
      glcdSetAddress(xpos, ypos);
      glcdPutStr(msg, inverted);
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

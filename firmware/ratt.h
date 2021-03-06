#define halt(x)  while (1)

#define DEBUGGING 0
#define DEBUG(x)  if (DEBUGGING) { x; }
#define DEBUGP(x) DEBUG(putstring_nl(x))

// Software options. Uncomment to enable.
//BACKLIGHT_ADJUST - Allows software control of backlight, assuming you mounted your 100ohm resistor in R2'.
#define BACKLIGHT_ADJUST 1

//Advanced Software Options
//Note AutoDim will only work if the backlight is adjustable.
#ifdef BACKLIGHT_ADJUST
#define AUTODIM

//This option allows AutoDim to save it's settings. It will only work if AutoDim is enabled. Uncomment to enable.
//Warning: Enabling this option uses shared memory. 
//         Before enabling it, ensure it does not conflict with any other memory usage.
//Note: AutoDim will work without the eeprom usage. It just will not keep it's settings in the event of a reset.s
#ifdef AUTODIM
#define AUTODIM_EEPROM
#endif
#endif

//AutoDST automatically changes your clock's time for DST. Uncomment to enable.
//#define AUTODST

// how fast to proceed the animation, note that the redrawing
// takes some time too so you dont want this too small or itll
// 'hiccup' and appear jittery
#define ANIMTICK_MS 75

// Beeep!
#define ALARM_FREQ 4000
#define ALARM_MSONOFF 300
 
#define MAXSNOOZE 600 // 10 minutes

// how many seconds we will wait before turning off menus
#define INACTIVITYTIMEOUT 10 

//Button values
//Each button turns on one bit in just_pressed
//By logical anding just pressed with a number like 0x1 we can determine which button was pressed
//Menu button is 0x1
//Set button is 0x2
//+ button is 0x3
//Multiple buttons can be watched for by setting combinations of bits, i.e. 0x4 which is + and menu button.
/*************************** DISPLAY PARAMETERS */

// how many pixels to indent the menu items
#define MENU_INDENT 8

// Where the HOUR10 HOUR1 MINUTE10 and MINUTE1 digits are
// in pixels
#define DISPLAY_H10_X 30
#define DISPLAY_H1_X 45
#define DISPLAY_M10_X 70
#define DISPLAY_M1_X 85

#define DISPLAY_DOW1_X 35
#define DISPLAY_DOW2_X 50
#define DISPLAY_DOW3_X 70

#define DISPLAY_MON1_X 20
#define DISPLAY_MON2_X 35
#define DISPLAY_MON3_X 50

#define DISPLAY_DAY10_X 70
#define DISPLAY_DAY1_X 85

// buffer space from the top
#define DISPLAY_TIME_Y 4

// how big are the pixels (for math purposes)
#define DISPLAY_DIGITW 10
#define DISPLAY_DIGITH 16


/* not used
#define ALARMBOX_X 20
#define ALARMBOX_Y 24
#define ALARMBOX_W 80
#define ALARMBOX_H 20
*/

/*************************** PIN DEFINITIONS */
// there's more in KS0108.h for the display

#define ALARM_DDR DDRB
#define ALARM_PIN PINB
#define ALARM_PORT PORTB
#define ALARM 6

#define PIEZO_PORT PORTC
#define PIEZO_PIN  PINC
#define PIEZO_DDR DDRC
#define PIEZO 3


/*************************** ENUMS */

// Whether we are displaying time (99% of the time)
// alarm (for a few sec when alarm switch is flipped)
// date/year (for a few sec when + is pressed)
#define SCORE_MODE_TIME 0
#define SCORE_MODE_DATE 1
#define SCORE_MODE_YEAR 2
#define SCORE_MODE_ALARM 3
#define SCORE_MODE_DOW 4
#define SCORE_MODE_DATELONG 5

// Constants for how to display time & date
#define REGION_US 0
#define REGION_EU 1
#define DOW_REGION_US 2
#define DOW_REGION_EU 3
#define DATELONG 4
#define DATELONG_DOW 5
#define TIME_12H 0
#define TIME_24H 1

//Contstants for calcualting the Timer2 interrupt return rate.
//Desired rate, is to have the i2ctime read out about 6 times
//a second, and a few other values about once a second.
#define OCR2B_BITSHIFT 0
#define OCR2B_PLUS 1
#define OCR2A_VALUE 16
#define TIMER2_RETURN 80
//#define TIMER2_RETURN (8000000 / (OCR2A_VALUE * 1024 * 6))

// displaymode
#define NONE 99
#define SHOW_TIME 0
#define SHOW_DATE 1
#define SHOW_ALARM 2
#define SET_TIME 3
#define SET_ALARM 4
#define SET_DATE 5
#define SET_BRIGHTNESS 6
#define SET_VOLUME 7
#define SET_REGION 8
#define SHOW_SNOOZE 9
#define SET_SNOOZE 10

#define SET_MONTH 11
#define SET_DAY 12
#define SET_YEAR 13

#define SET_HOUR 101
#define SET_MIN 102
#define SET_SEC 103

#define SET_REG 104

#define SET_BRT 105

//DO NOT set EE_INITIALIZED to 0xFF / 255,  as that is
//the state the eeprom will be in, when totally erased.
#define EE_INITIALIZED 0xC3
#define EE_INIT 0
#define EE_ALARM_HOUR 1
#define EE_ALARM_MIN 2
#define EE_BRIGHT 3
#define EE_VOLUME 4
#define EE_REGION 5
#define EE_TIME_FORMAT 6
#define EE_SNOOZE 7
#ifdef AUTODIM_EEPROM
#define EE_AUTODIM_DAY_TIME 8 //Note this variable is 2 bytes.
#define EE_AUTODIM_NIGHT_TIME 10 //Note this variable is 2 bytes.
#define EE_AUTODIM_DAY_BRIGHT 12
#define EE_AUTODIM_NIGHT_BRIGHT 13
#endif
#ifdef AUTODST
#define EE_AUTODST 14
#endif // #ifdef AUTODST

/*************************** FUNCTION PROTOTYPES */

uint8_t leapyear(uint16_t y);
void clock_init(void);
void initbuttons(void);
void tick(void);
void setsnooze(void);
void initanim(void);
void initdisplay(uint8_t inverted);
void step(void);
void setscore(void);
void draw(uint8_t inverted);

void set_alarm(void);
void set_time(void);
void set_region(void);
void set_date(void);
void set_backlight(void);
#ifdef AUTODIM
void autoDim(uint8_t hour, uint8_t minute);
void setBacklightAutoDim(void);
#ifdef AUTODIM_EEPROM
void init_autodim_eeprom(void);
void update_autodst_eeprom(uint8_t value);
uint32_t secondsIntoYear(uint8_t day, uint8_t month, uint8_t year);
uint32_t dstCalculate(uint8_t hour, uint8_t dotw, uint8_t n, uint8_t month, uint8_t year);
void autodst(uint8_t* rule);
#endif
#endif
#ifdef AUTODST
void init_autodst_eeprom(void);
#endif //#ifdef AUTODST
void print_timehour(uint8_t h, uint8_t inverted);
void print_alarmhour(uint8_t h, uint8_t inverted);
void display_menu(void);
void drawArrow(uint8_t x, uint8_t y, uint8_t l);
void setalarmstate(void);
void beep(uint16_t freq, uint8_t duration);
void printnumber(uint8_t n, uint8_t inverted);

void init_crand(void);
uint8_t dotw(uint8_t mon, uint8_t day, uint8_t yr);

uint8_t i2bcd(uint8_t x);

uint8_t readi2ctime(void);

void writei2ctime(uint8_t sec, uint8_t min, uint8_t hr, uint8_t day,
		  uint8_t date, uint8_t mon, uint8_t yr);

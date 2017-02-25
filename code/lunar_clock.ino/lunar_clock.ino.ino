

/******************************************************************************
Arduino 3D printed Lunar Clock
by G4lile0 


V3.0 13/11/2016
- First public release

V4.0 25/02/2017
- Status is now stored on the RTC NVRAM, this mean that after power failure, clock will return to the old status, moon mode, and alarm configuration instead of load defaul configuration.
NVRAM configuration
00  init_controlA   // if init_contrl_A and B is not 16, NVRAM will load default values
01  init_controlB  // 
02  BRIGHTNESS    // Leds Brightness 
03  beep          // Buttons Beep
04  menu          // menu status
05  moonMode      // Moon Mode
06  alarmHour     // alarm hour
07  alarmMinute   // alarm minute
08  alarmStatus   // Alam status

V4.1  26/02/2017
Added new screen/menu with bigger digital clock.



OLEd Analog Clock using U8GLIB Library
// Button Long / Short Press script by: 
// (C) 2011 By P. Bauermeister
// http://www.instructables.com/id/Arduino-Dual-Function-Button-Long-PressShort-Press/

// Demo colors moon animations from "FastLED "100-lines-of-code" demo reel" by Mark Kriegsman, December 2014
// Analog clock (first screen) from "OLEd Analog Clock using U8GLIB Library" by Chris Rouse Oct 2014
// moon phase calculations,  algorithm adapted from Stephen R. Schmitt's by  Tim Farley  18 Aug 2009

Note:: Sketch uses 99% of program storage space, 
       Global variables use 56% of dyamic memory, 
       leaving 882 bytes for local variables.

Using a IIC 128x64 OLED with SSD1306 chip
RTC DS1307 
Optional Temperature Sensor TMP 36

Wire RTC:
  VCC +5v
  GND GND
  SDA Analog pin 4
  SCL Analog pin 5
Wire OLED:
  VCC +5v
  GND GND
  SDA Analog pin 4
  SCL Analog pin 5
Wire TMP36:
  VCC +3.3v
  GND GND
  Out Analog pin 1
  


******************************************************************************/

// Add libraries
  #include "U8glib.h"
  #include <SPI.h>
  #include <Wire.h>
  #include "RTClib.h"
  #include "DHT.h"
  #include "FastLED.h"

  // How many leds in your strip?
  #define NUM_LEDS 18
// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
#define DATA_PIN 5
#define CLOCK_PIN 6

byte BRIGHTNESS=150;
// Define the array of leds
CRGB leds[NUM_LEDS];

#define FRAMES_PER_SECOND  120

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
//SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns



// blinking variables
boolean blinking = true;
unsigned long previousBlinking = 0;  
const long period_Blinking = 500;

// BEEP (to enable o disable buttons BEEPs) 
boolean beep = true;

#define DHTPIN 2     // what digital pin we're connected to

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);


// setup u8g object
 // U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);	// I2C 
  U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_NO_ACK);  
//

// Setup RTC
  RTC_DS1307 RTC;
  char monthString[37]= {"JanFebMarAprMayJunJulAugSepOctNovDec"};
  String thisMonth = "";
  String thisTime = "";
  String alarmTime ="";
  String thisDay="";
  byte clockCentreX = 64; // used to fix the centre the analog clock
  byte clockCentreY = 32; // used to fix the centre the analog clock
  

// Menu variables

byte menu = 0;
byte moonMode=0;

//temp
  char buffer[16];

//moonphase variables
int y, m, d, h;


// Alarm Clock
byte alarmHour   = 17; // alarm hour
byte alarmMinute = 04; // alarm minute
boolean alarmStatus = false; 
boolean alarm_button_off = false ; 

// Adapt these to your board and application timings:

#define BUTTON1_PIN              A0  // Button 1     MENU
#define BUTTON2_PIN              A1  // Button 2     UP
#define BUTTON3_PIN              A2  // Button 3     DOWN

#define DEFAULT_LONGPRESS_LEN    10  // Min nr of loops for a long press
#define DELAY                    20  // Delay per loop in ms


//////////////////////////////////////////////////////////////////////////////

enum { EV_NONE=0, EV_SHORTPRESS, EV_LONGPRESS };

//////////////////////////////////////////////////////////////////////////////
// Class definition

class ButtonHandler {
  public:
    // Constructor
    ButtonHandler(int pin, int longpress_len=DEFAULT_LONGPRESS_LEN);

    // Initialization done after construction, to permit static instances
    void init();

    // Handler, to be called in the loop()
    int handle();

  protected:
    boolean was_pressed;     // previous state
    int pressed_counter;     // press running duration
    const int pin;           // pin to which button is connected
    const int longpress_len; // longpress duration
};

ButtonHandler::ButtonHandler(int p, int lp)
: pin(p), longpress_len(lp)    
{
}

void ButtonHandler::init()
{
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH); // pull-up
  was_pressed = false;
  pressed_counter = 0;
}

int ButtonHandler::handle()
{
  int event;
  int now_pressed = !digitalRead(pin);

  if (!now_pressed && was_pressed) {
    // handle release event
    if (pressed_counter < longpress_len)
      event = EV_SHORTPRESS;
    else
      event = EV_LONGPRESS;
  }
  else
    event = EV_NONE;

  // update press running duration
  if (now_pressed)
    ++pressed_counter;
  else
    pressed_counter = 0;

  // remember state, and we're done
  was_pressed = now_pressed;
  return event;
}

//////////////////////////////////////////////////////////////////////////////

// Instanciate button objects
ButtonHandler button1(BUTTON1_PIN);
ButtonHandler button2(BUTTON2_PIN, DEFAULT_LONGPRESS_LEN*2);
ButtonHandler button3(BUTTON3_PIN);


void print_event(const char* button_name, int event)
{
  if (event)
    Serial.print(button_name);
  //  Serial.print(".SL"[event]);
 //   Serial.print(event);
}



void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}




void menu0(void) {
  // graphic commands to redraw the complete screen should be placed here  
  u8g.setFont(u8g_font_profont15r);
  //u8g.setFont(u8g_font_6x10); 
  //
  //***** RTC **********
  DateTime now = RTC.now();
   // display date at bottom of screen
  
  if (now.day() < 10){ thisDay=" ";} else { thisDay="";}// add leading space if required
  thisDay += String(now.day(), DEC) + "/"; 
  thisMonth="";
  for (int i=0; i<=2; i++){
  thisMonth += monthString[((now.month()-1)*3)+i];
   }  
  thisDay=thisDay + thisMonth + "/"; 
  thisDay=thisDay + String(now.year() , DEC);
  const char* newDay = (const char*) thisDay.c_str(); 
  u8g.drawStr(22,63, newDay);   

  // ********************* 
  // display time in digital format
  thisTime="";
  if (now.hour() < 10){ thisTime=thisTime + " ";} // add leading space if required
  thisTime=String(now.hour()) + ":";
  if (now.minute() < 10){ thisTime=thisTime + "0";} // add leading zero if required
  thisTime=thisTime + String(now.minute()) + ":";
  if (now.second() < 10){ thisTime=thisTime + "0";} // add leading zero if required
  thisTime=thisTime + String(now.second());
  const char* newTime = (const char*) thisTime.c_str();
  
  u8g.drawStr(10,10, newTime);  
  // ********************* 
  //
  // Now draw the clock face
  u8g.drawCircle(clockCentreX, clockCentreY, 20); // main outer circle
  u8g.drawCircle(clockCentreX, clockCentreY, 2);  // small inner circle
  //
  //hour ticks
  for( int z=0; z < 360;z= z + 30 ){
  //Begin at 0° and stop at 360°
    float angle = z ;
    angle=(angle/57.29577951) ; //Convert degrees to radians
    int x2=(clockCentreX+(sin(angle)*20));
    int y2=(clockCentreY-(cos(angle)*20));
    int x3=(clockCentreX+(sin(angle)*(20-5)));
    int y3=(clockCentreY-(cos(angle)*(20-5)));
    u8g.drawLine(x2,y2,x3,y3);
  }
  // display second hand
  float angle = now.second()*6 ;
  angle=(angle/57.29577951) ; //Convert degrees to radians  
  int x3=(clockCentreX+(sin(angle)*(20)));
  int y3=(clockCentreY-(cos(angle)*(20)));
  u8g.drawLine(clockCentreX,clockCentreY,x3,y3);
  //
  // display minute hand
  angle = now.minute() * 6 ;
  angle=(angle/57.29577951) ; //Convert degrees to radians  
  x3=(clockCentreX+(sin(angle)*(20-3)));
  y3=(clockCentreY-(cos(angle)*(20-3)));
  u8g.drawLine(clockCentreX,clockCentreY,x3,y3);
  //
  // display hour hand
  angle = now.hour() * 30 + int((now.minute() / 12) * 6 )   ;
  angle=(angle/57.29577951) ; //Convert degrees to radians  
  x3=(clockCentreX+(sin(angle)*(20-11)));
  y3=(clockCentreY-(cos(angle)*(20-11)));
  u8g.drawLine(clockCentreX,clockCentreY,x3,y3);
 //
 // now add temperature if needed
 //getting the voltage reading from the temperature sensor
  // tempReading = analogRead(tempPin);   
 // converting that reading to voltage, for 3.3v arduino use 3.3
  // float voltage = tempReading * aref_voltage;
//   voltage /= 1024.0; 
 // now print out the temperature
 //  int temperatureC = (voltage - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
   //float t = dht.readTemperature();

  //
   //char Tstr[5];
   //dtostrf(dht.readTemperature(), 3,0, Tstr);
   //String thisTemp1 = String(abs(dht.readTemperature())) + "C";
   String thisTemp1 = String((int) dht.readTemperature()) + "C";
      // printing output as follows used less program storage space
   const char* thisTemp = (const char*) thisTemp1.c_str();
   u8g.drawStr(90,10,thisTemp); 
   // the print command could be used, but uses more memory
   //u8g.setPrintPos(100,10); thisMonth="";
  for (int i=0; i<=2; i++){
  thisMonth += monthString[((now.month()-1)*3)+i];
   }  
 
   //u8g.print(thisTemp1);

//
//
}




void menu1(void) {
   //***** RTC **********
  DateTime now = RTC.now();
  // ********************* 
  // display time in digital format
  u8g.setFont(u8g_font_osb21n); 
//  thisTime="";
//  if (now.hour() < 10){ thisTime=thisTime + " ";} // add leading space if required
  thisTime=String(now.hour()) + ":";
  if (now.minute() < 10){ thisTime=thisTime + "0";} // add leading zero if required
  thisTime=thisTime + String(now.minute()) + ":";
  if (now.second() < 10){ thisTime=thisTime + "0";} // add leading zero if required
  thisTime=thisTime + String(now.second());
  const char* newTime = (const char*) thisTime.c_str();
  if (now.hour()  < 10){ u8g.drawStr(21,30, newTime);} else {u8g.drawStr(5,30, newTime);} // add leading space if required

  u8g.setFont(u8g_font_profont15r);
  alarmTime="";
  if (alarmHour < 10){ alarmTime="0";} // add leading zero if required
  alarmTime=alarmTime + String (alarmHour)+":";
  if (alarmMinute < 10){ alarmTime=alarmTime+ "0";} // add leading zero if required
  alarmTime=alarmTime + String(alarmMinute);
  if (alarmStatus == 0) { alarmTime=alarmTime + " Off" ;} else { alarmTime=alarmTime + " On" ;}
  const char* newalarmTime = (const char*) alarmTime.c_str(); 
  u8g.drawStr(35,50, newalarmTime);

}


void menu11(void) {
menu1();
if (blinking) u8g.drawFrame(4, 4, 35, 30);
}

void menu12(void) {
menu1();
if (blinking) u8g.drawFrame(43, 4, 35, 30) ;
}

void menu13(void) {
menu1();
if (blinking) u8g.drawFrame(84, 4, 35, 30) ;

}


void menu14(void) {
menu1();
if (blinking) u8g.drawFrame(33, 38, 17, 16) ;

}
void menu15(void) {
menu1();
if (blinking) u8g.drawFrame(54, 38, 17, 16) ;

}
void menu16(void) {
menu1();
if (blinking) u8g.drawFrame(74, 38, 24, 16) ;

}




void menu3(void) {
  // graphic commands to redraw the complete screen should be placed here  
  // u8g.setFont(u8g_font_profont22);  
  u8g.setFont(u8g_font_osb21n); 
  //
  //***** RTC **********
  DateTime now = RTC.now();
   // display date at bottom of screen
  if (now.day() < 10){ thisDay="_";} // add leading space if required
  thisDay = String(now.day(), DEC) + "/"; 
  // thisDay=thisDay + thisMonth + "/"; 
  thisDay=thisDay + String(now.month() , DEC);
  thisDay=thisDay + thisMonth + "/"; 
// to add only the 3rd and the 4th digit.. other option is to subtract 2000
  thisDay=thisDay + String((now.year()-2000) , DEC);

// thisDay=thisDay + String(now.year()%10 + '0' , DEC);
// thisDay=thisDay + String((now.year()/10)%10 + '0', DEC);
   
  const char* newDay = (const char*) thisDay.c_str(); 
  if (now.day() < 10){ u8g.drawStr(18,35, newDay);} else {u8g.drawStr(2,35, newDay);} // add leading space if required
}


void menu31(void) {
menu3();
if (blinking) u8g.drawFrame(1, 10, 35, 29) ;

}

void menu32(void) {
menu3();
if (blinking) u8g.drawFrame(45, 10, 35, 29) ;

}

void menu33(void) {
menu3();
if (blinking) u8g.drawFrame(91, 10, 35, 29) ;

}



void menu2(void) {
    u8g.setFont(u8g_font_osb21n); 
    u8g.setScale2x2();  
   //char Tstr[5];
   //dtostrf(dht.readTemperature(), 3,0, Tstr);
   //String thisTemp1 = String(abs(dht.readTemperature())) + "C";
   String thisTemp1 = String((int) dht.readTemperature());
      // printing output as follows used less program storage space
   const char* thisTemp = (const char*) thisTemp1.c_str();
   u8g.drawStr(13,28,thisTemp); 
  u8g.setFont(u8g_font_profont15r);
  u8g.drawStr(45,13,"o"); 
  u8g.undoScale(); 
//  tone (11,2000,500);
  
}               


void menu4(void) {
    u8g.setFont(u8g_font_osb21n); 
    u8g.setScale2x2();  
   //char Tstr[5];
   //dtostrf(dht.readTemperature(), 3,0, Tstr);
   //String thisTemp1 = String(abs(dht.readTemperature())) + "C";
   String thisTemp1 = String((int) dht.readHumidity());
      // printing output as follows used less program storage space
   const char* thisTemp = (const char*) thisTemp1.c_str();
   u8g.drawStr(13,28,thisTemp); 
  u8g.setFont(u8g_font_profont15r);
  u8g.drawStr(46,24,"%"); 
  u8g.undoScale(); 
 
}               


void menu5(void) {

char buf[4];
u8g.drawStr( 0, 15, "Config Menu:");
u8g.drawStr( 0, 33, "Bright:");  // de 34 a 33
snprintf (buf, 4, "%d", BRIGHTNESS);
u8g.drawStr(80, 33, buf);  //de 90 a 80
u8g.drawStr( 0, 48, "Beep:");    
if (beep) { u8g.drawStr(80, 49, "On");} else { u8g.drawStr(80, 48, "Off"); }
u8g.drawStr( 0, 63, "Moon mode:");    

switch (moonMode) {
        case 0:
        u8g.drawStr(80, 63, "Phase");
        break;
     
        case 1:
        u8g.drawStr(80, 63, "Crep");
        break;

        case 2:
        u8g.drawStr(80, 63, "Demo 1");
        break;

        case 3:
        u8g.drawStr(80, 63, "Demo 2");
        break;

        case 4:
        u8g.drawStr(80, 63, "Demo 3");
        break;

        case 5:
        u8g.drawStr(80, 63, "Demo 4");
        break;

        case 6:
        u8g.drawStr(80, 63, "Demo 5");
        break;
        
        case 7:
        u8g.drawStr(80, 63, "Demo 6");
        break;
        
        case 8:
        u8g.drawStr(80, 63, "Full");
        break;


        
       
}

  }


void menu51(void) {
menu5();
if (blinking) u8g.drawFrame(77, 21, 24, 14) ;   //de 87 a 77
}


void menu52(void) {
menu5();
if (blinking) u8g.drawFrame(77, 37, 24, 14) ;
}

void menu53(void) {
menu5();
if (blinking) u8g.drawFrame(77, 51, 45, 13) ;
}



void menu6(void) {

    u8g.setFont(u8g_font_osb21n); 
    u8g.setScale2x2();  
 
  DateTime now = RTC.now();
  thisTime=String(now.hour()) + "";
  if (now.minute() < 10){ thisTime=thisTime + "0";} // add leading zero if required
  thisTime=thisTime + String(now.minute());
  const char* newTime = (const char*) thisTime.c_str();
  if (now.hour()  < 10){ u8g.drawStr(14,30, newTime);} else {u8g.drawStr(-2,30, newTime);} // add leading space if required

  
  u8g.setFont(u8g_font_profont15r);
  if (blinking) u8g.drawStr(27,30,"."); 
  u8g.undoScale(); 


}





void getPhase(int Y, int M, int D) {  // calculate the current phase of the moon
  double AG, IP;                      // based on the current date
  byte phase;                         // algorithm adapted from Stephen R. Schmitt's
                                      // Lunar Phase Computation program, originally
  long YY, MM, K1, K2, K3, JD;        // written in the Zeno programming language
                                      // http://home.att.net/~srschmitt/lunarphasecalc.html
  // calculate julian date
  YY = Y - floor((12 - M) / 10);
  MM = M + 9;
  if(MM >= 12)
    MM = MM - 12;
  
  K1 = floor(365.25 * (YY + 4712));
  K2 = floor(30.6 * MM + 0.5);
  K3 = floor(floor((YY / 100) + 49) * 0.75) - 38;

  JD = K1 + K2 + D + 59;
  if(JD > 2299160)
    JD = JD -K3;
  
  IP = normalize((JD - 2451550.1) / 29.530588853);
  AG = IP*29.53;
  phase = IP*39;
    Serial.print(AG);
    Serial.println();
    Serial.print(menu);
    Serial.println();
    

/*
 * 
  
  if(AG < 1.20369)
    //phase = B00000000;
  else if(AG < 3.61108)
    //phase = B00000001;
  else if(AG < 6.01846)
    //phase = B00000011;
  else if(AG < 8.42595)
    //phase = B00000111;
  else if(AG < 10.83323)
    //phase = B00001111;
  else if(AG < 13.24062)
    //phase = B00011111
  else if(AG < 15.64800)
    //phase = B00111111;
  else if(AG < 18.05539)
    //phase = B00111110;
  else if(AG < 20.46277)
    //phase = B00111100;
  else if(AG < 22.87016)
    //phase = B00111000;
  else if(AG < 25.27754)
    //phase = B00110000;
  else if(AG < 27.68493)
    //phase = B00100000;
  else
    //phase = 0;


 */

  for( int z=0; z < 18;z= z + 1 ){
 leds[z] = CRGB::Black;
  }
 
  
  if (phase <17) {
 for( int z=0; z < (phase-2) ;z= z + 3 ){
 leds[z] = CRGB::White;
 leds[z+1] = CRGB::White;
 leds[z+2] = CRGB::White;


// sprintf(buffer, "0 a 17 %d", (z+2));
//    Serial.println(buffer);

  }
}

  if ((phase >=17)&&(phase <39)) {
     for( int z=0; z < 18;z= z + 1 ){
 leds[z] = CRGB::White;
  }
      
  for( int z=0; z < (phase -21) ;z= z + 3 ){
 leds[z] = CRGB::Black;
 leds[z+1] = CRGB::Black;
 leds[z+2] = CRGB::Black;

// sprintf(buffer, "18 a 39 %d", (z+2));
//    Serial.println(buffer);

  }

}

  FastLED.show();

}



void demoMode0(void) {

  fadeToBlackBy( leds, NUM_LEDS, 20);
 byte dothue = 0;
 for( int i = 0; i < 8; i++) {
   leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
   dothue += 32;
 }
}


void demoMode1(void) {

  //Modo DEMO



  empty();
  
  DateTime now = RTC.now();
  if (now.second()<17) {
 for( int z=0; z < (now.second()) ;z= z + 3 ){
 leds[z] = CRGB::White;
 leds[z+1] = CRGB::White;
 leds[z+2] = CRGB::White;


// sprintf(buffer, "0 a 17 %d", (z+2));
//    Serial.println(buffer);

  }
}

  if ((now.second()>18)&&(now.second()<39)) {
    
  full();

      
  for( int z=0; z < (now.second()-21) ;z= z + 3 ){
 leds[z] = CRGB::Black;
 leds[z+1] = CRGB::Black;
 leds[z+2] = CRGB::Black;

// sprintf(buffer, "18 a 39 %d", (z+2));
//    Serial.println(buffer);

  }

}

  if (now.second()>38) {
 // rainbow();
 // addGlitter(80);

 demoMode0();

  }

FastLED.show();

  
  }

void empty(){

  for( int z=0; z < 18;z= z + 1 ){
 leds[z] = CRGB::Black;
  }

}



void full(){

  for( int z=0; z < 18;z= z + 1 ){
 leds[z] = CRGB::White;
  }
}

void demoMode2(void) {
   gHue++; 
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  addGlitter(80);
FastLED.show();
  }



void demoMode3(void) {
 gHue++; 
//   EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow

  // built-in FastLED rainbow, plus some random sparkly glitter
 // rainbow();
 // addGlitter(80);

  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
//  uint8_t BeatsPerMinute = 62;
//  CRGBPalette16 palette = PartyColors_p;
//  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
 // for( int i = 0; i < NUM_LEDS; i++) { //9948
 //   leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
 // }

 
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);

FastLED.show();

  
  }

void demoMode4() {

  
   // eight colored dots, weaving in and out of sync with each other << esta es buena
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 3; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  gHue++; 
  FastLED.show();
}



void demoMode5()
{
 
  rainbow();
  addGlitter(80);
  gHue++; 
  FastLED.show();

}


void demoMode6()
{

  empty();
  demoMode0();   
  FastLED.show();


}



/*
 * 
 * 
 * 

#define COOLING  55





// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120
bool gReverseDirection = false;

void demoMode5()
{
 // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < SPARKING ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < NUM_LEDS; j++) {
      CRGB color = HeatColor( heat[j]);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (NUM_LEDS-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }
 
 FastLED.show();
}

 */





void crepuscularMode(void) {

  //Modo DEMO

  for( int z=0; z < 18;z= z + 1 ){
 leds[z] = CRGB::Black;
  }

  DateTime now = RTC.now();
  
  int secondstoalarm=0;
   DateTime alarma =  DateTime( now.year(), now.month(),  now.day(), alarmHour, alarmMinute, 0);
   secondstoalarm =  alarma.unixtime()-now.unixtime();

   sprintf(buffer, "unixtime %d", (secondstoalarm));
   Serial.println(buffer);

   if ((secondstoalarm>0) && (secondstoalarm<254)) {
      FastLED.setBrightness(255-secondstoalarm);
          for( int z=0; z < 18;z= z + 1 ){
          leds[z] = CRGB::White;
    
    } }
    

FastLED.show();
}



double normalize(double v) {           // normalize moon calculation between 0-1
    v = v - floor(v);
    if (v < 0)
        v = v + 1;
    return v;
}


void write_NVRAM(void) {
  uint8_t writeData[9] = {16,16, BRIGHTNESS,beep,menu,moonMode,alarmHour,alarmMinute,alarmStatus };
  RTC.writenvram(0, writeData , 9);
}




void setup(void) {
  Serial.begin(9600);

  //We give the power supply to the DTH11 using the pin 3
  pinMode(3, OUTPUT);
  digitalWrite(3,HIGH);   

  // init buttons pins; I suppose it's best to do here
  button1.init();
  button2.init();
  button3.init();

  
  Wire.begin();
// RTC.begin();
// // following line sets the RTC to the date & time this sketch was compiled
// RTC.adjust(DateTime(__DATE__, __TIME__));


if (! RTC.isrunning()) {
   Serial.println("RTC is NOT running!");
 // following line sets the RTC to the date & time this sketch was compiled
   RTC.adjust(DateTime(__DATE__, __TIME__));
 }

if (!((RTC.readnvram(0)==16) & (RTC.readnvram(1)==16))) {
   Serial.println("Writing NVRAM default data");
// following line sets the NVRAM default data

//  uint8_t writeData[9] = {16,16, BRIGHTNESS,beep,menu,moonMode,alarmHour,alarmMinute,alarmStatus };
//  RTC.writenvram(0, writeData , 9);
  write_NVRAM();
  
 }

// reading data from NVRAM

BRIGHTNESS = RTC.readnvram(2);
beep = RTC.readnvram(3);
menu = RTC.readnvram(4);
moonMode = RTC.readnvram(5);
alarmHour = RTC.readnvram(6);
alarmMinute = RTC.readnvram(7);
alarmStatus = RTC.readnvram(8);


//   Serial.println(RTC.readnvram(0), DEC);
//   Serial.println(RTC.readnvram(1), DEC);
//   Serial.println(RTC.readnvram(2), DEC);
//   Serial.println(RTC.readnvram(3), DEC);
//   Serial.println(RTC.readnvram(4), DEC);



 //  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  
  

}



void loop(void) {


 // blinking timer for the setup selector

unsigned long currentMillis = millis();

  if (currentMillis - previousBlinking >= period_Blinking ) {
    // save the last time you blinked
    previousBlinking = currentMillis;
    blinking=!blinking;
  }
   

  
  // picture loop
  u8g.firstPage();  
  do {

             switch (menu)
               {  case 0:                       
                     menu0();
                     break;  
                  case 1:                       
                     menu1();
                     break;  
                  case 11:                       
                     menu11();
                     break;  
                  case 12:                       
                     menu12();
                     break;
                  case 13:                       
                     menu13();
                     break;
                  case 14:                       
                     menu14();
                     break;
                  case 15:                       
                     menu15();
                     break;
                  case 16:                       
                     menu16();
                     break;

                     
                  case 2:                       
                     menu2();
                     break;  

                  case 31:                       
                     menu31();
                     break;  
                  case 32:                       
                     menu32();
                     break;
                  case 33:                       
                     menu33();
                     break;
                  case 3:                       
                     menu3();
                     break;
                  case 4:                       
                     menu4();
                     break;
                     
                  case 5:                       
                     menu5();
                     break;

                  case 51:                       
                     menu51();
                     break;  
                  case 52:                       
                     menu52();
                     break;
                  case 53:                       
                     menu53();
                     break;               

                case 6:                       
                     menu6();
                     break;

               
               }

  
  } while( u8g.nextPage() );

  
  // rebuild the picture after some delay
//   delay(50);

   // handle button
  int event1 = button1.handle();
  int event2 = button2.handle();
  int event3 = button3.handle();
  

  // do other things
//  print_event("1 eve %d", event1);
//  print_event("2 eve %d" ,event2);
//  print_event("3 eve %d" ,event3);
//
//    sprintf(buffer, "Menu is %d", menu);
//    Serial.println(buffer);


//   sprintf(buffer, "Alarma %d", alarm_button_off);
//    Serial.println(buffer);

 
 // Action of the Long Press on Button 1 within each Menu.
   if (event1 == 2)
          { if (beep) tone (11,2000,40);             
             switch (menu)
               {
                  case 1:                       
                     menu=11;
                     break;  
                  case 3:                       
                     menu=31;
                     break;
                  case 5:                       
                     menu=51;
                     break;
                       
               }
         }

DateTime now = RTC.now();

 // Action of the Short Press on Button 2 within each Menu.
   if (event2 == 1)
          { if (beep) tone (11,3000,60);
             switch (menu)
               {
                  case 11:
                   RTC.adjust(DateTime( now.year(), now.month(),  now.day(), (now.hour()+1)%24, now.minute(), now.second()));
                   break;  
                  case 12:                       
                   RTC.adjust(DateTime( now.year(), now.month(),  now.day(), now.hour(), (now.minute()+1)%60, now.second()));
                    break;  
                  case 13:                       
                   RTC.adjust(DateTime( now.year(), now.month(),  now.day(), now.hour(), now.minute(), now.second()+1));
                   break;  

                  case 14:
                  alarmHour=alarmHour+1;
                   break;
                  case 15:
                  alarmMinute=alarmMinute+1;
                   break;
                  case 16:
                  alarmStatus=!alarmStatus;

                  case 31:
                   RTC.adjust(DateTime( now.year(), now.month(),  (now.day()+1)%32, now.hour(), now.minute(), now.second()));
                   break;  
                  case 32:                       
                   RTC.adjust(DateTime( now.year(), (now.month()+1) %13,  now.day(), now.hour(), now.minute(), now.second()));
                    break;  
                  case 33:                       
                   RTC.adjust(DateTime( now.year()+1, now.month(),  now.day(), now.hour(), now.minute(), now.second()));
                   break;  

                  case 51:
                  BRIGHTNESS=BRIGHTNESS+10;
                  FastLED.setBrightness(BRIGHTNESS);
                  break;

                  case 52:
                  beep=!beep;
                  break;

                  case 53:
                  moonMode=(moonMode+1)% 9;
                  
                  break;
                  
               
               }
         }




if ((alarmStatus) && (!alarm_button_off)) {
  
  if ((now.hour()==alarmHour) && (now.minute()== alarmMinute)) {
      
      moonMode=7;
      
      if (blinking) tone (11,6000,60);
            
      if ((event1 ==1) or (event2 ==1) or (event3 == 1)) { 
        alarm_button_off= true;
        moonMode=0;
      
      }
          }
  }

//rearm the alarm_button_off
if ((now.hour()==alarmHour) && (now.minute()== (alarmMinute+1))) alarm_button_off= false;
if ((now.hour()==alarmHour) && (now.minute()+1== (alarmMinute))) alarm_button_off= false;



 // Action of the Short Press on Button 3 within each Menu.
   if (event3 == 1)
          { if (beep) tone (11,6000,60);
             switch (menu)
               {  
                  case 11:
                   RTC.adjust(DateTime( now.year(), now.month(),  now.day(), now.hour()-1, now.minute(), now.second()));
                   break;  
                  case 12:                       
                   RTC.adjust(DateTime( now.year(), now.month(),  now.day(), now.hour(), now.minute()-1, now.second()));
                    break;  
                  case 13:                       
                   RTC.adjust(DateTime( now.year(), now.month(),  now.day(), now.hour(), now.minute(), now.second()-1));
                   break;  
                  case 14:
                  alarmHour=alarmHour-1;
                   break;
                  case 15:
                  alarmMinute=alarmMinute-1;
                   break;
                  case 16:
                  alarmStatus=!alarmStatus;

                  case 31:
                   RTC.adjust(DateTime( now.year(), now.month(),  now.day()-1, now.hour(), now.minute(), now.second()));
                   break;  
                  case 32:                       
                   RTC.adjust(DateTime( now.year(), now.month()-1,  now.day(), now.hour(), now.minute(), now.second()));
                    break;  
                  case 33:                       
                   RTC.adjust(DateTime( now.year()-1, now.month(),  now.day(), now.hour(), now.minute(), now.second()));
                   break;  


                  case 51:
                  BRIGHTNESS=BRIGHTNESS-10;
                  FastLED.setBrightness(BRIGHTNESS);
                  break;
             
                  case 52:
                  beep=!beep;
                  break;

                  case 53:
                  moonMode=(moonMode+1)% 9;
                  break;


               }
         }






          
 
 // Action of the Short Press on Button 1 within each Menu.
   if (event1 == 1)
          { if (beep) tone (11,2000,20);
            switch (menu)
               {
                  case 0:                       
                     menu=1;
                     write_NVRAM();
                     break;     
                  case 1:                       
                     menu=2;
                     write_NVRAM();
                     break;
                  case 11:                       
                     menu=12;
                     break;
                  case 12:                       
                     menu=13;
                     break;
                  case 13:                       
                     menu=14;
                     break;  
                  case 14:                       
                     menu=15;
                     break;  
                  case 15:                       
                     menu=16;
                     break;  
                  case 16:                       
                     menu=1;
                     write_NVRAM();
                     break;  
                  
                  case 2:                     
                     menu=3;
                     write_NVRAM();
                     break;
 
                  case 3:                    
                     menu=4;
                     write_NVRAM();
                     break;
                  case 31:                     
                     menu=32;
                     break;
                  case 32:                     
                     menu=33;
                     break;
                  case 33:                     
                     menu=3;
                     write_NVRAM();
                     break;

        case 4:                    
                     menu=5;
                     write_NVRAM();
                     break;
          
        case 5:                    
                     menu=6;
                     write_NVRAM();
                     break;

                  case 51:                     
                     menu=52;
                     break;
                  case 52:                     
                     menu=53;
                     break;
                  case 53:                     
                     menu=5;
                     write_NVRAM();
                     break;
        case 6:                    
                     menu=0;
                     write_NVRAM();
                     break;
         
               }
          }



switch (moonMode) {
       case 0:
       y = now.year();
       m = now.month();
       d = now.day();
       getPhase (y , m, d);
       break;
   
      case 1:
      crepuscularMode();
      break;

      case 2:
      demoMode1();
      break;

      case 3:
      demoMode2();
      break;

      case 4:
      demoMode3();
      break;

      case 5:
      demoMode4();
      break;

      case 6:
      demoMode5();
      break;

      case 7:
      demoMode6();
      break;
      
      case 8:
      full();
      FastLED.show();
      break;




}
 



}

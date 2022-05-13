/*


WORKING JUST WITH TEA5767 module
ROTARY SW SET MANUAL/AUTO TUNING
SHOWING SIGNAL LEVEL
Memoria EEPROM save a new station after 100 seconds if it is different from the previous one on EEPROM

https://github.com/RodLophus/TEA5767_Radio_UNO
https://github.com/RodLophus/TEA5767_Radio
https://github.com/andykarpov/TEA5767
https://www.hackster.io/mircemk/diy-retro-look-fm-radio-with-tea5767-module-370b88


Notes:
-----
- The TEA5767 maximum supply voltage is 5V.  Be sure your Arduino is not sourcing more than 5V
- The TEA5767 does not update the signal level indicator on read.  The signal level will only be update on station change
- If you get some glitches on the encoder or on the pushbutton, use a snubber network, like this:


Connections:
-----------
- Encoder (with "pushable" shaft switch):
Push button     ---> Arduino pin 2
Encoder pin "A" ---> Arduino pin 3
Encoder pin "B" ---> Arduino pin 4


- OLED SH1106:
SDA ---> Arduino pin A4
SCK ---> Arduino pin A5
GND ---> GND
VDD ---> Arduino 5V

- TEA5756 module:

Top view:
+-10--9--8--7--6-+
|  +------+  ++  |
|  | TEA  |  ||  |
|  | 5767 |  ||  |
|  +------+  ++  |
+--1--2--3--4--5-+

1 ----> Arduino SDA
2 ----> Arduino SCL
3 ----> GND
5 ----> +5V
6 ----> GND
7 ----> Audio out (right channel)
8 ----> Audio out (left channel)
10 ---> Antenna

*/


#include <Wire.h>
#include <SPI.h>                                 
#include <Adafruit_SH1106.h>
#include <EEPROM.h>;

#include <TEA5767.h> // Get TEA5767 library at https://github.com/andykarpov/TEA5767

// Encoder pins
#define ENCODER_SW 2
#define ENCODER_A  3
#define ENCODER_B  4

// Global status flags
#define ST_AUTO    0      // Auto mode (toggled by the push button)
#define ST_GO_UP   2      // Encoder being turned clockwise
#define ST_GO_DOWN 3      // Encoder being turned counterclockwise
#define ST_SEARCH  4      // Radio module is perfoming an automatic search

#define OLED_RESET 5
Adafruit_SH1106 display(OLED_RESET);

const int LED = 0;

unsigned char buf[5];
int stereo;
int signalLevel;
int searchDirection;
int i = 0;

long seconds = 0;
float memo;


TEA5767 Radio;
float frequency = 93.3;
byte status = 0;



/******************************\
 *         isrEncoder()      *
 * Catch encoders interrupts *
\******************************/
void isrEncoder() {
  delay(50);    // Debouncing (for crappy encoders)
  if(digitalRead(ENCODER_B) == HIGH){
    bitWrite(status, ST_GO_UP, 1);
  } else    bitWrite(status, ST_GO_DOWN, 1);
}


/*******************\
 * Arduino Setup() *
\*******************/
void setup() {
      
  int i;
  

  pinMode(ENCODER_SW, INPUT); digitalWrite(ENCODER_SW, HIGH);
  pinMode(ENCODER_A, INPUT);  digitalWrite(ENCODER_A, HIGH);
  pinMode(ENCODER_B, INPUT);  digitalWrite(ENCODER_B, HIGH);
  
  attachInterrupt(1, isrEncoder, RISING);

  // Initialize the radio module
  Wire.begin();
  Serial.begin(9600);

//memo = 93.3;          //Execute just once
//EEPROM.put(0, memo);  //Execute just once
EEPROM.get(0, memo);
frequency = memo;
      
  Radio.init();
  Radio.set_frequency(frequency);
  telaradio ();

display.begin(SH1106_SWITCHCAPVCC, 0x3C);
display.clearDisplay();
display.setTextColor(WHITE);
display.setCursor(0,5);
display.setTextSize(2);
display.print("by");
display.setCursor(0,30);
display.print("Eng. Roge");
display.setCursor(0,50);
display.setTextSize(1);
display.println("     Laoding...");
display.display();
delay(2000);


}

/******************\
 * Arduino Loop() *
\******************/
void loop() {


seconds++;

//Serial.println(seconds);

      if(seconds == 200)
      {
          if(EEPROM.get(0, memo)!=frequency)
          {
//Serial.println("loop(): Saving new frequency to EEPROM");
            memo = frequency;
            EEPROM.put(0, memo);
            telaradio ();            
          }
      } 

 
   
  if bitRead(status, ST_AUTO)   // Auto/manual LED 
  {
   
    digitalWrite(LED, LOW);
  }
  else
  {
    
    digitalWrite(LED, HIGH);
  }

  if (digitalRead(ENCODER_SW) == LOW)
    {
       if(bitRead(status, ST_AUTO))
       bitWrite(status, ST_AUTO, 0);
       else
       bitWrite(status, ST_AUTO, 1);
       delay(100);
       telaradio (); 
    }

  if (Radio.read_status(buf) == 1) {            // Get radio data
    frequency = floor(Radio.frequency_available(buf) / 100000 + .5) / 10;
    stereo = Radio.stereo(buf);                 // 0 <= Radio.signal_level <= 15
    signalLevel = (Radio.signal_level(buf) * 100) / 15;
    telaradio ();
    
    }
  
  if(bitRead(status, ST_SEARCH)) {   // Is the radio performing an automatic search?
    if(Radio.process_search(buf, searchDirection) == 1) {
      bitWrite(status, ST_SEARCH, 0);
      telaradio ();
      seconds = 0;
    }
  }
  


  // Encoder being turned clockwise (+)
  if(bitRead(status, ST_GO_UP)) {
    if(bitRead(status, ST_AUTO) && !bitRead(status, ST_SEARCH)) {
      // Automatic search mode (only processed if the radio is not currently performing a search)
      bitWrite(status, ST_SEARCH, 1);
      searchDirection = TEA5767_SEARCH_DIR_UP;
      Radio.search_up(buf);
      delay(50);
    } else {
      // Manual tuning mode
      if(frequency < 108.1) 
      {
        frequency += 0.1;
        Radio.set_frequency(frequency);
        seconds = 0;}

       else  {
           frequency = 88;
           Radio.set_frequency(frequency);
           seconds = 0;
             }  
                   
       }
    bitWrite(status, ST_GO_UP, 0);
    telaradio ();  
  }

  // Encoder being turned counterclockwise (-)
  if(bitRead(status, ST_GO_DOWN)) {
    if(bitRead(status, ST_AUTO) && !bitRead(status, ST_SEARCH)) {
      // Automatic search mode (only processed if the radio is not currently performing a search)
      bitWrite(status, ST_SEARCH, 1);
      searchDirection = TEA5767_SEARCH_DIR_DOWN;
      Radio.search_down(buf);
      delay(50);
    } else {
      // Manual tuning mode
      if(frequency > 88) {
        frequency -= 0.1;
        Radio.set_frequency(frequency);
        seconds = 0;
      }

     else  {
           frequency = 108;
           Radio.set_frequency(frequency);
           seconds = 0;
             } 

    }
    bitWrite(status, ST_GO_DOWN, 0);
    telaradio ();  
  }

}
/******************\
 * OLED UPDATE *
\******************/
void telaradio ()

{
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextSize(2);
        display.print(" FM RADIO");
        display.setCursor(0,20);
        display.setTextSize(2);
        if(frequency < 100) display.print(" ");
        display.print(" ");
        display.print(frequency);
        display.setTextSize(1);
        display.print(" MHz");
        display.setCursor(10,40);
        display.setTextSize(1);
        //display.print(" ");
        //display.println(bitRead(status, ST_AUTO) ? 'A' : 'M');
        display.print(bitRead(status, ST_AUTO) ? "AUTO" : "MANUAL");
        display.setCursor(70,40);
        display.setTextSize(1);
        display.print("Sig: ");
        display.print(String (signalLevel));
        display.print("%");

        display.setCursor(10,50);
        display.setTextSize(1);
        display.print("Memo: ");
        display.print(memo);
        
        display.display();
        

}

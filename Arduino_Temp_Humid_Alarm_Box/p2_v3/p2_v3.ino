#include <FastLED.h>
#include <SimpleDHT.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

/* ////////////////////////////////////////////
Wiring:
LED: Green 8, Red 9
RBG: Pin 7
RTC: SQW 1, SCL A5, SDA A4
DHT: DAT 2
*/
/////////////////////////////////////////////

// Initializes the names of the pins after their function
#define LED_GREEN 8
#define LED_RED 9
// EOL Token
#define t_EOL 200
// Commands for LED
#define t_LED 100
#define t_ON 101
#define t_OFF 102
#define t_BLINK 103
// Tokens of addtional words
#define t_SET 110
#define t_STATUS 111
#define t_VERSION 112
#define t_HELP 113
#define t_WORD 115
#define t_LEDS 116
#define t_RGB 117
#define t_COLOR 118
#define t_WEATHER 119

// Tokems for clock
#define t_TIME 121
#define t_PRINT 122
#define t_UNIT 123
// EEPROM Token
#define t_EEPROM 124
#define t_CLEAR 125
#define t_MINMAX 126

// Error
#define t_ERROR 255

// ADD token
#define t_ADD 20

// What LED
#define t_RED 51
#define t_GREEN 52
#define t_D13 53
// defines clock
RTC_DS3231 rtc;

// Configures DHT on pin 2
const int pinDHT22 PROGMEM = 2;
SimpleDHT22 dht22(pinDHT22);
// Keeps track of runtime
unsigned short int LAST_TIME = 0;
// RGB led color
unsigned char COLOR[3] = { 255 };
// How many RGB leds
CRGB leds[1];
// Init LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
//NEED VARIABLE TO CONTROL LED //
unsigned char LED_CONTROL = 0;
//NEED VARIABLE TO CONTROL D13 //
unsigned char D13_CONTROL = 0;
//NEED VARIABLE TO CONTROL RGB LED //
unsigned char RGB_CONTROL = 0;


// Temperture and Humidity
char TEMPERATURE;
unsigned char HUMIDITY;
long unsigned int LAST_TIME_RECORD = 0;
bool UNIT = 0;
unsigned char RECORD_COUNTER = 0;

// Sets blink rate
short unsigned int BLINK_RATE = 500;

// Max array size
static const unsigned char MAX_I PROGMEM = 22;
static const unsigned char MAX_T PROGMEM = 15;

// Makes Token Buffer
unsigned char TOKEN_BUFFER[MAX_T] = { 0 };
char INPUT_BUFFER[MAX_I] = { 0 };
// Creates lookup array
const char LOOKUP[] PROGMEM = {
  13, 0, 1, t_EOL,         // EOL
  'l', 'e', 3, t_LED,      // t_LED
  'o', 'n', 2, t_ON,       // t_ON
  'o', 'f', 3, t_OFF,      // t_OFF
  'b', 'l', 5, t_BLINK,    // t_BLINK
  's', 'e', 3, t_SET,      // t_SET
  's', 't', 6, t_STATUS,   // t_STATUS
  'v', 'e', 7, t_VERSION,  // t_VERSION
  'h', 'e', 4, t_HELP,     // t_HELP
  'r', 'e', 3, t_RED,      // t_RED
  'g', 'r', 5, t_GREEN,    // t_GREEN
  'r', 'g', 2, t_RGB,      // t_RG
  'd', '1', 3, t_D13,      // t_D13
  'l', 'e', 4, t_LEDS,     // t_D13
  'a', 'd', 3, t_ADD,      // t_ADD
  'c', 'o', 5, t_COLOR,    // t_COLOR
  'r', 'g', 3, t_RGB,      // t_COLOR
  't', 'i', 4, t_TIME,     // t_TIME
  'p', 'r', 5, t_PRINT,    // t_PRINT
  'w', 'e', 7, t_WEATHER,  // t_WEATHER
  'u', 'n', 4, t_UNIT,     // t_UNIT
  'e', 'e', 6, t_EEPROM,   // t_EEPROM
  'c', 'l', 5, t_CLEAR,    // t_CLEAR
  'm', 'i', 6, t_MINMAX    // t_MINMAX

};


void setup() {
  Serial.begin(9600);  // Starts serial
  // Declares pins as output pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  FastLED.addLeds<NEOPIXEL, 7>(leds, 1);

  // SETUP RTC MODULE
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1)
      ;
  }

  // automatically sets the RTC to the date & time on PC this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  lcd.init();  // initialize the lcd
}


void loop() {
  // put your main code here, to run repeatedly:
  input();
  processInput();  // Takes in user input and inputs tokens into token array. Returns 0 if a valid token was entered. Else Returns 1.
  tokenTree();     // Runs all tokens
  printTokens();
  tokenBufferClear();  // clears token buffer  }
  inputBufferClear();  // clears input buffer
}

/* The below code takes user input and puts it into a buffer array */
void input() {
  Serial.println(F("Enter Command"));  // Prompts user for command
  bool noEnter = 1;                    // used to break loop when enter key is pressed
  int index = 0;                       // keeps track of index in input buffer
  char curInput = 0;

  while (noEnter == 1) {

    while (Serial.available() == 0)  // Wait for data available
    {
      controlLeds();  // Controls power to LEDs
      storeData();
      temp2lcd();
    }
    curInput = Serial.read();
    if ((char)curInput == 13) {  // checks if enter key
      noEnter = 0;
      Serial.println(F(" "));
    } else if ((char)curInput == 127 && index > 0) {  // handles backspaces
      index--;
      INPUT_BUFFER[index] = 0;
      backspace();
    } else if (index == MAX_I) {  // Checks for too large input

    } else if (isalnum((char)curInput) != 0 || (char)curInput == ' ') {  // puts in input buffer

      INPUT_BUFFER[index] = (char)curInput;
      Serial.print((char)curInput);
      index++;
    }
  }
}


/* The code below asks for user input and parces it into a array of tokens (using toToken function) */
int processInput() {

  // Create a token buffer
  int TOKENCOUNTER = 0;  // Keeps track of how many tokens were generated

  char toTokenize[MAX_I] = { 0 };  // Array of stuff to tokenize. Resets after a space or EOL

  int counter = 0;                 // Keeps track of location in toTokenize array
  for (int i = 0; i < MAX_I; i++)  // Iterates through entire input array
  {
    if (isalnum(INPUT_BUFFER[i])) {  // If input is alph / num adds it to the toTokenize array
      toTokenize[counter] = INPUT_BUFFER[i];
      counter++;
      if (i == MAX_I - 1) {  // Handles last input
        char lastInput[2] = { toTokenize[0], toTokenize[1] };
        toToken(lastInput, strlen(toTokenize), TOKENCOUNTER);
      }
    } else {                     // Else it sends the toTokenize array to check against lookup table
      if (toTokenize[0] != 0) {  // Checks to ensure someting exists to check
        // adds any valid imputs into token buffer
        TOKENCOUNTER += toToken(toTokenize, strlen(toTokenize), TOKENCOUNTER);

        TOKENCOUNTER = check_if_blink_set(TOKENCOUNTER, i);  // Checks if there is a word token
        TOKENCOUNTER = check_if_add(TOKENCOUNTER, i);
        TOKENCOUNTER = check_if_set_color(TOKENCOUNTER, i);
      }
      for (int j = 0; j < MAX_T; j++) {  // Clears the to tokenize array
        toTokenize[j] = { 0 };
      }
      counter = 0;
    }
  }


  char first2[2] = { toTokenize[0], toTokenize[1] };
  for (int j = 0; j < MAX_I; j++) {  // Clears the to tokenize array
    toTokenize[j] = { 0 };
  }
  counter = 0;
  if (TOKENCOUNTER == 0) {
    return 1;
  }
  TOKEN_BUFFER[TOKENCOUNTER] = t_EOL;
  return 0;
}


/* The code below takes the token buffer and runs the corrosponding functions */
void tokenTree() {
  int current = 0;
  switch (TOKEN_BUFFER[current]) {
    unsigned short int rec;
    unsigned short int rec2;
    case t_SET:  // Deals with all possiblities from a set token
      current++;
      switch (TOKEN_BUFFER[current]) {
        unsigned short int rec;

        case t_BLINK:  // Deals with all possiblities from a set token
          current += 2;
          if (EOL(current + 2) == 1) {
            return;
          }
          rec = (int)(((unsigned)TOKEN_BUFFER[current + 1] << 8) | TOKEN_BUFFER[current]);  // gets origional number back
          BLINK_RATE = rec;
          Serial.print(F("Blink rate set to: "));
          Serial.println(rec);
          break;

        case t_COLOR:
          setColor();
          break;
        case t_TIME:
          promptForTimeAndDate(Serial);

          break;

        default:
          Serial.println(F("ERROR: Invalid command type \"help\""));
          break;
      };
      break;
    case t_LED:  // Deals with all possiblities from a LED token
      current++;
      switch (TOKEN_BUFFER[current]) {
        case t_BLINK:  // FIX


          if (TOKEN_BUFFER[current + 1] != t_EOL) {
            current++;

            if (TOKEN_BUFFER[current + 1] == t_EOL) {

              if (TOKEN_BUFFER[current] == t_RGB) {
                LED_CONTROL = 187;  // 10 11 10 11
                Serial.println(F("LED blink set to r/g"));
                return;
              } else {
                outputError();
                return;
              }
            }
          }

          else {
            LED_CONTROL = LED_CONTROL ^ 136;  // swaps 10 00 10 00
            Serial.println(F("LED blink toggle"));
          }
          break;

        case t_RED:
          current++;
          if (EOL(current + 1) == 1) {
            return;
          }
          LED_CONTROL = LED_CONTROL & 170;  // sets all color bits to 0 (170 = 10101010)
          Serial.println(F("LED red"));
          break;

        case t_GREEN:
          if (EOL(current + 1) == 1) {
            return;
          }
          LED_CONTROL = LED_CONTROL | 85;  // sets all color bits to 0 (85 = 01010101)
          Serial.println(F("LED green"));
          break;
        case t_ON:
          if (EOL(current + 1) == 1) {
            return;
          }
          LED_CONTROL = LED_CONTROL | 170;  // sets all power bits to 1 (170 = 10101010)
          Serial.println(F("LED on"));
          break;

        case t_OFF:
          if (EOL(current + 1) == 1) {
            return;
          }
          LED_CONTROL = LED_CONTROL & 85;  // 01010101
          Serial.println(F("LED off"));
          break;

        default:
          outputError();

          break;
      }

      break;

    case t_D13:  // Deals with all possiblities from a D13 token
      current++;
      switch (TOKEN_BUFFER[current]) {
        case t_BLINK:  // Toggles Blink
          if (EOL(current + 1) == 1) {
            return;
          }
          D13_CONTROL = 170;
          Serial.println(F("D13 blink toggled"));

          break;
        case t_ON:
          if (EOL(current + 1) == 1) {
            return;
          }
          D13_CONTROL = 255;
          Serial.println(F("D13 on"));

          break;

        case t_OFF:
          if (EOL(current + 1) == 1) {
            return;
          }
          D13_CONTROL = 0;
          Serial.println(F("D13 off"));

          break;

        default:
          Serial.println(F("Invalid command type \"help\""));
          break;
      }
      break;
    case t_RGB:
      current++;
      switch (TOKEN_BUFFER[current]) {
        case t_BLINK:  // Toggles Blink
          if (EOL(current + 1) == 1) {
            return;
          }
          RGB_CONTROL = 170;
          Serial.println(F("RGB blink toggled"));

          break;
        case t_ON:
          if (EOL(current + 1) == 1) {
            return;
          }
          RGB_CONTROL = 255;
          Serial.println(F("RGB on"));

          break;

        case t_OFF:
          if (EOL(current + 1) == 1) {
            return;
          }
          RGB_CONTROL = 0;
          Serial.println(F("RGB off"));

          break;

        default:
          Serial.println(F("Invalid command type \"help\""));
          break;
      }



      break;
    case t_HELP:
      if (EOL(current + 1) == 1) {
        return;
      }
      print_help();
      break;

    case t_STATUS:
      current++;
      if (TOKEN_BUFFER[current] == t_LEDS) {
        current++;
        if (EOL(current + 1) == 1) {
          return;
        }
        print_status();
      } else {
        Serial.println(F("Invalid command type \"help\""));
      }
      break;


    case t_VERSION:
      if (EOL(current + 1) == 1) {
        return;
      }
      Serial.println(F("Version: 7 +/- 2"));
      break;


    case t_ADD:
      current += 2;
      rec = (int)(((unsigned)TOKEN_BUFFER[current + 1] << 8) | TOKEN_BUFFER[current]);  // gets origional number back
      current += 3;
      rec2 = (int)(((unsigned)TOKEN_BUFFER[current + 1] << 8) | TOKEN_BUFFER[current]);  // gets origional number back
      current++;
      addNum(rec, rec2);
      break;

    case t_PRINT:
      current++;
      switch (TOKEN_BUFFER[current]) {

        case t_TIME:
          if (EOL(current + 1) == 1) {
            return;
          }
          printTime();
          break;

        case t_WEATHER:
          if (EOL(current + 1) == 1) {
            return;
          }
          printWeather(TEMPERATURE, HUMIDITY);

          break;
        case t_EEPROM:
          if (EOL(current + 1) == 1) {
            return;
          }
          printEEPROM();
          Serial.println(readIntFromEEPROM(0));
          Serial.println(F("EEPROM PRINTED"));
          break;
        case t_MINMAX:
          if (EOL(current + 1) == 1) {
            return;
          }
          printMinMax();

          break;
        default:
          if (EOL(current + 1) == 1) {
            return;
          }
          Serial.println(F("ERROR: Invalid command type \"help\""));
          break;
      };
      break;
    case t_UNIT:
      UNIT = (UNIT + 1) % 2;
      if (UNIT == 0) {
        Serial.println(F("TEMPERTURE UNIT: C"));
      } else {
        Serial.println(F("TEMPERTURE UNIT: F"));
      }

      break;
    case t_CLEAR:
      current++;
      if (TOKEN_BUFFER[current] == t_EEPROM) {
        if (EOL(current + 1) == 1) {
          return;
        }
        clearEEPROM();

      } else {
        Serial.println(F("ERROR: Invalid command type \"help\""));
      }
      break;
    default:
      if (EOL(current + 1) == 1) {
        return;
      }
      Serial.println(F("ERROR: Invalid command type \"help\""));
      break;
  }
}


/* The below code checks input against the lookup table */
int toToken(char toTokenize[], int len, int TOKENCOUNTER) {
  char first2[2];
  first2[0] = toTokenize[0];
  first2[1] = toTokenize[1];

  for (int i = 0; i < (sizeof(LOOKUP) / sizeof(LOOKUP[0])) / 4; i++)  // Iterates through every 4 element window
  {
    //Checks if first 2 letters and length are the same
    if (first2[0] == pgm_read_byte(&LOOKUP[i * 4]) && first2[1] == pgm_read_byte(&LOOKUP[i * 4 + 1]) && len == (int)pgm_read_byte(&LOOKUP[i * 4 + 2])) {
      TOKEN_BUFFER[TOKENCOUNTER] = (unsigned char)pgm_read_byte(&LOOKUP[i * 4 + 3]);
      return 1;
    }
  }
  return 0;
}


/* Controls the red/green LED (pins 8/9) and the onboard LED */
void controlLeds() {
  unsigned short int curr_time = millis();         // Timing of blinks
  bool color_led = ((LED_CONTROL >> 5) & 2) >> 1;  // Gets color from second bit
  bool power_led = LED_CONTROL >> 7;               // Gets power from first bit
  bool power_d13 = D13_CONTROL >> 7;               // Gets color from first bit
  bool power_rgb = RGB_CONTROL >> 7;               // Gets power from first bit
  unsigned short int timeDiff = curr_time - LAST_TIME;
  if (power_led == 1) {
    if (color_led == 0) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN, LOW);
    } else {
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GREEN, HIGH);
    }
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
  }
  if (power_d13 == 1) {
    digitalWrite(LED_BUILTIN, HIGH);

  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
  if (power_rgb == 1) {
    leds[0].setRGB(COLOR[0], COLOR[1], COLOR[2]);

    FastLED.show();
  } else {
    leds[0] = CRGB::Black;
    FastLED.show();
  }

  if (timeDiff >= BLINK_RATE) {
    // Move to next bits //
    LED_CONTROL = LED_CONTROL >> 2 | LED_CONTROL << 6;  // Shifts bits
    D13_CONTROL = D13_CONTROL >> 1 | D13_CONTROL << 7;  // Shifts bits
    RGB_CONTROL = RGB_CONTROL >> 1 | RGB_CONTROL << 7;  // Shifts bits
    LAST_TIME = curr_time;
  }
}


void outputError() {
  Serial.println(F("Invalid command type \"help\""));
}


/* The following code prints the help menu */
void print_help() {
  Serial.println(F("The following commands are valid (Case matters)"));
  Serial.println(F("led <on, off, blink <NONE, rg>, green, red>"));
  Serial.println(F("d13 <on, off, blink>"));
  Serial.println(F("RGB <on, off, blink>"));
  Serial.println(F("set blink <number 0-65535>"));
  Serial.println(F("set color <RRR GGG BBB"));
  Serial.println(F("status leds"));
  Serial.println(F("print <minimax, weather, eeprom>"));
  Serial.println(F("set time"));
  Serial.println(F("unit"));
  Serial.println(F("clear eeprom"));
  Serial.println(F("version"));
  Serial.println(F("help"));
}


void print_status() {
  Serial.println(F("Current status:"));
  bool color_led_1 = ((LED_CONTROL >> 5) & 2) >> 1;  // Gets color from second bit
  bool power_led_1 = LED_CONTROL >> 7;               // Gets power from first bit
  bool color_led_2 = ((LED_CONTROL >> 4) & 1);       // Gets color from 3rd bit
  bool power_led_2 = ((LED_CONTROL >> 3) & 1);       // gets power from 4th bit

  bool power_d13_1 = D13_CONTROL >> 7;               // Gets power from first bit
  bool power_d13_2 = ((D13_CONTROL >> 5) & 2) >> 1;  // Gets power from second bit
  Serial.print(F("LED: "));
  if (power_led_1 != 0 || power_led_2 != 0) {
    if (color_led_1 == 0 && color_led_2 == 0) {
      Serial.print(F("RED "));
    } else if (color_led_1 == 1 && color_led_2 == 1) {
      Serial.print(F("GREEN "));
    } else {
      Serial.print(F("RED / GREEN "));
    }
    if (power_led_1 == 1 && power_led_2 == 1 && color_led_1 == color_led_2) {
      Serial.println(F("SOLID "));
    } else {
      Serial.println(F("BLINKING"));
    }
  } else {
    Serial.println(F("OFF"));
  }

  Serial.print(F("D13: "));
  if (power_d13_1 == 1 && power_d13_2 == 1) {
    Serial.println(F("SOLID"));
  } else if (power_d13_1 == 0 && power_d13_2 == 0) {
    Serial.println(F("OFF"));
  } else {
    Serial.println(F("BLINKING"));
  }
  Serial.print(F("BLINK RATE: "));
  Serial.println(BLINK_RATE);
}

/* The code below clears the input buffer */
void inputBufferClear() {
  for (int i = 0; i < sizeof(INPUT_BUFFER) / sizeof(INPUT_BUFFER[0]) - 1; i++) {
    INPUT_BUFFER[i] = 0;
  }
}

/* The below code clears the token buffer */
void tokenBufferClear() {
  for (int j = 0; j < MAX_T; j++) {  // Clears the to tokenize array
    TOKEN_BUFFER[j] = 0;
  }
}


/* The below code implements backspace */
void backspace() {
  char back[1];
  sprintf(back, "\033[%dD", (1));  // Foramtted string to move curser back one
  Serial.print(back);
  Serial.print(F(" "));  // Overwrites last input as a space
  Serial.print(back);
}


/* The code below handles extra input */
bool EOL(int index) {
  if (TOKEN_BUFFER[index] != t_EOL) {
    Serial.println(F("Invalid command type \"help\""));
    return 1;
  }
  return 0;
}


void addNum(short unsigned int a, short unsigned int b) {
  int c = (int)a + (int)b;
  Serial.print((int)a);
  Serial.print(F(" + "));
  Serial.print((int)b);
  Serial.print(F(" = "));
  Serial.println((int)c);
}


// Handles Blink input
int check_if_blink_set(int TOKENCOUNTER, int i) {
  if ((int)TOKEN_BUFFER[TOKENCOUNTER - 1] == (int)t_BLINK && (int)TOKEN_BUFFER[TOKENCOUNTER - 2] == (int)t_SET) {
    TOKENCOUNTER = getNumbers(TOKENCOUNTER, i, 1);
  }
  return TOKENCOUNTER;
}


// Handles Add input
int check_if_add(int TOKENCOUNTER, int i) {
  if ((int)TOKEN_BUFFER[TOKENCOUNTER - 1] == (int)t_ADD) {
    TOKENCOUNTER = TOKENCOUNTER = getNumbers(TOKENCOUNTER, i, 2);
  }
  return TOKENCOUNTER;
}


// Handles set color input
int check_if_set_color(int TOKENCOUNTER, int i) {
  if ((int)TOKEN_BUFFER[TOKENCOUNTER - 1] == (int)t_COLOR && (int)TOKEN_BUFFER[TOKENCOUNTER - 2] == (int)t_SET) {
    TOKENCOUNTER = getNumbers(TOKENCOUNTER, i, 3);
  }
  return TOKENCOUNTER;
}


// Prints all tokens in buffer
void printTokens() {
  for (int i = 0; i < MAX_T - 1; i++) {
    Serial.println(TOKEN_BUFFER[i]);
  }
}


// Gets all ints from input buffer
int getNumbers(int TOKENCOUNTER, int i, int howMany) {
  unsigned char* cursor = &INPUT_BUFFER[i + 1];
  int count = 0;
  while (cursor != &INPUT_BUFFER[i + 1] + strlen(&INPUT_BUFFER[i + 1])) {
    for (int i = 0; i < strlen(cursor) - 2; i++) {
      if (isdigit(cursor[i]) == 0 && cursor[i] != 32) {
        Serial.println(F("ERROR: NON VALID INPUT: TYPE <help> "));
        return 0;
      }
    }
    long int num_input = (long int)strtol(cursor, &cursor, 10);
    if (num_input <= 65535 && num_input >= 0) {
      TOKEN_BUFFER[TOKENCOUNTER] = t_WORD;
      TOKENCOUNTER++;

      short unsigned int num = (short unsigned int)num_input;

      unsigned char lsb = (unsigned)num & 0xff;  // mask the lower 8 bits
      unsigned char msb = (unsigned)num >> 8;    // shift the higher 8 bits
      TOKEN_BUFFER[TOKENCOUNTER] = lsb;
      TOKENCOUNTER++;
      TOKEN_BUFFER[TOKENCOUNTER] = msb;
      TOKENCOUNTER++;
    } else {
      Serial.println(F("Error Overflow: Numbers over 65535 or less than zero are set to zero."));
    }
    count++;
  }
  if (count != howMany) {
    Serial.println(F("ERROR: WRONG NUMBER OR INPUTS: TYPE <help> "));
    Serial.println(count);

    return 0;
  }
  return TOKENCOUNTER;
}


// Sets LED Color
void setColor() {
  unsigned short int red = (int)((TOKEN_BUFFER[4] << 8) | TOKEN_BUFFER[3]);    // gets origional number back
  unsigned short int green = (int)((TOKEN_BUFFER[7] << 8) | TOKEN_BUFFER[6]);  // gets origional number back
  unsigned short int blue = (int)((TOKEN_BUFFER[10] << 8) | TOKEN_BUFFER[9]);  // gets origional number back
  Serial.print(F("Color set to: "));
  Serial.print(red);
  Serial.print(F(" "));
  Serial.println(green);
  Serial.print(F(" "));
  Serial.println(blue);



  if (red <= 255 && green <= 255 && blue <= 255 && red >= 0 && green >= 0 && blue >= 0) {
    COLOR[0] = (unsigned char)red;
    COLOR[1] = (unsigned char)green;
    COLOR[2] = (unsigned char)blue;
  } else {
    Serial.println(F("ERROR: OUT OF BOUNDS (0-255)"));
  }
}


void printTime() {

  DateTime now = rtc.now();
  Serial.print("Date & Time: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.println(now.minute(), DEC);
}


/* The code below prints the current weather */
void printWeather(char temperture, char humidity) {

  Serial.print(F("Temperture: "));
  if (UNIT == 1) {
    Serial.print(convert_c_f(temperture));
    Serial.print(F(" F"));
  } else {
    Serial.print((int)temperture);
    Serial.print(F(" C"));
  }
  Serial.print(F(" Humidity: "));
  Serial.print((int)humidity);
  Serial.println(F(" %"));
}

/* The code below reads the temp and humidity from DHT11 sensor */
int setTempHumid() {
  if (millis() - LAST_TIME_RECORD >= 5000) {    // Reads every 5 seconds
    dht22.read(&TEMPERATURE, &HUMIDITY, NULL);  // Reads the temp and humidity from DHT11 sensor
    LAST_TIME_RECORD = millis();
    RECORD_COUNTER++;
  }
}


void writeIntIntoEEPROM(unsigned int address, int number) {
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
}


int readIntFromEEPROM(unsigned int address) {
  return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}


/* The code below clears the eeprom and resets the index counter */
void clearEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
  writeIntIntoEEPROM(0, 2);
  Serial.println(F("EEPROM RESET"));
}


/* The code below saves temp, humid and datetime to eeprom */
void storeData() {
  setTempHumid();
  short int curr_index = readIntFromEEPROM(0);



  if (RECORD_COUNTER == 180 && (curr_index + 7) < EEPROM.length()) {  // Stores data every 180 * 5000 ms = 15 min
    DateTime now = rtc.now();


    EEPROM.write(curr_index, now.day());
    EEPROM.write(curr_index + 1, now.month());
    EEPROM.write(curr_index + 2, now.year() % 100);

    EEPROM.write(curr_index + 3, now.hour());
    EEPROM.write(curr_index + 4, now.minute());

    EEPROM.write(curr_index + 5, HUMIDITY);
    EEPROM.write(curr_index + 6, TEMPERATURE);


    writeIntIntoEEPROM(0, curr_index + 7);


    RECORD_COUNTER = 0;
  }
}


/* THe code below prints all contents of the eeprom */
void printEEPROM() {
  short int curr_index = readIntFromEEPROM(0);
  DateTime ToPrint;

  int humidity;
  int temperature;

  for (int i = 2; i < curr_index; i += 7) {

    humidity = EEPROM.read(i + 5);
    temperature = EEPROM.read(i + 6);

    Serial.print(EEPROM.read(i));

    Serial.print(F("/"));
    Serial.print(EEPROM.read(i + 1));

    Serial.print(F("/"));
    Serial.print(EEPROM.read(i + 2));

    Serial.print(F(" "));
    Serial.print(EEPROM.read(i + 3));
    Serial.print(F(":"));
    Serial.print(EEPROM.read(i + 4));
    Serial.print(F("    "));

    printWeather(temperature, humidity);
  }
}


/* THe code below finds the min/max temp and prints it */
void printMinMax() {
  short int curr_index = readIntFromEEPROM(0);
  DateTime Max;
  DateTime Min;
  unsigned char humidity;
  char temperature;

  unsigned char humidity_max;
  char temperature_max = EEPROM.read(6);
  unsigned char day_max;
  unsigned char month_max;
  unsigned char year_max;
  unsigned char hr_max;
  unsigned char minute_max;

  unsigned char humidity_min;
  char temperature_min = EEPROM.read(6);
  unsigned char day_min;
  unsigned char month_min;
  unsigned char year_min;
  unsigned char hr_min;
  unsigned char minute_min;

  for (int i = 2; i < curr_index; i += 7) {
    humidity = EEPROM.read(i + 5);
    temperature = EEPROM.read(i + 6);

    // if temp is lower than the prior min, save datastamp
    if (temperature <= temperature_min) {
      temperature_min = temperature;
      humidity_min = humidity;
      day_min = EEPROM.read(i);
      month_min = EEPROM.read(i + 1);
      year_min = EEPROM.read(i + 2);
      hr_min = EEPROM.read(i + 3);
      minute_min = EEPROM.read(i + 4);
    }
    // if temp is higher than the prior min, save datastamp
    if (temperature >= temperature_max) {
      temperature_max = temperature;
      humidity_max = humidity;
      day_max = EEPROM.read(i);
      month_max = EEPROM.read(i + 1);
      year_max = EEPROM.read(i + 2);
      hr_max = EEPROM.read(i + 3);
      minute_max = EEPROM.read(i + 4);
    }
  }
  // prints stamp of max

  Serial.print(F("MAX:"));
  Serial.print(day_max);
  Serial.print(F("/"));
  Serial.print(month_max);
  Serial.print(F("/"));
  Serial.print(year_max);
  Serial.print(F(" "));
  Serial.print(hr_max);
  Serial.print(F(":"));
  Serial.print(minute_max);
  Serial.print(F("    "));
  printWeather(temperature_max, humidity_max);
  // prints stamp of min
  Serial.print(F("MIN:"));
  Serial.print(day_min);
  Serial.print(F("/"));
  Serial.print(month_min);
  Serial.print(F("/"));
  Serial.print(year_min);
  Serial.print(F(" "));
  Serial.print(hr_min);
  Serial.print(F(":"));
  Serial.print(minute_min);
  Serial.print(F("    "));
  printWeather(temperature_min, humidity_min);
}


/* Sets date and time to a user inputted value. Adapted from Simple ds3231 lib*/
void promptForTimeAndDate(Stream& Serial) {
  char buffer[3] = { 0 };
  unsigned char day;
  unsigned char month;
  unsigned char year;
  unsigned char hour;
  unsigned char minute;


  Serial.println(F("Clock is set when all data is entered and you send 'Y' to confirm."));
  do {


    // Sets day
    memset(buffer, 0, sizeof(buffer));
    Serial.println();
    Serial.print(F("Enter Day of Month (2 digits, 01-31): "));
    while (!Serial.available())
      ;  // Wait until bytes
    Serial.readBytes(buffer, 2);
    while (Serial.available()) Serial.read();
    day = atoi(buffer[0] == '0' ? buffer + 1 : buffer);


    // Sets month
    memset(buffer, 0, sizeof(buffer));
    Serial.println();
    while (!Serial.available())
      ;  // Wait until bytes

    Serial.readBytes(buffer, 2);
    while (Serial.available()) Serial.read();
    month = atoi(buffer[0] == '0' ? buffer + 1 : buffer);
    // sets year
    memset(buffer, 0, sizeof(buffer));
    Serial.println();
    Serial.print(F("Enter Year (2 digits, 00-99): "));
    while (!Serial.available())
      ;  // Wait until bytes

    Serial.readBytes(buffer, 2);
    while (Serial.available()) Serial.read();
    year = atoi(buffer[0] == '0' ? buffer + 1 : buffer);
    // sets hour
    memset(buffer, 0, sizeof(buffer));
    Serial.println();
    Serial.print(F("Enter Hour (2 digits, 24 hour clock, 00-23): "));
    while (!Serial.available())
      ;  // Wait until bytes

    Serial.readBytes(buffer, 2);
    while (Serial.available()) Serial.read();
    hour = atoi(buffer[0] == '0' ? buffer + 1 : buffer);
    // sets minute
    memset(buffer, 0, sizeof(buffer));
    Serial.println();
    Serial.print(F("Enter Minute (2 digits, 00-59): "));
    while (!Serial.available())
      ;  // Wait until bytes

    Serial.readBytes(buffer, 2);
    while (Serial.available()) Serial.read();
    minute = atoi(buffer[0] == '0' ? buffer + 1 : buffer);


    {
      if (year >= 0 && year <= 99
          && month >= 1 && month <= 12
          && day >= 0 && day <= 31
          && hour >= 0 && hour <= 23
          && minute >= 0 && minute <= 59) {

        rtc.adjust(DateTime(year, month, day, hour, minute, 0));
        break;
      } else {
        Serial.println(F("Invalid Time"));
      }
    }
  } while (1);

  Serial.println(F(" "));
  Serial.println(F("Time set to: "));
  printTime();
}

void temp2lcd() {
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("Temp: "));
  if (UNIT == 1) {
    lcd.print(convert_c_f((float)TEMPERATURE));
    lcd.setCursor(9, 0);
    lcd.print(F(" F"));
  } else {
    lcd.print((int)TEMPERATURE);
    lcd.setCursor(9, 0);
    lcd.print(F(" C"));
  }


  lcd.setCursor(0, 1);
  lcd.print(F("Humidity: "));
  lcd.print((int)HUMIDITY);
  lcd.print(F("%"));
}



int convert_c_f(float c) {
  float d = (float)c;
  return (1.8 * d + 32);
}

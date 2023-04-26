#include <FastLED.h>
#include <SimpleDHT.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Ethernet.h>

/* ////////////////////////////////////////////
Wiring:
LED: Green 8, Red 9
RBG: Pin 7
RTC: SQW 1, SCL A5, SDA A4
DHT: DAT 2
OLED: SDA 15 , SCL 14
Ethernet: MISO 12, MOSI 11, SCS 10, SCLK 12 
*/
/////////////////////////////////////////////

#define PIN_PAD A0
// EOL Token
#define t_EOL 200
// Tokens of addtional words
#define t_SET 110
#define t_STATUS 111
#define t_VERSION 112
#define t_HELP 113
#define t_STATS 114
#define t_WORD 115
#define t_WEATHER 119

// Tokems for clock
#define t_TIME 121
#define t_PRINT 122
#define t_UNIT 123
// EEPROM Token
#define t_EEPROM 124
#define t_CLEAR 125
#define t_MINMAX 126
#define t_HISTORY 127
// Menu
#define t_SHOW 130
#define t_SETTINGS 131
// Network
#define t_IP 140
#define t_SUBNET 141
#define t_GATEWAY 142
// Alarm
#define t_ALARM 150
// PINPAD
#define RIGHT 160
#define LEFT 161
#define UP 162
#define DOWN 163
#define CONFIRM 164
#define t_MENU 165



// Error
#define t_ERROR 255

// defines clock
RTC_DS3231 rtc;

// Configures DHT on pin 2
const int pinDHT22 PROGMEM = 2;
SimpleDHT22 dht22(pinDHT22);

// How many RGB leds
CRGB leds[4];
// Init LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// Temperture and Humidity
char TEMPERATURE;
unsigned char HUMIDITY;
long unsigned int LAST_TIME_RECORD = 0;
bool UNIT = 0;
unsigned char RECORD_COUNTER = 0;

// Max array size
static const unsigned char MAX_I PROGMEM = 32;
static const unsigned char MAX_T PROGMEM = 20;

// Makes Token Buffer
unsigned char TOKEN_BUFFER[MAX_T] = { 0 };
char INPUT_BUFFER[MAX_I] = { 0 };

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
// Arduino
const IPAddress ip(192, 168, 1, 177);
const unsigned int localPort = 8888;  // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;
// Keeps track of last sent alarm
unsigned char LAST_SENT_ALARM = 0;

// For pinpad
long unsigned int LAST_PRESS = 0;
// menu buffer
unsigned char MENU_BUFFER[3] = { 0 };
// Creates lookup array
const char LOOKUP[] PROGMEM = {
  13, 0, 1, t_EOL,         // EOL
  's', 'e', 3, t_SET,      // t_SET
  's', 't', 6, t_STATUS,   // t_STATUS
  'v', 'e', 7, t_VERSION,  // t_VERSION
  'h', 'e', 4, t_HELP,     // t_HELP
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
  pinMode(PIN_PAD, INPUT);
  FastLED.addLeds<NEOPIXEL, 7>(leds, 4);

  // SETUP RTC MODULE
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1)
      ;
  }

  // start the Ethernet
  Ethernet.begin(mac, ip);

  // start UDP
  Udp.begin(localPort);

  // automatically sets the RTC to the date & time on PC this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  lcd.init();  // initialize the lcd
  lcd.backlight();

  pinMode(PIN_PAD, INPUT);  // inits pin a0 as input
}


void loop() {
  // put your main code here, to run repeatedly:
  input();
  // printInput();
  processInput();  // Takes in user input and inputs tokens into token array. Returns 0 if a valid token was entered. Else Returns 1.

  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  tokenTree();  // Runs all tokens
  Udp.endPacket();

  // printTokens();
  tokenBufferClear();  // clears token buffer  }
  inputBufferClear();  // clears input buffer
}


/* The below code takes user input and puts it into a buffer array */
void input() {

  Serial.println(F("Enter Command"));  // Prompts user for command
  bool noEnter = 1;                    // used to break loop when enter key is pressed
  int index = 0;                       // keeps track of index in input buffer
  char curInput = 0;
  int packetSize = Udp.parsePacket();

  while (noEnter == 1 && 0 == packetSize) {
    while (Serial.available() == 0 && 0 == packetSize)  // Wait for data available
    {
      packetSize = Udp.parsePacket();
      controlLeds();  // Controls power to LEDs
      storeData();
      menu();
      readPad();
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
    if (packetSize) {

      Serial.println(F("Packet Received"));
      inputBufferClear();

      if (packetSize > MAX_I - 1) {
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.print("Too Large");
        Udp.endPacket();
      } else {
        Udp.read(INPUT_BUFFER, 25);
        Serial.println(F(" "));
      }
    }
  }
}


/* The code below asks for user input and parces it into a array of tokens (using toToken function) */
int processInput() {

  // Create a token buffer
  int TOKENCOUNTER = 0;  // Keeps track of how many tokens were generated
  int temp = 0;
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
    } else {  // Else it sends the toTokenize array to check against lookup table

      if (toTokenize[0] != 0) {  // Checks to ensure someting exists to check
        // adds any valid imputs into token buffer
        TOKENCOUNTER = toToken(toTokenize, strlen(toTokenize), TOKENCOUNTER);
        TOKENCOUNTER = check_if_set_time(TOKENCOUNTER, i);
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

        case t_TIME:
          setTime();
          current += (3 * 5 + 1);  // 3 * num input numbers + 1
          if (EOL(current) == 1) {
            return;
          }

          break;

        default:
          outputError();
          break;
      };
      break;

    case t_HELP:
      if (EOL(current + 1) == 1) {
        return;
      }
      print_help();
      break;

    case t_STATUS:

      break;


    case t_VERSION:
      if (EOL(current + 1) == 1) {
        return;
      }
      printVersion();
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
          outputError();
          break;
      };
      break;
    case t_UNIT:
      setUnit();

      break;
    case t_CLEAR:
      current++;
      if (TOKEN_BUFFER[current] == t_EEPROM) {
        if (EOL(current + 1) == 1) {
          return;
        }
        clearEEPROM();

      } else {
        outputError();
      }
      break;
    default:
      if (EOL(current + 1) == 1) {
        return;
      }
      outputError();
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
      return TOKENCOUNTER + 1;
    }
  }
  return TOKENCOUNTER;
}


/* Controls the LEDS*/
void controlLeds() {
  leds[0].setRGB(0, 0, 255);  // controls power led
  alarm();                    // Contains logic for alarm led
  checkConnection();          // controls the connection led
  FastLED.show();
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

/* The below code clears the menu buffer */
void menuBufferClear() {
  for (int j = 0; j < 3; j++) {  // Clears the to menu array
    MENU_BUFFER[j] = 0;
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


/* The below code handles set time input */
int check_if_set_time(int TOKENCOUNTER, int i) {
  if ((int)TOKEN_BUFFER[TOKENCOUNTER - 1] == (int)t_TIME && (int)TOKEN_BUFFER[TOKENCOUNTER - 2] == (int)t_SET) {
    TOKENCOUNTER = getNumbers(TOKENCOUNTER, i, 5);
  }
  return TOKENCOUNTER;
}


/* The below code prints the input buffer */
void printInput() {
  for (int i = 0; i < MAX_I - 1; i++) {
    Serial.println(INPUT_BUFFER[i]);
  }
}


/* The below code prints the token buffer */
void printTokens() {
  for (int i = 0; i < MAX_T - 1; i++) {
    Serial.println(TOKEN_BUFFER[i]);
  }
}

/* The code below reads the temp and humidity from DHT11 sensor */
int setTempHumid() {
  if (millis() - LAST_TIME_RECORD >= 5000) {    // Reads every 5 seconds
    dht22.read(&TEMPERATURE, &HUMIDITY, NULL);  // Reads the temp and humidity from DHT11 sensor
    LAST_TIME_RECORD = millis();
    RECORD_COUNTER++;
    lcd.clear();
  }
}


void writeIntIntoEEPROM(unsigned int address, int number) {
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
}


int readIntFromEEPROM(unsigned int address) {
  return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
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


/* The code below gets a number of ints i from the input buffer and stores them in the token buffer as 2 bytes */
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
      Udp.println(F("Error Overflow: Numbers over 65535 or less than zero are set to zero."));
    }
    count++;
  }
  if (count != howMany) {
    Serial.println(F("ERROR: WRONG NUMBER OR INPUTS: TYPE <help> "));
    Serial.println(count);
    Udp.println(F("ERROR: WRONG NUMBER OR INPUTS: TYPE <help> "));
    Udp.println(count);


    return 0;
  }
  return TOKENCOUNTER;
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

  Udp.print("Date & Time: ");
  Udp.print(now.year(), DEC);
  Udp.print('/');
  Udp.print(now.month(), DEC);
  Udp.print('/');
  Udp.print(now.day(), DEC);
  Udp.print(" (");
  Udp.print(") ");
  Udp.print(now.hour(), DEC);
  Udp.print(':');
  Udp.println(now.minute(), DEC);
}


/* The code below prints the current weather */
void printWeather(char temperture, char humidity) {

  Serial.print(F("Temperture: "));
  if (UNIT == 1) {
    Serial.print(convert_c_f(temperture));
    Serial.print(F(" F"));

    Udp.print(convert_c_f(temperture));
    Udp.print(F(" F"));

  } else {
    Serial.print((int)temperture);
    Serial.print(F(" C"));

    Udp.print((int)temperture);
    Udp.print(F(" C"));
  }
  Serial.print(F(" Humidity: "));
  Serial.print((int)humidity);
  Serial.println(F(" %"));

  Udp.print(F(" Humidity: "));
  Udp.print((int)humidity);
  Udp.println(F(" %"));
}


/* Sets date and time to a user inputted value. Adapted from Simple ds3231 lib*/  ///////INCLUDE PACKETS
void setTime() {
  unsigned short int year = (int)((TOKEN_BUFFER[4] << 8) | TOKEN_BUFFER[3]);      // gets origional number back
  unsigned short int month = (int)((TOKEN_BUFFER[7] << 8) | TOKEN_BUFFER[6]);     // gets origional number back
  unsigned short int day = (int)((TOKEN_BUFFER[10] << 8) | TOKEN_BUFFER[9]);      // gets origional number back
  unsigned short int hour = (int)((TOKEN_BUFFER[10] << 8) | TOKEN_BUFFER[12]);    // gets origional number back
  unsigned short int minute = (int)((TOKEN_BUFFER[10] << 8) | TOKEN_BUFFER[15]);  // gets origional number back

  if (year >= 0 && year <= 99
      && month >= 1 && month <= 12
      && day >= 0 && day <= 31
      && hour >= 0 && hour <= 23
      && minute >= 0 && minute <= 59) {

    rtc.adjust(DateTime(year, month, day, hour, minute, 0));
  } else {
    Serial.println(F("Invalid Time"));
    Udp.println(F("Invalid Time"));
  }


  Serial.println(F(" "));
  Serial.println(F("Time set to: "));
  printTime();
}


/* The code below prints all contents of the eeprom */
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


    Udp.print(EEPROM.read(i));

    Udp.print(F("/"));
    Udp.print(EEPROM.read(i + 1));

    Udp.print(F("/"));
    Udp.print(EEPROM.read(i + 2));

    Udp.print(F(" "));
    Udp.print(EEPROM.read(i + 3));
    Udp.print(F(":"));
    Udp.print(EEPROM.read(i + 4));
    Udp.print(F("    "));

    printWeather(temperature, humidity);
  }

  Serial.println(readIntFromEEPROM(0));
  Serial.println(F("EEPROM PRINTED"));

  Udp.println(readIntFromEEPROM(0));
  Udp.println(F("EEPROM PRINTED"));
}


/* The code below finds the min/max temp and prints it */
void printMinMax() {
  // inits what to search
  short int curr_index = readIntFromEEPROM(0);
  unsigned char humidity;
  char temperature;

  // inits max and min stamps
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

  // Data is stored as day, month, year, hr, min, humid, temp
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



  Udp.print(F("MAX:"));
  Udp.print(day_max);
  Udp.print(F("/"));
  Udp.print(month_max);
  Udp.print(F("/"));
  Udp.print(year_max);
  Udp.print(F(" "));
  Udp.print(hr_max);
  Udp.print(F(":"));
  Udp.print(minute_max);
  Udp.print(F("    "));
  printWeather(temperature_max, humidity_max);
  // prints stamp of min
  Udp.print(F("MIN:"));
  Udp.print(day_min);
  Udp.print(F("/"));
  Udp.print(month_min);
  Udp.print(F("/"));
  Udp.print(year_min);
  Udp.print(F(" "));
  Udp.print(hr_min);
  Udp.print(F(":"));
  Udp.print(minute_min);
  Udp.print(F("    "));
  printWeather(temperature_min, humidity_min);
}


/* The code below prints the temp to the lcd */
void home() {
  lcd.setCursor(0, 0);
  lcd.print(F("Temp: "));
  if (UNIT == 1) {
    lcd.print(convert_c_f((float)TEMPERATURE));
    lcd.print(F(" F"));
  } else {
    lcd.print((int)TEMPERATURE);
    lcd.print(F(" C"));
  }
  lcd.setCursor(0, 1);
  lcd.print(F("Humidity: "));
  lcd.print((int)HUMIDITY);
  lcd.print(F("%"));
  if (readPad() == CONFIRM) {
    MENU_BUFFER[0] = t_MENU;
  }
}


/* The code below converts the temperature to F */
int convert_c_f(float c) {
  float d = (float)c;
  return (1.8 * d + 32);
}


/* The code below switches between C and F */
void setUnit() {
  UNIT = (UNIT + 1) % 2;
  if (UNIT == 0) {
    Serial.println(F("TEMPERTURE UNIT: C"));
    Udp.println(F("TEMPERTURE UNIT: C"));

  } else {
    Serial.println(F("TEMPERTURE UNIT: F"));
    Udp.println(F("TEMPERTURE UNIT: F"));
  }
}


/* The code below prints the version */
void printVersion() {
  Serial.println(F("Version: 7 +/- 2"));
  Udp.println(F("Version: 7 +/- 2"));
}


/* The code below clears the eeprom and resets the index counter */
void clearEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.update(i, 0);
  }
  writeIntIntoEEPROM(0, 2);
  Serial.println(F("EEPROM RESET"));
  Udp.println(F("EEPROM RESET"));
}


/* The code below outputs an error on invalid input */
void outputError() {
  Serial.println(F("Invalid command type \"help\""));
  Udp.println(F("Invalid command type \"help\""));
}


/* The following code prints the help menu */
void print_help() {
  Serial.println(F("The following commands are valid (Case matters)"));
  Serial.println(F("status"));
  Serial.println(F("print <minimax, weather, eeprom>"));
  Serial.println(F("set time YY MM DD HH MM"));
  Serial.println(F("unit (toggles between C and F)"));
  Serial.println(F("clear eeprom"));
  Serial.println(F("version"));
  Serial.println(F("help"));

  Udp.println(F("The following commands are valid (Case matters)"));
  Udp.println(F("status"));
  Udp.println(F("print <minimax, weather, eeprom>"));
  Udp.println(F("set time YY MM DD HH MM"));
  Udp.println(F("unit"));
  Udp.println(F("clear eeprom"));
  Udp.println(F("version"));
  Udp.println(F("help"));
}


/* The following code prints last received and last sent packets */
void print_status() {
}

/* The code below handles extra input */
bool EOL(int index) {
  if (TOKEN_BUFFER[index] != t_EOL) {
    Serial.println(F("Invalid command type \"help\""));
    Udp.println(F("Invalid command type \"help\""));

    return 1;
  }
  return 0;
}


/* The code below checks if connectivity exists */
void checkConnection() {
  if (Ethernet.linkStatus() == LinkON && Udp.remotePort() != 0) {
    leds[1].setRGB(0, 255, 0);
  } else {
    leds[1].setRGB(255, 0, 0);
  }
}


/* The code below checks the temperature against  */
void alarm() {

  int temperature = convert_c_f(TEMPERATURE);
  if (temperature <= 60 && LAST_SENT_ALARM != 1) {
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    leds[2].setRGB(230, 230, 250);  // Temp 60 and below
    Udp.print(F("Temperature Major Under: "));
    Udp.print(temperature);
    Udp.print(F("F"));
    Udp.endPacket();
    if (Udp.remotePort() != 0) {
      LAST_SENT_ALARM = 1;
    }


  } else if (temperature > 60 && temperature <= 70 && LAST_SENT_ALARM != 2) {
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());

    leds[2].setRGB(0, 0, 255);  // Temp 61-70
    Udp.print(F("Temperature Minor Under: "));
    Udp.print(temperature);
    Udp.print(F("F"));
    Udp.endPacket();
    if (Udp.remotePort() != 0) {  // This if is used to ensure that the initial condition gets sent
      LAST_SENT_ALARM = 2;
    }

  }

  else if (temperature > 70 && temperature <= 80 && LAST_SENT_ALARM != 3) {
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());

    leds[2].setRGB(0, 255, 0);  // Temp 71-80
    Udp.print(F("Temperature Comfortable: "));
    Udp.print(temperature);
    Udp.print(F("F"));
    Udp.endPacket();
    if (Udp.remotePort() != 0) {
      LAST_SENT_ALARM = 3;
    }

  }

  else if (temperature > 80 && temperature <= 90 && LAST_SENT_ALARM != 4) {
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());

    leds[2].setRGB(255, 165, 0);  // Temp 81-90
    Udp.print(F("Temperature Minor Over: "));
    Udp.print(temperature);
    Udp.print(F("F"));
    Udp.endPacket();
    if (Udp.remotePort() != 0) {
      LAST_SENT_ALARM = 4;
    }

  }

  else if (temperature > 90 && LAST_SENT_ALARM != 5) {
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());


    leds[2].setRGB(255, 0, 0);  // Temp 90
    Udp.print(F("Temperature Major Over: "));
    Udp.print(temperature);
    Udp.print(F("F"));
    Udp.endPacket();
    if (Udp.remotePort() != 0) {
      LAST_SENT_ALARM = 5;
    }
  }
}


unsigned char readPad() {
  if (millis() - LAST_PRESS > 200) {  // ms of debounce time
    short unsigned int a = analogRead(PIN_PAD);
    unsigned char return_value = 0;
    if (a < 1000) {
      if (a < 200) {
        return_value = LEFT;
      } else if (a > 100 && a < 200) {
        return_value = UP;
      } else if (a > 300 && a < 400) {
        return_value = DOWN;
      } else if (a > 450 && a < 550) {
        return_value = RIGHT;
      } else if (a > 700 && a < 800) {
        return_value = CONFIRM;
      } else {
        return;
      }
      Serial.println(return_value);
      Serial.println(a);
      lcd.clear();

      LAST_PRESS = millis();
    }
    if (millis() - LAST_PRESS > 30000) {  // clear menu after 30 sec of no input and return to home
      menuBufferClear();
    }
    return return_value;
  }
}


void mainMenu(){
  
}


void menu() {
  switch (MENU_BUFFER[0]) {
    case t_SHOW:
      switch (MENU_BUFFER[1]) {
        case t_TIME:
          break;
        case t_HISTORY:
          break;
        case t_MINMAX:
          break;
        case t_STATS:
          break;
        default:
          break;
      };
      break;
    case t_SETTINGS:
      switch (MENU_BUFFER[1]) {
        case t_TIME:
          break;
        case t_IP:
          break;
        case t_SUBNET:
          break;
        case t_GATEWAY:
          break;
        case t_UNIT:
          break;
        case t_ALARM:
          break;
        case t_VERSION:
          break;
        case t_CLEAR:
          break;
        default:
          break;
      };
      break;
    case t_MENU:
      mainMenu();
      break;
    default:
      home();
      break;
  };
}

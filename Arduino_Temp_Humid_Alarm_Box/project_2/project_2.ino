#include <Arduino.h>
#include <stdio.h>
#include <ctype.h>
#include <FastLED.h>

// Initializes the names of the pins after their function
#define ELD_RGB 7
#define LED_GREEN 8
#define LED_RED 9

// EOL Token
#define t_EOL 120
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
#define t_RG 117
// ADD token
#define t_ADD 20

// What LED
#define t_RED 51
#define t_GREEN 52
#define t_D13 53
#define t_RGB 54

// RGB LED
CRGB leds[1];

//NEED VARIABLE TO CONTROL LED //
unsigned char LED_CONTROL = 0;
//NEED VARIABLE TO CONTROL D13 //
unsigned char D13_CONTROL = 0;

// Sets blink rate
short unsigned int BLINK_RATE = 500;

// Max array size
static const int MAX = 32;
// Makes Token Buffer
unsigned char TOKEN_BUFFER[MAX] = { 0 };
char INPUT_BUFFER[MAX] = { '\0' };
// RGB color
unsigned char* COLOR[3];
// Creates lookup array
static const char LOOKUP[] = {
  '0', 0, 1, t_EOL,        // EOL
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
  'r', 'g', 2, t_RG,       // t_RG
  'd', '1', 3, t_D13,      // t_D13
  'l', 'e', 4, t_LEDS,     // t_D13
  'a', 'd', 3, t_ADD       // t_ADD
};


void setup() {
  Serial.begin(9600);  // Starts serial
  // Declares pins as output pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
}


void loop() {
  // put your main code here, to run repeatedly:
  input();
  processInput();  // Takes in user input and inputs tokens into token array.
  tokenTree();     // Runs all tokens
  printTokens();
  tokenBufferClear();  // clears token buffer
  inputBufferClear();  // clears input buffer
}


/* The below code takes user input and puts it into a buffer array */
void input() {
  Serial.println("Enter Command");  // Prompts user for command
  bool noEnter = 1;                 // used to break loop when enter key is pressed
  int index = 0;                    // keeps track of index in input buffer
  char curInput = 0;

  while (noEnter == 1) {

    while (Serial.available() == 0)  // Wait for data available
    {
      controlLeds();  // Controls power to LEDs
    }
    curInput = Serial.read();
    if ((char)curInput == 13) {  // checks if enter key
      INPUT_BUFFER[index + 1] = '0';
      noEnter = 0;
      Serial.println(' ');
    } else if ((char)curInput == 127 && index > 0) {  // handles backspaces
      index--;
      INPUT_BUFFER[index] = '/0';
      backspace();
    } else if (index == MAX) {  // Checks for too large input

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

  char toTokenize[MAX] = { 0 };  // Array of stuff to tokenize. Resets after a space or EOL

  int counter = 0;               // Keeps track of location in toTokenize array
  for (int i = 0; i < MAX; i++)  // Iterates through entire input array
  {
    if (isalnum(INPUT_BUFFER[i])) {  // If input is alph / num adds it to the toTokenize array
      toTokenize[counter] = INPUT_BUFFER[i];
      counter++;
      if (i == MAX - 1) {  // Handles last input
        char lastInput[2] = { toTokenize[0], toTokenize[1] };
        toToken(lastInput, strlen(toTokenize), TOKENCOUNTER);
      }
    } else {                     // Else it sends the toTokenize array to check against lookup table
      if (toTokenize[0] != 0) {  // Checks to ensure someting exists to check
        // adds any valid imputs into token buffer
        TOKENCOUNTER += toToken(toTokenize, strlen(toTokenize), TOKENCOUNTER);
        TOKENCOUNTER = check_if_blink_set(TOKENCOUNTER, i);  // Checks if there is a word token
        TOKENCOUNTER = check_if_add(TOKENCOUNTER, i);
      }
      for (int j = 0; j < MAX; j++) {  // Clears the to tokenize array
        toTokenize[j] = { 0 };
      }
      counter = 0;
    }
  }
  char first2[2] = { toTokenize[0], toTokenize[1] };
  for (int j = 0; j < MAX; j++) {  // Clears the to tokenize array
    toTokenize[j] = { 0 };
  }
  counter = 0;
  return 0;
}


/* The code below takes the token buffer and runs the corrosponding functions */
void tokenTree() {
  int current = 0;
  switch (TOKEN_BUFFER[current]) {
    case t_SET:  // Deals with all possiblities from a set token
      if (TOKEN_BUFFER[current + 1] == t_BLINK) {
        current += 3;
        if (extraInput(current + 1) == 1) {
          return;
        }
        unsigned short int rec = (int)(((unsigned)TOKEN_BUFFER[current + 1] << 8) | TOKEN_BUFFER[current]);  // gets origional number back
        BLINK_RATE = rec;
        Serial.print(F("Blink rate set to: "));
        Serial.println(rec);

      } else {
        Serial.println(F("Invalid command type \"help\""));
      }
      break;

    case t_LED:  // Deals with all possiblities from a LED token
      current++;
      switch (TOKEN_BUFFER[current]) {
        case t_BLINK:  // FIX


          if (TOKEN_BUFFER[current + 1] != t_EOL) {
            current++;

            if (TOKEN_BUFFER[current + 1] == t_EOL) {

              if (TOKEN_BUFFER[current] == t_RG) {
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
          if (extraInput(current + 1) == 1) {
            return;
          }
          LED_CONTROL = LED_CONTROL & 170;  // sets all color bits to 0 (170 = 10101010)
          Serial.println(F("LED red"));
          break;

        case t_GREEN:
          if (extraInput(current + 1) == 1) {
            return;
          }
          LED_CONTROL = LED_CONTROL | 85;  // sets all color bits to 0 (85 = 01010101)
          Serial.println(F("LED green"));
          break;
        case t_ON:
          if (extraInput(current + 1) == 1) {
            return;
          }
          LED_CONTROL = LED_CONTROL | 170;  // sets all power bits to 1 (170 = 10101010)
          Serial.println(F("LED on"));
          break;

        case t_OFF:
          if (extraInput(current + 1) == 1) {
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
          if (extraInput(current + 1) == 1) {
            return;
          }
          D13_CONTROL = 170;
          Serial.println(F("D13 blink toggled"));

          break;
        case t_ON:
          if (extraInput(current + 1) == 1) {
            return;
          }
          D13_CONTROL = 255;
          Serial.println(F("D13 on"));

          break;

        case t_OFF:
          if (extraInput(current + 1) == 1) {
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

    case t_HELP:
      if (extraInput(current + 1) == 1) {
        return;
      }
      print_help();
      break;

    case t_STATUS:
      current++;
      if (TOKEN_BUFFER[current] == t_LEDS) {
        if (extraInput(current + 1) == 1) {
          return;
        }
        print_status();
      } else {
        Serial.println(F("Invalid command type \"help\""));
      }
      break;


    case t_VERSION:
      if (extraInput(current + 1) == 1) {
        return;
      }
      Serial.println(F("Version: 2"));
      break;


    case t_ADD:
      current += 2;
      unsigned short int rec = (int)(((unsigned)TOKEN_BUFFER[current + 1] << 8) | TOKEN_BUFFER[current]);  // gets origional number back
      current += 3;
      unsigned short int rec2 = (int)(((unsigned)TOKEN_BUFFER[current + 1] << 8) | TOKEN_BUFFER[current]);  // gets origional number back
      current++;


      addNum(rec, rec2);
      break;





    default:
      if (extraInput(current + 1) == 1) {
        return;
      }
      Serial.println(F("Invalid command type \"help\""));
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
    if (first2[0] == LOOKUP[i * 4] && first2[1] == LOOKUP[i * 4 + 1] && len == (int)LOOKUP[i * 4 + 2]) {
      TOKEN_BUFFER[TOKENCOUNTER] = (unsigned char)LOOKUP[i * 4 + 3];
      return 1;
    }
  }
  return 0;
}


/* Controls the red/green LED (pins 8/9) and the onboard LED */
void controlLeds() {
  int timing = millis() % (BLINK_RATE);            // Timing of blinks
  bool color_led = ((LED_CONTROL >> 5) & 2) >> 1;  // Gets color from second bit
  bool power_led = LED_CONTROL >> 7;               // Gets power from first bit
  bool power_d13 = D13_CONTROL >> 7;               // Gets color from first bit

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

  if (timing == 0) {
    // Move to next bits //
    LED_CONTROL = LED_CONTROL >> 2 | LED_CONTROL << 6;  // Shifts bits
    D13_CONTROL = D13_CONTROL >> 1 | D13_CONTROL << 7;  // Shifts bits
  }
  delay(1);
}


void outputError() {
  Serial.println(F("Invalid command type \"help\""));
}


/* The following code prints the help menu */
void print_help() {
  Serial.println(F("The following commands are valid (Case matters)"));
  Serial.println(F("led <on, off, blink <NONE, rg>, green, red>"));
  Serial.println(F("d13 <on, off, blink>"));
  Serial.println(F("set blink <number 0-65535>"));
  Serial.println(F("status leds"));
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
    INPUT_BUFFER[i] = '\0';
  }
}

/* The below code clears the token buffer */
void tokenBufferClear() {
  for (int j = 0; j < MAX; j++) {  // Clears the to tokenize array
    TOKEN_BUFFER[j] = 0;
  }
}


/* The below code implements backspace */
void backspace() {
  char back[1];
  sprintf(back, "\033[%dD", (1));  // Foramtted string to move curser back one
  Serial.print(back);
  Serial.print(' ');  // Overwrites last input as a space
  Serial.print(back);
}


/* The code below handles extra input */
bool extraInput(int index) {
  if (TOKEN_BUFFER[index + 1] != t_EOL) {
    return 0;
  }
  Serial.println(F("Invalid command type \"help\""));
  return 1;
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
    getNumbers(TOKENCOUNTER, i, 2);
  }
  return TOKENCOUNTER;
}


int check_if_add(int TOKENCOUNTER, int i) {
  if ((int)TOKEN_BUFFER[TOKENCOUNTER - 1] == (int)t_ADD) {
    TOKENCOUNTER = getNumbers(TOKENCOUNTER, i, 1);
  }
  return TOKENCOUNTER;
}


void printTokens() {
  for (int i = 0; i < MAX - 1; i++) {
    Serial.println(TOKEN_BUFFER[i]);
  }
}


int getNumbers(int TOKENCOUNTER, int i, unsigned char use) {
  unsigned char* cursor = &INPUT_BUFFER[i + 1];
  int numCount = 0;
  while (cursor != &INPUT_BUFFER[i + 1] + strlen(&INPUT_BUFFER[i + 1])) {
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
    } else if (use == 1) {
      Serial.println(F("Error Overflow: Numbers over 65535 or less than zero are set to zero."));
    } else if (use == 2) {  // if number is over 65535 sets blinkrate to prior value
      TOKEN_BUFFER[TOKENCOUNTER] = t_WORD;
      TOKENCOUNTER++;
      short unsigned int num = (short unsigned int)BLINK_RATE;

      unsigned char lsb = (unsigned)num & 0xff;  // mask the lower 8 bits
      unsigned char msb = (unsigned)num >> 8;    // shift the higher 8 bits
      TOKEN_BUFFER[TOKENCOUNTER] = lsb;
      TOKENCOUNTER++;
      TOKEN_BUFFER[TOKENCOUNTER] = msb;
      TOKENCOUNTER++;
      Serial.println(F("Error Overflow: Numbers over 65535 or less than zero are set to prior value."));
    }
    numCount++;
  }
  if (numCount != 2 && use == 1){
    Serial.println(F("ADD TAKES 2 INPUTS"));
    return TOKENCOUNTER;
  }
  else if (numCount != 1 && use == 2){
    Serial.println(F("SET BLINK TAKES 1 INPUT"));
        return TOKENCOUNTER;

  }
    return TOKENCOUNTER;
}


void set_color(unsigned char red, unsigned char green, unsigned char blue) {
  COLOR[0] = red;
  COLOR[1] = green;
  COLOR[2] = blue;

  leds[0].setRGB(red, green, blue);
}

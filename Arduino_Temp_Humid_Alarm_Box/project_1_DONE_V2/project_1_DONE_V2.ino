#include <Arduino.h>
#include <stdio.h>
#include <ctype.h>
// Initializes the names of the pins after their function
#define LED_GREEN 5
#define LED_RED 6
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
// What LED
#define t_RED 51
#define t_GREEN 52
#define t_D13 53
// Defines needed globals for LED
short int LED_BLINK = 0;
bool LED_ON = 0;

bool D13_BLINK = 0;
bool D13_ON = 0;

bool COLOR = 0;  // Defults to red
short unsigned int BLINK_RATE = 500;
// Max array
static const int MAX = 32;
// Makes Token Buffer
unsigned char TOKEN_BUFFER[MAX] = { 0 };
char INPUT_BUFFER[MAX] = { 0 };
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
  'd', '1', 3, t_D13,      // t_D13
  'l', 'e', 4, t_LEDS      // t_D13
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
  processInput();      // Takes in user input and inputs tokens into token array.
  tokenTree();         // Runs all tokens
  tokenBufferClear();  // clears token buffer
  inputBufferClear();  // clears input buffer
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

        if ((int)TOKEN_BUFFER[TOKENCOUNTER - 1] == (int)t_BLINK && (int)TOKEN_BUFFER[TOKENCOUNTER - 2] == (int)t_SET) {
          TOKEN_BUFFER[TOKENCOUNTER] = t_WORD;
          TOKENCOUNTER++;
          long int blink_input = (long int)strtol(&INPUT_BUFFER[i + 1], NULL, 10);

          if (blink_input <= 65535 && blink_input >= 0) {
            short unsigned int blink_setting = (short unsigned int)blink_input;

            unsigned char lsb = (unsigned)blink_setting & 0xff;  // mask the lower 8 bits
            unsigned char msb = (unsigned)blink_setting >> 8;    // shift the higher 8 bits
            TOKEN_BUFFER[TOKENCOUNTER] = lsb;
            TOKENCOUNTER++;
            TOKEN_BUFFER[TOKENCOUNTER] = msb;
            TOKENCOUNTER++;
          }


          else {
            Serial.println("Invalid blink setting (0-65535)");
            unsigned char lsb = (unsigned)BLINK_RATE & 0xff;  // mask the lower 8 bits
            unsigned char msb = (unsigned)BLINK_RATE >> 8;    // shift the higher 8 bits
            TOKEN_BUFFER[TOKENCOUNTER] = lsb;
            TOKENCOUNTER++;
            TOKEN_BUFFER[TOKENCOUNTER] = msb;
            TOKENCOUNTER++;
          }
          TOKENCOUNTER++;
        }
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


/* The below code checks input against the lookup table */
int toToken(char toTokenize[], int len, int TOKENCOUNTER) {
  char first2[2];
  first2[0] = toTokenize[0];
  first2[1] = toTokenize[1];

  for (int i = 0; i < (sizeof(LOOKUP) / sizeof(LOOKUP[0])) / 4; i++)  // Iterates through every 4 element window
  {
    // Checks if first 2 letters and length are the same
    if (first2[0] == LOOKUP[i * 4] && first2[1] == LOOKUP[i * 4 + 1] && len == (int)LOOKUP[i * 4 + 2]) {
      TOKEN_BUFFER[TOKENCOUNTER] = (unsigned char)LOOKUP[i * 4 + 3];
      return 1;
      ;
    }
  }
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
        case t_BLINK:  // Toggles Blink
          if (extraInput(current + 1) == 1) {
            return;
          }
          LED_BLINK = (LED_BLINK + 1) % 3;
          Serial.println(F("LED blink toggled"));
          break;
        case t_RED:
          if (extraInput(current + 1) == 1) {
            return;
          }
          COLOR = 0;
          led_on();
          Serial.println(F("LED red"));
          break;

        case t_GREEN:
          if (extraInput(current + 1) == 1) {
            return;
          }
          COLOR = 1;
          led_on();
          Serial.println(F("LED green"));

          break;
        case t_ON:
          if (extraInput(current + 1) == 1) {
            return;
          }
          led_on();
          Serial.println(F("LED on"));

          break;

        case t_OFF:
          if (extraInput(current + 1) == 1) {
            return;
          }
          led_off();
          Serial.println(F("LED off"));

          LED_BLINK = 0;
          break;

        default:
          Serial.println(F("Invalid command type \"help\""));

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
          D13_BLINK = (D13_BLINK + 1) % 2;
          Serial.println(F("D13 blink toggled"));

          break;
        case t_ON:
          if (extraInput(current + 1) == 1) {
            return;
          }
          D13_on();
          Serial.println(F("D13 on"));

          break;

        case t_OFF:
          if (extraInput(current + 1) == 1) {
            return;
          }
          D13_off();
          D13_BLINK = 0;
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

    default:
      if (extraInput(current + 1) == 1) {
        return;
      }
      Serial.println(F("Invalid command type \"help\""));
      break;
  }
}


/* The code below turns on the led */
void led_on() {
  if (COLOR == 0) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
  }
  LED_ON = 1;
}


/* The code below turns off the led */
void led_off() {
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  LED_ON = 0;
}


/* The code below turns on the onboard led */
void D13_on() {
  digitalWrite(LED_BUILTIN, HIGH);
  D13_ON = 1;
}


/* The code below turns off the onboard led */

void D13_off() {
  digitalWrite(LED_BUILTIN, LOW);
  D13_ON = 0;
}


/* The following code prints the help menu */
void print_help() {
  Serial.println(F("The following commands are valid (Case matters)"));
  Serial.println(F("led <on, off, blink, green, red>"));
  Serial.println(F("d13 <on, off, blink>"));
  Serial.println(F("set blink <number 0-65535>"));
  Serial.println(F("status leds"));
  Serial.println(F("version"));
  Serial.println(F("help"));
}


/* The following code prints the status of all LEDS */
void print_status() {
  Serial.println(F("Current status:"));
  Serial.println(F("LED (on, color (0 = red, 1 = green), blink (0 = off, 1 = single color, 2 = red/green)): "));
  if (LED_BLINK == 1) {
    Serial.print('1');
  } else {

    Serial.print(LED_ON);
  }
  Serial.print(' ');
  Serial.print(COLOR);
  Serial.print(' ');
  Serial.println(LED_BLINK);
  Serial.println(F("D13 (on, blink): "));
  if (D13_BLINK == 1) {
    Serial.print('1');
  } else {

    Serial.print(D13_ON);
  }
  Serial.print(' ');
  Serial.println(D13_BLINK);
  Serial.println(F("Blink Rate:"));
  Serial.println(BLINK_RATE);
}


/* The code below prints the input buffer */
void printBuffer() {
  for (int i = 0; i < sizeof(INPUT_BUFFER) / sizeof(INPUT_BUFFER[0]) - 1; i++) {
    if (isalnum(INPUT_BUFFER[i]) || INPUT_BUFFER[i] == ' ') {
      Serial.print(INPUT_BUFFER[i]);
    }
  }
  Serial.println();
}


/* The code below clears the input buffer */
void inputBufferClear() {
  for (int i = 0; i < sizeof(INPUT_BUFFER) / sizeof(INPUT_BUFFER[0]) - 1; i++) {
    INPUT_BUFFER[i] = 0;
  }
}

/* The below code clears the token buffer */
void tokenBufferClear() {
  for (int j = 0; j < MAX; j++) {  // Clears the to tokenize array
    TOKEN_BUFFER[j] = { 0 };
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


/* The below code takes user input and puts it into a buffer array */
void input() {
  Serial.println("Enter Command");  // Prompts user for command
  bool noEnter = 1;                 // used to break loop when enter key is pressed
  int index = 0;                    // keeps track of index in input buffer
  char curInput = 0;
  int timing = millis() % (2 * BLINK_RATE);
  while (noEnter == 1) {


    while (Serial.available() == 0)  // Wait for data available
    {
      timing = millis() % (2 * BLINK_RATE);
      if (LED_BLINK == 1) {  // Handles blinking for led
        if (timing == 0) {
          led_off();
        }
        if (timing == BLINK_RATE - 1) {
          led_on();
        }
      }
      if (LED_BLINK == 2) {  // Handles blinking for r/g
        if (timing == 0) {
          COLOR = (COLOR + 1) % 2;
          led_off();
          led_on();
        }
      }
      if (D13_BLINK == 1) {  // Handles blinking for onboard
        if (timing == 0) {
          D13_off();
        }
        if (timing == BLINK_RATE - 1) {
          D13_on();
        }
      }
    }
    curInput = Serial.read();
    if ((char)curInput == 13) {  // checks if enter key
      INPUT_BUFFER[MAX - 1] = '0';
      noEnter = 0;
      Serial.println(' ');
    } else if ((char)curInput == 127 && index > 0) {  // handles backspaces
      index--;
      INPUT_BUFFER[index] = 0;
      backspace();
    } else if (index == MAX) {  // Checks for too large input

    } else {  // puts in input buffer
      INPUT_BUFFER[index] = (char)curInput;
      Serial.print((char)curInput);
      index++;
    }
  }
}


bool extraInput(int index) {
  if (TOKEN_BUFFER[index + 1] != t_EOL) {
    return 0;
  }
  Serial.println(F("Invalid command type \"help\""));
  return 1;
}

#include "funshield.h"

/* CONSTANTS */

//GLYPHS
constexpr byte c_letterGlyphs[] { // map of letter glyphs
  0b10001000,   // A
  0b10000011,   // b
  0b11000110,   // C
  0b10100001,   // d
  0b10000110,   // E
  0b10001110,   // F
  0b10000010,   // G
  0b10001001,   // H
  0b11111001,   // I
  0b11100001,   // J
  0b10000101,   // K
  0b11000111,   // L
  0b11001000,   // M
  0b10101011,   // n
  0b10100011,   // o
  0b10001100,   // P
  0b10011000,   // q
  0b10101111,   // r
  0b10010010,   // S
  0b10000111,   // t
  0b11000001,   // U
  0b11100011,   // v
  0b10000001,   // W
  0b10110110,   // ksi
  0b10010001,   // Y
  0b10100100,   // Z
};

constexpr byte c_numberGlyphs[] = {
  0xC0, // 0  0b11000000
  0xF9, // 1  0b11111001
  0xA4, // 2  0b10100100
  0xB0, // 3  0b10110000
  0x99, // 4  0b10011001
  0x92, // 5  0b10010010
  0x82, // 6  0b10000010
  0xF8, // 7  0b11111000
  0x80, // 8  0b10000000
  0x90  // 9  0b10010000
};

constexpr byte c_emptyGlyph = 0b11111111;

//MOD STATES
enum class ModStates { //an informative enum (for debugging)
  CONFIGURATION_MODE,
  NORMAL_MODE
};

ModStates currentMod = ModStates::CONFIGURATION_MODE;

//BUTTONS
constexpr size_t c_numButtons = 3;

constexpr int c_buttonPins[] = { button1_pin, button2_pin, button3_pin };

bool g_btnHeld[] = {false, false, false}; //is the button being held?

//DISPLAY
constexpr size_t c_numDigits = 4;

char g_displayedChars[] = { '1', 'd', '0', '4' }; //which chars are being displayed, starting at 1d04

size_t g_displayedDigit = 0; //which digit is being displayed? for multiplexing

//DICE

constexpr size_t c_maxThrows = 10;

constexpr size_t c_dice[] = { 4, 6, 8, 10, 12, 20, 100 }; //d4, d6, d8, d10, d12, d20, and d100

constexpr size_t c_diceCount = sizeof(c_dice)/sizeof(c_dice[0]);

size_t g_diceIdx = 0;

size_t g_diceThrows = 1;

/* FUNCTIONS */

/** 
 * Show chararcter on given position. If character is not letter or a digit, empty glyph is displayed instead.
 * @param ch character to be displayed
 * @param pos position (0 = leftmost)
 */
void displayChar(char ch, byte pos)
{
  byte glyph = c_emptyGlyph;
  if (isAlpha(ch)) {
    glyph = c_letterGlyphs[ ch - (isUpperCase(ch) ? 'A' : 'a') ];
  } else if (isdigit(ch)) {
    glyph = c_numberGlyphs[ ch - '0' ];
  }
  
  digitalWrite(latch_pin, LOW);
  shiftOut(data_pin, clock_pin, MSBFIRST, glyph);
  shiftOut(data_pin, clock_pin, MSBFIRST, 1 << pos);
  digitalWrite(latch_pin, HIGH);
}

void writeToDisplay() { //multiplexing, shows the "g_displayedChars" array to the display
  displayChar(g_displayedChars[g_displayedDigit], g_displayedDigit);
  g_displayedDigit = (g_displayedDigit + 1) % c_numDigits;
}

void changeDice() { //increments the dice index (which means it changes the current dice)
  g_diceIdx = (g_diceIdx + 1) % c_diceCount;
}

size_t toPwrOf(size_t a, size_t b) { //returns a^b
  size_t result = 1;
  for(size_t i = 0; i < b; ++i) {
    result *= a;
  }
  return result;
}

size_t findGlyph(size_t number, size_t pos) { //calculates the digit which should be shown on a position pos for any number, could be simplified to one line (this is still readable though)
  size_t base = 10;
  size_t pwrFirst = (((c_numDigits-1) - pos) % c_numDigits);
  size_t pwrSecond = pwrFirst + 1;
  size_t divFirst = toPwrOf(base, pwrFirst);
  size_t divSecond = toPwrOf(base, pwrSecond);
  size_t res = ((number % divSecond) - (number % divFirst)) / divFirst;
  return res;
}

void changeThrows() { //increments throws
  g_diceThrows = (g_diceThrows + 1) % c_maxThrows;
  if(g_diceThrows == 0) ++g_diceThrows; //we do not want 0 throws
}

void changeDisplay(char one, char two, char three, char four) { //changes the output of the display, takes 4 characters as args
  char charArr[] = { one, two, three, four };
  for(size_t i = 0; i < c_numDigits; ++i) {
    g_displayedChars[i] = charArr[i];
  }
}

size_t generateRandom(unsigned long t) { //takes current time as a seed for the srand() function, generates valid random numbers via rand() and adds them together
  size_t total = 0;
  srand(t);
  for(size_t i = 1; i <= g_diceThrows; i++) {
    size_t r = rand();
    total += (r % c_dice[g_diceIdx]) + 1;
  }
  return total;
}

void changeDispCfgMode() { //changes the display if set to cfg mode
    if (g_diceIdx == c_diceCount-1) changeDisplay(g_diceThrows + '0', 'd', '0', '0');
    else changeDisplay(g_diceThrows + '0', 'd', (c_dice[g_diceIdx] / 10) + '0', (c_dice[g_diceIdx] % 10) + '0');
}

void checkBtns(size_t btnNumber, unsigned long t) { //checks the button status
  size_t btnState = digitalRead(c_buttonPins[btnNumber-1]); //read the current button state
  if(btnState == ON && !g_btnHeld[btnNumber-1]) { //button is pressed
    g_btnHeld[btnNumber-1] = true; //we assume the button is being held now
    switch(btnNumber) {
      case 1:
        currentMod = ModStates::NORMAL_MODE;
        break;
      case 2:
        currentMod = ModStates::CONFIGURATION_MODE;
        changeThrows();
        changeDispCfgMode();
        break;
      case 3:
        currentMod = ModStates::CONFIGURATION_MODE;
        changeDice();
        changeDispCfgMode();
        break;
    }
  }
  
  else if (btnState == ON && g_btnHeld[0] && btnNumber == 1) { //holding button 1
    size_t newRand = generateRandom(t);
    changeDisplay(((newRand + 1) % 10) + '0', ((newRand + 2) % 10) + '0',
        ((newRand + 3) % 10) + '0', ((newRand + 4) % 10) + '0'); //shows random display output when generating
  }

  else if (g_btnHeld[0] && btnState == OFF && btnNumber == 1) { //on button 1 release
    size_t newRand = generateRandom(t);
    changeDisplay('0', findGlyph(newRand, 1) + '0', findGlyph(newRand, 2) + '0', findGlyph(newRand, 3) + '0'); //shows the generated number
    g_btnHeld[0] = false;
  }
  
  else if (btnState == OFF) { //the button is not being held anymore
    g_btnHeld[btnNumber-1] = false;
  }
}

void setup() {
  pinMode(latch_pin, OUTPUT);
  pinMode(clock_pin, OUTPUT);
  pinMode(data_pin, OUTPUT);
}

void loop() {
  unsigned long currentTime = millis();
  for (size_t i = 1; i <= c_numButtons; ++i) {
    checkBtns(i, currentTime);
  }
  writeToDisplay();
}
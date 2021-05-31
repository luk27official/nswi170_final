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

//FUNCTIONS (non-dependent on other objects)
size_t toPwrOf(size_t base, size_t exponent) {
  size_t result = 1;
  for(size_t i = 0; i < exponent; ++i) {
    result *= base;
  }
  return result;
} //returns base^exponent (supposing smaller than size_t)

//BUTTONS
struct Button {
  size_t id;
  bool is_pressed;
  Button(size_t btnid) {
    id = btnid;
    is_pressed = false;
  }

  void setup() {
    pinMode(id, INPUT);
  }
};

struct Buttons {
  Button btns[3] = { 
    Button(button1_pin),
    Button(button2_pin),
    Button(button3_pin),
  };

  size_t buttonPinsCount() {
    return sizeof(btns) / sizeof(btns[0]);
  }

  void setup() {
    for (size_t i = 0; i < buttonPinsCount(); ++i) {
      btns[i].setup();
    }
  }
};

Buttons buttons;

//DISPLAY
struct Display {
  char displayedChars[4] = { '1', 'd', '0', '4' }; //which chars are being displayed, starting at 1d04

  size_t displayedDigit = 0; //which digit is being displayed? for multiplexing

  size_t displayDigits() {
    return sizeof(digit_muxpos) / sizeof(digit_muxpos[0]);
  }

  void setup() {
    pinMode(latch_pin, OUTPUT);
    pinMode(clock_pin, OUTPUT);
    pinMode(data_pin, OUTPUT);
  }

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

  void writeToDisplay() { //multiplexing, shows the "displayedChars" array to the display
    displayChar(displayedChars[displayedDigit], displayedDigit);
    displayedDigit = (displayedDigit + 1) % displayDigits();
  }

  size_t findGlyph(size_t number, size_t pos) { //calculates the digit which should be shown on a position pos for any number, could be simplified to one line (this is still readable though)
    size_t base = 10;
    size_t pwrFirst = (((displayDigits()-1) - pos) % displayDigits());
    size_t pwrSecond = pwrFirst + 1;
    size_t divFirst = toPwrOf(base, pwrFirst);
    size_t divSecond = toPwrOf(base, pwrSecond);
    size_t res = ((number % divSecond) - (number % divFirst)) / divFirst;
    return res;
  }

  void changeDisplay(char one, char two, char three, char four) { //changes the output of the display, takes 4 characters as args
    char charArr[] = { one, two, three, four };
    for(size_t i = 0; i < displayDigits(); ++i) {
      displayedChars[i] = charArr[i];
    }
  }
};

Display disp;

//DICE
struct Dice {
  size_t maxThrows = 10;

  size_t types[7] = { 4, 6, 8, 10, 12, 20, 100 }; //d4, d6, d8, d10, d12, d20, and d100

  size_t countD() {
    return sizeof(types)/sizeof(types[0]);
  }

  size_t idx = 0;

  size_t throws = 1;

  void changeDice() { //increments the dice index (which means it changes the current dice)
    idx = (idx + 1) % countD();
  }

  void changeThrows() { //increments throws
    throws = (throws + 1) % maxThrows;
    if(throws == 0) ++throws; //we do not want 0 throws
  }

  size_t generateRandom(unsigned long t) { //takes current time as a seed for the srand() function, generates valid random numbers via rand() and adds them together
    size_t total = 0;
    srand(t);
    for(size_t i = 1; i <= throws; i++) {
      size_t r = rand();
      total += (r % types[idx]) + 1;
    }
    return total;
  }

};

Dice dice;

/* FUNCTIONS (dependent on more objects)*/

void changeDispCfgMode() { //changes the display if set to cfg mode
    if (dice.idx == dice.countD()-1) disp.changeDisplay(dice.throws + '0', 'd', '0', '0');
    else disp.changeDisplay(dice.throws + '0', 'd', (dice.types[dice.idx] / 10) + '0', (dice.types[dice.idx] % 10) + '0');
}

void checkBtns(size_t btnNumber, unsigned long t) { //checks the button status
  size_t btnState = digitalRead(buttons.btns[btnNumber-1].id); //read the current button state
  if(btnState == ON && !buttons.btns[btnNumber-1].is_pressed) { //button is pressed
    buttons.btns[btnNumber-1].is_pressed = true; //we assume the button is being held now
    switch(btnNumber) {
      case 1:
        currentMod = ModStates::NORMAL_MODE;
        break;
      case 2:
        currentMod = ModStates::CONFIGURATION_MODE;
        dice.changeThrows();
        changeDispCfgMode();
        break;
      case 3:
        currentMod = ModStates::CONFIGURATION_MODE;
        dice.changeDice();
        changeDispCfgMode();
        break;
    }
  }
  
  else if (btnState == ON && buttons.btns[0].is_pressed && btnNumber == 1) { //holding button 1
    size_t newRand = dice.generateRandom(t);
    disp.changeDisplay(((newRand + 1) % 10) + '0', ((newRand + 2) % 10) + '0',
        ((newRand + 3) % 10) + '0', ((newRand + 4) % 10) + '0'); //shows random display output when generating
  }

  else if (buttons.btns[0].is_pressed && btnState == OFF && btnNumber == 1) { //on button 1 release
    size_t newRand = dice.generateRandom(t);
    disp.changeDisplay('0', disp.findGlyph(newRand, 1) + '0', disp.findGlyph(newRand, 2) + '0', disp.findGlyph(newRand, 3) + '0'); //shows the generated number
    buttons.btns[0].is_pressed = false;
  }
  
  else if (btnState == OFF) { //the button is not being held anymore
    buttons.btns[btnNumber-1].is_pressed = false;
  }
}

//ARDUINO FUNCTIONS

void setup() {
  buttons.setup();
  disp.setup();
}

void loop() {
  unsigned long currentTime = millis();
  for (size_t i = 1; i <= buttons.buttonPinsCount(); ++i) {
    checkBtns(i, currentTime);
  }
  disp.writeToDisplay();
}

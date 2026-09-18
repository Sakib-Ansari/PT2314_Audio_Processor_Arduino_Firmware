#include <Wire.h>
#include <LiquidCrystal.h>
#include <PT2314.h> // Implements requested data types (KEYWORD1), methods (KEYWORD2), and LITERAL1

// Initialize Hardware Instances
PT2314 audioProcessor;
LiquidCrystal lcd(12, 11, 5, 6, 7, 8); // 16x2 Display configured in 4-bit operational mode

// Pin Layout
#define ENC_CLK 2
#define ENC_DT  3
#define ENC_SW  4

// Enumeration of unified configuration menus
enum MenuMode {
  MENU_VOLUME,
  MENU_BASS,
  MENU_TREBLE,
  MENU_GAIN,
  MENU_LOUDNESS,
  MENU_ATTN_L,
  MENU_ATTN_R,
  TOTAL_MENUS
};

MenuMode activeMenu = MENU_VOLUME;

// Variables for Rotary Encoder tracking
int previousClkState;
unsigned long lastButtonTime = 0;
bool refreshNeeded = true; 

void setup() {
  // Pin modes definition
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("PT2314 Booting");
  
  Wire.begin();
  delay(50); // Mandatory 50ms datasheet initialization safety delay
  
  // Initialize the processor using requested library methods
  if (audioProcessor.begin()) {
    lcd.setCursor(0, 1);
    lcd.print("I2C Conn: OK");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("I2C Conn: FAIL");
    while (1); // Halt execution if IC cannot be tracked down
  }
  
  delay(1500); // Display startup verification screen
  audioProcessor.setChannel(1);
  
  // Fetch fallback parameter states and apply standard baseline values
  audioProcessor.setVolume(20);         // 0 (Loudest) to 63 (Quietest)
  audioProcessor.setBass(0);            // -14dB to +14dB
  audioProcessor.setTreble(0);          // -14dB to +14dB
  audioProcessor.setGain(0);            // 0 to 3
  audioProcessor.setLoudness(true);     // Enable standard bass loudness filter
  audioProcessor.setAttnLeft(0);        // 0 to 31 step balance attenuation
  audioProcessor.setAttnRight(0);       // 0 to 31 step balance attenuation
  
  previousClkState = digitalRead(ENC_CLK);
}

void loop() {
  checkMenuButton();
  checkRotation();
  updateDisplayEngine();
}

/**
 * Iterates through menu indices sequentially upon pressing the shaft switch
 */
void checkMenuButton() {
  if (digitalRead(ENC_SW) == LOW) {
    if (millis() - lastButtonTime > 250) { // 250ms debouncing logic window
      activeMenu = static_cast<MenuMode>((activeMenu + 1) % TOTAL_MENUS);
      refreshNeeded = true;
      lastButtonTime = millis();
    }
  }
}

/**
 * Tracks physical rotation direction and maps adjustments dynamically to specific library parameters
 */
void checkRotation() {
  int currentClkState = digitalRead(ENC_CLK);
  
  if (currentClkState != previousClkState && currentClkState == LOW) {
    bool clockwise = (digitalRead(ENC_DT) != currentClkState);
    
    switch (activeMenu) {
      case MENU_VOLUME: {
        int currentVol = audioProcessor.getVolume();
        // Hardware note: lower steps mean a louder volume amplitude.
        // Clockwise rotation must decrease steps to sound louder to users.
        if (clockwise && currentVol > 0) currentVol--;
        else if (!clockwise && currentVol < 63) currentVol++;
        audioProcessor.setVolume(currentVol);
        break;
      }
      case MENU_BASS: {
        int8_t currentBass = audioProcessor.getBass();
        if (clockwise && currentBass < 14) currentBass += 2; // Steps operate strictly in 2dB increments
        else if (!clockwise && currentBass > -14) currentBass -= 2;
        audioProcessor.setBass(currentBass);
        break;
      }
      case MENU_TREBLE: {
        int8_t currentTreb = audioProcessor.getTreble();
        if (clockwise && currentTreb < 14) currentTreb += 2; // Steps operate strictly in 2dB increments
        else if (!clockwise && currentTreb > -14) currentTreb -= 2;
        audioProcessor.setTreble(currentTreb);
        break;
      }
      case MENU_GAIN: {
        uint8_t currentGain = audioProcessor.getGain();
        if (clockwise && currentGain < 3) currentGain++;
        else if (!clockwise && currentGain > 0) currentGain--;
        audioProcessor.setGain(currentGain);
        break;
      }
      case MENU_LOUDNESS: {
        bool currentLoud = audioProcessor.getLoudness();
        audioProcessor.setLoudness(!currentLoud); // Direct structural inversion
        break;
      }
      case MENU_ATTN_L: {
        uint8_t currentAttnL = audioProcessor.getAttnLeft();
        if (clockwise && currentAttnL < 31) currentAttnL++;
        else if (!clockwise && currentAttnL > 0) currentAttnL--;
        audioProcessor.setAttnLeft(currentAttnL);
        break;
      }
      case MENU_ATTN_R: {
        uint8_t currentAttnR = audioProcessor.getAttnRight();
        if (clockwise && currentAttnR < 31) currentAttnR++;
        else if (!clockwise && currentAttnR > 0) currentAttnR--;
        audioProcessor.setAttnRight(currentAttnR);
        break;
      }
    }
    refreshNeeded = true;
  }
  previousClkState = currentClkState;
}

/**
 * Visually manages display refreshes without rendering lag
 */
void updateDisplayEngine() {
  if (!refreshNeeded) return;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  
  // Render structural row 1 context headers
  switch (activeMenu) {
    case MENU_VOLUME:   lcd.print("Master Volume");    break;
    case MENU_BASS:     lcd.print("Bass Balance");     break;
    case MENU_TREBLE:   lcd.print("Treble Balance");   break;
    case MENU_GAIN:     lcd.print("Input Stage Gain"); break;
    case MENU_LOUDNESS: lcd.print("Loudness Filter");  break;
    case MENU_ATTN_L:   lcd.print("Speaker Attn L");   break;
    case MENU_ATTN_R:   lcd.print("Speaker Attn R");   break;
  }
  
  lcd.setCursor(0, 1);
  lcd.print("> ");
  
  // Render dynamic live value responses from the library
  switch (activeMenu) {
    case MENU_VOLUME:
      // Translate inverse parameters into intuitive percentage scaling (0% to 100%)
      lcd.print(map(audioProcessor.getVolume(), 63, 0, 0, 100));
      lcd.print("%");
      break;
    case MENU_BASS:
      lcd.print(audioProcessor.getBass());
      lcd.print(" dB");
      break;
    case MENU_TREBLE:
      lcd.print(audioProcessor.getTreble());
      lcd.print(" dB");
      break;
    case MENU_GAIN:
      // Translate raw numeric step indexing into real hardware DB output formats
      switch(audioProcessor.getGain()) {
        case 0: lcd.print("0 dB");      break;
        case 1: lcd.print("+3.75 dB");  break;
        case 2: lcd.print("+7.50 dB");  break;
        case 3: lcd.print("+11.25 dB"); break;
      }
      break;
    case MENU_LOUDNESS:
      lcd.print(audioProcessor.getLoudness() ? "ENABLED [ON]" : "BYPASSED [OFF]");
      break;
    case MENU_ATTN_L:
      lcd.print("-");
      lcd.print(audioProcessor.getAttnLeft() * 1.25, 2); // Map steps to standard dB cuts
      lcd.print(" dB");
      break;
    case MENU_ATTN_R:
      lcd.print("-");
      lcd.print(audioProcessor.getAttnRight() * 1.25, 2); // Map steps to standard dB cuts
      lcd.print(" dB");
      break;
  }
  
  refreshNeeded = false;
}

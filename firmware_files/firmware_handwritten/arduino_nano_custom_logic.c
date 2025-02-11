/*
  Split Keyboard Firmware for Arduino Nano board
  
  This firmware allows communication between two halves of a split keyboard.
  Left side scans keys and sends the state to the right side via serial.
  Right side combines both halves' states and sends to the computer via USB.
  
  Copyright FEB 2025 : AKASH O' MATICS 
  
  Licensed under the MIT License 
*/

#include <Arduino.h>
#include <Wire.h>
#include <HID-Project.h>

// Pin definitions
#define ROW_COUNT 4
#define COL_COUNT 6
#define TOTAL_KEYS (ROW_COUNT * COL_COUNT)

// Configuration pins
#define SIDE_SELECT_PIN 10  // HIGH for right side, LOW for left side

// I2C address for left side
#define LEFT_SIDE_ADDR 0x23

// Timing
#define SCAN_INTERVAL 10      // ms between key scans
#define DEBOUNCE_TIME 20      // ms for debounce

// Special keys and commands
#define CMD_LAYER_CHANGE 0xF0
#define CMD_MACRO_RECORD 0xF1
#define CMD_MACRO_PLAY   0xF2
#define CMD_PROGRAM_MODE 0xF3

// Keyboard states
typedef enum {
  STATE_NORMAL,
  STATE_LAYER_SWITCH,
  STATE_PROGRAMMING,
  STATE_PROGRAMMING_SRC,
  STATE_PROGRAMMING_DST,
  STATE_MACRO_RECORD_TRIGGER,
  STATE_MACRO_RECORD,
  STATE_MACRO_PLAY,
  STATE_WAITING,
  STATE_PRINTING
} KeyboardState;

// Layer definitions
#define LAYER_DEFAULT 0
#define LAYER_FN      1
#define LAYER_NUMPAD  2
#define MAX_LAYERS    3

// Key matrices
const uint8_t rowPins[ROW_COUNT] = {2, 3, 4, 5};
const uint8_t colPins[COL_COUNT] = {6, 7, 8, 9, A0, A1};

// Key state tracking
bool currentKeyState[TOTAL_KEYS] = {0};
bool previousKeyState[TOTAL_KEYS] = {0};
bool debouncedKeyState[TOTAL_KEYS] = {0};
uint32_t lastDebounceTime[TOTAL_KEYS] = {0};

// Received key states from the other half
bool otherHalfKeyState[TOTAL_KEYS] = {0};

// Combined key states for HID report
uint8_t combinedKeyReport[6] = {0};
uint8_t prevKeyReport[6] = {0};

// Current active layer
uint8_t currentLayer = LAYER_DEFAULT;

// Configuration
bool isRightSide = false;
unsigned long lastScanTime = 0;
uint32_t uptimeMs = 0;

// State machine variables
KeyboardState currentState = STATE_NORMAL;
KeyboardState nextState = STATE_NORMAL;
uint8_t pressedKeyCount = 0;
uint8_t programSrcKey = 0;
bool recordingMacro = false;

// Keymap definitions (each layer has ROW_COUNT * COL_COUNT * 2 keys - for both halves)
const uint8_t keymap[MAX_LAYERS][ROW_COUNT * COL_COUNT * 2] PROGMEM = {
  // Default layer
  {
    // Left half
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y,
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H,
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N,
    KEY_ESC, KEY_TAB, KEY_LEFT_CTRL, KEY_LEFT_SHIFT, KEY_BACKSPACE, KEY_LEFT_ALT,
    
    // Right half
    KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_BACKSLASH,
    KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_QUOTE, KEY_ENTER,
    KEY_M, KEY_COMMA, KEY_PERIOD, KEY_SLASH, KEY_RIGHT_SHIFT, KEY_RIGHT_ALT,
    CMD_LAYER_CHANGE, KEY_SPACE, KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_UP_ARROW, KEY_RIGHT_ARROW
  },
  
  // Function layer
  {
    // Left half
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_ESC, KEY_TAB, KEY_LEFT_CTRL, KEY_LEFT_SHIFT, KEY_BACKSPACE, KEY_LEFT_ALT,
    
    // Right half
    KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL,
    KEY_HOME, KEY_PAGE_DOWN, KEY_PAGE_UP, KEY_END, KEY_DELETE, KEY_ENTER,
    CMD_MACRO_RECORD, KEY_VOLUME_DOWN, KEY_VOLUME_UP, KEY_MUTE, KEY_PRINT_SCREEN, CMD_PROGRAM_MODE,
    CMD_LAYER_CHANGE, KEY_SPACE, KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_UP_ARROW, KEY_RIGHT_ARROW
  },
  
  // Numpad layer
  {
    // Left half
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    KEY_ESC, KEY_TAB, KEY_LEFT_CTRL, KEY_LEFT_SHIFT, KEY_BACKSPACE, KEY_LEFT_ALT,
    
    // Right half
    KEY_NUM_LOCK, KEY_KP_SLASH, KEY_KP_ASTERISK, KEY_KP_MINUS, KEY_RESERVED, KEY_RESERVED,
    KEY_KP_7, KEY_KP_8, KEY_KP_9, KEY_KP_PLUS, KEY_RESERVED, KEY_RESERVED,
    KEY_KP_4, KEY_KP_5, KEY_KP_6, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
    KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_ENTER, KEY_KP_0, KEY_KP_DOT
  }
};

// Function prototypes
void scanKeys();
void processKeys();
void updateLEDs();
void handleStateNormal();
void handleStateProgramming();
void handleStateMacroRecordTrigger();
void handleStateMacroRecord();
void sendKeyReport();
void receiveKeyStates();
void sendKeyStates();
uint8_t getKeyFromPosition(uint8_t row, uint8_t col);
void clearKeyReport();
void updateKeyReport();
void updateTimers();

void setup() {
  // Initialize pins
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }
  
  for (uint8_t i = 0; i < COL_COUNT; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
  
  // Determine side (left or right)
  pinMode(SIDE_SELECT_PIN, INPUT_PULLUP);
  isRightSide = digitalRead(SIDE_SELECT_PIN);
  
  delay(100);
  
  if (isRightSide) {
    // Initialize USB HID (only on right side)
    Keyboard.begin();
    
    // Initialize I2C as master to receive data from left side
    Wire.begin();
  } else {
    // Initialize I2C as slave to send data to right side
    Wire.begin(LEFT_SIDE_ADDR);
    Wire.onRequest(sendKeyStates);
  }
  
  Serial.begin(115200);
  lastScanTime = millis();
}

void loop() {
  updateTimers();
  scanKeys();
  
  if (isRightSide) {
    // Right side: get key states from left side, process all keys, send to computer
    receiveKeyStates();
    processKeys();
    updateLEDs();
    
    // Process keyboard states
    switch (currentState) {
      case STATE_NORMAL:
        handleStateNormal();
        break;
      case STATE_WAITING:
        if (pressedKeyCount == 0) {
          currentState = nextState;
          nextState = STATE_NORMAL;
        }
        break;
      case STATE_PROGRAMMING_SRC:
      case STATE_PROGRAMMING_DST:
        handleStateProgramming();
        break;
      case STATE_MACRO_RECORD_TRIGGER:
        handleStateMacroRecordTrigger();
        break;
      case STATE_MACRO_RECORD:
        handleStateMacroRecord();
        break;
      case STATE_MACRO_PLAY:
        // Handled by key processing
        break;
      default:
        // Unknown state, go back to normal
        currentState = STATE_NORMAL;
        break;
    }
    
    // Send key report to USB
    sendKeyReport();
  } else {
    // Left side: nothing else to do, key states will be sent when requested
  }
  
  // Ensure scanning interval
  while (millis() - lastScanTime < SCAN_INTERVAL) {
    // Wait
  }
  lastScanTime = millis();
}

void updateTimers() {
  uptimeMs = millis();
}

void scanKeys() {
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    // Set the current row LOW for scanning
    digitalWrite(rowPins[row], LOW);
    delayMicroseconds(10); // Give the row time to settle
    
    for (uint8_t col = 0; col < COL_COUNT; col++) {
      uint8_t keyIndex = row * COL_COUNT + col;
      bool keyState = !digitalRead(colPins[col]); // Inverted because pullup
      
      // Store the current key state
      currentKeyState[keyIndex] = keyState;
      
      // Debounce
      if (currentKeyState[keyIndex] != previousKeyState[keyIndex]) {
        lastDebounceTime[keyIndex] = uptimeMs;
      }
      
      if ((uptimeMs - lastDebounceTime[keyIndex]) > DEBOUNCE_TIME) {
        // If the debounce time has passed, update the stable state
        if (debouncedKeyState[keyIndex] != currentKeyState[keyIndex]) {
          debouncedKeyState[keyIndex] = currentKeyState[keyIndex];
        }
      }
      
      previousKeyState[keyIndex] = currentKeyState[keyIndex];
    }
    
    // Set the row back to HIGH
    digitalWrite(rowPins[row], HIGH);
  }
  
  // Count pressed keys
  pressedKeyCount = 0;
  for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
    if (debouncedKeyState[i]) {
      pressedKeyCount++;
    }
    if (otherHalfKeyState[i]) {
      pressedKeyCount++;
    }
  }
}

void processKeys() {
  // Clear current key report
  clearKeyReport();
  
  // Only process and send keys in normal state
  if (currentState == STATE_NORMAL || currentState == STATE_MACRO_RECORD) {
    updateKeyReport();
  }
}

void clearKeyReport() {
  for (uint8_t i = 0; i < 6; i++) {
    combinedKeyReport[i] = 0;
  }
}

void updateKeyReport() {
  uint8_t reportIndex = 0;
  
  // First process this side's keys
  for (uint8_t i = 0; i < TOTAL_KEYS && reportIndex < 6; i++) {
    if (debouncedKeyState[i]) {
      uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i]);
      
      // Check for special commands
      if (keycode >= CMD_LAYER_CHANGE) {
        // Handle special commands
        if (keycode == CMD_LAYER_CHANGE) {
          // Toggle layer
          if (currentLayer == LAYER_DEFAULT) {
            currentLayer = LAYER_FN;
          } else {
            currentLayer = LAYER_DEFAULT;
          }
        }
        // Don't add command keys to the report
      } else if (keycode != KEY_RESERVED) {
        // Regular key, add to report
        combinedKeyReport[reportIndex++] = keycode;
      }
    }
  }
  
  // Then process the other half's keys
  uint8_t otherHalfOffset = TOTAL_KEYS; // Index offset for the other half in keymap
  for (uint8_t i = 0; i < TOTAL_KEYS && reportIndex < 6; i++) {
    if (otherHalfKeyState[i]) {
      uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i + otherHalfOffset]);
      
      // Check for special commands
      if (keycode >= CMD_LAYER_CHANGE) {
        // Handle special commands
        if (keycode == CMD_LAYER_CHANGE) {
          // Toggle layer
          if (currentLayer == LAYER_DEFAULT) {
            currentLayer = LAYER_FN;
          } else {
            currentLayer = LAYER_DEFAULT;
          }
        }
        // Don't add command keys to the report
      } else if (keycode != KEY_RESERVED) {
        // Regular key, add to report
        combinedKeyReport[reportIndex++] = keycode;
      }
    }
  }
}

void sendKeyReport() {
  bool reportChanged = false;
  
  // Check if report is different from previous one
  for (uint8_t i = 0; i < 6; i++) {
    if (combinedKeyReport[i] != prevKeyReport[i]) {
      reportChanged = true;
      break;
    }
  }
  
  // Only send if changed
  if (reportChanged) {
    // Release all keys first
    Keyboard.releaseAll();
    
    // Press the current keys
    for (uint8_t i = 0; i < 6; i++) {
      if (combinedKeyReport[i] != 0) {
        Keyboard.press(combinedKeyReport[i]);
      }
    }
    
    // Save current report
    memcpy(prevKeyReport, combinedKeyReport, 6);
  }
}

void receiveKeyStates() {
  // Request data from left side
  Wire.requestFrom(LEFT_SIDE_ADDR, TOTAL_KEYS);
  
  uint8_t bytesRead = 0;
  while (Wire.available() && bytesRead < TOTAL_KEYS) {
    uint8_t data = Wire.read();
    
    // Each byte contains up to 8 key states
    for (uint8_t bitPos = 0; bitPos < 8 && bytesRead < TOTAL_KEYS; bitPos++) {
      otherHalfKeyState[bytesRead++] = (data & (1 << bitPos)) ? true : false;
    }
  }
}

void sendKeyStates() {
  // Pack key states into bytes to minimize I2C transfer
  uint8_t packedData[TOTAL_KEYS / 8 + 1] = {0};
  
  for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
    if (debouncedKeyState[i]) {
      packedData[i / 8] |= (1 << (i % 8));
    }
  }
  
  Wire.write(packedData, sizeof(packedData));
}

void handleStateNormal() {
  // Check for special key combinations
  bool programKeyPressed = false;
  bool macroRecordKeyPressed = false;
  
  // Check this side's keys
  for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
    if (debouncedKeyState[i]) {
      uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i]);
      if (keycode == CMD_PROGRAM_MODE) {
        programKeyPressed = true;
      } else if (keycode == CMD_MACRO_RECORD) {
        macroRecordKeyPressed = true;
      }
    }
  }
  
  // Check other half's keys
  uint8_t otherHalfOffset = TOTAL_KEYS;
  for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
    if (otherHalfKeyState[i]) {
      uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i + otherHalfOffset]);
      if (keycode == CMD_PROGRAM_MODE) {
        programKeyPressed = true;
      } else if (keycode == CMD_MACRO_RECORD) {
        macroRecordKeyPressed = true;
      }
    }
  }
  
  // Enter special states based on key combinations
  if (programKeyPressed && macroRecordKeyPressed) {
    currentState = STATE_WAITING;
    nextState = STATE_MACRO_RECORD_TRIGGER;
  } else if (programKeyPressed) {
    currentState = STATE_WAITING;
    nextState = STATE_PROGRAMMING_SRC;
  }
}

void handleStateProgramming() {
  // Simplified programming mode for demonstration
  // In a real implementation, this would allow remapping keys
  
  if (pressedKeyCount == 1) {
    // Find which key is pressed
    uint8_t pressedKey = 0xFF; // Invalid value
    
    // Check this side's keys
    for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
      if (debouncedKeyState[i]) {
        uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i]);
        pressedKey = keycode;
        break;
      }
    }
    
    // Check other half's keys
    uint8_t otherHalfOffset = TOTAL_KEYS;
    for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
      if (otherHalfKeyState[i]) {
        uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i + otherHalfOffset]);
        pressedKey = keycode;
        break;
      }
    }
    
    if (currentState == STATE_PROGRAMMING_SRC) {
      // Save the source key
      programSrcKey = pressedKey;
      currentState = STATE_WAITING;
      nextState = STATE_PROGRAMMING_DST;
    } else if (currentState == STATE_PROGRAMMING_DST) {
      // In a real implementation, we would save the key remapping
      // For this demo, we'll just go back to programming mode
      currentState = STATE_WAITING;
      nextState = STATE_PROGRAMMING_SRC;
    }
  } else if (pressedKeyCount >= 2) {
    // Check for exit combo (PROGRAM + FN keys)
    if (programKeyPressed()) {
      currentState = STATE_WAITING;
      nextState = STATE_NORMAL;
    }
  }
}

bool programKeyPressed() {
  // Check if program key is pressed on either half
  for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
    if (debouncedKeyState[i]) {
      uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i]);
      if (keycode == CMD_PROGRAM_MODE) return true;
    }
  }
  
  uint8_t otherHalfOffset = TOTAL_KEYS;
  for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
    if (otherHalfKeyState[i]) {
      uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i + otherHalfOffset]);
      if (keycode == CMD_PROGRAM_MODE) return true;
    }
  }
  
  return false;
}

void handleStateMacroRecordTrigger() {
  // Simplified for demonstration
  // In a real implementation, this would record a key combination as a macro trigger
  
  if (pressedKeyCount > 0) {
    // Record triggered
    currentState = STATE_WAITING;
    nextState = STATE_MACRO_RECORD;
  }
}

void handleStateMacroRecord() {
  if (!recordingMacro) {
    recordingMacro = true;
    // In a real implementation, we would initialize macro recording
  }
  
  // Check for exit combo (MACRO_RECORD + PROGRAM keys)
  if (macroRecordKeyPressed() && programKeyPressed()) {
    recordingMacro = false;
    // In a real implementation, we would save the recorded macro
    currentState = STATE_WAITING;
    nextState = STATE_NORMAL;
  }
}

bool macroRecordKeyPressed() {
  // Check if macro record key is pressed on either half
  for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
    if (debouncedKeyState[i]) {
      uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i]);
      if (keycode == CMD_MACRO_RECORD) return true;
    }
  }
  
  uint8_t otherHalfOffset = TOTAL_KEYS;
  for (uint8_t i = 0; i < TOTAL_KEYS; i++) {
    if (otherHalfKeyState[i]) {
      uint8_t keycode = pgm_read_byte(&keymap[currentLayer][i + otherHalfOffset]);
      if (keycode == CMD_MACRO_RECORD) return true;
    }
  }
  
  return false;
}

void updateLEDs() {
  // LED management for different states
  
  // Example using Serial instead of actual LEDs
  static KeyboardState previousState = STATE_NORMAL;
  static uint8_t previousLayer = LAYER_DEFAULT;
  
  if (currentState != previousState || currentLayer != previousLayer) {
    Serial.print("State: ");

    // rgblight_sethsv(currentState * 5, 230, 70);

    Serial.print(currentState);

    // flash_led(currentLayer);
    Serial.print(", Layer: ");
    
    Serial.println(currentLayer);

    
    previousState = currentState;
    previousLayer = currentLayer;
  }
}

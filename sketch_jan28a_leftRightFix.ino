#include <Adafruit_BluefruitLE_SPI.h>

// ------ Bluefruit SPI Pins ------
#define BLUEFRUIT_SPI_CS   8
#define BLUEFRUIT_SPI_IRQ  7
#define BLUEFRUIT_SPI_RST  4

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// ------------------- Pins -------------------
#define BUTTON_M      2
#define BUTTON_P      3
#define BUTTON_J      5
#define BUTTON_U      6
#define BUTTON_I      9
#define BUTTON_K      10   // LEFT ARROW (real keyboard + max stability)
#define BUTTON_A      11   // RIGHT ARROW (real keyboard + max stability)
#define BUTTON_C      12
#define BUTTON_V      13

#define BUTTON_O      A0
#define BUTTON_UP     A1
#define BUTTON_DOWN   A2
#define BUTTON_LEFT   A3
#define BUTTON_RIGHT  A4

#define LAYOUT_BUTTON A5

// ------------------- Layout -------------------
int currentLayout = 0;

// ------------------- Button state -------------------
struct Button {
  uint8_t pin;
  bool wasPressed;
  bool holdKey;   // true = LEFT/RIGHT, false = others
};

Button buttons[] = {
  { BUTTON_M,     false, false },
  { BUTTON_P,     false, false },
  { BUTTON_J,     false, false },
  { BUTTON_U,     false, false },
  { BUTTON_I,     false, false },
  { BUTTON_K,     false, true  }, // LEFT ARROW
  { BUTTON_A,     false, true  }, // RIGHT ARROW
  { BUTTON_C,     false, false },
  { BUTTON_V,     false, false },
  { BUTTON_O,     false, false },
  { BUTTON_UP,    false, false },
  { BUTTON_DOWN,  false, false },
  { BUTTON_LEFT,  false, false },
  { BUTTON_RIGHT, false, false }
};

const size_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

// ------------------- Timing constants -------------------
const unsigned long debounceMs      = 30;
const unsigned long cooldownMs      = 150; // max stability between presses (hold keys)
const unsigned long releaseLockMs   = 60;  // block new press shortly after release (hold keys)
const unsigned long tapPressMs      = 60;  // duration for single-shot keys

// ------------------- Per-button timing -------------------
unsigned long lastChange[NUM_BUTTONS]      = {0};
unsigned long lastPressTime[NUM_BUTTONS]   = {0}; // for cooldown
unsigned long lastReleaseTime[NUM_BUTTONS] = {0}; // for release-lock

// ------------------- Keycodes per button per layout -------------------
// "AT+BLEKEYBOARDCODE=MM-00-KK"
const char* keymap[][4] = {
  // BUTTON_M
  { "AT+BLEKEYBOARDCODE=01-00-10",
    "AT+BLEKEYBOARDCODE=00-00-10",
    "AT+BLEKEYBOARDCODE=01-00-10",
    "AT+BLEKEYBOARDCODE=01-00-10"
  },
  // BUTTON_P
  { "AT+BLEKEYBOARDCODE=00-00-1C",
    "AT+BLEKEYBOARDCODE=00-00-0A",
    "AT+BLEKEYBOARDCODE=00-00-0A",
    "AT+BLEKEYBOARDCODE=00-00-0A"
  },
  // BUTTON_J
  { "AT+BLEKEYBOARDCODE=00-00-18",
    "AT+BLEKEYBOARDCODE=00-00-0F",
    "AT+BLEKEYBOARDCODE=00-00-13",
    "AT+BLEKEYBOARDCODE=00-00-15"
  },
  // BUTTON_U
  { "AT+BLEKEYBOARDCODE=00-00-0E",
    "AT+BLEKEYBOARDCODE=00-00-1B",
    "AT+BLEKEYBOARDCODE=00-00-1B",
    "AT+BLEKEYBOARDCODE=04-00-0A"
  },
  // BUTTON_I
  { "AT+BLEKEYBOARDCODE=00-00-52",
    "AT+BLEKEYBOARDCODE=00-00-52",
    "AT+BLEKEYBOARDCODE=00-00-1A",
    "AT+BLEKEYBOARDCODE=00-00-52"
  },
  // BUTTON_K (LEFT ARROW)
  { "AT+BLEKEYBOARDCODE=00-00-50",
    "AT+BLEKEYBOARDCODE=00-00-50",
    "AT+BLEKEYBOARDCODE=00-00-50",
    "AT+BLEKEYBOARDCODE=00-00-50"
  },
  // BUTTON_A (RIGHT ARROW)
  { "AT+BLEKEYBOARDCODE=00-00-4F",
    "AT+BLEKEYBOARDCODE=00-00-4F",
    "AT+BLEKEYBOARDCODE=00-00-4F",
    "AT+BLEKEYBOARDCODE=00-00-4F"
  },
  // BUTTON_C
  { "AT+BLEKEYBOARDCODE=00-00-0C",
    "AT+BLEKEYBOARDCODE=00-00-16",
    "AT+BLEKEYBOARDCODE=00-00-1D",
    "AT+BLEKEYBOARDCODE=00-00-13"
  },
  // BUTTON_V
  { "AT+BLEKEYBOARDCODE=00-00-51",
    "AT+BLEKEYBOARDCODE=00-00-51",
    "AT+BLEKEYBOARDCODE=00-00-16",
    "AT+BLEKEYBOARDCODE=00-00-51"
  },
  // BUTTON_O
  { "AT+BLEKEYBOARDCODE=00-00-04",
    "AT+BLEKEYBOARDCODE=00-00-13",
    "AT+BLEKEYBOARDCODE=00-00-19",
    "AT+BLEKEYBOARDCODE=04-00-0B"
  },
  // BUTTON_UP
  { "AT+BLEKEYBOARDCODE=00-00-06",
    "AT+BLEKEYBOARDCODE=00-00-19",
    "AT+BLEKEYBOARDCODE=00-00-08",
    "AT+BLEKEYBOARDCODE=00-00-14"
  },
  // BUTTON_DOWN
  { "AT+BLEKEYBOARDCODE=00-00-19",
    "AT+BLEKEYBOARDCODE=00-00-09",
    "AT+BLEKEYBOARDCODE=00-00-28",
    "AT+BLEKEYBOARDCODE=00-00-09"
  },
  // BUTTON_LEFT
  { "AT+BLEKEYBOARDCODE=00-00-0D",
    "AT+BLEKEYBOARDCODE=00-00-07",
    "AT+BLEKEYBOARDCODE=00-00-14",
    "AT+BLEKEYBOARDCODE=00-00-08"
  },
  // BUTTON_RIGHT
  { "AT+BLEKEYBOARDCODE=00-00-12",
    "AT+BLEKEYBOARDCODE=00-00-12",
    "AT+BLEKEYBOARDCODE=00-00-0B",
    "AT+BLEKEYBOARDCODE=00-00-0B"
  }
};

// ------------------- Helpers -------------------
void sendAllKeysUp() {
  ble.sendCommandCheckOK("AT+BLEKEYBOARDCODE=00-00-00");
}

void sendTap(const char* cmd) {
  ble.sendCommandCheckOK(cmd);
  delay(tapPressMs);
  sendAllKeysUp();
}

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  Serial.println("4-layout BLE keyboard (MAX stability arrows)");

  for (size_t i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
  pinMode(LAYOUT_BUTTON, INPUT_PULLUP);

  if (!ble.begin(true)) {
    Serial.println("Couldn't find Bluefruit");
    while (1);
  }
  ble.reset();
  ble.sendCommandCheckOK("AT+BleHIDEn=1");
  ble.sendCommandCheckOK("AT+GAPDEVNAME=BACUSTOMBOX");
  ble.sendCommandCheckOK("AT+GAPSTARTADV");
  Serial.println("Advertising as BACUSTOMBOX...");
}

// ------------------- Main Loop -------------------
void loop() {
  unsigned long now = millis();

  // Layout toggle (simple debounce)
  if (digitalRead(LAYOUT_BUTTON) == LOW) {
    delay(50);
    if (digitalRead(LAYOUT_BUTTON) == LOW) {
      currentLayout = (currentLayout + 1) % 4;
      Serial.print("Layout: ");
      Serial.println(currentLayout);
      while (digitalRead(LAYOUT_BUTTON) == LOW) { delay(10); }
      delay(50);
    }
  }

  // Scan all buttons
  for (size_t i = 0; i < NUM_BUTTONS; i++) {
    Button &b = buttons[i];
    int reading = digitalRead(b.pin);

    // -------- PRESS EDGE (HIGH -> LOW) --------
    if (reading == LOW && !b.wasPressed && (now - lastChange[i] > debounceMs)) {

      if (b.holdKey) {
        // Cooldown: block ultra-fast re-presses
        if (now - lastPressTime[i] < cooldownMs) {
          continue;
        }
        // Release-lock: block presses right after release
        if (now - lastReleaseTime[i] < releaseLockMs) {
          continue;
        }
        lastPressTime[i] = now;
      }

      b.wasPressed = true;
      lastChange[i] = now;

      const char* cmd = keymap[i][currentLayout];

      if (b.holdKey) {
        // MAX stability: always send a global key-up before any new keydown
        sendAllKeysUp();
        // Real keyboard behavior: keydown only, OS handles repeat
        ble.sendCommandCheckOK(cmd);
      } else {
        // Non-arrow: single-shot tap
        sendTap(cmd);
      }
    }

    // -------- RELEASE EDGE (LOW -> HIGH) --------
    if (reading == HIGH && b.wasPressed && (now - lastChange[i] > debounceMs)) {
      b.wasPressed = false;
      lastChange[i] = now;

      if (b.holdKey) {
        // Track release time for release-lock
        lastReleaseTime[i] = now;
        // Real keyboard behavior: keyup
        sendAllKeysUp();
      }
      // Non-hold keys already sent tap on press; nothing to do here
    }
  }
}
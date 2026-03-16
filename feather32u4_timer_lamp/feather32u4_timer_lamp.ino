#include <Arduino.h>

#include <Adafruit_NeoPixel.h>

#include <Adafruit_BluefruitLE_SPI.h>
#include <Adafruit_BLE.h>

#include "BluefruitConfig.h"

// -----------------------------
// Hardware / LED configuration
// -----------------------------
static constexpr uint8_t LED_PIN = 3; // Feather 32u4: Pin 3 is labeled SCL on the board
static constexpr uint16_t STRIPS = 4;
static constexpr uint16_t LEDS_PER_STRIP = 56;
static constexpr uint16_t LEDS_TOTAL = STRIPS * LEDS_PER_STRIP;

static constexpr neoPixelType PIXEL_TYPE = NEO_GRB + NEO_KHZ800; // WS2812B

Adafruit_NeoPixel leds(LEDS_TOTAL, LED_PIN, PIXEL_TYPE);

// -----------------------------
// BLE (Bluefruit LE nRF51)
// -----------------------------
static constexpr bool VERBOSE_MODE = false;
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// -----------------------------
// Timer behavior
// -----------------------------
static constexpr uint32_t DEFAULT_MINUTES = 5;
static constexpr uint32_t LAST_MINUTE_MS = 60UL * 1000UL;
static constexpr uint32_t YELLOW_MS = 50UL * 1000UL;
static constexpr uint32_t RED_MS = 10UL * 1000UL;
// Korrekturfaktor für Zeitabweichung (millis() + NeoPixel-Blockierung)
// Wert aus Messungen (~2 Minuten): echte Zeit ~1.06–1.07x länger als berechnet
// → Korrekturfaktor ~ 1 / 1.06 ≈ 0.94–0.95
static constexpr float TIME_CORRECTION = 0.945f;

enum class Mode : uint8_t {
  Idle,
  Running,
  BlinkExpired,
};

Mode mode = Mode::Idle;
uint32_t configuredMinutes = DEFAULT_MINUTES;
uint32_t startMs = 0;

uint32_t blinkLastToggleMs = 0;
bool blinkOn = false;

// Letzte Zeit, zu der ein LED-Frame an den Strip geschickt wurde
uint32_t lastFrameMs = 0;

// -----------------------------
// Helpers
// -----------------------------
static inline uint32_t msFromMinutes(uint32_t minutes) {
  const uint32_t raw = minutes * 60UL * 1000UL;
  return (uint32_t)(raw * TIME_CORRECTION);
}

static inline uint32_t c(uint8_t r, uint8_t g, uint8_t b) {
  return leds.Color(r, g, b);
}

static void setAll(uint32_t color) {
  for (uint16_t i = 0; i < LEDS_TOTAL; i++) {
    leds.setPixelColor(i, color);
  }
}

static void setLevelAcrossStrips(uint16_t levelIndex0to55, uint32_t color) {
  for (uint16_t s = 0; s < STRIPS; s++) {
    const uint16_t idx = s * LEDS_PER_STRIP + levelIndex0to55;
    leds.setPixelColor(idx, color);
  }
}

static void renderCountdown(uint32_t elapsedMs) {
  const uint32_t totalMs = msFromMinutes(configuredMinutes);

  // Phase boundaries:
  // - Green: [0, greenEnd)
  // - Yellow overwrite: [greenEnd, yellowEnd)
  // - Red overwrite: [yellowEnd, totalMs)
  const uint32_t greenEnd = (totalMs > LAST_MINUTE_MS) ? (totalMs - LAST_MINUTE_MS) : 0;
  const uint32_t yellowEnd = (totalMs > RED_MS) ? (totalMs - RED_MS) : 0;

  // Base: everything off
  setAll(c(0, 0, 0));

  if (totalMs == 0) {
    return;
  }

  if (elapsedMs < greenEnd) {
    // Fill from bottom to top, proportional
    const float p = (greenEnd == 0) ? 1.0f : (float)elapsedMs / (float)greenEnd;
    uint16_t on = (uint16_t)(p * LEDS_PER_STRIP);
    if (on > LEDS_PER_STRIP) {
      on = LEDS_PER_STRIP;
    }
    // Wenn der Countdown gerade gestartet hat, mindestens eine LED anzeigen
    if (on == 0) {
      on = 1;
    }
    for (uint16_t i = 0; i < on; i++) {
      setLevelAcrossStrips(i, c(0, 255, 0));
    }
    return;
  }

  // From here on, green is fully filled (all 56 levels green)
  for (uint16_t i = 0; i < LEDS_PER_STRIP; i++) {
    setLevelAcrossStrips(i, c(0, 255, 0));
  }

  if (elapsedMs < yellowEnd) {
    const uint32_t t = elapsedMs - greenEnd;
    const float p = (YELLOW_MS == 0) ? 1.0f : (float)t / (float)YELLOW_MS;
    uint16_t on = (uint16_t)(p * LEDS_PER_STRIP);
    if (on > LEDS_PER_STRIP) {
      on = LEDS_PER_STRIP;
    }
    for (uint16_t i = 0; i < on; i++) {
      setLevelAcrossStrips(i, c(255, 255, 0));
    }
    return;
  }

  // Yellow is fully filled (overwrite all)
  for (uint16_t i = 0; i < LEDS_PER_STRIP; i++) {
    setLevelAcrossStrips(i, c(255, 255, 0));
  }

  // Red overwrite in last 10s
  const uint32_t t = (elapsedMs > yellowEnd) ? (elapsedMs - yellowEnd) : 0;
  const float p = (RED_MS == 0) ? 1.0f : (float)t / (float)RED_MS;
  uint16_t on = (uint16_t)(p * LEDS_PER_STRIP);
  if (on > LEDS_PER_STRIP) {
    on = LEDS_PER_STRIP;
  }
  for (uint16_t i = 0; i < on; i++) {
    setLevelAcrossStrips(i, c(255, 0, 0));
  }
}

static void resetLamp() {
  mode = Mode::Idle;
  setAll(c(0, 0, 0));
  leds.show();
}

static void startCountdown() {
  mode = Mode::Running;
  startMs = millis();
}

static void setMinutesAndRestart(uint32_t minutes) {
  // Guardrails: allow 1..999 minutes
  configuredMinutes = (uint32_t)constrain((int)minutes, 1, 999);
  if (mode == Mode::Running) {
    startCountdown(); // restart from 0 with new duration
  }
}

static void handleBleInput() {
  static char cmdBuf[8];
  static uint8_t cmdLen = 0;

  while (ble.available()) {
    const char ch = (char)ble.read();
    // Zeilenende als Terminator für t-Befehle nutzen
    if (ch == '\r' || ch == '\n') {
      if (cmdLen >= 1 && cmdBuf[0] == 't') {
        if (cmdLen >= 2) {
          // tNN: Zeit setzen
          const uint32_t minutes = (uint32_t)atoi(cmdBuf + 1);
          if (minutes >= 1) {
            setMinutesAndRestart(minutes);
            ble.print(F("Timer: "));
            ble.print(minutes);
            ble.println(F(" minutes"));
          }
        } else {
          // Nur "t": aktuelle Zeit zurückmelden
          ble.print(F("Timer: "));
          ble.print(configuredMinutes);
          ble.println(F(" minutes"));
        }
      }
      cmdLen = 0;
      continue;
    }

    // Single-char commands
    if (ch == 's') {
      startCountdown();
      cmdLen = 0;
      ble.println(F("Started"));
      continue;
    }
    if (ch == 'r') {
      resetLamp();
      cmdLen = 0;
      ble.println(F("Stopped"));
      continue;
    }

    // Time command: tNN (1-3 digits). We accept digits after 't' until Zeilenende.
    if (ch == 't') {
      cmdLen = 0;
      cmdBuf[cmdLen++] = ch;
      continue;
    }

    if (cmdLen > 0 && cmdBuf[0] == 't') {
      if (ch >= '0' && ch <= '9') {
        if (cmdLen < sizeof(cmdBuf) - 1) {
          cmdBuf[cmdLen++] = ch;
          cmdBuf[cmdLen] = '\0';
        }
        continue;
      }
      // Nicht-Ziffer vor Zeilenende: Befehl verwerfen
      cmdLen = 0;
    }
  }
}

void setup() {
  leds.begin();
  leds.setBrightness(255);
  resetLamp();

  if (!ble.begin(VERBOSE_MODE)) {
    // If BLE init fails, we still keep LED timer functional (without remote control).
  } else {
    ble.echo(false);
    ble.setMode(BLUEFRUIT_MODE_DATA); // Bluefruit Connect "UART" pipes bytes in data mode
  }

  // Direkt nach dem Einschalten mit der Standarddauer starten
  startCountdown();
}

void loop() {
  handleBleInput();

  const uint32_t now = millis();
  const uint32_t totalMs = msFromMinutes(configuredMinutes);

  if (mode == Mode::Running) {
    const uint32_t elapsed = now - startMs;
    if (elapsed >= totalMs) {
      mode = Mode::BlinkExpired;
      blinkLastToggleMs = now;
      blinkOn = true;
    } else {
      // LEDs nur in begrenzter Frequenz aktualisieren, damit millis() korrekt läuft
      if (now - lastFrameMs >= 80) { // ca. 12–13 FPS
        lastFrameMs = now;
        renderCountdown(elapsed);
        leds.show();
      }
    }
  } else if (mode == Mode::BlinkExpired) {
    // Blink: red - off - red - off ...
    if (now - blinkLastToggleMs >= 500) {
      blinkLastToggleMs = now;
      blinkOn = !blinkOn;
      setAll(blinkOn ? c(255, 0, 0) : c(0, 0, 0));
      leds.show();
    }
  }
}


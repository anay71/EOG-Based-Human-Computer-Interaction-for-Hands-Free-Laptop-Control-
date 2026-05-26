/*
 * ============================================================
 * Eye-Controlled Laptop Interface — ESP32 Firmware (FIXED)
 * ============================================================
 */

#include <Arduino.h>

// ── Pin Definitions ────────────────────────────────────────
#define EOG_PIN         34    
#define LO_PLUS_PIN     32    
#define LO_MINUS_PIN    33    

// ── Sampling ───────────────────────────────────────────────
#define SAMPLE_RATE_HZ      256
#define SAMPLE_INTERVAL_US  (1000000 / SAMPLE_RATE_HZ)

// ── Thresholds — CALIBRATE THESE AFTER SEEING THE SPARKS ───
#define BLINK_UPPER_THRESHOLD   2000    // Lowered slightly to be more sensitive
#define BLINK_LOWER_THRESHOLD   1600    
#define BASELINE_VALUE          2048    
#define MOVE_THRESHOLD_HIGH     1900    
#define MOVE_THRESHOLD_LOW      1200    

// ── Timing Parameters ──────────────────────────────────────
#define BLINK_MIN_MS            30      // Slightly shorter for faster blinks
#define BLINK_MAX_MS            400     
#define DOUBLE_BLINK_WINDOW_MS  600
#define TRIPLE_BLINK_WINDOW_MS  1000
#define BLINK_COOLDOWN_MS       150     
#define MOVE_SUSTAIN_MS         120
#define MOVE_RETRIGGER_MS       600

// ── Debug Mode ─────────────────────────────────────────────
// #define DEBUG_MODE // Ensure this is active to see signals in Plotter

// ── State Variables ────────────────────────────────────────
bool leadOffPrinted = false; 
unsigned long lastSampleUs = 0;
unsigned long blinkStartMs = 0;
unsigned long lastBlinkMs  = 0;
int  blinkCount = 0;
bool inBlink    = false;

#define ENVELOPE_WINDOW 16
int   envBuf[ENVELOPE_WINDOW] = {0};
int   envIdx   = 0;
float dynamicBaseline = (float)BASELINE_VALUE;
float filteredADC     = (float)BASELINE_VALUE;

unsigned long moveHighStart = 0;
unsigned long moveLowStart  = 0;
bool inMoveHigh = false;
bool inMoveLow  = false;

// ── IIR Low-Pass Filter ────────────────────────────────────
float iirLowPass(float newVal, float prevVal, float alpha) {
  return alpha * newVal + (1.0f - alpha) * prevVal;
}

// ── Rolling Envelope ───────────────────────────────────────
float computeEnvelope(int fADC) {
  envBuf[envIdx] = abs(fADC - (int)dynamicBaseline);
  envIdx = (envIdx + 1) % ENVELOPE_WINDOW;
  long sum = 0;
  for (int i = 0; i < ENVELOPE_WINDOW; i++) sum += envBuf[i];
  return (float)sum / ENVELOPE_WINDOW;
}

void sendBlinkEvent(int count) {
  if      (count == 1) Serial.println("BLINK:SINGLE");
  else if (count == 2) Serial.println("BLINK:DOUBLE");
  else if (count >= 3) Serial.println("BLINK:TRIPLE");
}

// ── Process One ADC Sample ─────────────────────────────────
void processSample(int rawADC) {
  unsigned long nowMs = millis();

  // FIX 1: Increase alpha (0.15f -> 0.45f). 
  // A higher alpha allows sharp "sparks" to pass through the filter.
  filteredADC = iirLowPass((float)rawADC, filteredADC, 0.45f);
  int fADC = (int)filteredADC;

  // Very slow baseline tracking
  dynamicBaseline = iirLowPass((float)rawADC, dynamicBaseline, 0.001f);

  // FIX 2: Ensure proper formatting for Arduino Serial Plotter
  #ifdef DEBUG_MODE
    Serial.print("Raw:");      Serial.print(rawADC);
    Serial.print(",");
    Serial.print("Filtered:"); Serial.print(fADC);
    Serial.print(",");
    Serial.print("Baseline:"); Serial.println((int)dynamicBaseline);
    return; // Skip event logic in debug mode to keep timing clean
  #endif

  // --- Detection Logic (Only runs when DEBUG_MODE is commented out) ---
  bool inCooldown = ((nowMs - lastBlinkMs) < BLINK_COOLDOWN_MS);
  if (!inCooldown) {
    if (!inBlink && fADC > BLINK_UPPER_THRESHOLD) {
      inBlink = true;
      blinkStartMs = nowMs;
    }
    if (inBlink && fADC < BLINK_LOWER_THRESHOLD) {
      unsigned long dur = nowMs - blinkStartMs;
      if (dur >= BLINK_MIN_MS && dur <= BLINK_MAX_MS) {
        blinkCount++;
        lastBlinkMs = nowMs;
      }
      inBlink = false;
    }
  }

  if (blinkCount > 0) {
    unsigned long msSinceLast = nowMs - lastBlinkMs;
    if (msSinceLast > DOUBLE_BLINK_WINDOW_MS) {
      sendBlinkEvent(blinkCount);
      blinkCount = 0;
    }
  }

  if (fADC > MOVE_THRESHOLD_HIGH && !inBlink) {
    if (!inMoveHigh) { inMoveHigh = true; moveHighStart = nowMs; }
    else if ((nowMs - moveHighStart) > MOVE_SUSTAIN_MS) {
      Serial.println("MOVE:RIGHT");
      moveHighStart = nowMs + MOVE_RETRIGGER_MS;
    }
  } else { inMoveHigh = false; }

  if (fADC < MOVE_THRESHOLD_LOW && !inBlink) {
    if (!inMoveLow) { inMoveLow = true; moveLowStart = nowMs; }
    else if ((nowMs - moveLowStart) > MOVE_SUSTAIN_MS) {
      Serial.println("MOVE:LEFT");
      moveLowStart = nowMs + MOVE_RETRIGGER_MS;
    }
  } else { inMoveLow = false; }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(LO_PLUS_PIN,  INPUT);
  pinMode(LO_MINUS_PIN, INPUT);
  delay(500);
  Serial.println("STATUS:READY");
}

void loop() {
  unsigned long nowUs = micros();
  if ((nowUs - lastSampleUs) >= SAMPLE_INTERVAL_US) {
    lastSampleUs = nowUs;

    if (digitalRead(LO_PLUS_PIN) || digitalRead(LO_MINUS_PIN)) {
      if (!leadOffPrinted) {
        Serial.println("STATUS:LEAD_OFF");
        leadOffPrinted = true;
      }
      return; 
    }

    if (leadOffPrinted) {
      Serial.println("STATUS:RESTORED");
      leadOffPrinted = false;
    }

    processSample(analogRead(EOG_PIN));
  }
}
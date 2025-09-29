// LineLight-1: Single-lamp, band-limited LED level from line audio
// Target: Arduino Pro Micro (ATmega32u4, 5V/16MHz)
//
// Signal path summary:
//   Audio L/R -> passive sum -> op-amp buffer -> AC couple -> VCC/2 bias -> ADC (A0)
//   Two pots (A1/A2) select FFT band [bin_lo..bin_hi]; energy drives PWM on pin 9 (OC1A)
//
// NOTE: This is a minimal, production-leaning prototype: polling sampler + FFT.
//       Upgrade sampling to a timer ISR after validating analog front-end.

#include <Arduino.h>
#include <arduinoFFT.h>

// ---------- Pins (adjust to your wiring) ----------
static const uint8_t PIN_AUDIO = A0;   // biased at VCC/2
static const uint8_t PIN_POT1  = A1;   // low edge selector
static const uint8_t PIN_POT2  = A2;   // high edge selector
static const uint8_t PIN_PWM   = 9;    // OC1A on 32u4 (LED lamp MOSFET gate)

// ---------- FFT setup ----------
static const uint16_t N  = 256;        // FFT size
static const float    Fs = 9600.0f;    // sampling rate (Hz)
static const uint32_t SAMPLE_DT_US = (uint32_t)(1000000.0f / Fs + 0.5f);

arduinoFFT FFT;
double vReal[N];
double vImag[N];

// Hann window (computed at startup)
double hann[N];

// AGC / smoothing
double ema = 0.0;
double agc = 1.0;

// Forward declarations
void setupFastPWM31kHz();
inline void pwmWrite(uint8_t d) { OCR1A = d; }  // direct write (Timer1, OC1A -> D9)

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PWM, OUTPUT);
  analogReadResolution(10);              // 0..1023
  analogReference(DEFAULT);              // 5V reference

  // Precompute Hann
  for (uint16_t n=0; n<N; ++n) {
    hann[n] = 0.5 * (1.0 - cos(2.0 * PI * n / (N - 1)));
  }

  setupFastPWM31kHz();
  pwmWrite(0);

  // Small boot banner
  delay(50);
  Serial.println(F("LineLight-1 boot"));
  Serial.print(F("Fs=")); Serial.println(Fs);
  Serial.print(F("N="));  Serial.println(N);
}

void loop() {
  // --- Acquire N samples with crude timing control ---
  uint32_t tNext = micros();
  for (uint16_t i=0; i<N; ++i) {
    // Simple sample-time control; avoids interrupts for first proto
    while ((int32_t)(micros() - tNext) < 0) { /* spin */ }
    tNext += SAMPLE_DT_US;

    int raw = analogRead(PIN_AUDIO);  // 0..1023, ~512 mid (biased)
    double centered = (double)raw - 512.0; // center around 0
    // Window
    vReal[i] = centered * hann[i];
    vImag[i] = 0.0;
  }

  // --- FFT ---
  FFT.Windowing(vReal, N, FFT_WIN_TYP_RECTANGLE, FFT_FORWARD); // window already applied
  FFT.Compute(vReal, vImag, N, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, N);

  // --- Read pots and map to bins ---
  int p1 = analogRead(PIN_POT1);  // 0..1023
  int p2 = analogRead(PIN_POT2);
  uint16_t nyq = (N/2) - 1;
  uint16_t binLo = map(p1, 0, 1023, 1, nyq-1);
  uint16_t binHi = map(p2, 0, 1023, binLo+1, nyq);
  if (binHi <= binLo) binHi = binLo + 1;

  // --- Band energy ---
  double e = 0.0;
  for (uint16_t b = binLo; b <= binHi; ++b) {
    double m = vReal[b];
    e += m * m;
  }

  // Normalize by number of bins for consistent feel
  e /= (double)(binHi - binLo + 1);

  // --- AGC + smoothing ---
  const double alpha  = 0.2;  // EMA factor (~10–50 ms feel depending on Fs/N loop)
  const double target = 0.35; // desired normalized level
  ema = (1.0 - alpha) * ema + alpha * e;
  // Nudge agc to keep ema*agc near target
  double err = target - min(1.0, ema * agc);
  agc *= (1.0 + 0.015 * err);
  agc = constrain(agc, 0.05, 200.0);

  // Compress, gamma-correct, scale to 8-bit PWM
  double level = sqrt(ema * agc);            // soft comp
  level = constrain(level, 0.0, 1.0);
  const double gamma = 2.0;
  uint8_t duty = (uint8_t)(pow(level, gamma) * 255.0 + 0.5);
  pwmWrite(duty);

  // --- Debug once per second ---
  static uint32_t tLast = 0;
  if (millis() - tLast > 1000) {
    tLast = millis();
    double binWidth = Fs / (double)N;
    Serial.print(F("bins ")); Serial.print(binLo); Serial.print('-'); Serial.print(binHi);
    Serial.print(F(" (")); Serial.print(binLo*binWidth,0); Serial.print('-'); Serial.print(binHi*binWidth,0); Serial.print(F(" Hz) "));
    Serial.print(F("level ")); Serial.print(level,3);
    Serial.print(F("  agc ")); Serial.print(agc,3);
    Serial.print(F("  ema ")); Serial.println(ema,3);
  }
}

// Configure Timer1 for ~31 kHz PWM on OC1A (D9) using phase-correct 8-bit
void setupFastPWM31kHz() {
  // Disconnect analogWrite from Timer1 on pin 9 if previously set
  pinMode(PIN_PWM, OUTPUT);

  // Clear control registers
  TCCR1A = 0;
  TCCR1B = 0;

  // Phase Correct PWM 8-bit: WGM10=1
  TCCR1A |= _BV(WGM10);
  // No prescaling: CS10=1
  TCCR1B |= _BV(CS10);
  // Clear OC1A on up-count compare, set on down-count compare (non-inverting)
  TCCR1A |= _BV(COM1A1);

  // Start with 0 duty
  OCR1A = 0;
}

// LineLight-1: Single-lamp, band-limited LED level from line audio
// Target: SparkFun Pro Mini (ATmega328P, 5V/16MHz) flashed via FTDI Basic
//
// This file intentionally reads like a tour guide. Every stage is annotated so you can hand it to a
// manufacturing test tech, a firmware intern, or future-you and they all get the "why" as well as the "what".
// Nothing here is "magic"—the punk ethos is transparency.

#include <Arduino.h>
#include <arduinoFFT.h>

// -----------------------------------------------------------------------------
// Pin map — keep the numbers here so harness diagrams and firmware match.
// -----------------------------------------------------------------------------
static const uint8_t PIN_AUDIO = A0;   // Audio feed, AC-coupled and biased around VCC/2.
static const uint8_t PIN_POT1  = A1;   // Potentiometer selecting the low edge of the FFT band.
static const uint8_t PIN_POT2  = A2;   // Potentiometer selecting the high edge of the FFT band.
static const uint8_t PIN_PWM   = 9;    // Timer1 OC1A output → MOSFET gate driving the lamp.

// -----------------------------------------------------------------------------
// FFT / DSP constants — tuned for a 9.6 kHz sample rate which plays nicely with the loop timing.
// -----------------------------------------------------------------------------
static const uint16_t FFT_BIN_COUNT = 128;            // 128 keeps the double buffers under 2 KB SRAM on the Pro Mini.
static const float    SAMPLE_RATE_HZ = 9600.0f;       // Set by our crude spin-wait sampler.
static const uint32_t SAMPLE_PERIOD_US =              // Microseconds between samples.
    static_cast<uint32_t>(1000000.0f / SAMPLE_RATE_HZ + 0.5f);
static const uint16_t ADC_BIAS = 512;                 // 10-bit ADC mid-point after bias network.

// -----------------------------------------------------------------------------
// AGC / smoothing constants — values chosen by ear for a smooth, musical response.
// -----------------------------------------------------------------------------
static const double EMA_ALPHA     = 0.20;   // Weight of the current FFT frame in the exponential moving average.
static const double TARGET_LEVEL  = 0.35;   // Normalized level we try to hover around after AGC.
static const double AGC_STEP      = 0.015;  // How aggressively the AGC reacts to error.
static const double AGC_MIN_GAIN  = 0.05;   // Safety floor to prevent locking up on silence.
static const double AGC_MAX_GAIN  = 200.0;  // Safety ceiling to avoid numeric blow-ups.
static const double PWM_GAMMA     = 2.0;    // Gamma curve to tame perceived brightness steps.

// -----------------------------------------------------------------------------
// Working buffers — global to avoid stack thrash.
// -----------------------------------------------------------------------------
static arduinoFFT FFT;
static double vReal[FFT_BIN_COUNT];
static double vImag[FFT_BIN_COUNT];
static double hannWindow[FFT_BIN_COUNT];

// -----------------------------------------------------------------------------
// State for the slow-control loops.
// -----------------------------------------------------------------------------
static double emaEnergy = 0.0;   // Exponential moving average of band energy.
static double agcGain   = 1.0;   // Adaptive gain multiplier.

// -----------------------------------------------------------------------------
// Utility prototypes.
// -----------------------------------------------------------------------------
void setupFastPWM31kHz();
inline void pwmWrite(uint8_t duty) { OCR1A = duty; }

void primeHannWindow();
void acquireWindowedSamples();
void performFft();

struct BandSelection {
  uint16_t binLo;
  uint16_t binHi;
};
BandSelection readBandSelection();
double computeBandEnergy(uint16_t binLo, uint16_t binHi);
double normalizeEnergy(double rawEnergy);
uint8_t renderDutyFromLevel(double level);
void logDebugOncePerSecond(const BandSelection& band, double level);

// -----------------------------------------------------------------------------
// setup() — Configure serial, ADC expectations, PWM hardware, and precompute the Hann window.
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_PWM, OUTPUT);

  // AVR analogRead is always 10-bit, but calling this documents intent when ported.
#if defined(analogReadResolution)
  analogReadResolution(10);
#endif
  analogReference(DEFAULT);  // Using VCC (5 V) as reference; matches the bias divider assumption.

  primeHannWindow();
  setupFastPWM31kHz();
  pwmWrite(0);  // Guarantee the lamp is off during boot.

  delay(50);  // Let the FTDI Basic and host terminal settle before spitting logs.
  Serial.println(F("LineLight-1 boot"));
  Serial.print(F("Fs=")); Serial.println(SAMPLE_RATE_HZ);
  Serial.print(F("N="));  Serial.println(FFT_BIN_COUNT);
}

// -----------------------------------------------------------------------------
// loop() — One pass = sample → FFT → band energy → AGC → PWM update → optional debug log.
// -----------------------------------------------------------------------------
void loop() {
  acquireWindowedSamples();      // 1. Pull N samples into vReal/vImag with Hann applied.
  performFft();                  // 2. Transform into magnitude space.
  BandSelection band = readBandSelection();
  double bandEnergy = computeBandEnergy(band.binLo, band.binHi);
  double normalized = normalizeEnergy(bandEnergy);
  uint8_t duty = renderDutyFromLevel(normalized);
  pwmWrite(duty);
  logDebugOncePerSecond(band, normalized);
}

// -----------------------------------------------------------------------------
// primeHannWindow() — Precompute Hann coefficients so each frame just multiplies.
// -----------------------------------------------------------------------------
void primeHannWindow() {
  for (uint16_t n = 0; n < FFT_BIN_COUNT; ++n) {
    const double phase = (2.0 * PI * n) / (FFT_BIN_COUNT - 1);
    hannWindow[n] = 0.5 * (1.0 - cos(phase));
  }
}

// -----------------------------------------------------------------------------
// acquireWindowedSamples() — Spin-wait sampling keeps phase predictable without ISR complexity.
// -----------------------------------------------------------------------------
void acquireWindowedSamples() {
  uint32_t nextSampleMicros = micros();
  for (uint16_t i = 0; i < FFT_BIN_COUNT; ++i) {
    // Busy-wait until the planned sample time. On 16 MHz AVR this lands within a few CPU cycles.
    while (static_cast<int32_t>(micros() - nextSampleMicros) < 0) {
      // Intentional empty body: we want deterministic timing without interrupts for v0.1.
    }
    nextSampleMicros += SAMPLE_PERIOD_US;

    int raw = analogRead(PIN_AUDIO);                 // 0..1023, biased around ~512.
    double centered = static_cast<double>(raw) - ADC_BIAS;  // Remove the DC bias from the buffer.
    vReal[i] = centered * hannWindow[i];              // Apply window to reduce spectral leakage.
    vImag[i] = 0.0;                                   // Start with zero imaginary part.
  }
}

// -----------------------------------------------------------------------------
// performFft() — Classic FFT dance: window (already done) → compute → magnitude.
// -----------------------------------------------------------------------------
void performFft() {
  FFT.Windowing(vReal, FFT_BIN_COUNT, FFT_WIN_TYP_RECTANGLE, FFT_FORWARD);  // Library expects a call even if windowed.
  FFT.Compute(vReal, vImag, FFT_BIN_COUNT, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, FFT_BIN_COUNT);
}

// -----------------------------------------------------------------------------
// readBandSelection() — Convert pot voltages into FFT bin indices, honoring Nyquist limits.
// -----------------------------------------------------------------------------
BandSelection readBandSelection() {
  const uint16_t nyquistBin = (FFT_BIN_COUNT / 2) - 1;  // Ignore mirrored bins.

  int pot1 = analogRead(PIN_POT1);  // 0..1023
  int pot2 = analogRead(PIN_POT2);

  // Convert to floating point ratios for smoother scaling than Arduino's integer map().
  double loRatio = constrain(static_cast<double>(pot1) / 1023.0, 0.0, 1.0);
  double hiRatio = constrain(static_cast<double>(pot2) / 1023.0, 0.0, 1.0);

  uint16_t binLo = 1 + static_cast<uint16_t>(loRatio * (nyquistBin - 2));
  uint16_t binHi = binLo + 1 + static_cast<uint16_t>(hiRatio * (nyquistBin - binLo - 1));
  binHi = constrain(binHi, static_cast<uint16_t>(binLo + 1), nyquistBin);

  return {binLo, binHi};
}

// -----------------------------------------------------------------------------
// computeBandEnergy() — Sum magnitudes squared within the chosen band and average them.
// -----------------------------------------------------------------------------
double computeBandEnergy(uint16_t binLo, uint16_t binHi) {
  double accumulator = 0.0;
  for (uint16_t bin = binLo; bin <= binHi; ++bin) {
    double magnitude = vReal[bin];  // vReal holds magnitudes after ComplexToMagnitude().
    accumulator += magnitude * magnitude;
  }

  const uint16_t binCount = (binHi - binLo + 1);
  return accumulator / static_cast<double>(binCount);
}

// -----------------------------------------------------------------------------
// normalizeEnergy() — Apply smoothing + AGC so LED motion feels organic instead of twitchy.
// -----------------------------------------------------------------------------
double normalizeEnergy(double rawEnergy) {
  // Update exponential moving average to keep short bursts from jittering the light.
  emaEnergy = (1.0 - EMA_ALPHA) * emaEnergy + EMA_ALPHA * rawEnergy;

  // Compute error between where we are and where we want to be, then tweak the gain.
  double normalizedMeasure = min(1.0, emaEnergy * agcGain);
  double error = TARGET_LEVEL - normalizedMeasure;
  agcGain *= (1.0 + AGC_STEP * error);
  agcGain = constrain(agcGain, AGC_MIN_GAIN, AGC_MAX_GAIN);

  // Return the post-AGC level in the 0..1 range, squared for mild compression.
  double leveled = sqrt(emaEnergy * agcGain);
  return constrain(leveled, 0.0, 1.0);
}

// -----------------------------------------------------------------------------
// renderDutyFromLevel() — Gamma-correct the normalized level and map to an 8-bit PWM duty.
// -----------------------------------------------------------------------------
uint8_t renderDutyFromLevel(double level) {
  double gammaCorrected = pow(level, PWM_GAMMA);
  gammaCorrected = constrain(gammaCorrected, 0.0, 1.0);
  return static_cast<uint8_t>(gammaCorrected * 255.0 + 0.5);
}

// -----------------------------------------------------------------------------
// logDebugOncePerSecond() — Print bin edges, their rough Hz equivalents, and the AGC state.
// -----------------------------------------------------------------------------
void logDebugOncePerSecond(const BandSelection& band, double level) {
  static uint32_t lastLogMs = 0;
  if (millis() - lastLogMs < 1000) {
    return;
  }
  lastLogMs = millis();

  const double binWidthHz = SAMPLE_RATE_HZ / static_cast<double>(FFT_BIN_COUNT);
  const double loHz = band.binLo * binWidthHz;
  const double hiHz = band.binHi * binWidthHz;

  Serial.print(F("bins "));
  Serial.print(band.binLo);
  Serial.print('-');
  Serial.print(band.binHi);
  Serial.print(F(" ("));
  Serial.print(loHz, 0);
  Serial.print('-');
  Serial.print(hiHz, 0);
  Serial.print(F(" Hz) "));
  Serial.print(F("level "));
  Serial.print(level, 3);
  Serial.print(F("  agc "));
  Serial.print(agcGain, 3);
  Serial.print(F("  ema "));
  Serial.println(emaEnergy, 3);
}

// -----------------------------------------------------------------------------
// setupFastPWM31kHz() — Configure Timer1 for high-frequency PWM on OC1A (pin 9).
// -----------------------------------------------------------------------------
void setupFastPWM31kHz() {
  pinMode(PIN_PWM, OUTPUT);  // Ensure pin is driven by Timer1 hardware.

  // Reset control registers before configuring. This avoids ghosts from Arduino's analogWrite().
  TCCR1A = 0;
  TCCR1B = 0;

  // Waveform Generation Mode: Phase Correct PWM, 8-bit (WGM10 = 1).
  TCCR1A |= _BV(WGM10);

  // Clock select: no prescaler (CS10 = 1). 16 MHz / 510 ≈ 31.37 kHz PWM frequency.
  TCCR1B |= _BV(CS10);

  // Compare Output Mode: non-inverting on OC1A so 0 = off, 255 = full-on.
  TCCR1A |= _BV(COM1A1);

  OCR1A = 0;  // Start with the lamp off.
}

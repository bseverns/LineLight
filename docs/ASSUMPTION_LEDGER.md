# ASSUMPTION LEDGER

- Typical input: line‑level (≈ −10 dBV to +4 dBu). v0.1 verified −20..+4 dBu.
- Single‑ended, unbalanced I/O for v0.1; balanced/isolated variants planned.
- PWM at ~31 kHz is above most audible bands and shouldn’t leak into audio with proper layout.
- Pots are linear 10–50 k; mapped directly to FFT bins with a small safety margin.
- FFT size 256 @ 9.6 kHz; bin width ≈ 37.5 Hz. This is adequate for “area” cues (kick/sibilance), not for surgical bands.

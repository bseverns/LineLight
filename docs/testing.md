# Testing

- **Noise floor**: with inputs shorted, lamp should remain near‑off; duty < 3/255.
- **Hum susceptibility**: test with grounded and floating source; check for 50/60 Hz sensitivity.
- **Through integrity**: null test by splitting the source; ensure buffer is unity and pops are acceptable on power‑cycle.
- **PWM leakage**: listen for whine; verify Timer1 ~31 kHz on D9 with a scope if possible.

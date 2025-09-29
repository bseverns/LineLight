# Build Guide

This is the quickest path to a reliable prototype. Keep leads short and star‑ground power returns.

## 1) Parts
- SparkFun Pro Mini (5 V, 16 MHz, ATmega328P)
- SparkFun FTDI Basic (5 V) or equivalent 6-pin USB‑serial adapter
- MCP6002 (dual RRIO op‑amp) + socket
- TRS jacks (in/out) or 3.5 mm equivalents
- Film cap 1 µF, ceramics 1 nF, electrolytics 10–100 µF
- Resistors: 1 k, 10 k, 100 k (1% where possible)
- BAT54S (Schottky dual) or two BAT54
- MOSFET (AO3400/IRLZ series), 100 Ω gate, 100 k pulldown
- Buck 12→5 V (≥1 A) and 12 V LED lamp

## 2) Analog porch wiring
```
L in ----1k----+
               +---- node SUM ----10k---- GND
R in ----1k----+

SUM -> MCP6002(A) buffer (gain=1) ->
  -> (through out) via 100Ω series to OUT jack tip
  -> 1µF (film) -> VCC/2 bias (100k to 5V, 100k to GND) -> 10k -> ADC A0
                                     |-> 1nF to GND at ADC pin
                                     |-> BAT54S clamps to 0/5V
```

Notes:
- Keep the **ADC node** compact. Place the 1 nF and clamps right at the MCU pin.
- Tie sleeve/ground of the IN/OUT jacks to analog ground near the buffer.
- Optionally add a **1:1 transformer** between buffer and OUT to break loops.

## 3) Pots
- POT1 (low edge): wiper → A1, ends → 5 V and GND via 10 k–50 k linear.
- POT2 (high edge): wiper → A2, ends → 5 V and GND.
- Mount on panel; route with shielded pairs if long.

## 4) Lamp driver
- D9 → 100 Ω → MOSFET gate; 100 k gate pulldown.
- LED lamp between +12 V and MOSFET drain; MOSFET source → GND.
- Add 100 µF reservoir on 12 V near the lamp connector.

## 5) Power
- 12 V DC input → buck to 5 V (MCU + analog). Star‑ground at power entry.
- Add a TVS (e.g., SMAJ18A) on 12 V if you expect hot‑plugging.

## 6) Flash & test
- Wire the FTDI Basic: DTR→RESET, TXO→RXI, RXI→TXO, VCC→VCC (5 V), CTS→GND.
- `pio run -t upload`
- Open serial monitor @115200. You should see bin edges and levels once per second.
- Sweep the knobs and watch the frequency readout change.

## 7) Enclosure
- Hammond 1591 class or laser‑cut acrylic sandwich. Keep audio jacks far from buck.
- Consider a service toggle that hard‑bypasses IN→OUT when unpowered (relay or DPDT).

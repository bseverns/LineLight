# Schematic crib notes

This folder will hold the KiCad/EasyEDA sources. Until the official schematic drops, use the annotated map below to keep the bench wiring honest.

```mermaid
flowchart TD
    INL["Line In L"] --> SUM
    INR["Line In R"] --> SUM
    SUM["1k/1k Passive Summer"] --> BUF["MCP6002 Buffer (A)"]
    BUF --> THRU["Line Through Out"]
    BUF --> HPF["AC Coupler 1 µF"] --> BIAS["VCC/2 Bias Node"] --> RC["10k//1 nF RC"] --> ADC["ATmega328P A0"]
    ADC --> CLAMP["BAT54 Clamp"]

    POT1["POT1 → A1"] --> MCU
    POT2["POT2 → A2"] --> MCU
    MCU["ATmega328P (Pro Mini)"] --> GATE["100 Ω Gate"] --> FET["Logic MOSFET"] --> LAMP["12 V LED Fixture"]
    PSU["12 V DC"] -->{feeds}
    {feeds} --> LAMP
    {feeds} --> BUCK["Buck 12→5 V"] --> MCU
```

Recommended symbols/footprints:
- Audio TRS jacks with switched normals (if you add mute/bypass).
- MCP6002 in SOIC‑8 with DIP‑8 footprint option (adapter) for easy replacement.
- Test pads: SUM, ADC, 5V, GND, D9.
- SparkFun-style 6-pin FTDI header (DTR, TXO, RXI, VCC, CTS, GND) near the MCU edge.

Production sanity checks:
- Keep the ADC node <5 mm trace length and guard it with ground pour.
- Drop TVS diodes near the 12 V entry and the lamp connector if you expect abuse.
- If you spin a 4-layer PCB, dedicate an inner plane to analog ground and stitch it at the buffer and MCU references.
- Bring the FTDI header outboard so you can flash the Pro Mini on an assembled unit without opening the enclosure fully.

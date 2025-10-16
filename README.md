# LineLight-1 Production Kit

Welcome to the gutsy little repo that makes a band-responsive lamp glow on cue. You're in the right place if you want to build, audit, or remix LineLight-1 for production deployment and some learning.

## How to Use This Repo
- **Need the concept sketch?** Hit [`docs/README.md`](docs/README.md) for the full story, signal path, and ethics.
- **Building hardware tonight?** Jump to [`docs/build-guide.md`](docs/build-guide.md), then stroll through [`hardware/`](hardware/).
- **Flashing firmware?** The PlatformIO project lives under [`firmware/`](firmware/). Clip a SparkFun FTDI Basic (5 V) onto the
  Pro Mini header, then `pio run -t upload` and you’re moshing.
- **Coordinating production runs?** Check the BOM notes in [`bom/`](bom/) and the manufacturing/testing briefs in [`docs/manufacturing.md`](docs/manufacturing.md) and [`docs/testing.md`](docs/testing.md).

## Production-lean Defaults
- Firmware is tuned for a release build (`-Os`, LTO, dead-strip). See [`firmware/platformio.ini`](firmware/platformio.ini).
- Tooling assumes the **FTDI-programmed SparkFun Pro Mini (5 V/16 MHz)**—no onboard USB, so route the 6-pin header.
- Documentation now includes subsystem diagrams so your fabricator, firmware hacker, and QA engineer speak the same language.
- Every sketch, schematic, and README is annotated like a zine—follow the breadcrumbs, learn the intent, remix responsibly.

## What Changed Recently?
- Added architecture diagrams to teach the signal flow at a glance.
- Expanded inline comments so future you knows **why** each design choice exists.
- Tightened the build flags for a quieter, cooler-running microcontroller.

Now go build something that glows with purpose.

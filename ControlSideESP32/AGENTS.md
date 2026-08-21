# ControlSideESP32 Agent Guide

## Project scope

- This is PlatformIO firmware for an ESP32-S3-N16R8 target, currently configured with the `esp32-s3-devkitc-1` board and the ESP-IDF framework in [platformio.ini](platformio.ini).
- The intended device bridges two input paths: ADC-derived digital PWM signals from two Hall joysticks and a toggle switch into an ELRS uplink to the flight controller; it also receives PWM output from the flight controller and forwards it over Bluetooth or another wireless link to a second ESP32.
- The repository is currently a PlatformIO skeleton. Do not assume that the described protocol, pin map, radio module, or board variant is already implemented.

## Working rules

- Use ESP-IDF APIs and PlatformIO configuration already selected by the project. Prefer ESP-IDF peripherals, FreeRTOS tasks/queues, and driver interfaces over Arduino abstractions unless the project explicitly adds Arduino support.
- Treat ADC, PWM, ELRS, and the inter-ESP wireless transport as separate modules with explicit data types and conversion boundaries. Keep hardware sampling, protocol encoding/decoding, and transport I/O independently testable.
- Before changing GPIO assignments or timing, verify the ESP32-S3-N16R8 board schematic/pinout and record the decision in project documentation or the relevant header. Never infer pins from a generic DevKit name.
- Preserve signal units and timing metadata at module boundaries. Document ADC/PWM scaling, channel order, failsafe values, update rates, packet framing, and timeout behavior before implementing or changing a link.
- Do not hand-wave ELRS compatibility or invent packet layouts. Reuse a checked-in implementation or a pinned, reviewed dependency; otherwise stop at a clearly named interface and identify the missing protocol source.
- Make loss-of-signal behavior explicit and fail safe. Validate packet lengths, channel ranges, checksums, sequence/timeout state, and malformed or stale input before applying outputs.
- Avoid blocking work in timing-sensitive sampling or radio paths. Use bounded queues and explicit ownership for buffers; report queue overflow, invalid frames, and watchdog-relevant errors through the project logging mechanism.
- Keep secrets, pairing credentials, and device identifiers out of source control. Put board-specific values in documented configuration or ignored local settings where appropriate.
- Keep changes focused. Do not add libraries, change the board, or switch frameworks without recording why and verifying the resulting build.

## Validation

Run these commands from the directory containing [platformio.ini](platformio.ini):

```text
pio run
pio test
pio run -t upload
pio device monitor
```

- Use `pio run` after every firmware change. Use `pio test` for pure packet, scaling, timeout, and failsafe logic before relying on hardware.
- Hardware validation must include ADC/PWM calibration, channel ordering, ELRS link loss, flight-controller PWM loss, wireless reconnects, and power-up/reset behavior.
- Do not claim hardware behavior from a compile-only check. Record the board, wiring, firmware configuration, and observed timing when reporting hardware results.

## Repository layout

- `src/`: application and component implementation; currently empty.
- `include/`: shared public headers; keep declarations here only when shared across source modules.
- `lib/`: project-local libraries; add a library only when its ownership and dependency boundary are clear.
- `test/`: PlatformIO tests and host-testable protocol logic; currently contains only the template README.

## Documentation and decisions

- Update this guide only for conventions that affect future agent work. Put detailed wiring, calibration tables, protocol references, and hardware test results in dedicated project documentation when those files are introduced.
- When requirements are ambiguous, preserve the existing interface and ask for the missing board pinout, ELRS transport/module details, wireless protocol choice, or failsafe policy rather than silently choosing one.
# Embedded deployment notes

`radar_core` is written for fixed `tick(dt)` — suitable for bare-metal or FreeRTOS.

## Suggested MCU flow

1. Timer ISR every 10–50 ms → `RadarEngine::tick`
2. Sweep azimuth from stepper or phased-array index register
3. ADC DMA buffer → peak detect → `Detection` list
4. Draw PPI on SPI display (ST7789) using polar pixel plot, or send tracks on CAN

## Memory

- Keep `tracks_` cap (e.g. 32 targets) on embedded
- Avoid `std::string` in hot path; use `char callsign[8]`

## Build

```bash
cmake -DRADAR_BUILD_GRAPHICS=OFF -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake ..
```

Provide your toolchain file; only `radar_engine.cpp` + `sensor_feed` implementation are required.

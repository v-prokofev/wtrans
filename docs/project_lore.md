# Project Lore - DVU-01 to 4-20mA Converter

## Rules and Guidelines
1. **Source of Truth**: All critical technical decisions, pinouts, and logic rules must be documented in `docs/project_spec.md` or `docs/project_lore.md`. 
2. **Persistence**: Any new implementation details or architectural shifts decided during the conversation must be recorded back into these files.
3. **HAL usage**: The project now uses STM32 HAL (Hardware Abstraction Layer) for peripheral control.
4. **Code Structure**: Hardware-specific register manipulations must be isolated from the high-level business logic.
5. **USER CODE blocks**: All manual code MUST be written inside `/* USER CODE BEGIN ... */` and `/* USER CODE END ... */` blocks to persist through CubeMX regeneration.
6. **Workflow**: Always update hardware configuration in `.ioc` (CubeMX) first, then implement logic in code.
7. **Code Organization**: Prefer placing ISR callbacks and custom logic before the `main()` function (e.g., in `USER CODE BEGIN 0` or `USER CODE BEGIN PFP`).

## Key Decisions
- **[2026-04-27] Error Signaling**: Use NAMUR NE43 standard for error signaling.
    - Sensor Failure/Timeout = 3.5 mA (Low Error).
- **[2026-04-27] Range Mapping**:
    - Wind Speed: 0...60 m/s -> 4...20 mA.
    - Wind Direction: 0...360° -> 4...20 mA.
- **[2026-04-27] Update Rate**: Internal processing and sensor polling at 1 Hz.
- **[2026-04-27] Interface Mapping**:
    - X2 Connector (RS-485) -> Connected to Sensor (DVU-01). Controlled via UART2 + DMA1 Ch6. Pins: PA0(nRE), PA1(DE).
    - X3 Connector (RS-485) -> Debug/Configuration. Controlled via UART3 (115200 8N1). Pins: PB13(nRE), PB14(DE).
- **[2026-04-27] DMA Strategy**:
    - Use DMA for sensor data reception to avoid CPU overhead and missing characters during SPI transactions.
- **[2026-04-27] Documentation**:
    - Detailed hardware connection and loop testing (multimeter) guides are kept in `docs/connection_guide.md`.
- [2026-04-28] Timer Architecture:
    - **Low-priority Timer (TIM3)**: 5 Hz (200ms period). Handles heartbeat LED (PA9) and debug console output (UART3).
    - **High-priority Timer (TIM2)**: 1 Hz (1s period). Handles sensor polling (UART2).
    - **Optional DAC Update Timer**: If synchronization is critical, a separate timer or a faster tick will be used to update DAC outputs.

## Hardware Topology

### SPI Isolation via ADUM1401

The SPI bus between the STM32 and the DAC161S997 chips passes through an **ADUM1401** galvanic isolator.
Signal flow (MCU side → isolator → DAC side):

| Signal | MCU Pin | ADUM1401 Side-1 | ADUM1401 Side-2 | DAC Pin |
|--------|---------|-----------------|-----------------|---------|
| CS     | PB0 / PA4 | **pin 3** (VIA, input)  | **pin 14** (VOA, output) | ~CS |
| SCK    | PA5     | **pin 4** (VIB, input)  | **pin 13** (VOB, output) | SCLK    |
| MOSI   | PA7     | **pin 5** (VIC, input)  | **pin 12** (VOC, output) | SDI     |
| MISO   | PA6     | **pin 6** (VOD, output) | **pin 11** (VID, input)  | pin 8 (SDO) via 2Ω |
| VE1/VE2| —       | **pin 7** (tied to VDD1)| **pin 10** (tied to VDD2)| — |

**MISO channel direction**: Side-2 → Side-1 (DAC → MCU).
The ADUM1401 variant used must have the channel on pins 6/11 configured as **side-2 input → side-1 output**. This is correct for a standard SPI MISO path.

**2Ω resistor on SDO**: Series damping resistor between DAC SDO (pin 8) and ADUM pin 11. Standard practice to suppress ringing and protect the isolator input.

> ⚠️ **Known Issue (2026-05-25)**: `DAC_ReadStatus()` returns `0xFFFF` on both channels, indicating SPI frames are not reaching the DAC (or DAC SDO is not driving MISO).
> Possible causes:
> 1. DAC SDO is in Hi-Z when ~CS is inactive — ADUM output floats HIGH → 0xFF on MISO. The DAC161S997 SDO is only driven during an active ~CS transaction; between transactions it is Hi-Z. This is normal behaviour and does **not** indicate a wiring fault. The readback must be measured while CS is asserted.
> 2. ADUM1401 not powered (check AVDD1/AVDD2 on isolator).
> 3. Poor solder joint on MISO / SDO / ADUM pin 11 or 6.

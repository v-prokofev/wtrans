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
- **[2026-04-27] Update Rate**: Internal processing at 1 Hz. Sensor pushes data at 1 Hz via AMES.
- **[2026-06-16] Sensor Protocol: AMES mode (было M 1 — ограничение ~50 м/с)**:
    - Прибор больше не использует поллинг `M 1`. Теперь используется режим автоматической отправки AMES.
    - **Причина**: команда `M 1` не позволяла датчику передавать скорость выше ~50 м/с.
    - **Последовательность инициализации**: `@1 AMES 0 0` → `OPEN 1` → `VER` → `@1 AMES 0 1`
    - ⚠️ **[2026-06-22] ИСПРАВЛЕН БАГ**: AMES interval — в **секундах** (диапазон 5..3600 по РЭ, но прибор принимает 1). Ранее использовали `@1 AMES 0 1000` — это 1000 секунд (~16 минут), сенсор молчал.
    - **Формат AMES пакета** (датчик шлёт сам каждые 1000 мс):
      ```
      Spd=X.XX Dir=XXX.X vX=X.XX vY=X.XX sq=100 dr1=0 dr2=0 dr3=0 dr4=0 tm1=XXXXX tm2=XXXXX tm3=XXXXX tm4=XXXXX sndx=XXX.X sndy=XXX.X
      ```
    - Парсинг через `Sensor_Parse` Option C: поля `Spd=` и `Dir=`.
    - **Состояния датчика** (`SensorState_t`):
        - `SENSOR_INIT_STOP_AMES` → посылает `@N AMES 0 0`
        - `SENSOR_INIT_OPEN` → посылает `OPEN N`
        - `SENSOR_INIT_GET_VER` → посылает `VER`, ждёт строку с "DVU"
        - `SENSOR_READY_START_AMES` → посылает `@N AMES 0 1000` (однократно), переходит в AMES
        - `SENSOR_READY_AMES` → слушает поток, таймаут 5 с → рестарт (`SENSOR_INIT_STOP_AMES`)
    - `wind_sensor.ames_last_data` обновляется в main.c при каждом успешном `Sensor_Parse`.
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
- **[2026-05-30] Debug Console Menu (fw v1.1.0)**:
    - The debug RS-485 port (X3 / UART3, PB10=TX, PB11=RX, 115200 8N1) now accepts text commands.
    - Commands are polled non-blocking in the main loop via `DebugConsole_Poll()` / `DebugConsole_ProcessCmd()`.
    - Supported commands:
        - `version` → responds `OK WindTrans fw v1.1.0`
        - `set_dac1 NNNN` → forces DAC1 (speed) to `NNNN/100` mA for 60 seconds, then resumes normal mode. e.g. `set_dac1 2100` = 21.00 mA.
        - `set_dac2 NNNN` → same for DAC2 (direction).
        - `continue` → immediately cancels any active override, returns to normal mode.
    - **Valid current range**: 3.5 … 20.0 mA (argument 350 … 2000). Values outside this range are rejected with `ERR current out of range`.
    - All successful commands respond with `OK` (possibly with additional detail on the same line).
    - While override is active: sensor DAC output is frozen (DAC_UpdateOutputs is bypassed), and sensor-timeout error detection is suppressed.
    - Override expiry is announced with `INFO override expired, resuming normal mode`.
- **[2026-05-30] Auto-versioning via git pre-commit hook**:
    - `fw/Core/Inc/version.h` — tracked file, auto-generated on every commit. Defines `FW_VERSION_MAJOR`, `FW_VERSION_MINOR`, `FW_VERSION_BUILD`, `FW_VERSION` (string, e.g. `"1.1.047"`).
    - `scripts/hooks/pre-commit` — bash hook (tracked). Reads current commit count, writes `BUILD = count + 1`, regenerates `version.h`, runs `git add version.h`.
    - `scripts/install_hooks.ps1` — copies hooks from `scripts/hooks/` into `.git/hooks/`. Run once after cloning.
    - MAJOR/MINOR are hardcoded in `pre-commit`; edit there and commit to bump them.
    - `main.c` includes `version.h`; `FW_VERSION` is no longer defined inline.

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

> ⚠️ **Known Issue (2026-05-25) — MISO Bus Contention**:
> 1. **Hardware Topology**: The PCB instantiates the `LOOP_DAC.SchDoc` schematic sheet twice, meaning there are physically **two independent ADuM1401 digital isolators** (one for each DAC).
> 2. **Push-Pull Outputs Tied Together**: The MISO outputs (Pin 6, VOD) of both isolators are tied directly to the MCU's single PA6 (MISO) line. Because the ADuM1401's output enable pins (VE1) are permanently tied to VDD1, their outputs are always actively driven (push-pull) and never go High-Z. This causes physical bus contention when both are powered.
> 3. **Unpowered Channel Fail-Safe**: When one of the loops (e.g. Channel 2) is unpowered, its isolator's input side (Side 2) loses power. The ADuM1401's fail-safe behavior drives its output (Pin 6, VOD) **HIGH** (3.3V) constantly. This overrides any attempt by the other powered channel to pull MISO low, resulting in a constant `0xFFFF` on MISO.
> 4. **Impact**: SPI write operations work perfectly (CPHA=0/Mode 0 is correct), allowing full current loop control. However, SPI register readback (like `DAC_ReadStatus` and `DAC_SpiProbe`) will always return `0xFFFF` under these hardware conditions. This is a design limitation and is safe to ignore in software.

# Technical Specification: DVU-01 to 4-20mA Converter

## 1. Hardware Pinout (STM32F103C8T6)

### 1.1 RS-485 (UART2) - Sensor (X2)
- **PA0**: nRE (Receiver Enable)
- **PA1**: DE (Driver Enable)
- **PA2**: TX
- **PA3**: RX
- **DMA**: DMA1 Channel 6 (RX)

### 1.2 RS-485 (UART3) - Debug/Config (X3)
- **PB10**: TX
- **PB11**: RX
- **PB12**: nRE
- **PB13**: DE
- **PA4**: DAC1_nCS (Chip Select for Speed DAC)
- **PA5**: SCK (Serial Clock)
- **PA6**: MISO (Master In Slave Out - for status)
- **PA7**: MOSI (Master Out Slave In)
- **PB0**: DAC2_nCS (Chip Select for Direction DAC)
- **PB1**: DAC1_nERR (Error feedback from DAC1)
- **PB2**: DAC2_nERR (Error feedback from DAC2)

### 1.3 Indicators & Other
- **PA9**: System LED (Heartbeat)

## 2. Communication Protocol (Sensor DVU-01)
- **Baudrate**: 9600 bps
- **Format**: 8 bits, No parity, 1 stop bit (8N1)
- **Poll Command**: `@1 MES<CR><LF>` (assuming ID 1)
- **Expected Response**: `@1:0 status spd_act spd_avg spd_max spd_min dir_act dir_avg dir_max dir_min<CR><LF>`
    - `status`: 0 = OK.
    - `spd_act`: Current speed (m/s).
    - `dir_act`: Current direction (deg).

## 3. Conversion Logic
### 3.1 Speed Channel (DAC1)
- **Input**: 0 ... 60.0 m/s
- **Output**: 4.0 ... 20.0 mA
- **Formula**: `mA = 4.0 + (spd / 60.0) * 16.0`

### 3.2 Direction Channel (DAC2)
- **Input**: 0 ... 360.0 deg
- **Output**: 4.0 ... 20.0 mA
- **Formula**: `mA = 4.0 + (dir / 360.0) * 16.0`

### 3.3 DAC161S997 Register Values
The DAC uses a 16-bit code where:
- `I_out = (CODE / 2^16) * 24 mA`
- `CODE = (I_target / 24.0) * 65536`

| State | Current (mA) | CODE (Hex) | CODE (Dec) |
| :--- | :--- | :--- | :--- |
| **Min (4mA)** | 4.00 | `0x2AAA` | 10922 |
| **Max (20mA)** | 20.00 | `0xD555` | 54613 |
| **Error (Low)** | 3.50 | `0x2555` | 9557 |

## 4. Error Handling
- **Sensor Timeout**: 2.0 seconds without valid response.
- **Hardware Error**: If `nERR` pin is LOW, or `status` in message is non-zero.
- **Action**: Both channels output 3.5 mA.

## 5. Status Indication (PA9 LED)
- **Fast Blink (5Hz)**: Sensor communication OK.
- **Slow Blink (0.5Hz)**: Sensor timeout or data error.

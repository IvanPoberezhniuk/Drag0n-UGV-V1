# UGV Control Station

Qt6 desktop app for controlling an unmanned ground vehicle over CRSF/serial (ELRS TX module).

## Requirements

- [MSYS2](https://www.msys2.org/) with the UCRT64 toolchain
- [xmake](https://xmake.io/)

Install Qt6 and the MinGW toolchain via MSYS2 (one-time):

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-qt6-base
```

---

## Build

### Configure (first time or after changing xmake.lua)

```bash
xmake f -c -p mingw --mingw=C:\msys64\ucrt64 --qt=C:\msys64\ucrt64
```

### Build

```bash
xmake
```

### Configure + build in one step

```bash
xmake f -c -p mingw --mingw=C:\msys64\ucrt64 --qt=C:\msys64\ucrt64 && xmake
```

### Build with verbose output

```bash
xmake -v
```

### Debug build

```bash
xmake f -m debug && xmake
```

### Release build (default)

```bash
xmake f -m release && xmake
```

### Clean build artifacts

```bash
xmake clean
```

---

## Run

```bash
xmake run
```

### With a specific config file

```bash
xmake run UGVControlStation --config path\to\config.json
```

### With verbose logging

```bash
xmake run UGVControlStation --verbose
```

---

## Deploy (copy Qt DLLs next to the exe)

```bash
windeployqt6 build\mingw\x86_64\release\UGVControlStation.exe
```

After this the exe can be run directly without xmake.

---

## IntelliSense (VS Code)

Generate `compile_commands.json` so the C/C++ extension resolves all includes:

```bash
xmake project -k compile_commands
```

Re-run this whenever you add packages or change `xmake.lua`.

---

## Config file

Place `config.json` next to the exe or pass `--config <path>`. All fields are optional — omit any to use the default.

```json
{
  "serial": {
    "port": "auto",
    "baudrate": 420000
  },
  "control": {
    "rateHz": 50,
    "failsafeTimeoutMs": 300
  },
  "channels": {
    "steering": 1,
    "throttle": 2,
    "mode":     3,
    "lights":   4,
    "arm":      5,
    "estop":    6
  },
  "ui": {
    "fontSize": 10
  }
}
```

`"port": "auto"` auto-detects the first connected ELRS TX module (CP210x, CH340, CH341, STM32 VCP, FTDI).

---

## Keyboard controls

| Key | Action |
|-----|--------|
| `W` / `S` | Throttle forward / reverse |
| `A` / `D` | Steer left / right |
| `Enter` | Arm / disarm toggle (also clears ESTOP latch) |
| `Space` | Emergency stop (latches — re-arm to clear) |
| `L` | Toggle lights |
| `1` / `2` / `3` | Drive mode |

---

## Project structure

```
src/
├── main.cpp                  — app entry point, single-instance guard, dark theme
├── core/                     — ECS state components (entt)
│   ├── AppState.h            — central registry + mutex
│   ├── ControlState.h        — throttle, steering, arm, estop, lights, drive mode
│   ├── TelemetryState.h      — RSSI, LQ, battery voltage
│   ├── SafetyState.h         — failsafe + estop latch flags
│   ├── ConnectionState.h     — port name, status, pkt/s
│   └── LogBuffer.h           — thread-safe circular log + spdlog sink
├── crsf/                     — CRSF protocol
│   ├── CrsfTypes.h           — frame type constants, RcChannels struct
│   └── CrsfPacket.cpp/h      — RC_CHANNELS_PACKED encoder, CRC8 DVB-S2
├── io/                       — serial communication
│   ├── SerialPort.cpp/h      — Win32 COM port wrapper, auto-detect, enumeration
│   └── SerialWorker.cpp/h    — worker thread: send loop, RX parser, reconnect
├── input/
│   ├── KeyboardInput.cpp/h   — Win32 GetAsyncKeyState polling
│   └── GamepadInput.cpp/h    — SDL2 gamepad (optional)
├── services/
│   ├── SafetyService.cpp/h   — failsafe timeout, armed interlock, ESTOP latch
│   └── ControlService.cpp/h  — ControlState → RcChannels channel mapping
├── config/
│   └── AppConfig.cpp/h       — JSON config load/defaults
└── ui/
    ├── MainWindow.cpp/h      — QMainWindow, dock layout, 30 ms poll timer
    └── panels/
        ├── ConnectionPanel   — port selector, connect/disconnect, status
        ├── ControlPanel      — throttle/steering bars, arm, estop, drive mode
        ├── TelemetryPanel    — RSSI, LQ, battery (live when RX sends frames)
        ├── LogsPanel         — filterable real-time log viewer
        └── LegendPanel       — keyboard & Xbox controller visual reference
```

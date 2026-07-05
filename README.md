# UGV Control Station

Qt6 desktop app for controlling an unmanned ground vehicle over CRSF/serial (ELRS TX module).

## Requirements

- [MSYS2](https://www.msys2.org/) with the UCRT64 toolchain
- [CMake](https://cmake.org/) 3.21+ and [Ninja](https://ninja-build.org/)

Install Qt6 and the MinGW toolchain via MSYS2 (one-time):

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-qt6-base \
          mingw-w64-ucrt-x86_64-qt6-declarative \
          mingw-w64-ucrt-x86_64-ninja
```

`spdlog`, `nlohmann_json`, and `entt` are fetched automatically by CMake
(`FetchContent`) — no separate install step.

---

## Build

Qt's host tools (rcc, moc, qmlimportscanner, ...) need their DLLs on `PATH`
at configure and build time, so put `C:\msys64\ucrt64\bin` on `PATH` first.

### Configure (first time or after changing CMakeLists.txt)

```bash
cmake -G Ninja -B build_cmake ^
  -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 ^
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/ninja.exe ^
  -DCMAKE_BUILD_TYPE=Release
```

### Build

```bash
cmake --build build_cmake
```

### Debug build

```bash
cmake -B build_cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build_cmake
```

### Clean build artifacts

```bash
cmake --build build_cmake --target clean
```

---

## Run

```bash
build_cmake\UGVControlStation.exe
```

### With a specific config file

```bash
build_cmake\UGVControlStation.exe --config path\to\config.json
```

---

## Deploy (copy Qt DLLs next to the exe)

```bash
windeployqt6 build_cmake\UGVControlStation.exe
```

After this the exe can be run directly without the MSYS2 `bin` on `PATH`.

---

## IntelliSense (VS Code)

`compile_commands.json` is generated automatically in `build_cmake/` on every
configure (`CMAKE_EXPORT_COMPILE_COMMANDS=ON` is set in `CMakeLists.txt`), and
`.vscode/c_cpp_properties.json` already points there.

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

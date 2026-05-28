#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <regstr.h>
// GUID_DEVCLASS_PORTS = {4D36E978-E325-11CE-BFC1-08002BE10318}
static const GUID GUID_DEVCLASS_PORTS = {0x4D36E978, 0xE325, 0x11CE, {0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18}};
#include <vector>
#include "io/SerialPort.h"
#include <spdlog/spdlog.h>
#include <cstring>

static_assert(sizeof(void*) == sizeof(HANDLE), "HANDLE size mismatch");

SerialPort::SerialPort() : m_handle(INVALID_HANDLE_VALUE) {}

SerialPort::~SerialPort() { close(); }

SerialPort::SerialPort(SerialPort&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = INVALID_HANDLE_VALUE;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept {
    if (this != &other) {
        close();
        m_handle = other.m_handle;
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    return *this;
}

bool SerialPort::open(const std::string& portName, uint32_t baud) {
    std::string fullName = "\\\\.\\" + portName;
    HANDLE h = CreateFileA(fullName.c_str(),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        spdlog::error("SerialPort: failed to open {} (error {})", portName, GetLastError());
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) {
        spdlog::error("SerialPort: GetCommState failed (error {})", GetLastError());
        CloseHandle(h);
        return false;
    }

    dcb.BaudRate        = baud;
    dcb.ByteSize        = 8;
    dcb.Parity          = NOPARITY;
    dcb.StopBits        = ONESTOPBIT;
    dcb.fBinary         = TRUE;
    dcb.fParity         = FALSE;
    dcb.fOutxCtsFlow    = FALSE;
    dcb.fOutxDsrFlow    = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX           = FALSE;
    dcb.fInX            = FALSE;
    dcb.fDtrControl     = DTR_CONTROL_DISABLE;
    dcb.fRtsControl     = RTS_CONTROL_DISABLE;

    if (!SetCommState(h, &dcb)) {
        spdlog::error("SerialPort: SetCommState failed (error {}). "
                      "If baud {} is unsupported try 400000 or 500000.",
                      GetLastError(), baud);
        CloseHandle(h);
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 50;

    if (!SetCommTimeouts(h, &timeouts)) {
        spdlog::error("SerialPort: SetCommTimeouts failed (error {})", GetLastError());
        CloseHandle(h);
        return false;
    }

    m_handle = static_cast<void*>(h);
    spdlog::info("SerialPort: opened {} at {} baud", portName, baud);
    return true;
}

void SerialPort::close() {
    HANDLE h = static_cast<HANDLE>(m_handle);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        m_handle = INVALID_HANDLE_VALUE;
        spdlog::info("SerialPort: closed");
    }
}

bool SerialPort::isOpen() const {
    return static_cast<HANDLE>(m_handle) != INVALID_HANDLE_VALUE;
}

bool SerialPort::write(const uint8_t* data, size_t len) {
    HANDLE h = static_cast<HANDLE>(m_handle);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    if (!WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr)) {
        spdlog::warn("SerialPort: write failed (error {})", GetLastError());
        return false;
    }
    return written == static_cast<DWORD>(len);
}

int SerialPort::read(uint8_t* buf, size_t maxLen) {
    HANDLE h = static_cast<HANDLE>(m_handle);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD bytesRead = 0;
    if (!ReadFile(h, buf, static_cast<DWORD>(maxLen), &bytesRead, nullptr)) {
        DWORD err = GetLastError();
        if (err == ERROR_OPERATION_ABORTED || err == ERROR_IO_PENDING) return 0;
        spdlog::warn("SerialPort: read failed (error {})", err);
        return -1;
    }
    return static_cast<int>(bytesRead);
}

// --- enumeration helpers ---

static HDEVINFO openPortDevInfo() {
    return SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
}

std::string SerialPort::autoDetect() {
    static const char* knownIds[] = {
        "VID_10C4&PID_EA60",  // CP210x — most common in ELRS TX modules
        "VID_1A86&PID_7523",  // CH340
        "VID_1A86&PID_7522",  // CH341
        "VID_0483&PID_5740",  // STM32 Virtual COM Port
        "VID_0403&PID_6001",  // FTDI FT232
        nullptr
    };

    HDEVINFO devInfo = openPortDevInfo();
    if (devInfo == INVALID_HANDLE_VALUE) {
        spdlog::warn("SerialPort::autoDetect: SetupDiGetClassDevs failed");
        return {};
    }

    std::string result;
    SP_DEVINFO_DATA devData{};
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devData); ++i) {
        char hwId[512] = {};
        SetupDiGetDeviceRegistryPropertyA(devInfo, &devData, SPDRP_HARDWAREID,
            nullptr, reinterpret_cast<PBYTE>(hwId), sizeof(hwId) - 1, nullptr);

        bool matched = false;
        for (int k = 0; knownIds[k]; ++k)
            if (strstr(hwId, knownIds[k])) { matched = true; break; }
        if (!matched) continue;

        HKEY hKey = SetupDiOpenDevRegKey(devInfo, &devData,
            DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hKey == INVALID_HANDLE_VALUE) continue;

        char portName[32] = {};
        DWORD size = sizeof(portName), type = REG_SZ;
        RegQueryValueExA(hKey, "PortName", nullptr, &type,
            reinterpret_cast<LPBYTE>(portName), &size);
        RegCloseKey(hKey);

        if (strlen(portName) == 0) continue;

        char friendly[256] = {};
        SetupDiGetDeviceRegistryPropertyA(devInfo, &devData, SPDRP_FRIENDLYNAME,
            nullptr, reinterpret_cast<PBYTE>(friendly), sizeof(friendly) - 1, nullptr);

        spdlog::info("SerialPort: auto-detected {} — {} [{}]", portName, friendly, hwId);
        result = portName;
        break;
    }

    SetupDiDestroyDeviceInfoList(devInfo);

    if (result.empty())
        spdlog::warn("SerialPort: auto-detect found no known ELRS TX module");
    return result;
}

std::vector<std::string> SerialPort::listAll() {
    std::vector<std::string> ports;

    HDEVINFO devInfo = openPortDevInfo();
    if (devInfo == INVALID_HANDLE_VALUE) return ports;

    SP_DEVINFO_DATA devData{};
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devData); ++i) {
        HKEY hKey = SetupDiOpenDevRegKey(devInfo, &devData,
            DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hKey == INVALID_HANDLE_VALUE) continue;

        char portName[32] = {};
        DWORD size = sizeof(portName), type = REG_SZ;
        RegQueryValueExA(hKey, "PortName", nullptr, &type,
            reinterpret_cast<LPBYTE>(portName), &size);
        RegCloseKey(hKey);

        if (strlen(portName) > 0)
            ports.emplace_back(portName);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return ports;
}

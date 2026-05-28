#pragma once
#include <string>
#include <cstdint>
#include <cstddef>

class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&&) noexcept;
    SerialPort& operator=(SerialPort&&) noexcept;

    bool open(const std::string& portName, uint32_t baud);
    void close();
    bool isOpen() const;
    bool write(const uint8_t* data, size_t len);

    // Scan COM ports and return the first one matching a known ELRS TX USB chip.
    // Returns empty string if nothing found.
    static std::string autoDetect();

private:
    void* m_handle;  // HANDLE — kept as void* to avoid pulling <windows.h> into this header
};

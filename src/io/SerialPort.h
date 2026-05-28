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

    // Enumerate all present COM ports matching known ELRS TX USB chips.
    // Returns the first match, or empty string if none found.
    static std::string autoDetect();

    // Returns all COM port names currently present on the system.
    static std::vector<std::string> listAll();

private:
    void* m_handle; // HANDLE — opaque to avoid pulling <windows.h> into this header
};

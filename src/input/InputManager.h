#pragma once
#include "input/IInputSource.h"
#include "core/AppState.h"
#include <vector>
#include <memory>
#include <chrono>

class InputManager {
public:
    void addSource(std::unique_ptr<IInputSource> src);
    void poll(AppState& state);

private:
    struct Entry {
        std::unique_ptr<IInputSource> source;
        std::chrono::steady_clock::time_point lastAxisTime{};
    };
    std::vector<Entry> m_sources;
};

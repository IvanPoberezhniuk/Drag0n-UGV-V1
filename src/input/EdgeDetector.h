#pragma once
#include <array>

class EdgeDetector {
    bool m_prev = false;
public:
    bool rising(bool cur) {
        bool edge = cur && !m_prev;
        m_prev = cur;
        return edge;
    }
};

template<size_t N>
class EdgeDetectorArray {
    std::array<bool, N> m_prev{};
public:
    bool rising(size_t i, bool cur) {
        bool edge = cur && !m_prev[i];
        m_prev[i] = cur;
        return edge;
    }
};

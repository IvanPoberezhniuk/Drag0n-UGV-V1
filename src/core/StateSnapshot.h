#pragma once
#include "core/AppState.h"
#include <mutex>
#include <tuple>

template<typename T>
T snapshot(AppState& state) {
    std::lock_guard<std::mutex> lk(state.registryMutex);
    return state.registry.get<T>(state.ugv);
}

template<typename T1, typename T2, typename... Ts>
std::tuple<T1, T2, Ts...> snapshot(AppState& state) {
    std::lock_guard<std::mutex> lk(state.registryMutex);
    return std::tuple<T1, T2, Ts...>{ state.registry.get<T1>(state.ugv),
                                      state.registry.get<T2>(state.ugv),
                                      state.registry.get<Ts>(state.ugv)... };
}

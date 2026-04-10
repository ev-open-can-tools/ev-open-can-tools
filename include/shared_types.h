// include/shared_types.h
#pragma once

#ifdef NATIVE_BUILD
template <typename T>
using Shared = T;
#else
#include <atomic>
template <typename T>
using Shared = std::atomic<T>;
#endif

#pragma once

// Alphabetically-friendly Windows header that ensures windows.h comes first
// This header solves clang-format alphabetization breaking Windows include order

// Prevent Windows min/max macro conflicts with C++ STL
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Include windows.h first to establish base Windows definitions
#include <windows.h>
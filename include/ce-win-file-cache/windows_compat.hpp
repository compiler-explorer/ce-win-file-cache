#pragma once

// Unified Windows compatibility header
// Handles Wine cross-compilation and native Windows compilation

#ifdef WINE_CROSS_COMPILE
#include "wine_compat.hpp"
#else
// Native Windows includes
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "alfaheader.h"
#include <ntstatus.h>
#include <shellapi.h>
#include <strsafe.h>
#include <winnetwk.h>
#include <winternl.h>
#endif
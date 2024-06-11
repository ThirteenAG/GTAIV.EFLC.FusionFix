#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <subauth.h>
#include <string>


// For some unknown reason windows.h has custom macros for std::min & std::max
// that mess with SIMDString.
#undef min
#undef max
#include "SIMDString.h"


#include "IniReader.h"
#include "injector/injector.hpp"
#include "injector/calling.hpp"
#include "injector/hooking.hpp"
#include "injector/assembly.hpp"
#include "injector/utility.hpp"
#include "Hooking.Patterns.h"
#include "ModuleList.hpp"
#include <thread>
#include <mutex>
#include <set>
#include <map>
#include <iomanip>
#include <array>
#include <future>
#include <d3d9.h>

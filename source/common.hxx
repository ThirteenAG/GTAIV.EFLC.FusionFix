#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <subauth.h>
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
#include <maths.hxx>

#define force_return_address(addr) (*(uintptr_t*)(regs.esp - 4) = (addr))
#define return_to(addr) do { force_return_address(addr); return; } while (0)
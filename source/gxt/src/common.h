#pragma once

#include <iostream>
#include <set>
#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <string>
#include <filesystem>
#include <fstream>
#include "utils.h"

static_assert(sizeof(char) == 1, "char must be 1 byte");
static_assert(sizeof(wchar_t) == 2, "wchar_t must be 2 bytes");

//using namespace std::string_literals;

void throw_error(const wchar_t* format, ...);

inline static char t_toupper(char c) { return toupper((unsigned char)c); }
inline static wchar_t t_toupper(wchar_t c) { return towupper(c); }

inline static char t_tolower(char c) { return tolower((unsigned char)c); }
inline static wchar_t t_tolower(wchar_t c) { return towlower(c); }

inline static char t_byteswap(char c) { return c; }
inline static wchar_t t_byteswap(wchar_t c) { return _byteswap_ushort(c); }

#define UNREACHABLE_ID_KEY L"]"

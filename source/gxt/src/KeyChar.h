#pragma once

template<size_t count>
struct KeyChar
{
	char value[count] = {0};
public:
	KeyChar() = default;
	KeyChar(const char* val)
	{
		memset(value, 0, count);
		strcpy_s(value, val);
	}

	KeyChar(const wchar_t* val)
	{
		memset(value, 0, count);
		for (size_t i = 0; i < count-1; i++)
		{
			if (!val[i]) break;
			value[i] = static_cast<char>(val[i]);
		}
	}
	KeyChar(const std::string& val) : KeyChar(val.c_str()) { }
	KeyChar(const std::wstring& val) : KeyChar(val.c_str()) { }

	inline friend bool operator<(const KeyChar<count>& l, const KeyChar<count>& r) { return strcmp(l.value, r.value) < 0; }
	inline friend bool operator>(const KeyChar<count>& l, const KeyChar<count>& r) { return strcmp(l.value, r.value) > 0; }
	inline friend bool operator<=(const KeyChar<count>& l, const KeyChar<count>& r) { return strcmp(l.value, r.value) <= 0; }
	inline friend bool operator>=(const KeyChar<count>& l, const KeyChar<count>& r) { return strcmp(l.value, r.value) >= 0; }
	inline friend bool operator==(const KeyChar<count>& l, const KeyChar<count>& r) { return strcmp(l.value, r.value) == 0; }
	inline friend bool operator!=(const KeyChar<count>& l, const KeyChar<count>& r) { return strcmp(l.value, r.value) != 0; }

	friend std::wstring to_wstring(const KeyChar<count>& v)
	{
		std::wstring out;
		out.reserve(count);
		for (size_t i = 0; i < count; i++)
		{
			if (!v.value[i])
				break;
			out.push_back(v.value[i]);
		}
		return out;
	}

	inline friend void SwapEndian(KeyChar<count>& v) {} // nothing to swap here
};

typedef KeyChar<8> KeyChar8;
typedef KeyChar<16> KeyChar16;


struct TableName : public KeyChar8
{
	using KeyChar8::KeyChar8;

	inline friend bool operator<(const TableName& l, const TableName& r) { return _stricmp(l.value, r.value) < 0; }
	inline friend bool operator>(const TableName& l, const TableName& r) { return _stricmp(l.value, r.value) > 0; }
	inline friend bool operator<=(const TableName& l, const TableName& r) { return _stricmp(l.value, r.value) <= 0; }
	inline friend bool operator>=(const TableName& l, const TableName& r) { return _stricmp(l.value, r.value) >= 0; }
	inline friend bool operator==(const TableName& l, const TableName& r) { return _stricmp(l.value, r.value) == 0; }
	inline friend bool operator!=(const TableName& l, const TableName& r) { return _stricmp(l.value, r.value) != 0; }
};

template<size_t count>
struct KeyCharWithTime : public KeyChar<count>
{
	int time = -1;
public:
	KeyCharWithTime() = default;

	KeyCharWithTime(const char* val)
	{
		const char* c = strrchr(val, ' ');
		if (c)
		{
			*(char*)c = '\0';
			time = std::atoi(c + 1);
		}
		else
			time = -1;
		memset(this->value, 0, count);
		strcpy_s(this->value, val);
		if (c)
			*(char*)c = ' ';
	}

	KeyCharWithTime(const wchar_t* val)
	{
		const wchar_t* c = wcsrchr(val, L' ');
		if (c)
		{
			*(wchar_t*)c = L'\0';
			time = std::stoi(c + 1);
		}
		else
			time = -1;
		for (size_t i = 0; i < count - 1; i++)
		{
			if (!val[i]) break;
			this->value[i] = static_cast<char>(val[i]);
		}
		if (c)
			*(wchar_t*)c = L' ';
	}
	KeyCharWithTime(const std::string& val) : KeyCharWithTime(val.c_str()) { }
	KeyCharWithTime(const std::wstring& val) : KeyCharWithTime(val.c_str()) { }

	friend std::wstring to_wstring(const KeyCharWithTime<count>& v)
	{
		std::wstring out;
		out.reserve(count);
		for (size_t i = 0; i < count; i++)
		{
			if (!v.value[i])
				break;
			out.push_back(v.value[i]);
		}

		if (v.time > 0)
			out += L' ' + std::to_wstring(v.time);

		return out;
	}

	inline friend void SwapEndian(KeyCharWithTime<count>& v)
	{
		v.time = _byteswap_ulong(v.time);
	}
};

typedef KeyCharWithTime<8> KeyCharWithTime8;
typedef KeyCharWithTime<12> KeyCharWithTime12;
#include "common.h"
#include "utils.h"
#include <Windows.h>

void throw_error(const wchar_t* format, ...)
{
	wchar_t buf[512];

	va_list va;
	va_start(va, format);
	vswprintf_s(buf, format, va);
	va_end(va);

	throw buf;
}

const std::filesystem::path& GetApplicationPath()
{
	static std::filesystem::path path;

	if (path.empty())
	{
		std::vector<WCHAR> name(MAX_PATH);
		DWORD errorCode = 0;
		do
		{
			DWORD result = GetModuleFileName(NULL, name.data(), name.size());
			errorCode = GetLastError();
			switch (errorCode)
			{
			case ERROR_SUCCESS:
				path = name.data();
				path.remove_filename();
				break;
			case ERROR_INSUFFICIENT_BUFFER:
				//printf("ERROR_INSUFFICIENT_BUFFER\n");
				name.resize(name.size() + MAX_PATH);
				break;
			default:
				printf("GetApplicationPath() unknown error %i\n", errorCode);
				break;
			}
		} while (errorCode == ERROR_INSUFFICIENT_BUFFER);
	}

	return path;
}

std::wstring ReadFileToWString(const std::filesystem::path& filename, int suggestedEncoding)
{
	std::wstring outstr;
	std::ifstream t(filename, std::ios::in | std::ios::binary);
	if (!t.is_open())
		throw_error(L"ERROR: File %s could not be opened.", filename.c_str());

	t.seekg(0, std::ios::end);
	size_t filesize = static_cast<size_t>(t.tellg());
	t.seekg(0, std::ios::beg);

	uint32_t BomCheck = 0;
	t.read((char*)&BomCheck, 3);
	if (((BomCheck & 0xFFFF) == 0xFEFF) || ((BomCheck & 0xFFFF) == 0xFFFE))
	{
		t.seekg(2);
		outstr.resize(filesize / 2);
		t.read((char*)outstr.c_str(), filesize - 2);
		if ((BomCheck & 0xFFFF) == 0xFFFE) // big endian
		{
			for (wchar_t& c : outstr)
				c = _byteswap_ushort(c);
		}
	}
	else
	{
		if (BomCheck == 0xBFBBEF)
		{
			suggestedEncoding = CP_UTF8;
			filesize -= 3;
		}
		else t.seekg(0);

		if (suggestedEncoding != -1)
		{
			outstr.resize(filesize);

			// read all stream to string
			std::string str_char(filesize, '\0');
			t.read(str_char.data(), filesize);
			int requiredSize = MultiByteToWideChar(suggestedEncoding, 0, str_char.c_str(), str_char.size(), (LPWSTR)outstr.data(), outstr.size());
			outstr.resize(requiredSize);
		}
		else
			throw_error(L"ERROR: Couldn't determine the codepage.");
	}

	t.close();

	return outstr;
}
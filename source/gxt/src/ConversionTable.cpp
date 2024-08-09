#include "common.h"
#include "ConversionTable.h"

void cConversionTable::ReadFromFile(const std::filesystem::path& path)
{
	FILE* fp = nullptr;
	_wfopen_s(&fp, path.c_str(), L"r");

	if (!fp) return;

	map.clear();
	reverseMap.clear();
	uint32_t wc, gxtc;
	while (fscanf_s(fp, "%x=%x\n", &wc, &gxtc) == 2)
		SetUnicodeChar(static_cast<wchar_t>(wc), static_cast<wchar_t>(gxtc));

	fclose(fp);
}

void cConversionTable::WriteToFile(const std::filesystem::path& path) const
{
	FILE* fp = nullptr;
	_wfopen_s(&fp, path.c_str(), L"w");

	if (!fp) return;

	for (auto [key, value] : map)
		fprintf_s(fp, "%04X=%04X\n", key, value);

	fclose(fp);
}
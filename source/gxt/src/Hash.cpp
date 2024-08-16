#include "common.h"
#include "Hash.h"

std::set<std::wstring> usedKeys;

void DumpUsedKeys()
{
	FILE* f = nullptr;
	fopen_s(&f, "usedkeys.txt", "w");
	if (!f) return;

	for (const auto& key : usedKeys)
	{
		fprintf(f, "%S\n", key.c_str());
		printf("%S\n", key.c_str());
	}

	fclose(f);
}
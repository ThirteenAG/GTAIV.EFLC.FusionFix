#include "common.h"
#include "utils.h"
#include "StringContainer.h"
#include "Hash_CRC32.h"
#include "Hash_Jenkins.h"

eKeyType GetKeyTypeFromWString(std::wstring& value)
{
	to_upper(value);
	if (value == L"CHAR8")
		return KEYTYPE_CHAR8;
	else if (value == L"CHAR16")
		return KEYTYPE_CHAR16;
	else if (value == L"CRC32")
		return KEYTYPE_CRC32;
	else if (value == L"JENKINS")
		return KEYTYPE_JENKINS;
	else if (value == L"CHAR8_TIME")
		return KEYTYPE_CHAR8_TIME;
	else if (value == L"CHAR12_TIME")
		return KEYTYPE_CHAR12_TIME;
	return KEYTYPE_NONE;
}

eKeyType GetKeyTypeFromWString(const std::wstring& value)
{
	std::wstring v = value;
	return GetKeyTypeFromWString(v);
}

cStringContainer::sTable::sTable(eKeyType keyType, bool isWide)
{
	switch (keyType)
	{
	case KEYTYPE_CHAR8:
		pKeys = std::make_unique<cKeyContainer<KeyChar8>>();
		break;
	case KEYTYPE_CHAR16:
		pKeys = std::make_unique<cKeyContainer<KeyChar16>>();
		break;
	case KEYTYPE_CRC32:
		pKeys = std::make_unique<cKeyContainer<Hash<CRC32>>>();
		break;
	case KEYTYPE_JENKINS:
		pKeys = std::make_unique<cKeyContainer<Hash<Jenkins>>>();
		break;
	case KEYTYPE_CHAR8_TIME:
		pKeys = std::make_unique<cKeyContainer<KeyCharWithTime8>>();
		break;
	case KEYTYPE_CHAR12_TIME:
		pKeys = std::make_unique<cKeyContainer<KeyCharWithTime12>>();
		break;
	default:
		//assert(0);
		break;
	}

	if (isWide)
		pData = std::make_unique<cDataContainer<wchar_t>>();
	else
		pData = std::make_unique<cDataContainer<char>>();
}

cStringContainer::sTable& cStringContainer::GetTable(const TableName& name)
{
	if (name.value[0] == '\0' || _stricmp(name.value, "MAIN") == 0)
		return *m_mainTable;
	if (!m_missionTables[name])
		m_missionTables[name] = std::make_unique<cStringContainer::sTable>(m_keyType, m_isWide);
	return *m_missionTables[name];
}

const cStringContainer::sTable& cStringContainer::GetTable(const TableName& name) const
{
	if (name.value[0] == '\0' || _stricmp(name.value, "MAIN") == 0)
		return *m_mainTable;
	if (!m_missionTables[name])
		throw_error(L"Missing table %s", to_wstring(name).c_str());
	return *m_missionTables[name];
}

bool cStringContainer::HasTable(const TableName& name) const
{
	if (name.value[0] == '\0' || _stricmp(name.value, "MAIN") == 0)
		return true;
	if (!m_missionTables[name])
		return false;
	return true;
}

void cStringContainer::ConvertUnicodeStringToGXTString(std::wstring& str, bool bIsFallthrough) const
{
	bool bIsTag = false;
	for (wchar_t& c : str)
	{
		bool justSet = false;
		if (!bIsTag && m_enableTagHighlight) {
			bIsTag = c == L'~';
			justSet = true;
		}

		if (bIsFallthrough && bIsTag)
			c |= 0x8000;
		else
			c = bIsTag ? (c | 0x8000) : conversionTable.GetGxtChar(c);
		if (c == (L'~' | 0x8000) && !justSet)
			bIsTag = false;
	}
}

void cStringContainer::ConvertGXTStringToUnicodeString(std::wstring& str, bool bIsFallthrough) const
{
	bool bIsTag = false;
	bool b807EIsBusy = conversionTable.GetUnicodeChar(0x8000 | L'~') != (0x8000 | L'~');
	for (wchar_t& wc : str)
	{
		bool justSet = false;
		if (!bIsTag) {
			bIsTag = b807EIsBusy ? wc == L'~' : (wc & 0x7FFF) == L'~';
			justSet = true;
		}
		if (!bIsTag)
		{
			if (!bIsFallthrough)
				wc = conversionTable.GetUnicodeChar(wc);
		}
		else
			wc &= 0x7FFF;
		if (bIsTag && !justSet)
			bIsTag = b807EIsBusy ? wc != L'~' : (wc & 0x7FFF) != L'~';
	}
}

std::wstring cStringContainer::ConvertUnicodeStringToGXTString(const std::wstring& text, bool bIsFallthrough) const
{
	std::wstring str = text;
	ConvertUnicodeStringToGXTString(str, bIsFallthrough);
	return str;
}

std::wstring cStringContainer::ConvertGXTStringToUnicodeString(const std::wstring& text, bool bIsFallthrough) const
{
	std::wstring str = text;
	ConvertGXTStringToUnicodeString(str, bIsFallthrough);
	return str;
}

void cStringContainer::AddString(sTable& table, const std::wstring& key, const std::wstring& text)
{
	table.AddString(key, ConvertUnicodeStringToGXTString(text, fallthroughKeys.find(key) != fallthroughKeys.end()));
}

void cStringContainer::AddString(const TableName& table, const std::wstring& key, const std::wstring& text)
{
	AddString(GetTable(table), key, text);
}

void cStringContainer::AddString(sTable& table, const std::wstring& key, std::wstring& text)
{
	ConvertUnicodeStringToGXTString(text, fallthroughKeys.find(key) != fallthroughKeys.end());
	table.AddString(key, text);
}

void cStringContainer::AddString(const TableName& table, const std::wstring& key, std::wstring& text)
{
	AddString(GetTable(table), key, text);
}

void cStringContainer::AddFallthroughKeys(const std::wstring& keys)
{
	if (keys.empty()) return;

	size_t pos = 0;
	bool lastOne = false;
	do
	{
		size_t posEnd = keys.find(',', pos);
		if (posEnd == std::wstring::npos)
		{
			posEnd = keys.length();
			lastOne = true;
		}
		std::wstring key = keys.substr(pos, posEnd - pos);
		AddFallthroughKey(std::move(key));
		pos = posEnd + 1;
	} while (!lastOne);
}

void cStringContainer::WriteGXTTableList(std::ofstream& os) const
{
	const size_t nSectionHeaderSize = sizeof(uint32_t)*2; // TKEY/TDAT/TABL + size

	if (m_missionTables.size() > 0)
	{
		uint32_t startPos = static_cast<uint32_t>(os.tellp());

		os.write("TABL", 4);

		uint32_t n = (m_missionTables.size() + 1) * (sizeof(sHeaderTableEntry));

		os.write((const char*)&n, sizeof(n));

		uint32_t pos = startPos + nSectionHeaderSize + n;

		sHeaderTableEntry mainTable = { "MAIN", pos };
		os.write((const char*)&mainTable, sizeof(mainTable));

		pos += nSectionHeaderSize + m_mainTable->pKeys->GetSize() + nSectionHeaderSize + m_mainTable->pData->GetSize();
		if (pos & 3)
			pos = (pos & ~3) + 4;
		for (const auto& [name, missionTable] : m_missionTables)
		{
			sHeaderTableEntry entry = { name, pos };
			os.write((const char*)&entry, sizeof(entry));

			pos += sizeof(TableName);
			pos += nSectionHeaderSize + missionTable->pKeys->GetSize() + nSectionHeaderSize + missionTable->pData->GetSize();
			if (pos & 3)
				pos = (pos & ~3) + 4;
		}
	}
}

void cStringContainer::WriteGXTFileHeader(std::ofstream& os) const
{
	uint16_t s = static_cast<uint16_t>(m_mainTable->pKeys->KeySize());
	os.write((const char*)&s, sizeof(s));
	s = m_isWide ? 16 : 8;
	os.write((const char*)&s, sizeof(s));
}

bool cStringContainer::ReadGXT(const std::filesystem::path& path, bool bSwapEndian)
{
	std::ifstream is(path, std::ios::binary | std::ios::in);

	if (!is) return false;

	uint32_t buf;

	enum
	{
		NOTHING = 0,
		NO_TABLES,
		MULTITABLE
	};

	uint32_t stuffFound = NOTHING; // 0 - nothing found, 1 - one table found, 2 - multitable found
	is.read((char*)&buf, sizeof(buf));
	if (buf == 'YEKT')
		stuffFound = NO_TABLES;
	else if (buf == 'LBAT')
		stuffFound = MULTITABLE;
	else if ((buf & 0xFFFFFF) == 'LBG')
	{
		// gta 2
		is.seekg(2, std::ios_base::cur);
		is.read((char*)&buf, sizeof(buf));
		if (buf == 'YEKT')
			stuffFound = NO_TABLES;
	}
	else
	{
		is.read((char*)&buf, sizeof(buf));
		if (buf == 'YEKT')
			stuffFound = NO_TABLES;
		else if (buf == 'LBAT')
			stuffFound = MULTITABLE;
	}

	if (stuffFound == NOTHING)
		return false;

	//Clear();
	switch (stuffFound)
	{
	case NO_TABLES: // read one table
		is.seekg(-4, std::ios_base::cur);
		m_mainTable->Read(is, bSwapEndian);
		break;
	case MULTITABLE: // read multitable
	{
		is.read((char*)&buf, sizeof(buf));
		uint32_t numOfTables = buf / sizeof(sHeaderTableEntry);
		assert(numOfTables * sizeof(sHeaderTableEntry) == buf);
		std::vector<sHeaderTableEntry> allTables(numOfTables);
		is.read((char*)allTables.data(), sizeof(sHeaderTableEntry) * numOfTables);

		bool firstRead = false;
		for (const auto& table : allTables)
		{
			if (!firstRead)
			{
				// first table is a main table
				is.seekg(table.offset);
				m_mainTable->Read(is, bSwapEndian);
				firstRead = true;
			}
			else
			{
				is.seekg(table.offset + sizeof(TableName)); // skip table name
				m_missionTables[table.tableName] = std::make_unique<cStringContainer::sTable>(m_keyType, m_isWide);
				m_missionTables[table.tableName]->Read(is, bSwapEndian);
			}
		}
		break;
	}
	default:
		break;
	}
	return true;
}

bool cStringContainer::WriteGXT(const std::filesystem::path& path, sWriteFlags flags, bool bSwapEndian) const
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream of(path, std::ios::binary | std::ios::out);
	if (!of) return false;

	return WriteGXT(of, flags, bSwapEndian);
}

bool cStringContainer::WriteGXT(std::ofstream& os, sWriteFlags flags, bool bSwapEndian) const
{
	if (flags.m_bHeaderType == 1)
		WriteGXTFileHeader(os);
	else if (flags.m_bHeaderType == 2)
	{
		char GBLheader[] = { 'G', 'B', 'L', static_cast<char>(flags.m_LangId), 'd', '\0' };
		os.write(GBLheader, sizeof(GBLheader));
	}
	WriteGXTTableList(os);

	if (flags.m_bSortMainTableKeys)
		m_mainTable->pKeys->SortByKeys();
	else
		m_mainTable->pKeys->SortByOffset();

	printf("Table MAIN has %i keys\n", m_mainTable->pKeys->GetNumberOfKeys());
	m_mainTable->Write(os, bSwapEndian);

	for (const auto& [key, tbl] : m_missionTables)
	{
		char z = 0;
		while (os.tellp() & 3) os.write(&z, 1);
		os.write((const char*)&key, sizeof(key));
		tbl->pKeys->SortByKeys();
		printf("Table %s has %i keys\n", key.value, tbl->pKeys->GetNumberOfKeys());
		tbl->Write(os, bSwapEndian);
	}
	return true;
}

std::wstring cStringContainer::GetStringFromKey(const sTable& table, const std::wstring& key) const
{
	std::wstring str = table.GetString(key);
	ConvertGXTStringToUnicodeString(str, fallthroughKeys.find(key) != fallthroughKeys.end());
	return str;
}

#define ofstream_write_wstr(os, str) { static const wchar_t p[] = str; os.write((const char*)p, sizeof(p)-sizeof(wchar_t)); }

bool cStringContainer::WriteTXT(const std::filesystem::path& path, const cStringContainer* otherGXT, bool bSortUsingOriginal) const
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream os(path, std::ios::binary | std::ios::out);
	if (!os) return false;
	wchar_t c = 0xFEFF;
	os.write((const char*)&c, sizeof(c));

	WriteTableToTXT(os, *m_mainTable, otherGXT, bSortUsingOriginal);

	printf("Table MAIN has %i keys\n", m_mainTable->pKeys->GetNumberOfKeys());

	for (const auto& [tblName, tbl] : m_missionTables)
	{
		if (tbl->pKeys->GetNumberOfKeys() == 0) continue;
		WriteTableToTXT(os, *tbl, otherGXT, bSortUsingOriginal, tblName);
		printf("Table %s has %i keys\n", tblName.value, tbl->pKeys->GetNumberOfKeys());
	}

	ofstream_write_wstr(os, L"[DUMMY]\r\nTHIS LABEL NEEDS TO BE HERE !!!\r\nAS THE LAST LABEL DOES NOT GET COMPILED!!");
	return true;
}

void cStringContainer::WriteTableToTXT(std::ofstream& os, sTable& table, const cStringContainer* otherGXT, bool bSortUsingOriginal, const TableName& _tableName) const
{
	const sTable* otherTable = nullptr;
	if (otherGXT && otherGXT->HasTable(_tableName))
	{
		otherTable = &otherGXT->GetTable(_tableName);
		otherTable->pKeys->SortByKeys();
	}

	table.FindUnreachableStrings();

	if (bSortUsingOriginal && otherTable)
		table.pKeys->SortByOffset(otherTable->pKeys.get());
	else
		table.pKeys->SortByOffset();
	std::wstring tableName = to_wstring(_tableName);
	if (!tableName.empty())
	{
		std::wstring comment = L"{=================================== MISSION TABLE ";
		comment += tableName;
		comment += L" ===================================}\r\n\r\n";
		os.write((const char*)comment.c_str(), comment.size() * sizeof(wchar_t));
	}

	for (auto [key, str] : table)
	{
		if (key == UNREACHABLE_ID_KEY)
		{
			ofstream_write_wstr(os, L"{ UNREACHABLE TEXT:\r\n");
			ConvertGXTStringToUnicodeString(str, false);
			os.write((const char*)str.data(), str.size() * sizeof(wchar_t));
			ofstream_write_wstr(os, L" }\r\n\r\n");
			continue;
		}

		std::wstring keystr = L'[' + key;
		if (!tableName.empty())
			keystr += L':' + tableName;
		keystr += L"]\r\n";
		os.write((const char*)keystr.c_str(), keystr.size() * sizeof(wchar_t));

		ConvertGXTStringToUnicodeString(str, fallthroughKeys.find(key) != fallthroughKeys.end());
		if (otherTable)
		{
			std::wstring origstr = otherGXT->GetStringFromKey(*otherTable, key);
			if (!origstr.empty() && origstr != str)
			{
				os.write((const char*)L"{", sizeof(wchar_t));
				os.write((const char*)origstr.data(), origstr.size() * sizeof(wchar_t));
				ofstream_write_wstr(os, L"}\r\n");
			}
		}

		os.write((const char*)str.data(), str.size() * sizeof(wchar_t));

		ofstream_write_wstr(os, L"\r\n\r\n");
	}

	table.RemoveUnreachableStrings();
}

#undef ofstream_write_wstr

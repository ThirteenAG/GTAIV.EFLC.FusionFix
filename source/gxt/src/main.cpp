#include "common.h"
#include "utils.h"
#include "TableData.h"
#include "StringContainer.h"

std::wstring RemoveReturnsAndComments(const std::wstring& s)
{
	std::wstring newS;
	newS.reserve(s.size());

	// this works SO MUCH FASTER than wstring::find
	bool r_was_just_here = false;
	bool comment = false;
	for (wchar_t c : s)
	{
		if (comment)
		{
			if (c == L'}')
				comment = false;
			continue;
		}
		switch (c)
		{
		case L'{':
			comment = true;
			break;
		case L'\r':
			r_was_just_here = true;
			[[fallthrough]];
		case L'\t':
			newS.push_back(L' ');
			break;
		case L'\n':
			if (!r_was_just_here)
				newS.push_back(L' ');
			else
				r_was_just_here = false; // it's a pair of \r\n
			break;
		default:
			newS.push_back(c);
			break;
		}
	}

	return newS;
}

std::wstring ReadFileToWStringAndProcess(const std::filesystem::path &filename, int suggestedEncoding)
{
	std::wstring outstr = ReadFileToWString(filename, suggestedEncoding);

	trim(outstr);

	// remove some start-end block
	if (outstr.find(L"start") == 0)
	{
		size_t endPos = outstr.find(L"end", 5);
		size_t firstKey = outstr.find(L"[", 5);
		if (firstKey == std::wstring::npos)
			throw_error(L"ERROR: File %s has no keys.", filename.c_str());
		if (endPos > firstKey)
			throw_error(L"ERROR: 'end' for the 'start' block is located after the first key in file %s, this is wrong.", filename.c_str());
		if (endPos == std::wstring::npos)
			throw_error(L"ERROR: Couldn't find 'end' for the 'start' block in file %s.", filename.c_str());
		outstr.erase(0, endPos + 3);
	}

	outstr = std::move(RemoveReturnsAndComments(outstr));

	return outstr;
}

int GetSuggestedCodepage()
{
	//switch (Language)
	//{
	//case LANGUAGE_EFIGS: return 1252;
	//case LANGUAGE_RUSSIAN: return 1251;
	//case LANGUAGE_POLISH: return 1250;
	//default: return -1;
	//}
	return 1252;
}

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		printf("GXT Compiler/Decompiler v2.01 by Serge (aka Sergeanur)\n");

		if (argc < 2)
		{
			printf(
				"Usage:\n"
				"gxt -i [input_file(s)] -o [output_file] [[optional parameters]]\n"
				"\n"
				"Parameters:\n"
				"-i [input_file(s)]: list of TXT files which you want to compile into GXT (can be one or multiple files) or a GXT file you want to decompile into TXT\n"
				"-o [output_file]: name of a GXT/TXT file that will be generated\n"
				"\n"
				"Optional parameters:\n"
				"    -t [table_file]: charset table that is used for GXT\n"
				"    -k (CHAR8|CHAR16|CRC32|JENKINS|CHAR8_TIME|CHAR12_TIME): type of keys used in GXT. CRC32 for SA, JENKINS for IV, CHAR8_TIME and CHAR12_TIME - MH2, CHAR8 - rest of the games. Default: CHAR8\n"
				"    -w(0|1): 0 - use one byte chars, 1 - use two byte chars. Only some versions of SA and IV should have this set to 0. Rest of the games use 1. Default: 1\n"
				"    -f (KEY1,KEY2,KEY3...): list of comma separated keys that would have no conversion applied to them\n"
				"    -b: enable big endian. Only useful for MH2 Wii.\n"
				"\n"
				"    Compilation only:\n"
				"    -c (CODEPAGE): TXT codepage ID if the file isn't UTF-8 BOM, UTF-16 LE BOM or UTF-16 BE BOM. Default: 1252\n"
				"    -h(0|1|2): 0 - don't write GXT header, 1 - write GXT header, 2 - write GTA 2 header. Only SA and IV should have this set to 1. Default: 0\n"
				"    -s(0|1): 0 - don't sort tables, 1 - sort tables. All of the games have tables sorted except LCS and VCS. Default: 1\n"
				"    -m(0|1): 0 - don't sort keys in the main table, 1 - sort keys in the main table. All games should have this enabled except IV. Default: 1\n"
				"    -u(0|1): 0 - leave key case as is, 1 - turn all keys to uppercase. Should be 0 only for GTA 2. Default: 1\n"
				"    -d(0|1): 0 - don't remove repeated spaces, 1 - remove repeated spaces. Default: 1\n"
				"    -~: enable highlighting ~TAGS~ with 0x8000\n"
				"    -l (LANGUAGE): language id. Only useful for GTA 2\n"
				"\n"
				"    Decompilation only:\n"
				"    --original-i [input_file]: GXT file of an untranslated text to have comments of the original text added to each line\n"
				"    --original-t [table_file]: charset table that is used for the untranslated GXT\n"
				"    --original-k (CHAR8|CHAR16|CRC32|JENKINS|CHAR8_TIME|CHAR12_TIME): type of keys used in the untranslated GXT. Default: whatever is supplied in -k\n"
				"    --original-w(0|1): 0 - use one byte chars, 1 - use two byte chars. Default: whatever is supplied in -w\n"
				"    --original-f (KEY1,KEY2,KEY3...): list of comma separated keys that would have no conversion applied to them\n"
				"    --original-b: enable big endian\n"
				"    --original-s: sort keys using order from the original untranslated GXT\n"
			);
			return 0;
		}

		std::vector<std::filesystem::path> inputTxtFiles;
		std::vector<std::filesystem::path> inputGxtFiles;
		std::filesystem::path outputFile;
		std::filesystem::path tableFile;

		bool isWide = true;
		eKeyType keyType = KEYTYPE_CHAR8;
		uint32_t WriteHeader = 0;
		bool bSortTables = true;
		bool bSortMain = true;
		bool bUppercase = true;
		bool bSpecialTaggingOperation = false;
		bool bRemoveDoubleSpaces = true;
		bool bSwapEndian = false;
		char LangId = 'E';
		int codepage = -1;
		std::wstring strFallthroughKeys;

		std::filesystem::path original_inputGxtFile;
		int8_t original_isWide = -1;
		eKeyType original_keyType = KEYTYPE_NONE;
		std::filesystem::path original_tableFile;
		std::wstring original_strFallthroughKeys;
		bool original_bSwapEndian = false;
		bool original_bUsedForSorting = false;

		std::vector<std::wstring> arguments;
		for (int i = 1; i < argc; i++)
			arguments.emplace_back(argv[i]);

		auto GetBoolArg = [](const std::wstring& arg, bool& val, size_t pos = 2)
		{
			if (arg.size() >= pos+1)
			{
				switch (arg[pos])
				{
				case '0':
					val = false;
					break;
				case '1':
					val = true;
					break;
				default:
					break;
				}
			}
		};

		size_t cmdI = 0;
		while (cmdI < arguments.size() && arguments[cmdI][0] == L'-')
		{
			if (arguments[cmdI].size() >= 2)
				switch (arguments[cmdI][1])
				{
				case L'i': // input file(s)
					cmdI++;
					if (cmdI >= arguments.size())
						throw_error(L"argument -i provided with no paths");
					while (cmdI < arguments.size() && arguments[cmdI][0] != L'-')
					{
						std::filesystem::path thispath = arguments[cmdI++];
						std::filesystem::path ext = thispath.extension();
						if (_wcsicmp(ext.c_str(), L".GXT") == 0)
							inputGxtFiles.emplace_back(std::move(thispath));
						else if (_wcsicmp(ext.c_str(), L".TXT") == 0)
							inputTxtFiles.emplace_back(std::move(thispath));
					}
					cmdI--;
					break;
				case L'o': // output file
					if (cmdI + 1 >= arguments.size())
						throw_error(L"argument -o provided with no path");
					outputFile = arguments[++cmdI];
					break;
				case L't': // table file
					if (cmdI + 1 >= arguments.size())
						throw_error(L"argument -t provided with no path");
					tableFile = arguments[++cmdI];
					break;
				case L'c': // codepage
					if (cmdI + 1 >= arguments.size())
						throw_error(L"argument -c provided with no codepage");
					codepage = std::stoi(arguments[++cmdI]);
					break;
				case L'l': // language
				{
					if (cmdI + 1 >= arguments.size())
						throw_error(L"argument -l provided with no language");

					std::wstring value = to_upper_copy(arguments[++cmdI]);
					LangId = static_cast<char>(value[0]);
					break;
				}
				case L'h': // header
					if (arguments[cmdI].size() >= 3)
					{
						switch (arguments[cmdI][2])
						{
						case L'0': // none
						case L'1': // SA & IV
						case L'2': // GTA 2
							WriteHeader = arguments[cmdI][2] - L'0';
							break;
						default:
							break;
						}
					}
					break;
				case L's': // sort tables
					GetBoolArg(arguments[cmdI], bSortTables);
					break;
				case L'w': // wide char
					GetBoolArg(arguments[cmdI], isWide);
					break;
				case L'm': // sort main table
					GetBoolArg(arguments[cmdI], bSortMain);
					break;
				case L'k': // key type
				{
					if (cmdI + 1 >= arguments.size())
						throw_error(L"argument -k provided with no value");
					std::wstring& value = arguments[++cmdI];
					keyType = GetKeyTypeFromWString(value);
					if (keyType == KEYTYPE_NONE)
						throw_error(L"invalid value for argument -k - %s", value.c_str());
					break;
				}
				case L'u': // turn keys to uppercase
					GetBoolArg(arguments[cmdI], bUppercase);
					break;
				case L'f': // fallthrough keys
					if (cmdI + 1 >= arguments.size())
						throw_error(L"argument -f provided with no value");
					strFallthroughKeys = to_upper_copy(arguments[++cmdI]);
					break;
				case L'~': // compiler mode only
					bSpecialTaggingOperation = true;
					break;
				case L'd':
					GetBoolArg(arguments[cmdI], bRemoveDoubleSpaces);
					break;
				case L'b':
					bSwapEndian = true;
					break;
				case L'-':
					if (arguments[cmdI].size() < 12 || arguments[cmdI].rfind(L"--original-", 0) != 0)
					{
						break;
					}
					switch (arguments[cmdI][11])
					{
					case L'i':
						if (cmdI + 1 >= arguments.size())
							throw_error(L"argument --original-i provided with no value");
						original_inputGxtFile = arguments[++cmdI];
						break;
					case L'w':
						GetBoolArg(arguments[cmdI], *(bool*)&original_isWide, 12);
						break;
					case L'k':
					{
						if (cmdI + 1 >= arguments.size())
							throw_error(L"argument --original-k provided with no value");
						std::wstring& value = arguments[++cmdI];
						original_keyType = GetKeyTypeFromWString(value);
						if (original_keyType == KEYTYPE_NONE)
							throw_error(L"invalid value for argument --original-k - %s", value.c_str());
						break;
					}
					case L't':
						if (cmdI + 1 >= arguments.size())
							throw_error(L"argument --original-t provided with no path");
						original_tableFile = arguments[++cmdI];
						break;
					case L'f':
						if (cmdI + 1 >= arguments.size())
							throw_error(L"argument --original-f provided with no value");
						original_strFallthroughKeys = to_upper_copy(arguments[++cmdI]);
						break;
					case L'b':
						original_bSwapEndian = true;
						break;
					case L's':
						original_bUsedForSorting = true;
						break;
					default:
						break;
					}
					break;
				default:
					break;
				}
			cmdI++;
		}

		if (inputTxtFiles.size() < 1 && inputGxtFiles.size() < 1)
			throw_error(L"ERROR: No input file specified.");
		else if (inputTxtFiles.size() > 0 && inputGxtFiles.size() > 0)
			throw_error(L"ERROR: You have provided both GXT and TXT files. You should only provide either TXT files or GXT files");

		if (inputGxtFiles.size() > 1)
			throw_error(L"ERROR: You have to provide only one GXT file.");

		if (outputFile.empty())
			throw_error(L"ERROR: No output file specified.");

		//if (tableFile.empty())
		//	throw_error(L"ERROR: No table file specified.");

		cStringContainer container(keyType, isWide, bSpecialTaggingOperation);
		if (!tableFile.empty())
			container.conversionTable.ReadFromFile(tableFile);

		container.AddFallthroughKeys(strFallthroughKeys);

		if (inputGxtFiles.size() > 0)
		{
			printf("Converting GXT to TXT...\n");
			std::unique_ptr<cStringContainer> container2;
			
			if (!original_inputGxtFile.empty())
			{
				if (original_keyType == KEYTYPE_NONE)
					original_keyType = keyType;
				if (original_isWide == -1)
					original_isWide = isWide;
				container2 = std::make_unique<cStringContainer>(original_keyType, original_isWide, false);
				if (container2->ReadGXT(original_inputGxtFile, original_bSwapEndian))
				{
					if (!original_tableFile.empty())
					{
						container2->conversionTable.ReadFromFile(original_tableFile);
					}
					container2->AddFallthroughKeys(original_strFallthroughKeys);
				}
				else
					container2.reset();
			}

			for (const auto& filepath : inputGxtFiles)
			{
				container.ReadGXT(filepath, bSwapEndian);

				container.WriteTXT(outputFile, container2.get(), original_bUsedForSorting);

				//DumpUsedKeys();
			}
		}
		else
		{
			printf("Converting TXT to GXT...\n");
			// glue all of the files together
			std::wstring str_new;
			for (const auto& inputFile : inputTxtFiles)
				str_new += ReadFileToWStringAndProcess(inputFile, codepage < 0 ? GetSuggestedCodepage() : codepage);

			if (bRemoveDoubleSpaces)
			{
				std::wstring::iterator it = std::unique(str_new.begin(), str_new.end(), [](wchar_t lhs, wchar_t rhs) { return (lhs == rhs) && (lhs == L' '); });
				str_new.erase(it, str_new.end());
			}

			size_t bracket_open_pos = str_new.find(L'[');
			while (bracket_open_pos != std::wstring::npos)
			{
				size_t bracket_close_pos = str_new.find(L']', bracket_open_pos);
				if (bracket_close_pos == std::wstring::npos)
					throw_error(L"ERROR: Didn't find closing key bracket near %s\n", str_new.substr(bracket_open_pos, 15).c_str());

				TableName tableName;

				std::wstring keyName = str_new.substr(bracket_open_pos + 1, bracket_close_pos - bracket_open_pos - 1);

				// looking for the table name in the key
				size_t tableNamePos = keyName.find(L':');
				if (tableNamePos != std::wstring::npos)
				{
					tableName = keyName.substr(tableNamePos + 1);
					keyName.resize(tableNamePos);
				}

				// turn key to uppercase
				if (bUppercase)
					to_upper(keyName);

				bracket_open_pos = str_new.find(L'[', bracket_close_pos);
				if (bracket_open_pos == std::wstring::npos)
				{
					// THAT DUMMY LABEL NEEDS TO BE THERE !!!
					break;
				}

				std::wstring text = str_new.substr(bracket_close_pos + 1, bracket_open_pos - bracket_close_pos - 1);
				trim(text);
				if (!text.empty())
					container.AddString(tableName, keyName, text);
			}

			cStringContainer::sWriteFlags flags;
			flags.m_LangId = LangId;
			flags.m_bHeaderType = WriteHeader;
			flags.m_bSortMainTableKeys = bSortMain;

			if (bSortTables)
				container.SortMissionTables();

			if (!container.WriteGXT(outputFile, flags, bSwapEndian))
				throw_error(L"ERROR: Output file %s could not be created.\n", outputFile.c_str());
		}
	}
	catch (const wchar_t* e)
	{
		wprintf(L"%s\n", e);
	}
	catch (std::exception& e)
	{
		printf("%s\n", e.what());
	}
	catch (...)
	{
	}
	return 0;
}
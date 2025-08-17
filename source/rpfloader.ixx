module;

#include <common.hxx>
#include <filesystem>
#include <vector>
#include <memory>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

export module rpfloader;

import common;
import settings;
import comvars;

struct VFSFileEntry
{
    std::filesystem::path sourcePath;
    std::string virtualName;
};

enum eRPF_version
{
    RPF_VERSION_0,  // RPF0 format
    RPF_VERSION_1,  // RPF1 format  
    RPF_VERSION_2,  // RPF2 format
    RPF_VERSION_3   // RPF3 format (GTA IV)
};

struct TOCEntry
{
    uint32_t name_value = 0;  // hash (V3) or offset (V2)
    std::string name;
    bool isDirectory = false;
    eRPF_version version = RPF_VERSION_3;
    int index = -1;  // Assigned during TOC rebuild
    virtual ~TOCEntry() = default;
};

struct DirectoryEntry : TOCEntry
{
    uint32_t flags = 0;
    uint32_t contentEntryIndex = 0;
    uint32_t contentEntryCount = 0;
    std::vector<std::shared_ptr<TOCEntry>> children;
    DirectoryEntry()
    {
        isDirectory = true;
    }
};

struct FileEntry : TOCEntry
{
    uint32_t uncompressedSize = 0;
    uint32_t offset = 0;
    uint32_t sizeInArchive = 0;
    uint32_t sizeUsed = 0;
    bool isCompressed = false;
    bool isResourceFile = false;
    uint8_t resourceType = 0;
    uint32_t rscFlags = 0;
    std::vector<uint8_t> data;
    std::vector<uint8_t> customData;
    FileEntry()
    {
        isDirectory = false;
    }
};

struct ExtractedRpf
{
    std::shared_ptr<DirectoryEntry> root;
    eRPF_version version;
    bool isEncrypted;
    int32_t unknown;
    uint32_t tocSize;
    std::vector<uint8_t> rpfData;
    std::vector<uint8_t> tocData;  // Decrypted
    std::vector<std::shared_ptr<TOCEntry>> allEntries;
};

class RpfProcessor
{
public:
    static constexpr size_t RPF_HEADER_SIZE = 20;
    static constexpr size_t RPF_TOC_START_OFFSET = 0x800; // 2048 bytes
    static constexpr size_t RPF_ENTRY_SIZE = 16;
    static constexpr uint32_t RPF_TOC_SIZE = 2048;
    static constexpr uint32_t MAX_FILESIZE = 0x7FFFFFFF; // 2GB limit
    static constexpr uint32_t MAX_FILENAME_LENGTH = 255;
    static constexpr uint32_t BLOCK_SIZE = 0x800;

    // RPF version magic IDs
    static constexpr uint32_t RPF0_MAGIC_ID = 0x30465052; // 'RPF0'
    static constexpr uint32_t RPF1_MAGIC_ID = 0x31465052; // 'RPF1'
    static constexpr uint32_t RPF2_MAGIC_ID = 0x32465052; // 'RPF2'
    static constexpr uint32_t RPF3_MAGIC_ID = 0x33465052; // 'RPF3'

    #pragma pack(push, 1)
    // RPF0/1 header (12 bytes)
    struct RPF_Header_V01
    {
        uint32_t magicId;       // RPF0 or RPF1
        uint32_t tocSize;       // TOC size in bytes
        uint32_t entryCount;    // Number of entries
    };

    // RPF2/3 header (20 bytes) 
    struct RPF_Header_V23
    {
        uint32_t magicId;       // RPF2 or RPF3
        uint32_t tocSize;       // TOC size in bytes
        int32_t entryCount;     // Number of entries
        int32_t unknown;        // Unknown field (0x00000000 for unencrypted, varies)
        uint32_t encrypted;     // Encryption flag (0 = unencrypted, non-zero = encrypted)
    };

    // RPF2/3 Entry (16 bytes)
    struct RPF_Entry_V23
    {
        uint32_t field1;        // nameHash (V3) or nameOffset (V2)
        uint32_t field2;        // uncompressed size for files, flags for directories  
        uint32_t field3;        // offset for files, content_index for directories (+ directory flag in MSB for V3)
        uint32_t field4;        // sizeInArchive+flags for files, content_count for directories
    };
    #pragma pack(pop)

    static std::shared_ptr<std::vector<uint8_t>> CreateRpfFromFolder(const std::filesystem::path& folderPath, eRPF_version version = RPF_VERSION_3, bool encrypted = false)
    {
        std::vector<VFSFileEntry> fileList;

        std::error_code ec;
        if (!std::filesystem::exists(folderPath, ec) || !std::filesystem::is_directory(folderPath, ec))
        {
            return nullptr;
        }

        constexpr auto perms = std::filesystem::directory_options::skip_permission_denied |
            std::filesystem::directory_options::follow_directory_symlink;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath, perms, ec))
        {
            if (ec) continue;
            if (!entry.is_regular_file(ec)) continue;

            VFSFileEntry fileEntry;
            fileEntry.sourcePath = entry.path();

            // Build relative path from folder
            auto relativePath = std::filesystem::relative(entry.path(), folderPath, ec);
            if (!ec)
            {
                fileEntry.virtualName = relativePath.string();
                std::replace(fileEntry.virtualName.begin(), fileEntry.virtualName.end(), '\\', '/');
            }
            else
            {
                fileEntry.virtualName = entry.path().filename().string();
            }

            fileList.push_back(fileEntry);
        }

        return CreateRpfFromFileList(fileList, version, encrypted);
    }

    static std::shared_ptr<std::vector<uint8_t>> CreateRpfFromFileList(const std::vector<VFSFileEntry>& fileList, eRPF_version version = RPF_VERSION_3, bool encrypted = false)
    {
        if (fileList.empty())
        {
            return nullptr;
        }

        // Load all files into memory
        std::vector<FileEntry> entries;
        for (const auto& fileEntry : fileList)
        {
            std::error_code ec;
            if (!std::filesystem::exists(fileEntry.sourcePath, ec) || !std::filesystem::is_regular_file(fileEntry.sourcePath, ec))
            {
                continue; // Skip missing files
            }

            // Load file data
            std::ifstream file(fileEntry.sourcePath, std::ios::binary | std::ios::ate);
            if (!file.is_open())
                continue;

            auto size = file.tellg();
            if (size > MAX_FILESIZE)
                continue; // Skip files that are too large

            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileData(static_cast<size_t>(size));
            if (!file.read(reinterpret_cast<char*>(fileData.data()), size))
                continue;

            FileEntry entry;
            entry.name = fileEntry.virtualName.empty() ? fileEntry.sourcePath.filename().string() : fileEntry.virtualName;

            // Truncate filename if needed
            if (entry.name.length() > MAX_FILENAME_LENGTH)
            {
                entry.name = entry.name.substr(0, MAX_FILENAME_LENGTH);
            }

            entry.name_value = (version == RPF_VERSION_3) ? ComputeStringHash(entry.name) : 0;
            entry.data = std::move(fileData);
            entry.uncompressedSize = static_cast<uint32_t>(entry.data.size());
            entry.sizeInArchive = static_cast<uint32_t>(entry.data.size());
            entry.version = version;
            entries.push_back(std::move(entry));
        }

        if (entries.empty())
        {
            return nullptr;
        }

        // For Create, we need to build a tree from the virtualNames which may have paths
        auto root = std::make_shared<DirectoryEntry>();
        root->version = version;
        root->name = "";
        root->name_value = (version == RPF_VERSION_3) ? ComputeStringHash("") : 0;

        for (auto& entry : entries)
        {
            std::string path = entry.name;
            std::replace(path.begin(), path.end(), '\\', '/');
            std::vector<std::string> parts;
            size_t pos;
            while ((pos = path.find('/')) != std::string::npos)
            {
                parts.push_back(path.substr(0, pos));
                path.erase(0, pos + 1);
            }
            parts.push_back(path);

            auto current = root;
            for (size_t i = 0; i < parts.size() - 1; ++i)
            {
                const std::string& part = parts[i];
                bool found = false;
                for (auto& child : current->children)
                {
                    if (child->isDirectory && child->name == part)
                    {
                        current = std::dynamic_pointer_cast<DirectoryEntry>(child);
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    auto newDir = std::make_shared<DirectoryEntry>();
                    newDir->version = version;
                    newDir->name = part;
                    newDir->name_value = (version == RPF_VERSION_3) ? ComputeStringHash(part) : 0;
                    current->children.push_back(newDir);
                    current = newDir;
                }
            }

            auto newFile = std::make_shared<FileEntry>();
            newFile->version = version;
            newFile->name = parts.back();
            newFile->name_value = (version == RPF_VERSION_3) ? ComputeStringHash(parts.back()) : 0;
            newFile->data = std::move(entry.data);
            newFile->uncompressedSize = entry.uncompressedSize;
            newFile->sizeInArchive = entry.sizeInArchive;
            newFile->isCompressed = false;
            current->children.push_back(newFile);
        }

        ExtractedRpf fakeExtracted;
        fakeExtracted.root = root;
        fakeExtracted.version = version;
        fakeExtracted.isEncrypted = encrypted;
        fakeExtracted.unknown = 0;
        fakeExtracted.tocSize = 0;
        fakeExtracted.rpfData.assign(RPF_TOC_START_OFFSET + RPF_TOC_SIZE, 0);  // Initial size
        fakeExtracted.tocData.assign(RPF_TOC_SIZE, 0);
        fakeExtracted.allEntries = {};

        return SaveRpf(fakeExtracted);
    }

    static std::shared_ptr<std::vector<uint8_t>> MergeRpfWithFolder(const std::filesystem::path& originalRpfPath, const std::filesystem::path& updateFolderPath)
    {
        auto extractedOpt = ExtractRpfArchive(originalRpfPath);
        if (!extractedOpt.has_value())
        {
            return nullptr; // Failed to read original RPF
        }

        auto& extracted = extractedOpt.value();

        auto replacementFiles = GetReplacementFiles(updateFolderPath);

        // Apply replacements
        ApplyReplacements(extracted, replacementFiles);

        // Create new RPF archive
        return SaveRpf(extracted);
    }

    static std::optional<ExtractedRpf> ExtractRpfArchive(const std::filesystem::path& rpfPath)
    {
        std::error_code ec;
        if (!std::filesystem::exists(rpfPath, ec) || !std::filesystem::is_regular_file(rpfPath, ec))
        {
            return std::nullopt;
        }

        std::ifstream file(rpfPath, std::ios::binary);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        // Read file into memory
        file.seekg(0, std::ios::end);
        size_t fileSize = (size_t)file.tellg();
        file.seekg(0, std::ios::beg);

        if (fileSize < 12) // Minimum header size
        {
            return std::nullopt;
        }

        std::vector<uint8_t> rpfData(fileSize);
        if (!file.read(reinterpret_cast<char*>(rpfData.data()), fileSize))
        {
            return std::nullopt;
        }

        auto extracted = ExtractRpfData(rpfData);
        if (extracted.has_value())
        {
            extracted->rpfData = std::move(rpfData);
        }
        return extracted;
    }

    static std::optional<std::vector<uint8_t>> ExtractFileFromRpf(const std::filesystem::path& rpfPath, const std::string& fileName)
    {
        auto extracted = ExtractRpfArchive(rpfPath);
        if (!extracted.has_value())
        {
            return std::nullopt;
        }

        auto found = FindFileByPath(extracted->root, fileName);
        if (found)
        {
            return found->data;
        }
        return std::nullopt;
    }

    static bool ReplaceFileInRpf(const std::filesystem::path& rpfPath, const std::string& fileName, const std::vector<uint8_t>& newData)
    {
        auto extracted = ExtractRpfArchive(rpfPath);
        if (!extracted.has_value())
        {
            return false;
        }

        auto found = FindFileByPath(extracted->root, fileName);
        if (found)
        {
            found->customData = newData;
        }
        else
        {
            return false;
        }

        auto newRpfData = SaveRpf(extracted.value());
        if (!newRpfData)
        {
            return false;
        }

        std::ofstream outFile(rpfPath, std::ios::binary);
        if (!outFile.is_open())
        {
            return false;
        }

        outFile.write(reinterpret_cast<const char*>(newRpfData->data()), newRpfData->size());
        return outFile.good();
    }

private:
    static uint32_t ComputeStringHash(const std::string& str)
    {
        uint32_t hash = 0;
        for (char c : str)
        {
            char lowerC = std::tolower(c);
            hash += static_cast<uint32_t>(lowerC);
            hash += (hash << 10);
            hash ^= (hash >> 6);
        }
        hash += (hash << 3);
        hash ^= (hash >> 11);
        hash += (hash << 15);
        return hash;
    }

    static eRPF_version GetVersionFromMagic(uint32_t magic)
    {
        switch (magic)
        {
        case RPF0_MAGIC_ID: return RPF_VERSION_0;
        case RPF1_MAGIC_ID: return RPF_VERSION_1;
        case RPF2_MAGIC_ID: return RPF_VERSION_2;
        case RPF3_MAGIC_ID: return RPF_VERSION_3;
        default: return RPF_VERSION_3;
        }
    }

    static uint32_t GetMagicFromVersion(eRPF_version version)
    {
        switch (version)
        {
        case RPF_VERSION_0: return RPF0_MAGIC_ID;
        case RPF_VERSION_1: return RPF1_MAGIC_ID;
        case RPF_VERSION_2: return RPF2_MAGIC_ID;
        case RPF_VERSION_3: return RPF3_MAGIC_ID;
        default: return RPF3_MAGIC_ID;
        }
    }

    static std::vector<uint8_t> DecryptRpfToc(const std::vector<uint8_t>& encryptedData)
    {
        if (encryptedData.empty() || encryptedData.size() % 16 != 0)
        {
            return {};
        }

        if (encryptedData.size() < RPF_TOC_SIZE)
        {
            return {};
        }

        HCRYPTPROV hCryptProv = 0;
        HCRYPTKEY hKey = 0;

        std::vector<uint8_t> result = encryptedData;

        try
        {
            if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            {
                return {};
            }

            struct
            {
                BLOBHEADER hdr;
                DWORD keySize;
                BYTE keyData[32];
            } keyBlob = {};

            keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
            keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
            keyBlob.hdr.reserved = 0;
            keyBlob.hdr.aiKeyAlg = CALG_AES_256;
            keyBlob.keySize = 32;
            memcpy(keyBlob.keyData, GTAIV_ENCRYPTION_KEY, 32);

            if (!CryptImportKey(hCryptProv, (BYTE*)&keyBlob, sizeof(keyBlob), 0, 0, &hKey))
            {
                CryptReleaseContext(hCryptProv, 0);
                return {};
            }

            DWORD mode = CRYPT_MODE_ECB;
            if (!CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&mode, 0))
            {
                CryptDestroyKey(hKey);
                CryptReleaseContext(hCryptProv, 0);
                return {};
            }

            for (int round = 0; round < 16; ++round)
            {
                for (size_t offset = 0; offset < result.size(); offset += 16)
                {
                    DWORD blockLen = 16;
                    if (offset + 16 > result.size() ||
                        !CryptDecrypt(hKey, 0, FALSE, 0, result.data() + offset, &blockLen) ||
                        blockLen != 16)
                    {
                        CryptDestroyKey(hKey);
                        CryptReleaseContext(hCryptProv, 0);
                        return {};
                    }
                }
            }

            CryptDestroyKey(hKey);
            CryptReleaseContext(hCryptProv, 0);
            return result;
        }
        catch (...)
        {
            if (hKey) CryptDestroyKey(hKey);
            if (hCryptProv) CryptReleaseContext(hCryptProv, 0);
            return {};
        }
    }

    static std::vector<uint8_t> EncryptRpfToc(const std::vector<uint8_t>& plainData)
    {
        if (plainData.empty() || plainData.size() % 16 != 0)
        {
            return {};
        }

        if (plainData.size() < RPF_TOC_SIZE)
        {
            return {};
        }

        HCRYPTPROV hCryptProv = 0;
        HCRYPTKEY hKey = 0;

        std::vector<uint8_t> result = plainData;

        try
        {
            if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            {
                return {};
            }

            struct
            {
                BLOBHEADER hdr;
                DWORD keySize;
                BYTE keyData[32];
            } keyBlob = {};

            keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
            keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
            keyBlob.hdr.reserved = 0;
            keyBlob.hdr.aiKeyAlg = CALG_AES_256;
            keyBlob.keySize = 32;
            memcpy(keyBlob.keyData, GTAIV_ENCRYPTION_KEY, 32);

            if (!CryptImportKey(hCryptProv, (BYTE*)&keyBlob, sizeof(keyBlob), 0, 0, &hKey))
            {
                CryptReleaseContext(hCryptProv, 0);
                return {};
            }

            DWORD mode = CRYPT_MODE_ECB;
            if (!CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&mode, 0))
            {
                CryptDestroyKey(hKey);
                CryptReleaseContext(hCryptProv, 0);
                return {};
            }

            for (int round = 0; round < 16; ++round)
            {
                for (size_t offset = 0; offset < result.size(); offset += 16)
                {
                    DWORD blockLen = 16;
                    if (offset + 16 > result.size() ||
                        !CryptEncrypt(hKey, 0, FALSE, 0, result.data() + offset, &blockLen, 16) ||
                        blockLen != 16)
                    {
                        CryptDestroyKey(hKey);
                        CryptReleaseContext(hCryptProv, 0);
                        return {};
                    }
                }
            }

            CryptDestroyKey(hKey);
            CryptReleaseContext(hCryptProv, 0);
            return result;
        }
        catch (...)
        {
            if (hKey) CryptDestroyKey(hKey);
            if (hCryptProv) CryptReleaseContext(hCryptProv, 0);
            return {};
        }
    }

    static std::optional<ExtractedRpf> ExtractRpfData(const std::vector<uint8_t>& rpfData)
    {
        if (rpfData.size() < 12) // Minimum header size
        {
            return std::nullopt;
        }

        // Read magic ID to determine version
        uint32_t magic = *reinterpret_cast<const uint32_t*>(rpfData.data());
        eRPF_version version = GetVersionFromMagic(magic);

        // Validate magic ID
        if (magic != RPF0_MAGIC_ID && magic != RPF1_MAGIC_ID &&
            magic != RPF2_MAGIC_ID && magic != RPF3_MAGIC_ID)
        {
            return std::nullopt;
        }

        if (version == RPF_VERSION_0 || version == RPF_VERSION_1)
        {
            // Implement if needed
            return std::nullopt;
        }

        return ExtractRpfV23(rpfData, version);
    }

    static std::optional<ExtractedRpf> ExtractRpfV23(const std::vector<uint8_t>& rpfData, eRPF_version version)
    {
        if (rpfData.size() < sizeof(RPF_Header_V23))
        {
            return std::nullopt;
        }

        const RPF_Header_V23* header = reinterpret_cast<const RPF_Header_V23*>(rpfData.data());
        bool isEncrypted = (header->encrypted != 0);

        if (rpfData.size() < RPF_TOC_START_OFFSET + header->tocSize)
        {
            return std::nullopt;
        }

        // Read TOC data
        std::vector<uint8_t> tocData;
        tocData.reserve(header->tocSize);
        tocData.assign(rpfData.begin() + RPF_TOC_START_OFFSET,
            rpfData.begin() + RPF_TOC_START_OFFSET + header->tocSize);

        // Decrypt TOC if encrypted (this decrypts both entries AND string table together)
        if (isEncrypted)
        {
            auto decryptedData = DecryptRpfToc(tocData);
            if (decryptedData.empty())
            {
                return std::nullopt;
            }
            tocData = std::move(decryptedData);
        }

        // Parse entries
        std::vector<std::shared_ptr<TOCEntry>> allEntries(header->entryCount);

        if (tocData.size() < static_cast<size_t>(header->entryCount) * RPF_ENTRY_SIZE)
        {
            return std::nullopt;
        }

        const RPF_Entry_V23* rawEntries = reinterpret_cast<const RPF_Entry_V23*>(tocData.data());

        // For RPF2, extract string table
        std::string stringTable;
        if (version == RPF_VERSION_2)
        {
            size_t stringTableOffset = static_cast<size_t>(header->entryCount) * RPF_ENTRY_SIZE;
            if (stringTableOffset < tocData.size())
            {
                size_t stringTableSize = tocData.size() - stringTableOffset;
                stringTable.assign(reinterpret_cast<const char*>(tocData.data() + stringTableOffset), stringTableSize);
            }
        }

        // Load filename hash mapping for V3
        std::unordered_map<uint32_t, std::string> knownHashes;
        if (version == RPF_VERSION_3)
        {
            knownHashes = LoadKnownHashes();
        }

        for (int32_t i = 0; i < header->entryCount; ++i)
        {
            const RPF_Entry_V23& rawEntry = rawEntries[i];
            bool isDir = (rawEntry.field3 & 0x80000000) != 0;

            uint32_t nameValue = rawEntry.field1;
            std::string name;

            if (version == RPF_VERSION_2)
            {
                if (nameValue < stringTable.length())
                {
                    size_t endOffset = nameValue;
                    while (endOffset < stringTable.length() && stringTable[endOffset] != '\0')
                        endOffset++;
                    name = stringTable.substr(nameValue, endOffset - nameValue);
                }
                else
                {
                    name = isDir ? "directory_" + std::to_string(i) : "file_" + std::to_string(i);
                }
            }
            else
            {
                name = GetNameFromHash(knownHashes, nameValue);
            }

            if (isDir)
            {
                auto dirEntry = std::make_shared<DirectoryEntry>();
                dirEntry->name_value = nameValue;
                dirEntry->name = name;
                dirEntry->flags = rawEntry.field2;
                dirEntry->contentEntryIndex = rawEntry.field3 & 0x7FFFFFFF;
                dirEntry->contentEntryCount = rawEntry.field4 & 0x0FFFFFFF;
                allEntries[i] = dirEntry;
            }
            else
            {
                auto fileEntry = std::make_shared<FileEntry>();
                fileEntry->name_value = nameValue;
                fileEntry->name = name;
                fileEntry->uncompressedSize = rawEntry.field2;
                fileEntry->offset = rawEntry.field3;
                uint32_t temp = rawEntry.field4;
                if ((temp & 0xC0000000) == 0xC0000000)
                {
                    fileEntry->isResourceFile = true;
                    fileEntry->resourceType = static_cast<uint8_t>(rawEntry.field3 & 0xFF);
                    fileEntry->offset = rawEntry.field3 & 0x7FFFFF00;
                    fileEntry->sizeInArchive = fileEntry->uncompressedSize;
                    fileEntry->isCompressed = false;
                    fileEntry->rscFlags = temp;
                }
                else
                {
                    fileEntry->sizeInArchive = temp & 0xBFFFFFFF;
                    fileEntry->isCompressed = (temp & 0x40000000) != 0;
                }
                fileEntry->sizeUsed = ((fileEntry->sizeInArchive + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

                // Extract data
                if (fileEntry->offset + fileEntry->sizeInArchive <= rpfData.size())
                {
                    fileEntry->data.assign(rpfData.begin() + fileEntry->offset, rpfData.begin() + fileEntry->offset + fileEntry->sizeInArchive);
                }

                allEntries[i] = fileEntry;
            }
        }

        // Build the tree structure
        auto root = std::dynamic_pointer_cast<DirectoryEntry>(allEntries[0]);
        if (!root)
        {
            return std::nullopt;
        }

        BuildTree(root, allEntries);

        ExtractedRpf result;
        result.root = root;
        result.version = version;
        result.isEncrypted = isEncrypted;
        result.unknown = header->unknown;
        result.tocSize = header->tocSize;
        result.tocData = std::move(tocData);
        result.allEntries = std::move(allEntries);
        return result;
    }

    static void BuildTree(const std::shared_ptr<DirectoryEntry>& dir, const std::vector<std::shared_ptr<TOCEntry>>& allEntries)
    {
        dir->children.reserve(dir->contentEntryCount);
        for (uint32_t i = 0; i < dir->contentEntryCount; ++i)
        {
            uint32_t childIndex = dir->contentEntryIndex + i;
            if (childIndex < allEntries.size())
            {
                auto child = allEntries[childIndex];
                dir->children.push_back(child);
                if (child->isDirectory)
                {
                    BuildTree(std::dynamic_pointer_cast<DirectoryEntry>(child), allEntries);
                }
            }
        }
    }

    static std::string GetNameFromHash(const std::unordered_map<uint32_t, std::string>& knownHashes, uint32_t hash)
    {
        auto it = knownHashes.find(hash);
        if (it != knownHashes.end())
        {
            return it->second;
        }
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "0x%08X", hash);
        return buffer;
    }

    static std::unordered_map<uint32_t, std::string> LoadKnownHashes()
    {
        std::unordered_map<uint32_t, std::string> hashes;
        std::filesystem::path hashesPath = GetThisModulePath<std::filesystem::path>() / "hashes.ini";

        std::ifstream file(hashesPath);
        if (file.is_open())
        {
            std::string line;
            while (std::getline(file, line))
            {
                if (line.empty() || line[0] == '#') continue;
                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    try
                    {
                        uint32_t hash = static_cast<uint32_t>(std::stoull(line.substr(0, pos), nullptr, 16));
                        std::string name = line.substr(pos + 1);
                        hashes[hash] = name;
                    }
                    catch (...)
                    {
                    }
                }
            }
        }
        return hashes;
    }

    static void ApplyReplacements(ExtractedRpf& extracted, const std::vector<FileEntry>& replacementFiles)
    {
        for (const auto& rep : replacementFiles)
        {
            std::string path = rep.name;
            std::replace(path.begin(), path.end(), '\\', '/');

            std::vector<std::string> parts;
            size_t pos = 0;
            while ((pos = path.find('/')) != std::string::npos)
            {
                parts.push_back(path.substr(0, pos));
                path.erase(0, pos + 1);
            }
            parts.push_back(path);

            auto current = extracted.root;
            for (size_t i = 0; i < parts.size() - 1; ++i)
            {
                const std::string& part = parts[i];
                bool found = false;
                for (const auto& child : current->children)
                {
                    if (child->isDirectory && iequals(child->name, part))
                    {
                        current = std::dynamic_pointer_cast<DirectoryEntry>(child);
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    // Create new directory
                    auto newDir = std::make_shared<DirectoryEntry>();
                    newDir->version = extracted.version;
                    newDir->name = part;
                    newDir->name_value = (extracted.version == RPF_VERSION_3) ? ComputeStringHash(part) : 0; // Will be updated later for V2
                    newDir->flags = 0; // Default flags
                    current->children.push_back(newDir);
                    current = newDir;
                }
            }

            const std::string& fileName = parts.back();
            bool found = false;
            for (const auto& child : current->children)
            {
                if (!child->isDirectory && iequals(child->name, fileName))
                {
                    auto file = std::dynamic_pointer_cast<FileEntry>(child);
                    file->customData = rep.data;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                auto newFile = std::make_shared<FileEntry>();
                newFile->version = extracted.version;
                newFile->name = fileName;
                newFile->name_value = (extracted.version == RPF_VERSION_3) ? ComputeStringHash(fileName) : 0; // Will be updated later for V2
                newFile->customData = rep.data;
                newFile->offset = 0; // New file
                current->children.push_back(newFile);
            }
        }
    }

    static std::shared_ptr<std::vector<uint8_t>> SaveRpf(ExtractedRpf& extracted)
    {
        // Collect all entries in TOC order
        std::vector<std::shared_ptr<TOCEntry>> newAllEntries;
        int index = 0;
        CollectEntries(extracted.root, newAllEntries, index);

        extracted.allEntries = newAllEntries;

        // Rebuild string table for V2
        std::string newStringTable;
        if (extracted.version == RPF_VERSION_2)
        {
            uint32_t strOffset = 0;
            for (auto& entry : extracted.allEntries)
            {
                entry->name_value = strOffset;
                newStringTable += entry->name + '\0';
                strOffset += static_cast<uint32_t>(entry->name.length() + 1);
            }
        }

        int32_t newEntryCount = static_cast<int32_t>(extracted.allEntries.size());
        size_t newEntriesSize = static_cast<size_t>(newEntryCount) * RPF_ENTRY_SIZE;
        size_t minTocSize = newEntriesSize + newStringTable.size();
        if (minTocSize < extracted.tocSize)
        {
            minTocSize = extracted.tocSize;
        }
        size_t newTocSize = ((minTocSize + 15) / 16) * 16;

        // Resize tocData
        extracted.tocData.resize(newTocSize, 0);
        extracted.tocSize = static_cast<uint32_t>(newTocSize);

        // Find max data offset
        uint32_t dataEnd = RPF_TOC_START_OFFSET + extracted.tocSize;
        dataEnd = ((dataEnd + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE; // Align

        for (const auto& entry : extracted.allEntries)
        {
            if (!entry->isDirectory)
            {
                auto file = std::dynamic_pointer_cast<FileEntry>(entry);
                uint32_t end = file->offset + file->sizeUsed;
                if (end > dataEnd) dataEnd = end;
            }
        }

        // Process customData
        for (auto& entry : extracted.allEntries)
        {
            if (!entry->isDirectory)
            {
                auto file = std::dynamic_pointer_cast<FileEntry>(entry);
                if (!file->customData.empty())
                {
                    std::vector<uint8_t> newData = std::move(file->customData);
                    uint32_t newSize = static_cast<uint32_t>(newData.size());
                    file->uncompressedSize = newSize;
                    file->sizeInArchive = newSize;
                    file->isCompressed = false;
                    file->isResourceFile = false; // Assume not resource for simplicity, adjust if needed

                    uint32_t newBlockCount = (newSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
                    uint32_t newUsed = newBlockCount * BLOCK_SIZE;

                    if (file->offset != 0 && newUsed <= file->sizeUsed)
                    {
                        // Overwrite existing
                        if (extracted.rpfData.size() < file->offset + file->sizeUsed)
                        {
                            extracted.rpfData.resize(file->offset + file->sizeUsed, 0);
                        }
                        std::fill(extracted.rpfData.begin() + file->offset, extracted.rpfData.begin() + file->offset + file->sizeUsed, 0);
                        std::copy(newData.begin(), newData.end(), extracted.rpfData.begin() + file->offset);
                        std::fill(extracted.rpfData.begin() + file->offset + newSize, extracted.rpfData.begin() + file->offset + newUsed, 0);
                    }
                    else
                    {
                        // Clear old if exists
                        if (file->offset != 0)
                        {
                            if (extracted.rpfData.size() < file->offset + file->sizeUsed)
                            {
                                extracted.rpfData.resize(file->offset + file->sizeUsed, 0);
                            }
                            std::fill(extracted.rpfData.begin() + file->offset, extracted.rpfData.begin() + file->offset + file->sizeUsed, 0);
                        }
                        // Append to end
                        file->offset = dataEnd;
                        if (extracted.rpfData.size() < dataEnd + newUsed)
                        {
                            extracted.rpfData.resize(dataEnd + newUsed, 0);
                        }
                        std::copy(newData.begin(), newData.end(), extracted.rpfData.begin() + dataEnd);
                        std::fill(extracted.rpfData.begin() + dataEnd + newSize, extracted.rpfData.begin() + dataEnd + newUsed, 0);
                        dataEnd += newUsed;
                    }
                    file->sizeUsed = newUsed;
                }
            }
        }

        // Write TOC entries to tocData
        RPF_Entry_V23* rawToc = reinterpret_cast<RPF_Entry_V23*>(extracted.tocData.data());
        for (const auto& entry : extracted.allEntries)
        {
            RPF_Entry_V23& raw = rawToc[entry->index];
            raw.field1 = entry->name_value;
            if (entry->isDirectory)
            {
                auto dir = std::dynamic_pointer_cast<DirectoryEntry>(entry);
                raw.field2 = dir->flags;
                raw.field3 = dir->contentEntryIndex | 0x80000000u;
                raw.field4 = dir->contentEntryCount;
            }
            else
            {
                auto file = std::dynamic_pointer_cast<FileEntry>(entry);
                raw.field2 = file->uncompressedSize;
                if (file->isResourceFile)
                {
                    raw.field3 = file->offset | file->resourceType;
                    raw.field4 = file->rscFlags;
                }
                else
                {
                    raw.field3 = file->offset;
                    raw.field4 = file->sizeInArchive;
                    if (file->isCompressed)
                    {
                        raw.field4 |= 0x40000000u;
                    }
                }
            }
        }

        // Write string table if V2
        if (extracted.version == RPF_VERSION_2)
        {
            size_t strOffset = newEntriesSize;
            std::copy(newStringTable.begin(), newStringTable.end(), extracted.tocData.begin() + strOffset);
        }

        // Encrypt TOC if encrypted
        std::vector<uint8_t> finalToc = extracted.tocData;
        if (extracted.isEncrypted)
        {
            finalToc = EncryptRpfToc(extracted.tocData);
            if (finalToc.empty())
            {
                return nullptr;
            }
        }

        // Update header
        RPF_Header_V23* header = reinterpret_cast<RPF_Header_V23*>(extracted.rpfData.data());
        header->tocSize = extracted.tocSize;
        header->entryCount = newEntryCount;
        header->unknown = extracted.unknown;
        header->encrypted = extracted.isEncrypted ? 0xFFFFFFFFu : 0u;

        // Write TOC to rpfData
        if (extracted.rpfData.size() < RPF_TOC_START_OFFSET + extracted.tocSize)
        {
            extracted.rpfData.resize(RPF_TOC_START_OFFSET + extracted.tocSize, 0);
        }
        std::copy(finalToc.begin(), finalToc.end(), extracted.rpfData.begin() + RPF_TOC_START_OFFSET);

        // Trim excess zeros at end if any
        while (!extracted.rpfData.empty() && extracted.rpfData.back() == 0)
        {
            extracted.rpfData.pop_back();
        }

        return std::make_shared<std::vector<uint8_t>>(std::move(extracted.rpfData));
    }

    static void CollectEntries(const std::shared_ptr<TOCEntry>& entry, std::vector<std::shared_ptr<TOCEntry>>& all, int& index)
    {
        entry->index = index++;
        all.push_back(entry);
        if (entry->isDirectory)
        {
            auto dir = std::dynamic_pointer_cast<DirectoryEntry>(entry);
            dir->contentEntryIndex = index;
            dir->contentEntryCount = static_cast<uint32_t>(dir->children.size());
            for (const auto& child : dir->children)
            {
                CollectEntries(child, all, index);
            }
        }
    }

    static std::shared_ptr<FileEntry> FindFileByPath(const std::shared_ptr<DirectoryEntry>& dir, const std::string& path)
    {
        std::string p = path;
        std::replace(p.begin(), p.end(), '\\', '/');
        std::vector<std::string> parts;
        size_t pos = 0;
        while ((pos = p.find('/')) != std::string::npos)
        {
            parts.push_back(p.substr(0, pos));
            p.erase(0, pos + 1);
        }
        parts.push_back(p);

        auto current = dir;
        for (size_t i = 0; i < parts.size() - 1; ++i)
        {
            const std::string& part = parts[i];
            bool found = false;
            for (const auto& child : current->children)
            {
                if (child->isDirectory && iequals(child->name, part))
                {
                    current = std::dynamic_pointer_cast<DirectoryEntry>(child);
                    found = true;
                    break;
                }
            }
            if (!found) return nullptr;
        }

        const std::string& fileName = parts.back();
        for (const auto& child : current->children)
        {
            if (!child->isDirectory && iequals(child->name, fileName))
            {
                return std::dynamic_pointer_cast<FileEntry>(child);
            }
        }
        return nullptr;
    }

    static std::vector<FileEntry> GetReplacementFiles(const std::filesystem::path& updateFolderPath)
    {
        std::vector<FileEntry> replacementFiles;
        std::error_code ec;

        if (!std::filesystem::exists(updateFolderPath, ec) || !std::filesystem::is_directory(updateFolderPath, ec))
        {
            return replacementFiles;
        }

        constexpr auto perms = std::filesystem::directory_options::skip_permission_denied |
            std::filesystem::directory_options::follow_directory_symlink;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(updateFolderPath, perms, ec))
        {
            if (ec) continue;
            if (!entry.is_regular_file(ec)) continue;

            std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
            if (!file.is_open()) continue;

            auto size = file.tellg();
            if (size > MAX_FILESIZE) continue;

            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileData(static_cast<size_t>(size));
            if (!file.read(reinterpret_cast<char*>(fileData.data()), size)) continue;

            FileEntry replacementFile;

            auto relativePath = std::filesystem::relative(entry.path(), updateFolderPath, ec);
            if (!ec)
            {
                replacementFile.name = relativePath.string();
                std::replace(replacementFile.name.begin(), replacementFile.name.end(), '\\', '/');
            }
            else
            {
                replacementFile.name = entry.path().filename().string();
            }

            replacementFile.name_value = ComputeStringHash(replacementFile.name);
            replacementFile.data = std::move(fileData);
            replacementFile.uncompressedSize = static_cast<uint32_t>(replacementFile.data.size());
            replacementFile.sizeInArchive = static_cast<uint32_t>(replacementFile.data.size());

            replacementFiles.push_back(std::move(replacementFile));
        }

        return replacementFiles;
    }
};

inline std::shared_ptr<std::vector<uint8_t>> MergeRpfWithFolder(const std::filesystem::path& originalRpfPath, const std::filesystem::path& updateFolderPath)
{
    return RpfProcessor::MergeRpfWithFolder(originalRpfPath, updateFolderPath);
}

inline std::shared_ptr<std::vector<uint8_t>> CreateRpfFromFolder(const std::filesystem::path& folderPath, eRPF_version version = RPF_VERSION_3, bool encrypted = false)
{
    return RpfProcessor::CreateRpfFromFolder(folderPath, version, encrypted);
}

inline std::shared_ptr<std::vector<uint8_t>> CreateRpfFromFileList(const std::vector<VFSFileEntry>& fileList, eRPF_version version = RPF_VERSION_3, bool encrypted = false)
{
    return RpfProcessor::CreateRpfFromFileList(fileList, version, encrypted);
}

inline std::optional<std::vector<uint8_t>> ExtractFileFromRpf(const std::filesystem::path& rpfPath, const std::string& fileName)
{
    return RpfProcessor::ExtractFileFromRpf(rpfPath, fileName);
}

inline bool ReplaceFileInRpf(const std::filesystem::path& rpfPath, const std::string& fileName, const std::vector<uint8_t>& newData)
{
    return RpfProcessor::ReplaceFileInRpf(rpfPath, fileName, newData);
}

class RPFLoader
{
public:
    RPFLoader()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            bool bLoadRPF = iniReader.ReadInteger("FILELOADER", "LoadRPF", 0) != 0;

            if (bLoadRPF && UAL::AddVirtualFileForOverloadW)
            {
                std::wstring s;
                s.resize(MAX_PATH, L'\0');
                if (!UAL::GetOverloadPathW || !UAL::GetOverloadPathW(s.data(), s.size()))
                {
                    s = GetExeModulePath() / L"update";
                }

                static auto updatePath = std::filesystem::path(s.data());

                static std::future<void> BuildRPFsFuture = std::async(std::launch::async, []()
                {
                    std::error_code ec;
                    if (std::filesystem::exists(updatePath, ec))
                    {
                        constexpr auto perms = std::filesystem::directory_options::skip_permission_denied |
                            std::filesystem::directory_options::follow_directory_symlink;

                        for (const auto& it : std::filesystem::recursive_directory_iterator(updatePath, perms, ec))
                        {
                            if (ec)
                                continue;

                            auto folderPath = std::filesystem::path(it.path());

                            // Check for folder with .rpf extension
                            if (std::filesystem::is_directory(it, ec) && iequals(folderPath.extension().native(), L".rpf"))
                            {
                                // Look for corresponding original RPF
                                auto relativePath = lexicallyRelativeCaseIns(folderPath, updatePath);
                                auto originalRpfPath = GetExeModulePath() / relativePath;

                                // Change extension back to .rpf for the original file
                                originalRpfPath.replace_extension(".rpf");

                                if (std::filesystem::exists(originalRpfPath, ec) && std::filesystem::is_regular_file(originalRpfPath, ec))
                                {
                                    // Merge original RPF with the folder inside update
                                    auto mergedRpfData = MergeRpfWithFolder(originalRpfPath, folderPath);
                                    if (mergedRpfData)
                                    {
                                        UAL::AddVirtualFileForOverloadW(relativePath.wstring().c_str(), mergedRpfData->data(), mergedRpfData->size(), 1000);
                                    }
                                }
                                else
                                {
                                    // Create new RPF from folder (default to RPF3 format)
                                    auto rpfData = CreateRpfFromFolder(folderPath, RPF_VERSION_3);
                                    if (rpfData)
                                    {
                                        auto gamePath = GetExeModulePath();
                                        auto path = lexicallyRelativeCaseIns(folderPath, gamePath);
                                        UAL::AddVirtualFileForOverloadW(path.wstring().c_str(), rpfData->data(), rpfData->size(), 1000);
                                    }
                                }
                            }
                        }
                    }
                });

                FusionFix::onReadGameConfig() += []()
                {
                    if (BuildRPFsFuture.valid())
                        BuildRPFsFuture.wait();
                };
            }
        };
    }
} RPFLoader;
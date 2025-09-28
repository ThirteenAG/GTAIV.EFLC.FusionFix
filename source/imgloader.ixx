module;

#define NOMINMAX
#include <common.hxx>
#include <filesystem>
#include <vector>
#include <memory>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include <cwctype>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

export module imgloader;

import common;
import settings;
import comvars;

struct VFSFileEntry
{
    std::filesystem::path sourcePath;
    std::string virtualName;
};

enum eIMG_version
{
    IMG_VERSION_1,  // GTA III, GTA VC
    IMG_VERSION_2,  // GTA SA
    IMG_VERSION_3   // GTA IV
};

class ImgProcessor
{
public:
    static constexpr size_t IMG_BLOCK_SIZE = 2048;
    static constexpr size_t MAX_FILESIZE = 0xFFFF * IMG_BLOCK_SIZE;
    static constexpr size_t MAX_FILENAME_LENGTH_V2 = 23;
    static constexpr size_t MAX_FILENAME_LENGTH_V3 = 255;
    static constexpr uint32_t GTAIV_MAGIC_ID = 0xA94E2A52;

    #pragma pack(push, 1)
    struct IMG_Header_V2
    {
        char signature[4];      // "VER2"
        uint32_t numFiles;
    };

    struct IMG_Entry_V2
    {
        uint32_t position;      // In blocks
        uint16_t sizeLow;       // In blocks
        uint16_t sizeHigh;      // In blocks
        char name[24];          // Null terminated
    };

    struct IMG_Header_V3
    {
        uint32_t magicId;       // 0xA94E2A52
        uint32_t version;       // 3
        uint32_t numItems;
        uint32_t tableSize;
        uint16_t itemSize;      // 16
        uint16_t unknown;
    };

    struct IMG_Entry_V3
    {
        uint32_t sizeOrRSCFlags;   // Size for normal files, RSC flags for resource files
        uint32_t resourceType;     // ResourceType
        uint32_t offsetBlock;      // OffsetBlock
        uint16_t usedBlocks;       // UsedBlocks
        uint16_t flags;            // Flags
    };
    #pragma pack(pop)

    struct FileEntry
    {
        std::string name;
        std::vector<uint8_t> data;
        uint32_t position = 0;
        bool isReplacement = false; // Tracks if this file was replaced (for merging)
    };

    static std::shared_ptr<std::vector<uint8_t>> CreateImgFromFolder(const std::filesystem::path& folderPath, eIMG_version version = IMG_VERSION_3, bool encrypted = false)
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
            fileEntry.virtualName = entry.path().filename().string(); // Use just filename
            fileList.push_back(fileEntry);
        }

        return CreateImgFromFileList(fileList, version, encrypted);
    }

    static std::shared_ptr<std::vector<uint8_t>> CreateImgFromFileList(const std::vector<VFSFileEntry>& fileList, eIMG_version version = IMG_VERSION_3, bool encrypted = false)
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
            size_t maxLen = (version == IMG_VERSION_3) ? MAX_FILENAME_LENGTH_V3 : MAX_FILENAME_LENGTH_V2;
            if (entry.name.length() > maxLen)
            {
                entry.name = entry.name.substr(0, maxLen);
            }

            entry.data = std::move(fileData);
            entries.push_back(std::move(entry));
        }

        if (entries.empty())
        {
            return nullptr;
        }

        return CreateImgFromEntries(entries, version, encrypted);
    }

    static std::shared_ptr<std::vector<uint8_t>> MergeImgWithFolder(const std::filesystem::path& originalImgPath, const std::filesystem::path& updateFolderPath)
    {
        // Step 1: Extract original IMG archive
        auto originalFiles = ExtractImgArchive(originalImgPath);
        if (!originalFiles.has_value())
        {
            return nullptr; // Failed to read original IMG
        }

        auto [extractedFiles, version, wasEncrypted] = originalFiles.value();

        // Step 2: Get replacement files from update folder
        auto replacementFiles = GetReplacementFiles(updateFolderPath);

        // Step 3: Merge files (replace existing, add new)
        auto mergedFiles = MergeFiles(extractedFiles, replacementFiles);

        // Step 4: Create new IMG archive with merged content (preserve encryption state)
        return CreateImgFromEntries(mergedFiles, version, wasEncrypted);
    }

    // Extract all files from an existing IMG archive
    static std::optional<std::tuple<std::vector<FileEntry>, eIMG_version, bool>> ExtractImgArchive(const std::filesystem::path& imgPath)
    {
        std::error_code ec;
        if (!std::filesystem::exists(imgPath, ec) || !std::filesystem::is_regular_file(imgPath, ec))
        {
            return std::nullopt;
        }

        std::ifstream file(imgPath, std::ios::binary);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        // Read file into memory
        file.seekg(0, std::ios::end);
        size_t fileSize = (size_t)file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> imgData(fileSize);
        if (!file.read(reinterpret_cast<char*>(imgData.data()), fileSize))
        {
            return std::nullopt;
        }

        // Detect IMG version and extract
        if (fileSize >= 4 && memcmp(imgData.data(), "VER2", 4) == 0)
        {
            auto result = ExtractImgV2(imgData);
            if (result.has_value())
            {
                return std::make_tuple(result.value().first, result.value().second, false);
            }
        }
        else if (fileSize >= 4)
        {
            uint32_t magicId = *reinterpret_cast<const uint32_t*>(imgData.data());
            if (magicId == GTAIV_MAGIC_ID) // GTA IV unencrypted
            {
                auto result = ExtractImgV3(imgData);
                if (result.has_value())
                {
                    return std::make_tuple(result.value().first, result.value().second, false);
                }
            }
            else // GTA IV encrypted
            {
                auto decryptedData = DecryptGTAIVArchive(imgData);
                if (!decryptedData.empty() && decryptedData.size() >= 4)
                {
                    uint32_t decryptedMagicId = *reinterpret_cast<const uint32_t*>(decryptedData.data());
                    if (decryptedMagicId == GTAIV_MAGIC_ID)
                    {
                        auto result = ExtractImgV3(decryptedData);
                        if (result.has_value())
                        {
                            return std::make_tuple(result.value().first, result.value().second, true);
                        }
                    }
                }
            }
        }

        return std::nullopt; // Unknown format
    }

private:
    static std::vector<uint8_t> DecryptGTAIVArchive(const std::vector<uint8_t>& encryptedData)
    {
        if (encryptedData.empty() || encryptedData.size() < sizeof(IMG_Header_V3))
        {
            return {};
        }

        // Check if header is already unencrypted
        const IMG_Header_V3* header = reinterpret_cast<const IMG_Header_V3*>(encryptedData.data());
        if (header->magicId == GTAIV_MAGIC_ID && header->version == 3 && header->itemSize == 16)
        {
            return encryptedData;
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
            } keyBlob;
            keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
            keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
            keyBlob.hdr.reserved = 0;
            keyBlob.hdr.aiKeyAlg = CALG_AES_256;
            keyBlob.keySize = 32;
            memcpy(keyBlob.keyData, *GTAIV_ENCRYPTION_KEY, 32);

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

            // Decrypt header (16 rounds)
            std::vector<uint8_t> decryptedHeaderData(encryptedData.begin(), encryptedData.begin() + 20);
            for (int round = 0; round < 16; ++round)
            {
                DWORD blockLen = 16;
                if (!CryptDecrypt(hKey, 0, FALSE, 0, decryptedHeaderData.data(), &blockLen) || blockLen != 16)
                {
                    CryptDestroyKey(hKey);
                    CryptReleaseContext(hCryptProv, 0);
                    return {};
                }
            }

            const IMG_Header_V3* decryptedHeader = reinterpret_cast<const IMG_Header_V3*>(decryptedHeaderData.data());
            if (decryptedHeader->magicId != GTAIV_MAGIC_ID || decryptedHeader->version != 3 || decryptedHeader->itemSize != 16)
            {
                CryptDestroyKey(hKey);
                CryptReleaseContext(hCryptProv, 0);
                return {};
            }

            std::copy(decryptedHeaderData.begin(), decryptedHeaderData.end(), result.begin());

            // Decrypt TOC (entries + filename table)
            size_t entriesOffset = sizeof(IMG_Header_V3);
            size_t entriesSize = decryptedHeader->numItems * sizeof(IMG_Entry_V3);
            size_t filenameTableOffset = entriesOffset + entriesSize;
            size_t filenameTableSize = decryptedHeader->tableSize - entriesSize;

            if (entriesOffset + entriesSize > result.size() || filenameTableOffset + filenameTableSize > result.size())
            {
                CryptDestroyKey(hKey);
                CryptReleaseContext(hCryptProv, 0);
                return {};
            }

            // Decrypt TOC entries (16 rounds per entry)
            for (uint32_t i = 0; i < decryptedHeader->numItems; ++i)
            {
                size_t entryOffset = entriesOffset + (i * sizeof(IMG_Entry_V3));
                if (entryOffset + sizeof(IMG_Entry_V3) <= result.size())
                {
                    for (int round = 0; round < 16; ++round)
                    {
                        DWORD blockLen = 16;
                        if (!CryptDecrypt(hKey, 0, FALSE, 0, result.data() + entryOffset, &blockLen) || blockLen != 16)
                        {
                            CryptDestroyKey(hKey);
                            CryptReleaseContext(hCryptProv, 0);
                            return {};
                        }
                    }
                }
            }

            // Decrypt filename table (16 rounds per 16-byte block)
            size_t alignedSize = filenameTableSize & ~0xF;
            if (alignedSize > 0 && filenameTableOffset + alignedSize <= result.size())
            {
                for (size_t offset = 0; offset < alignedSize; offset += 16)
                {
                    for (int round = 0; round < 16; ++round)
                    {
                        DWORD blockLen = 16;
                        if (!CryptDecrypt(hKey, 0, FALSE, 0, result.data() + filenameTableOffset + offset, &blockLen) || blockLen != 16)
                        {
                            CryptDestroyKey(hKey);
                            CryptReleaseContext(hCryptProv, 0);
                            return {};
                        }
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

    static void EncryptGTAIVArchive(std::vector<uint8_t>& data)
    {
        if (data.empty() || data.size() < sizeof(IMG_Header_V3))
        {
            return;
        }

        // Read header to determine TOC size
        const IMG_Header_V3* header = reinterpret_cast<const IMG_Header_V3*>(data.data());
        size_t tocSize = sizeof(IMG_Header_V3) + header->tableSize;
        if (tocSize > data.size() || tocSize % 16 != 0)
        {
            return;
        }

        // Extract TOC for encryption
        std::vector<uint8_t> tocData(data.begin(), data.begin() + tocSize);

        HCRYPTPROV hCryptProv = 0;
        HCRYPTKEY hKey = 0;

        try
        {
            // Acquire crypto context
            if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            {
                return;
            }

            // Import key
            struct
            {
                BLOBHEADER hdr;
                DWORD keySize;
                BYTE keyData[32];
            } keyBlob;
            keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
            keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
            keyBlob.hdr.reserved = 0;
            keyBlob.hdr.aiKeyAlg = CALG_AES_256;
            keyBlob.keySize = 32;
            memcpy(keyBlob.keyData, *GTAIV_ENCRYPTION_KEY, 32);

            if (!CryptImportKey(hCryptProv, (BYTE*)&keyBlob, sizeof(keyBlob), 0, 0, &hKey))
            {
                CryptReleaseContext(hCryptProv, 0);
                return;
            }

            // Set ECB mode
            DWORD mode = CRYPT_MODE_ECB;
            if (!CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&mode, 0))
            {
                CryptDestroyKey(hKey);
                CryptReleaseContext(hCryptProv, 0);
                return;
            }

            // Perform 16 rounds of encryption
            for (int round = 0; round < 16; ++round)
            {
                for (size_t offset = 0; offset < tocSize; offset += 16)
                {
                    DWORD blockLen = 16;
                    if (!CryptEncrypt(hKey, 0, FALSE, 0, tocData.data() + offset, &blockLen, 16) || blockLen != 16)
                    {
                        CryptDestroyKey(hKey);
                        CryptReleaseContext(hCryptProv, 0);
                        return;
                    }
                }
            }

            CryptDestroyKey(hKey);
            CryptReleaseContext(hCryptProv, 0);

            // Replace original TOC with encrypted TOC
            std::copy(tocData.begin(), tocData.end(), data.begin());
        }
        catch (...)
        {
            if (hKey) CryptDestroyKey(hKey);
            if (hCryptProv) CryptReleaseContext(hCryptProv, 0);
        }
    }

    static uint32_t GetActualFileSize(const IMG_Entry_V3& entry)
    {
        bool isResourceFile = ((entry.sizeOrRSCFlags & 0xC0000000) != 0) || ((entry.sizeOrRSCFlags & 0x80000000) == 0x80000000);

        if (!isResourceFile)
        {
            return entry.sizeOrRSCFlags; // Direct size
        }
        else
        {
            // For resource files, calculate size from blocks and padding
            int paddingCount = entry.flags & 0x7FF;
            return entry.usedBlocks * IMG_BLOCK_SIZE - paddingCount;
        }
    }

    static std::shared_ptr<std::vector<uint8_t>> CreateImgFromEntries(const std::vector<FileEntry>& files, eIMG_version version, bool encrypted = false)
    {
        if (files.empty())
        {
            return nullptr;
        }

        // Create mutable copy for position calculation
        std::vector<FileEntry> entries = files;

        switch (version)
        {
        case IMG_VERSION_2:
            return CreateImgV2(entries);
        case IMG_VERSION_3:
            return CreateImgV3(entries, encrypted);
        default:
            return CreateImgV3(entries, encrypted);
        }
    }

    // Extract GTA SA IMG (Version 2)
    static std::optional<std::pair<std::vector<FileEntry>, eIMG_version>> ExtractImgV2(const std::vector<uint8_t>& imgData)
    {
        if (imgData.size() < sizeof(IMG_Header_V2))
        {
            return std::nullopt;
        }

        const IMG_Header_V2* header = reinterpret_cast<const IMG_Header_V2*>(imgData.data());
        if (memcmp(header->signature, "VER2", 4) != 0)
        {
            return std::nullopt;
        }

        size_t entriesOffset = sizeof(IMG_Header_V2);
        size_t entriesSize = header->numFiles * sizeof(IMG_Entry_V2);

        if (imgData.size() < entriesOffset + entriesSize)
        {
            return std::nullopt;
        }

        std::vector<FileEntry> files;
        const IMG_Entry_V2* entries = reinterpret_cast<const IMG_Entry_V2*>(imgData.data() + entriesOffset);

        for (uint32_t i = 0; i < header->numFiles; ++i)
        {
            const IMG_Entry_V2& entry = entries[i];

            // Calculate file size in bytes
            uint32_t sizeInBlocks = (entry.sizeHigh != 0) ? entry.sizeHigh : entry.sizeLow;
            uint32_t sizeInBytes = sizeInBlocks * IMG_BLOCK_SIZE;
            uint32_t dataOffset = entry.position * IMG_BLOCK_SIZE;

            if (dataOffset + sizeInBytes > imgData.size())
            {
                continue; // Skip invalid entries
            }

            FileEntry file;
            file.name = std::string(entry.name, strnlen(entry.name, sizeof(entry.name)));
            file.data.assign(imgData.begin() + dataOffset, imgData.begin() + dataOffset + sizeInBytes);

            // Remove padding from the end
            while (!file.data.empty() && file.data.back() == 0)
            {
                file.data.pop_back();
            }

            files.push_back(std::move(file));
        }

        return std::make_pair(files, IMG_VERSION_2);
    }

    static std::optional<std::pair<std::vector<FileEntry>, eIMG_version>> ExtractImgV3(const std::vector<uint8_t>& imgData)
    {
        if (imgData.size() < sizeof(IMG_Header_V3))
        {
            return std::nullopt;
        }

        const IMG_Header_V3* header = reinterpret_cast<const IMG_Header_V3*>(imgData.data());
        if (header->magicId != GTAIV_MAGIC_ID || header->version != 3 || header->itemSize != 16)
        {
            return std::nullopt;
        }

        size_t entriesOffset = sizeof(IMG_Header_V3);
        size_t entriesSize = header->numItems * sizeof(IMG_Entry_V3);

        if (imgData.size() < entriesOffset + entriesSize)
        {
            return std::nullopt;
        }

        std::vector<FileEntry> files;
        const IMG_Entry_V3* entries = reinterpret_cast<const IMG_Entry_V3*>(imgData.data() + entriesOffset);

        // Read filename table
        size_t filenameTableOffset = entriesOffset + entriesSize;
        size_t filenameTableSize = header->tableSize - (header->numItems * header->itemSize);

        if (filenameTableOffset + filenameTableSize > imgData.size())
        {
            return std::nullopt;
        }

        std::vector<std::string> filenames;
        size_t currentOffset = filenameTableOffset;

        for (uint32_t i = 0; i < header->numItems; ++i)
        {
            if (currentOffset >= imgData.size())
            {
                break;
            }

            const char* nameStart = reinterpret_cast<const char*>(imgData.data() + currentOffset);
            size_t maxNameLen = imgData.size() - currentOffset;
            size_t nameLen = strnlen(nameStart, maxNameLen);

            if (currentOffset + nameLen >= imgData.size())
            {
                break;
            }

            filenames.emplace_back(nameStart, nameLen);
            currentOffset += nameLen + 1; // +1 for null terminator
        }

        for (uint32_t i = 0; i < header->numItems && i < filenames.size(); ++i)
        {
            const IMG_Entry_V3& entry = entries[i];

            uint32_t actualSize = GetActualFileSize(entry);
            uint32_t dataOffset = entry.offsetBlock * IMG_BLOCK_SIZE;

            if (dataOffset + actualSize > imgData.size())
            {
                continue; // Skip invalid entries
            }

            FileEntry file;
            file.name = filenames[i];
            file.data.assign(imgData.begin() + dataOffset, imgData.begin() + dataOffset + actualSize);
            files.push_back(std::move(file));
        }

        return std::make_pair(files, IMG_VERSION_3);
    }

    static std::vector<FileEntry> GetReplacementFiles(const std::filesystem::path& updateFolderPath)
    {
        std::vector<FileEntry> replacementFiles;
        std::error_code ec;

        if (!std::filesystem::exists(updateFolderPath, ec) || !std::filesystem::is_directory(updateFolderPath, ec))
        {
            return replacementFiles;
        }

        // Scan folder for replacement files
        constexpr auto perms = std::filesystem::directory_options::skip_permission_denied |
            std::filesystem::directory_options::follow_directory_symlink;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(updateFolderPath, perms, ec))
        {
            if (ec) continue;
            if (!entry.is_regular_file(ec)) continue;

            // Load replacement file
            std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
            if (!file.is_open()) continue;

            auto size = file.tellg();
            if (size > MAX_FILESIZE) continue; // Skip files that are too large

            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileData(static_cast<size_t>(size));
            if (!file.read(reinterpret_cast<char*>(fileData.data()), size)) continue;

            FileEntry replacementFile;
            replacementFile.name = entry.path().filename().string();
            replacementFile.data = std::move(fileData);
            replacementFile.isReplacement = true;

            replacementFiles.push_back(std::move(replacementFile));
        }

        return replacementFiles;
    }

    // Merge original files with replacement files
    static std::vector<FileEntry> MergeFiles(const std::vector<FileEntry>& originalFiles, const std::vector<FileEntry>& replacementFiles)
    {
        // Create lookup map for replacements (case-insensitive)
        std::unordered_map<std::string, const FileEntry*> replacementMap;
        for (const auto& file : replacementFiles)
        {
            std::string lowerName = file.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            replacementMap[lowerName] = &file;
        }

        std::vector<FileEntry> mergedFiles;
        std::unordered_set<std::string> processedFiles;

        // Process original files (replace if needed)
        for (const auto& originalFile : originalFiles)
        {
            std::string lowerName = originalFile.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            auto it = replacementMap.find(lowerName);
            if (it != replacementMap.end())
            {
                // Use replacement file
                FileEntry mergedFile = *it->second;
                mergedFile.name = originalFile.name; // Keep original casing
                mergedFiles.push_back(std::move(mergedFile));
                processedFiles.insert(lowerName);
            }
            else
            {
                // Keep original file
                mergedFiles.push_back(originalFile);
            }
        }

        // Add new files that weren't replacements
        for (const auto& replacementFile : replacementFiles)
        {
            std::string lowerName = replacementFile.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (processedFiles.find(lowerName) == processedFiles.end())
            {
                // This is a new file, add it
                mergedFiles.push_back(replacementFile);
            }
        }

        return mergedFiles;
    }

    // Create GTA SA format IMG (Version 2)
    static std::shared_ptr<std::vector<uint8_t>> CreateImgV2(std::vector<FileEntry>& entries)
    {
        // Calculate header size
        size_t headerSize = sizeof(IMG_Header_V2) + (entries.size() * sizeof(IMG_Entry_V2));
        size_t alignedHeaderSize = AlignToBlocks(headerSize);

        // Calculate file positions and total size
        uint32_t currentPosition = static_cast<uint32_t>(alignedHeaderSize / IMG_BLOCK_SIZE);
        size_t totalSize = alignedHeaderSize;

        for (auto& entry : entries)
        {
            entry.position = currentPosition;
            size_t alignedFileSize = AlignToBlocks(entry.data.size());
            currentPosition += static_cast<uint32_t>(alignedFileSize / IMG_BLOCK_SIZE);
            totalSize += alignedFileSize;
        }

        // Create IMG data
        auto imgData = std::make_shared<std::vector<uint8_t>>(totalSize);
        uint8_t* data = imgData->data();
        size_t offset = 0;

        // Write header
        IMG_Header_V2 header;
        memcpy(header.signature, "VER2", 4);
        header.numFiles = static_cast<uint32_t>(entries.size());
        memcpy(data + offset, &header, sizeof(header));
        offset += sizeof(header);

        // Write file entries
        for (const auto& entry : entries)
        {
            IMG_Entry_V2 imgEntry = {};
            imgEntry.position = entry.position;

            size_t sizeInBlocks = AlignToBlocks(entry.data.size()) / IMG_BLOCK_SIZE;
            if (sizeInBlocks <= 0xFFFF)
            {
                imgEntry.sizeLow = static_cast<uint16_t>(sizeInBlocks);
                imgEntry.sizeHigh = 0;
            }
            else
            {
                imgEntry.sizeLow = 0;
                imgEntry.sizeHigh = static_cast<uint16_t>(sizeInBlocks);
            }

            strncpy_s(imgEntry.name, sizeof(imgEntry.name), entry.name.c_str(), _TRUNCATE);

            memcpy(data + offset, &imgEntry, sizeof(imgEntry));
            offset += sizeof(imgEntry);
        }

        // Pad header to block boundary
        offset = alignedHeaderSize;

        // Write file data
        for (const auto& entry : entries)
        {
            // Write file data
            memcpy(data + offset, entry.data.data(), entry.data.size());
            offset += entry.data.size();

            // Pad to block boundary
            while (offset % IMG_BLOCK_SIZE != 0)
            {
                data[offset++] = 0;
            }
        }

        return imgData;
    }

    struct RSCHeader
    {
        static constexpr uint32_t MAGIC_VALUE = 0x05435352;

        uint32_t magic;
        uint32_t type;
        uint32_t flags;
        uint16_t compressCodec;
        uint16_t padding;
    };

    // Helper function to check if file data represents an RSC resource
    static bool IsResourceFile(const std::vector<uint8_t>& fileData)
    {
        if (fileData.size() < sizeof(RSCHeader))
            return false;

        const RSCHeader* header = reinterpret_cast<const RSCHeader*>(fileData.data());
        return (header->magic == RSCHeader::MAGIC_VALUE);
    }

    // Helper function to get RSC flags and resource type from file data
    static std::pair<uint32_t, uint32_t> GetRSCInfo(const std::vector<uint8_t>& fileData)
    {
        if (fileData.size() < sizeof(RSCHeader))
            return { 0, 0 }; // Default values

        const RSCHeader* header = reinterpret_cast<const RSCHeader*>(fileData.data());

        // Validate magic number first
        if (header->magic != RSCHeader::MAGIC_VALUE)
            return { 0, 0 }; // Default values for non-RSC files

        uint32_t flags = header->flags;
        uint32_t type = header->type;

        return { flags, type };
    }

    // Create GTA IV format IMG (Version 3)
    static std::shared_ptr<std::vector<uint8_t>> CreateImgV3(std::vector<FileEntry>& entries, bool encrypted = false)
    {
        // Calculate filename table size
        size_t filenameTableSize = 0;
        for (const auto& entry : entries)
        {
            filenameTableSize += entry.name.length() + 1; // +1 for null terminator
        }

        // Calculate TOC size
        size_t baseTocSize = sizeof(IMG_Header_V3) + (entries.size() * sizeof(IMG_Entry_V3)) + filenameTableSize;

        // Add 32 bytes
        size_t tocWithPadding = baseTocSize + 32;

        // Align the entire TOC to block boundaries
        size_t alignedTocSize = AlignToBlocks(tocWithPadding);

        // Calculate file positions and total size
        uint32_t currentPosition = static_cast<uint32_t>(alignedTocSize / IMG_BLOCK_SIZE);
        size_t totalSize = alignedTocSize;

        for (auto& entry : entries)
        {
            entry.position = currentPosition;
            size_t alignedFileSize = AlignToBlocks(entry.data.size());
            currentPosition += static_cast<uint32_t>(alignedFileSize / IMG_BLOCK_SIZE);
            totalSize += alignedFileSize;
        }

        // Create IMG data with exact calculated size
        auto imgData = std::make_shared<std::vector<uint8_t>>(totalSize, 0);
        uint8_t* data = imgData->data();
        size_t offset = 0;

        // Write header
        IMG_Header_V3 header = {};
        header.magicId = GTAIV_MAGIC_ID;
        header.version = 3;
        header.numItems = static_cast<uint32_t>(entries.size());
        header.tableSize = static_cast<uint32_t>((entries.size() * sizeof(IMG_Entry_V3)) + filenameTableSize);
        header.itemSize = sizeof(IMG_Entry_V3);
        header.unknown = 0x00E9;

        memcpy(data + offset, &header, sizeof(header));
        offset += sizeof(header);

        for (const auto& entry : entries)
        {
            IMG_Entry_V3 imgEntry = {};

            bool isResourceFile = IsResourceFile(entry.data);

            if (isResourceFile)
            {
                // Handle RSC files
                auto [rscFlags, resourceType] = GetRSCInfo(entry.data);

                // For RSC files, the sizeOrRSCFlags field contains the RSC flags from file header
                imgEntry.sizeOrRSCFlags = rscFlags;
                imgEntry.resourceType = resourceType;

                // Calculate padding for RSC files
                size_t alignedSize = AlignToBlocks(entry.data.size());
                uint16_t paddingCount = static_cast<uint16_t>(alignedSize - entry.data.size());

                // Store padding in lower 11 bits and set RSC flag
                imgEntry.flags = (paddingCount & 0x7FF) | 0x2000; // Set bit 13 to indicate RSC
            }
            else
            {
                // Handle normal files
                imgEntry.sizeOrRSCFlags = static_cast<uint32_t>(entry.data.size());
                imgEntry.resourceType = 0;
                imgEntry.flags = 0;
            }

            imgEntry.offsetBlock = entry.position;
            imgEntry.usedBlocks = static_cast<uint16_t>(AlignToBlocks(entry.data.size()) / IMG_BLOCK_SIZE);

            memcpy(data + offset, &imgEntry, sizeof(imgEntry));
            offset += sizeof(imgEntry);
        }

        // Write filename table
        for (const auto& entry : entries)
        {
            memcpy(data + offset, entry.name.c_str(), entry.name.length());
            offset += entry.name.length();
            data[offset++] = 0;
        }

        // Skip over the 32 bytes padding
        offset += 32;

        // Write file data at their calculated positions
        for (const auto& entry : entries)
        {
            size_t fileOffset = entry.position * IMG_BLOCK_SIZE;
            size_t allocatedSize = AlignToBlocks(entry.data.size());

            // Write the actual file data
            memcpy(data + fileOffset, entry.data.data(), entry.data.size());
        }

        // Apply encryption if requested
        if (encrypted)
        {
            EncryptGTAIVArchive(*imgData);
        }

        return imgData;
    }

    // Helper function to align size to IMG block boundaries
    static size_t AlignToBlocks(size_t size)
    {
        return ((size + IMG_BLOCK_SIZE - 1) / IMG_BLOCK_SIZE) * IMG_BLOCK_SIZE;
    }
};

inline std::shared_ptr<std::vector<uint8_t>> MergeImgWithFolder(const std::filesystem::path& originalImgPath, const std::filesystem::path& updateFolderPath)
{
    return ImgProcessor::MergeImgWithFolder(originalImgPath, updateFolderPath);
}

inline std::shared_ptr<std::vector<uint8_t>> CreateImgFromFolder(const std::filesystem::path& folderPath, eIMG_version version = IMG_VERSION_3, bool encrypted = false)
{
    return ImgProcessor::CreateImgFromFolder(folderPath, version, encrypted);
}

inline std::shared_ptr<std::vector<uint8_t>> CreateImgFromFileList(const std::vector<VFSFileEntry>& fileList, eIMG_version version = IMG_VERSION_3, bool encrypted = false)
{
    return ImgProcessor::CreateImgFromFileList(fileList, version, encrypted);
}

class IMGLoader
{
public:
    IMGLoader()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            static bool bLoadIMG = iniReader.ReadInteger("FILELOADER", "LoadIMG", 0) != 0;

            std::wstring s;
            s.resize(MAX_PATH, L'\0');
            if (!UAL::GetOverloadPathW || !UAL::GetOverloadPathW(s.data(), s.size()))
            {
                s = GetExeModulePath() / L"update";
            }
            else
            {
                // Redirect update device
                static std::string update = std::filesystem::path(s.data()).filename().string();
                static std::string supdate = "%s" + update;

                auto pattern = find_pattern("68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? 68 ? ? ? ? EB", "68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? 68 ? ? ? ? EB");
                injector::WriteMemory(pattern.get_first(1), update.data(), true);

                pattern = find_pattern("68 ? ? ? ? 50 E8 ? ? ? ? 83 C4 ? 8D 04 24 6A", "68 ? ? ? ? 50 E8 ? ? ? ? 83 C4 ? 6A ? 8D 4C 24 ? 51 B9");
                injector::WriteMemory(pattern.get_first(1), supdate.data(), true);
            }

            static auto updatePath = std::filesystem::path(s.data());

            if (bLoadIMG && UAL::AddVirtualFileForOverloadW)
            {
                static std::future<void> BuildIMGsFuture = std::async(std::launch::async, []()
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

                            // Check for folder with .img extension
                            if (std::filesystem::is_directory(it, ec) && iequals(folderPath.extension().native(), L".img"))
                            {
                                // Look for corresponding original IMG
                                auto relativePath = lexicallyRelativeCaseIns(folderPath, updatePath);
                                auto originalImgPath = GetExeModulePath() / relativePath;

                                // Change extension back to .img for the original file
                                originalImgPath.replace_extension(".img");

                                if (std::filesystem::exists(originalImgPath, ec) && std::filesystem::is_regular_file(originalImgPath, ec))
                                {
                                    // Merge original IMG with the folder inside update
                                    auto mergedImgData = MergeImgWithFolder(originalImgPath, folderPath);
                                    if (mergedImgData)
                                    {
                                        UAL::AddVirtualFileForOverloadW(relativePath.wstring().c_str(), mergedImgData->data(), mergedImgData->size(), 1000);
                                    }
                                }
                                else
                                {
                                    auto ImgData = CreateImgFromFolder(folderPath);
                                    if (ImgData)
                                    {
                                        auto gamePath = GetExeModulePath();
                                        auto path = lexicallyRelativeCaseIns(folderPath, gamePath);
                                        UAL::AddVirtualFileForOverloadW(path.wstring().c_str(), ImgData->data(), ImgData->size(), 1000);
                                    }
                                }
                            }
                        }
                    }
                });

                FusionFix::onReadGameConfig() += []()
                {
                    if (BuildIMGsFuture.valid())
                        BuildIMGsFuture.wait();
                };
            }

            //IMG Loader
            auto pattern = find_pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 14 6A 00 B9 ? ? ? ? E8 ? ? ? ? 83 3D", "E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 14 6A 00 B9");
            static auto CImgManager__addImgFile = (void(__cdecl*)(const char*, char, int)) injector::GetBranchDestination(pattern.get_first(0)).get();

            pattern = find_pattern("89 44 24 44 8B 44 24 4C 53 68 ? ? ? ? 50 E8 ? ? ? ? 8B D8 6A 01 53 E8 ? ? ? ? 83 C4 10", "89 44 24 44 8B 44 24 4C 57 68 ? ? ? ? 50 E8 ? ? ? ? 8B F8 6A 01 57 E8 ? ? ? ? 83 C4 10");
            struct ImgListHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    std::vector<const char*> defaultImgList =
                    {
                        "update/update.img",
                        "common/data/cdimages/carrec.img",
                        "common/data/cdimages/navgen_script.img",
                        "common/data/cdimages/script.img",
                        "common/data/cdimages/script_network.img",
                        "pc/anim/anim.img",
                        "pc/anim/cuts.img",
                        "pc/anim/cutsprops.img",
                        "pc/data/cdimages/gtxd.img",
                        "pc/data/cdimages/navmeshes.img",
                        "pc/data/cdimages/navmeshes_animviewer.img",
                        "pc/data/cdimages/paths.img",
                        "pc/data/cdimages/scripttxds.img",
                        "pc/data/maps/east/bronx_e.img",
                        "pc/data/maps/east/bronx_e2.img",
                        "pc/data/maps/east/bronx_w.img",
                        "pc/data/maps/east/bronx_w2.img",
                        "pc/data/maps/east/brook_n.img",
                        "pc/data/maps/east/brook_n2.img",
                        "pc/data/maps/east/brook_s.img",
                        "pc/data/maps/east/brook_s2.img",
                        "pc/data/maps/east/brook_s3.img",
                        "pc/data/maps/east/east_xr.img",
                        "pc/data/maps/east/queens_e.img",
                        "pc/data/maps/east/queens_m.img",
                        "pc/data/maps/east/queens_w.img",
                        "pc/data/maps/east/queens_w2.img",
                        "pc/data/maps/generic/lodcull_e.img",
                        "pc/data/maps/generic/lodcull_j.img",
                        "pc/data/maps/generic/lodcull_m.img",
                        "pc/data/maps/generic/procobj.img",
                        "pc/data/maps/interiors/generic/bars_1.img",
                        "pc/data/maps/interiors/generic/bars_2.img",
                        "pc/data/maps/interiors/generic/bars_3.img",
                        "pc/data/maps/interiors/generic/blocks.img",
                        "pc/data/maps/interiors/generic/blocks_2.img",
                        "pc/data/maps/interiors/generic/blocks_3.img",
                        "pc/data/maps/interiors/generic/brownstones.img",
                        "pc/data/maps/interiors/generic/homes_1.img",
                        "pc/data/maps/interiors/generic/homes_2.img",
                        "pc/data/maps/interiors/generic/homes_3.img",
                        "pc/data/maps/interiors/generic/indust_1.img",
                        "pc/data/maps/interiors/generic/public_1.img",
                        "pc/data/maps/interiors/generic/public_2.img",
                        "pc/data/maps/interiors/generic/public_3.img",
                        "pc/data/maps/interiors/generic/retail_1.img",
                        "pc/data/maps/interiors/generic/retail_2.img",
                        "pc/data/maps/interiors/generic/retail_3.img",
                        "pc/data/maps/interiors/generic/retail_4.img",
                        "pc/data/maps/interiors/int_props/props_ab.img",
                        "pc/data/maps/interiors/int_props/props_ah.img",
                        "pc/data/maps/interiors/int_props/props_km.img",
                        "pc/data/maps/interiors/int_props/props_ld.img",
                        "pc/data/maps/interiors/int_props/props_mp.img",
                        "pc/data/maps/interiors/int_props/props_ss.img",
                        "pc/data/maps/interiors/mission/level_1.img",
                        "pc/data/maps/interiors/mission/level_2.img",
                        "pc/data/maps/interiors/mission/level_3.img",
                        "pc/data/maps/interiors/mission/level_4.img",
                        "pc/data/maps/interiors/mission/level_5.img",
                        "pc/data/maps/interiors/mission/level_6.img",
                        "pc/data/maps/interiors/test/interiors.img",
                        "pc/data/maps/jersey/nj_01.img",
                        "pc/data/maps/jersey/nj_02.img",
                        "pc/data/maps/jersey/nj_03.img",
                        "pc/data/maps/jersey/nj_04e.img",
                        "pc/data/maps/jersey/nj_04w.img",
                        "pc/data/maps/jersey/nj_05.img",
                        "pc/data/maps/jersey/nj_docks.img",
                        "pc/data/maps/jersey/nj_liberty.img",
                        "pc/data/maps/jersey/nj_xref.img",
                        "pc/data/maps/leveldes/levelmap.img",
                        "pc/data/maps/leveldes/level_xr.img",
                        "pc/data/maps/manhat/manhat01.img",
                        "pc/data/maps/manhat/manhat02.img",
                        "pc/data/maps/manhat/manhat03.img",
                        "pc/data/maps/manhat/manhat04.img",
                        "pc/data/maps/manhat/manhat05.img",
                        "pc/data/maps/manhat/manhat06.img",
                        "pc/data/maps/manhat/manhat07.img",
                        "pc/data/maps/manhat/manhat08.img",
                        "pc/data/maps/manhat/manhat09.img",
                        "pc/data/maps/manhat/manhat10.img",
                        "pc/data/maps/manhat/manhat11.img",
                        "pc/data/maps/manhat/manhat12.img",
                        "pc/data/maps/manhat/manhatsw.img",
                        "pc/data/maps/manhat/manhatxr.img",
                        "pc/data/maps/manhat/subwayxr.img",
                        "pc/data/maps/props/commercial/7_11.img",
                        "pc/data/maps/props/commercial/bar.img",
                        "pc/data/maps/props/commercial/beauty.img",
                        "pc/data/maps/props/commercial/clothes.img",
                        "pc/data/maps/props/commercial/fastfood.img",
                        "pc/data/maps/props/commercial/garage.img",
                        "pc/data/maps/props/commercial/office.img",
                        "pc/data/maps/props/doors/ext_door.img",
                        "pc/data/maps/props/doors/int_door.img",
                        "pc/data/maps/props/industrial/build.img",
                        "pc/data/maps/props/industrial/drums.img",
                        "pc/data/maps/props/industrial/industrial.img",
                        "pc/data/maps/props/industrial/railway.img",
                        "pc/data/maps/props/industrial/skips.img",
                        "pc/data/maps/props/lev_des/icons.img",
                        "pc/data/maps/props/lev_des/minigame.img",
                        "pc/data/maps/props/lev_des/mission_int.img",
                        "pc/data/maps/props/prop_test/cj_test.img",
                        "pc/data/maps/props/residential/bathroom.img",
                        "pc/data/maps/props/residential/bedroom.img",
                        "pc/data/maps/props/residential/details.img",
                        "pc/data/maps/props/residential/dining.img",
                        "pc/data/maps/props/residential/electrical.img",
                        "pc/data/maps/props/residential/kitchen.img",
                        "pc/data/maps/props/residential/lights.img",
                        "pc/data/maps/props/residential/soft.img",
                        "pc/data/maps/props/roadside/bins.img",
                        "pc/data/maps/props/roadside/bllbrd.img",
                        "pc/data/maps/props/roadside/crates.img",
                        "pc/data/maps/props/roadside/fences.img",
                        "pc/data/maps/props/roadside/lamppost.img",
                        "pc/data/maps/props/roadside/rubbish.img",
                        "pc/data/maps/props/roadside/sign.img",
                        "pc/data/maps/props/roadside/st_vend.img",
                        "pc/data/maps/props/roadside/traffic.img",
                        "pc/data/maps/props/roadside/works.img",
                        "pc/data/maps/props/street/amenitie.img",
                        "pc/data/maps/props/street/elecbox.img",
                        "pc/data/maps/props/street/misc.img",
                        "pc/data/maps/props/street/rooftop.img",
                        "pc/data/maps/props/vegetation/ext_veg.img",
                        "pc/data/maps/props/vegetation/int_veg.img",
                        "pc/data/maps/props/windows/gen_win.img",
                        "pc/data/maps/util/animviewer.img",
                        "pc/models/cdimages/componentpeds.img",
                        "pc/models/cdimages/pedprops.img",
                        "pc/models/cdimages/radar.img",
                        "pc/models/cdimages/vehicles.img",
                        "pc/models/cdimages/weapons.img",
                        "TBoGT/common/data/cdimages/carrec.img",
                        "TBoGT/common/data/cdimages/script.img",
                        "TBoGT/common/data/cdimages/script_network.img",
                        "TBoGT/pc/anim/anim.img",
                        "TBoGT/pc/anim/cuts.img",
                        "TBoGT/pc/anim/cutsprops.img",
                        "TBoGT/pc/data/cdimages/e2_scripttxds.img",
                        "TBoGT/pc/data/cdimages/navmeshes.img",
                        "TBoGT/pc/data/cdimages/paths.img",
                        "TBoGT/pc/data/maps/east/bronx_e.img",
                        "TBoGT/pc/data/maps/east/bronx_e2.img",
                        "TBoGT/pc/data/maps/east/bronx_w.img",
                        "TBoGT/pc/data/maps/east/bronx_w2.img",
                        "TBoGT/pc/data/maps/east/brook_n.img",
                        "TBoGT/pc/data/maps/east/brook_n2.img",
                        "TBoGT/pc/data/maps/east/brook_s.img",
                        "TBoGT/pc/data/maps/east/brook_s2.img",
                        "TBoGT/pc/data/maps/east/queens_e.img",
                        "TBoGT/pc/data/maps/east/queens_m.img",
                        "TBoGT/pc/data/maps/east/queens_w2.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_1.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_10.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_11.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_12.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_13.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_14.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_15.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_2.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_3.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_4.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_5.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_6.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_7.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_8.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_9.img",
                        "TBoGT/pc/data/maps/interiors/int_1.img",
                        "TBoGT/pc/data/maps/interiors/int_2.img",
                        "TBoGT/pc/data/maps/interiors/int_3.img",
                        "TBoGT/pc/data/maps/interiors/int_4.img",
                        "TBoGT/pc/data/maps/interiors/int_5.img",
                        "TBoGT/pc/data/maps/interiors/int_6.img",
                        "TBoGT/pc/data/maps/interiors/int_7.img",
                        "TBoGT/pc/data/maps/interiors/int_8.img",
                        "TBoGT/pc/data/maps/interiors/int_9.img",
                        "TBoGT/pc/data/maps/interiors/rep_public_3.img",
                        "TBoGT/pc/data/maps/jersey/nj_01.img",
                        "TBoGT/pc/data/maps/jersey/nj_02.img",
                        "TBoGT/pc/data/maps/jersey/nj_03.img",
                        "TBoGT/pc/data/maps/jersey/nj_04w.img",
                        "TBoGT/pc/data/maps/jersey/nj_05.img",
                        "TBoGT/pc/data/maps/jersey/nj_docks.img",
                        "TBoGT/pc/data/maps/jersey/nj_liberty.img",
                        "TBoGT/pc/data/maps/manhat/manhat01.img",
                        "TBoGT/pc/data/maps/manhat/manhat02.img",
                        "TBoGT/pc/data/maps/manhat/manhat03.img",
                        "TBoGT/pc/data/maps/manhat/manhat04.img",
                        "TBoGT/pc/data/maps/manhat/manhat05.img",
                        "TBoGT/pc/data/maps/manhat/manhat06.img",
                        "TBoGT/pc/data/maps/manhat/manhat07.img",
                        "TBoGT/pc/data/maps/manhat/manhat08.img",
                        "TBoGT/pc/data/maps/manhat/manhat09.img",
                        "TBoGT/pc/data/maps/manhat/manhat10.img",
                        "TBoGT/pc/data/maps/manhat/manhat11.img",
                        "TBoGT/pc/data/maps/manhat/manhat12.img",
                        "TBoGT/pc/data/maps/props/e2_xref.img",
                        "TBoGT/pc/data/maps/props/commercial/office.img",
                        "TBoGT/pc/models/cdimages/componentpeds.img",
                        "TBoGT/pc/models/cdimages/pedprops.img",
                        "TBoGT/pc/models/cdimages/vehicles.img",
                        "TBoGT/pc/models/cdimages/weapons_e2.img",
                        "TLAD/common/data/cdimages/carrec.img",
                        "TLAD/common/data/cdimages/script.img",
                        "TLAD/common/data/cdimages/script_network.img",
                        "TLAD/pc/anim/anim.img",
                        "TLAD/pc/anim/cuts.img",
                        "TLAD/pc/anim/cutsprops.img",
                        "TLAD/pc/data/cdimages/navmeshes.img",
                        "TLAD/pc/data/cdimages/paths.img",
                        "TLAD/pc/data/cdimages/scripttxds.img",
                        "TLAD/pc/data/maps/east/bronx_e.img",
                        "TLAD/pc/data/maps/east/bronx_e2.img",
                        "TLAD/pc/data/maps/east/bronx_w.img",
                        "TLAD/pc/data/maps/east/bronx_w2.img",
                        "TLAD/pc/data/maps/east/brook_n2.img",
                        "TLAD/pc/data/maps/east/brook_s.img",
                        "TLAD/pc/data/maps/east/queens_e.img",
                        "TLAD/pc/data/maps/east/queens_w2.img",
                        "TLAD/pc/data/maps/interiors/int_1.img",
                        "TLAD/pc/data/maps/interiors/int_2.img",
                        "TLAD/pc/data/maps/interiors/int_3.img",
                        "TLAD/pc/data/maps/interiors/int_4.img",
                        "TLAD/pc/data/maps/interiors/int_5.img",
                        "TLAD/pc/data/maps/interiors/int_6.img",
                        "TLAD/pc/data/maps/interiors/int_7.img",
                        "TLAD/pc/data/maps/interiors/int_8.img",
                        "TLAD/pc/data/maps/interiors/int_9.img",
                        "TLAD/pc/data/maps/interiors/int_test.img",
                        "TLAD/pc/data/maps/jersey/nj_01.img",
                        "TLAD/pc/data/maps/jersey/nj_02.img",
                        "TLAD/pc/data/maps/jersey/nj_03.img",
                        "TLAD/pc/data/maps/jersey/nj_04w.img",
                        "TLAD/pc/data/maps/jersey/nj_05.img",
                        "TLAD/pc/data/maps/jersey/nj_docks.img",
                        "TLAD/pc/data/maps/manhat/manhat01.img",
                        "TLAD/pc/data/maps/manhat/manhat02.img",
                        "TLAD/pc/data/maps/manhat/manhat04.img",
                        "TLAD/pc/data/maps/manhat/manhat05.img",
                        "TLAD/pc/data/maps/manhat/manhat06.img",
                        "TLAD/pc/data/maps/manhat/manhat07.img",
                        "TLAD/pc/data/maps/manhat/manhat08.img",
                        "TLAD/pc/data/maps/manhat/manhat09.img",
                        "TLAD/pc/data/maps/manhat/manhat11.img",
                        "TLAD/pc/data/maps/manhat/manhat12.img",
                        "TLAD/pc/data/maps/props/e1_xref.img",
                        "TLAD/pc/models/cdimages/componentpeds.img",
                        "TLAD/pc/models/cdimages/pedprops.img",
                        "TLAD/pc/models/cdimages/vehicles.img",
                        "TLAD/pc/models/cdimages/weapons_e1.img",
                    };

                    *(uint32_t*)(regs.esp + 0x44) = regs.eax;
                    regs.eax = *(uint32_t*)(regs.esp + 0x4C);

                    if (*_dwCurrentEpisode) {
                        if (std::string_view((const char*)regs.eax).contains(":"))
                            return;
                    }

                    std::vector<std::filesystem::path> episodicPaths = {
                        std::filesystem::path("IV"),
                        std::filesystem::path("TLAD"),
                        std::filesystem::path("TBoGT"),
                        std::filesystem::path("VICECITY"),
                    };

                    auto gamePath = GetExeModulePath();
                    std::error_code ec;

                    if (std::filesystem::exists(updatePath, ec))
                    {
                        // Collect all IMG files first
                        std::vector<std::filesystem::path> imgFiles;

                        constexpr auto perms = std::filesystem::directory_options::skip_permission_denied |
                            std::filesystem::directory_options::follow_directory_symlink;

                        for (const auto& file : std::filesystem::recursive_directory_iterator(updatePath, perms, ec))
                        {
                            if (ec) continue;

                            auto filePath = std::filesystem::path(file.path());

                            if (iequals(filePath.extension().native(), L".img"))
                            {
                                if ((!bLoadIMG || !UAL::AddVirtualFileForOverloadW) && std::filesystem::is_directory(file, ec))
                                    continue;

                                auto relativePath = lexicallyRelativeCaseIns(filePath, gamePath);
                                auto imgPath = relativePath.string();
                                std::replace(std::begin(imgPath), std::end(imgPath), '\\', '/');
                                auto pos = imgPath.find('/');

                                if (pos != imgPath.npos)
                                {
                                    imgPath = imgPath.substr(pos + 1);

                                    if (std::any_of(defaultImgList.begin(), defaultImgList.end(), [&](const char* s) { return iequals(s, imgPath.c_str()); }))
                                        continue;

                                    if (CText::hasViceCityStrings() && imgPath == "GTAIV.EFLC.FusionFix/GTAIV.EFLC.FusionFix.img")
                                        continue;

                                    imgFiles.push_back(relativePath);
                                }
                            }
                        }

                        static auto StrCmpLogicalW = [](const wchar_t* str1, const wchar_t* str2) -> int {
                            if (!str1 || !str2) return str1 ? 1 : (str2 ? -1 : 0);

                            while (*str1 || *str2)
                            {
                                // Handle numeric sequences
                                if (std::iswdigit(*str1) && std::iswdigit(*str2))
                                {
                                    // Skip leading zeros
                                    while (*str1 == L'0') str1++;
                                    while (*str2 == L'0') str2++;

                                    // Count digits
                                    int len1 = 0, len2 = 0;
                                    const wchar_t* p1 = str1;
                                    const wchar_t* p2 = str2;

                                    while (std::iswdigit(*p1))
                                    {
                                        p1++; len1++;
                                    }
                                    while (std::iswdigit(*p2))
                                    {
                                        p2++; len2++;
                                    }

                                    // Compare by length first (longer number is greater)
                                    if (len1 != len2) return len1 - len2;

                                    // Same length, compare digit by digit
                                    for (int i = 0; i < len1; i++)
                                    {
                                        if (str1[i] != str2[i]) return str1[i] - str2[i];
                                    }

                                    str1 += len1;
                                    str2 += len2;
                                }
                                else
                                {
                                    // Regular character comparison (case-insensitive)
                                    wchar_t c1 = std::towlower(*str1);
                                    wchar_t c2 = std::towlower(*str2);

                                    if (c1 != c2) return c1 - c2;

                                    if (*str1) str1++;
                                    if (*str2) str2++;
                                }
                            }

                            return 0;
                        };

                        // Sort IMG files: group by root directory, then by depth within each group, then alphabetically
                        std::sort(imgFiles.begin(), imgFiles.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
                            // Get the first directory after "update" for both paths
                            std::filesystem::path rootDirA, rootDirB;

                            auto itA = a.begin();
                            auto itB = b.begin();

                            // Skip "update" and get the first subdirectory
                            if (itA != a.end()) ++itA; // Skip "update"
                            if (itB != b.end()) ++itB; // Skip "update"

                            if (itA != a.end()) rootDirA = *itA;
                            if (itB != b.end()) rootDirB = *itB;

                            // Compare root directories first (case-insensitive)
                            std::wstring rootA = rootDirA.wstring();
                            std::wstring rootB = rootDirB.wstring();
                            std::transform(rootA.begin(), rootA.end(), rootA.begin(), ::towlower);
                            std::transform(rootB.begin(), rootB.end(), rootB.begin(), ::towlower);

                            int rootCompare = rootA.compare(rootB);
                            if (rootCompare != 0)
                                return rootCompare < 0;

                            // If same root directory, then sort by depth
                            size_t depthA = std::distance(a.begin(), a.end()) - 1; // -1 to exclude filename
                            size_t depthB = std::distance(b.begin(), b.end()) - 1; // -1 to exclude filename

                            if (depthA != depthB)
                                return depthA < depthB;

                            // If same root directory and same depth, use logical string comparison
                            return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
                        });

                        // Load sorted IMG files
                        auto contains_subfolder = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool {
                            for (auto& p : path)
                            {
                                if (p == *path.begin())
                                    continue;

                                if (iequals(p.native(), base.native()))
                                    return true;
                            }
                            return false;
                        };

                        for (const auto& relativePath : imgFiles)
                        {
                            auto imgPath = relativePath.string();
                            std::replace(std::begin(imgPath), std::end(imgPath), '\\', '/');
                            auto pos = imgPath.find('/');

                            if (pos != imgPath.npos)
                            {
                                imgPath = imgPath.substr(pos + 1);
                                imgPath = "update:/" + imgPath;

                                if (std::any_of(std::begin(episodicPaths), std::end(episodicPaths), [&](auto& it) { return contains_subfolder(relativePath, it); }))
                                {
                                    auto curEp = *_dwCurrentEpisode;
                                    if (CText::hasViceCityStrings())
                                        curEp = episodicPaths.size() - 1;

                                    if (curEp < int32_t(episodicPaths.size()) && contains_subfolder(relativePath, episodicPaths[curEp]))
                                        CImgManager__addImgFile(imgPath.data(), 1, -1);
                                }
                                else
                                    CImgManager__addImgFile(imgPath.data(), 1, -1);
                            }
                        }
                    }
                }
            }; injector::MakeInline<ImgListHook>(pattern.get_first(0), pattern.get_first(8));
        };
    }
} IMGLoader;
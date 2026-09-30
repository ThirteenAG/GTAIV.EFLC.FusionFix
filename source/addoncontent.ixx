module;

#include <common.hxx>
#include <xmllite.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <charconv>

#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "shlwapi.lib")

export module addoncontent;

import common;
import comvars;

namespace AddonContent
{
    namespace Episodes
    {
        struct Pack
        {
            int32_t mount;
            uint8_t reserved[0x221];
            bool enabled, valid;
            uint8_t networkGame, id, episode, padding[2];
        };

        struct Mount
        {
            char path[0x100];
            uint8_t reserved[0x44];
            char device[0x10];
            void* handle;
            uint32_t type;
            uint8_t reserved15C[8];
            bool mounted, parsed, enabled;
            uint8_t padding;
        };

        struct Manager
        {
            uint8_t reserved[0x144];
            Pack* packs;
            uint16_t packCount, packCapacity;
            Mount* mounts;
            uint16_t mountCount, mountCapacity;
            uint8_t reserved154[0x12];
            bool setupsLoaded;
        };

        static_assert(sizeof(Pack) == 0x22C);
        static_assert(sizeof(Mount) == 0x168);
        static_assert(offsetof(Manager, setupsLoaded) == 0x166);

        struct Content
        {
            uint32_t externalId = 0;
            uint8_t slot = 0, episode = 0;
            std::string name;
        };

        struct Episode
        {
            int32_t id = 0; // Zero denotes a shared content folder, not a new episode.
            std::string name, folder, device, savePrefix;
            std::vector<Content> contents;
            size_t parsedContents = 0;
            std::array<std::string, 16> saveFiles;
            int32_t mount = -1;
            bool mounted = false;
            bool invalid = false;
        };

        static std::vector<Episode> episodes;
        static Manager* registeredManager = nullptr;
        static int32_t (__thiscall* addMount)(Manager*, const Mount*) = nullptr;
        static bool (__thiscall* mountContent)(Manager*, int32_t) = nullptr;
        static SafetyHookInline loadSetups, enableEpisode, addPack;
        static SafetyHookInline storageFileName, nextEpisode, episodeAvailable;
        static SafetyHookMid filterSaves;
        static int32_t* activeEpisode = nullptr;
        static int32_t* requestedEpisode = nullptr;
        static uint8_t* hasRequestedEpisode = nullptr;
        static bool installed = false;

        static Episode* Find(int32_t id)
        {
            if (id < 3) return nullptr;
            for (auto& episode : episodes)
                if (episode.id == id) return &episode;
            return nullptr;
        }

        static void Log(const std::string& message)
        {
            OutputDebugStringA(("FusionFix addons: " + message + "\n").c_str());
        }

        static bool Number(std::string_view text, uint32_t& value)
        {
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }

        static std::string Narrow(const wchar_t* value, UINT length)
        {
            const auto size = WideCharToMultiByte(CP_UTF8, 0, value, length, nullptr, 0, nullptr, nullptr);
            std::string result(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, value, length, result.data(), size, nullptr, nullptr);
            return result;
        }

        static bool ReadSetup(const std::filesystem::path& path, Episode& folder)
        {
            Microsoft::WRL::ComPtr<IStream> stream;
            Microsoft::WRL::ComPtr<IXmlReader> reader;
            if (FAILED(SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr, &stream)) ||
                FAILED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.GetAddressOf()), nullptr)) ||
                FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit)) ||
                FAILED(reader->SetInput(stream.Get()))) return false;
            std::vector<std::string> elements;
            std::string value;
            Content content;
            bool inContent = false, hasId = false, valid = true;
            auto finish = [&]()
            {
                const auto first = value.find_first_not_of(" \t\r\n");
                value = first == value.npos ? "" : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
                const auto& tag = elements.back();
                if (elements.size() == 2 && tag == "device") folder.device = value;
                if (elements.size() == 2 && tag == "testmarketplace") valid = false;
                if (inContent && elements.size() == 3)
                {
                    if (tag == "id") hasId = Number(value, content.externalId) && content.externalId >= 5;
                    else if (tag == "name") content.name = value;
                    else if (tag == "episode")
                    {
                        uint32_t id = 0;
                        valid &= Number(value, id) && id >= 3 && id < 64;
                        content.episode = static_cast<uint8_t>(id);
                        if (folder.id && folder.id != id) valid = false;
                        folder.id = id;
                    }
                    // The engine copies these into fixed-size buffers. Reject
                    // oversized fields before handing the file to its parser.
                    else if (tag == "datfile") valid &= value.size() < 32;
                    else if (tag == "audiofolder" || tag == "audiometadata") valid &= value.size() < 64;
                    else if (tag == "loadingscreens" || tag == "loadingscreensdat" ||
                        tag == "loadingscreensingame" || tag == "loadingscreensingamedat" || tag == "texturepath")
                        valid &= value.size() + folder.device.size() + 2 < 64;
                }
                if (elements.size() == 2 && tag == "content")
                {
                    valid &= hasId && !content.name.empty() && content.name.size() < 64;
                    folder.contents.push_back(content);
                    if (content.episode || folder.name.empty()) folder.name = content.name;
                    inContent = false;
                }
                elements.pop_back();
                value.clear();
            };
            XmlNodeType type;
            HRESULT status;
            while ((status = reader->Read(&type)) == S_OK)
            {
                if (type == XmlNodeType_Element)
                {
                    const wchar_t* name; UINT length;
                    if (FAILED(reader->GetLocalName(&name, &length))) return false;
                    elements.push_back(Narrow(name, length));
                    value.clear();
                    if (elements.size() == 1 && elements.back() != "ini") return false;
                    if (elements.size() == 2 && elements.back() == "content")
                    {
                        content = {}; hasId = false; inContent = true;
                    }
                    if (reader->IsEmptyElement()) finish();
                }
                else if (type == XmlNodeType_EndElement) finish();
                else if (type == XmlNodeType_Text || type == XmlNodeType_CDATA || type == XmlNodeType_Whitespace)
                {
                    const wchar_t* text; UINT length;
                    if (FAILED(reader->GetValue(&text, &length))) return false;
                    value += Narrow(text, length);
                }
            }
            return status == S_FALSE && valid && !folder.contents.empty() && !folder.device.empty() &&
                folder.device.size() <= 13 && folder.device.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == folder.device.npos;
        }

        static void ReadDefinitions()
        {
            CIniReader settings("");
            const auto root = std::filesystem::path(settings.ReadString("FILELOADER", "DLCPath", "DLC"));
            std::vector<std::filesystem::path> folders;
            std::error_code error;
            if (!root.empty())
            {
                const auto base = root.is_absolute() ? root : GetExeModulePath() / root;
                for (std::filesystem::directory_iterator it(base, error), end; !error && it != end; it.increment(error))
                {
                    std::error_code entryError;
                    if (it->is_directory(entryError) && std::filesystem::is_regular_file(it->path() / "setup2.xml", entryError))
                        folders.push_back(it->path());
                }
            }
            // Existing explicit folder registrations remain usable, but XML is
            // authoritative: an INI section cannot turn a car pack into an episode.
            CIniReader oldDefinitions("FusionFixEpisodes.ini");
            for (int32_t id = 3; id < 64; ++id)
            {
                auto path = oldDefinitions.ReadString("Episode" + std::to_string(id), "Folder", "");
                if (!path.empty()) folders.push_back(GetExeModulePath() / path);
            }
            std::sort(folders.begin(), folders.end());
            CIniReader slots("FusionFixContentIDs.ini");
            std::array<uint32_t, 64> assigned{};
            for (size_t slot = 5; slot < assigned.size(); ++slot)
            {
                const auto value = slots.ReadString("ContentIDs", std::to_string(slot), "0");
                if (!Number(value, assigned[slot]))
                {
                    Log("invalid FusionFixContentIDs.ini; content discovery disabled");
                    return;
                }
                for (size_t earlier = 5; assigned[slot] && earlier < slot; ++earlier)
                    if (assigned[earlier] == assigned[slot])
                    {
                        Log("duplicate persistent content slot; content discovery disabled");
                        return;
                    }
            }
            for (const auto& path : folders)
            {
                const auto fullPath = std::filesystem::weakly_canonical(path, error);
                if (error || fullPath.string().size() >= sizeof(Mount::path)) continue;
                const auto folderPath = fullPath.lexically_relative(GetExeModulePath());
                if (std::any_of(episodes.begin(), episodes.end(), [&](auto& entry) { return _stricmp(entry.folder.c_str(), folderPath.string().c_str()) == 0; })) continue;
                Episode folder;
                folder.folder = (folderPath.empty() ? fullPath : folderPath).string();
                if (!ReadSetup(fullPath / "setup2.xml", folder))
                {
                    Log(fullPath.string() + ": invalid setup2.xml (device, content ID, episode or field length)");
                    continue;
                }
                bool conflict = false;
                for (const auto& existing : episodes)
                {
                    conflict |= _stricmp(existing.device.c_str(), folder.device.c_str()) == 0 || (folder.id && folder.id == existing.id);
                    for (const auto& a : existing.contents)
                        for (const auto& b : folder.contents) conflict |= a.externalId == b.externalId;
                }
                for (size_t i = 0; i < folder.contents.size(); ++i)
                    for (size_t j = 0; j < i; ++j) conflict |= folder.contents[i].externalId == folder.contents[j].externalId;
                if (conflict) { Log(folder.name + ": duplicate device, content ID or episode ID"); continue; }
                auto proposed = assigned;
                for (auto& content : folder.contents)
                {
                    size_t slot = 5;
                    while (slot < proposed.size() && proposed[slot] != content.externalId) ++slot;
                    if (slot == proposed.size())
                    {
                        slot = content.externalId < proposed.size() && !proposed[content.externalId] ? content.externalId : 5;
                        while (slot < proposed.size() && proposed[slot]) ++slot;
                        if (slot == proposed.size()) { conflict = true; break; }
                        proposed[slot] = content.externalId;
                    }
                    content.slot = static_cast<uint8_t>(slot);
                }
                if (conflict) { Log(folder.name + ": no free content save-mask slots (5..63)"); continue; }
                for (size_t slot = 5; slot < proposed.size(); ++slot)
                    if (proposed[slot] != assigned[slot]) slots.WriteString("ContentIDs", std::to_string(slot), std::to_string(proposed[slot]));
                CIniReader check(slots.GetIniPath());
                for (const auto& content : folder.contents)
                    conflict |= check.ReadString("ContentIDs", std::to_string(content.slot), "") != std::to_string(content.externalId);
                if (conflict) { Log(folder.name + ": could not persist content IDs; folder skipped"); continue; }
                assigned = proposed;
                if (folder.id)
                {
                    folder.savePrefix = "SGE" + std::string(folder.id < 10 ? "0" : "") + std::to_string(folder.id);
                    for (size_t slot = 0; slot < folder.saveFiles.size(); ++slot)
                        folder.saveFiles[slot] = folder.savePrefix + (slot < 10 ? "0" : "") + std::to_string(slot);
                    episodePaths[folder.id] = folder.folder;
                }
                episodes.push_back(std::move(folder));
            }
        }

        static void SelectPacks(Manager* manager, int32_t id)
        {
            for (uint16_t i = 0; manager->packs && i < manager->packCount; ++i)
            {
                auto& pack = manager->packs[i];
                if (!pack.valid) continue;
                if (pack.episode) pack.enabled = pack.episode == id;
                for (const auto& episode : episodes)
                    if (episode.mount >= 0 && pack.mount == episode.mount)
                        pack.enabled = episode.mounted && !episode.invalid && (pack.episode == 0 || pack.episode == id);
            }
        }

        static int32_t __fastcall AddPack(Manager* manager, void* edx, Pack* incoming)
        {
            Episode* owner = nullptr;
            for (auto& episode : episodes)
                if (episode.mount >= 0 && episode.mount == incoming->mount) owner = &episode;
            if (owner)
            {
                if (owner->parsedContents >= owner->contents.size())
                {
                    owner->invalid = true;
                    return -1;
                }
                const auto& definition = owner->contents[owner->parsedContents++];
                bool conflict = incoming->episode != definition.episode ||
                    strncmp(reinterpret_cast<const char*>(incoming) + 4, definition.name.c_str(), 64) != 0;
                incoming->id = definition.slot;
                incoming->enabled = incoming->episode == 0 || incoming->episode == *activeEpisode;
                for (uint16_t i = 0; manager->packs && i < manager->packCount; ++i)
                    conflict |= manager->packs[i].valid && manager->packs[i].id == incoming->id &&
                        manager->packs[i].mount != incoming->mount;
                if (conflict)
                {
                    owner->invalid = true;
                    Log(owner->name + ": conflicting content ID or episode ID in setup2.xml");
                    return -1;
                }
            }
            return addPack.fastcall<int32_t>(manager, edx, incoming);
        }

        static void __fastcall LoadSetups(Manager* manager, void* edx)
        {
            bool initialize = false;
            for (auto& episode : episodes)
            {
                const auto path = (GetExeModulePath() / episode.folder).string();
                if (registeredManager != manager || episode.mount < 0 || episode.mount >= manager->mountCount ||
                    !manager->mounts || !manager->mounts[episode.mount].enabled ||
                    _stricmp(manager->mounts[episode.mount].path, path.c_str()) != 0)
                {
                    Mount definition{};
                    strcpy_s(definition.path, path.c_str());
                    definition.type = 1;
                    definition.enabled = true;
                    episode.mount = addMount(manager, &definition);
                    episode.mounted = false;
                    episode.invalid = false;
                    manager->setupsLoaded = false;
                    initialize = true;
                }
            }
            registeredManager = manager;
            for (auto& episode : episodes)
                if (episode.mount >= 0 && episode.mount < manager->mountCount && !manager->mounts[episode.mount].parsed)
                    episode.parsedContents = 0;
            loadSetups.fastcall<void>(manager, edx);
            if (!initialize) return;

            for (auto& episode : episodes)
            {
                bool found = false, conflict = false;
                for (uint16_t i = 0; manager->packs && i < manager->packCount; ++i)
                {
                    const auto& pack = manager->packs[i];
                    if (pack.mount != episode.mount) continue;
                    found |= pack.valid;
                    conflict |= pack.episode != 0 && pack.episode != episode.id;
                }
                if (found && !conflict && !episode.invalid && episode.parsedContents == episode.contents.size() &&
                    episode.mount >= 0 && episode.mount < manager->mountCount)
                    episode.mounted = mountContent(manager, episode.mount);
                if (!episode.mounted)
                    Log(episode.name + ": unable to mount content; check setup2.xml and content IDs");
            }
            SelectPacks(manager, *activeEpisode);
        }

        static void __fastcall EnableEpisode(Manager* manager, void* edx, int32_t id, bool enable)
        {
            enableEpisode.fastcall<void>(manager, edx, id, enable);
            if (enable) SelectPacks(manager, id);
            else if (Find(id)) SelectPacks(manager, 0);
        }

        static Episode* SaveEpisode()
        {
            if (!installed) return nullptr;
            const auto id = *hasRequestedEpisode && *requestedEpisode >= 0 ? *requestedEpisode : *activeEpisode;
            auto episode = Find(id);
            return episode && episode->mounted ? episode : nullptr;
        }

        static const char* __stdcall StorageFileName(int32_t user, int32_t slot)
        {
            const auto episode = SaveEpisode();
            if (episode && static_cast<uint32_t>(slot) < episode->saveFiles.size())
                return episode->saveFiles[slot].c_str();
            return storageFileName.stdcall<const char*>(user, slot);
        }

        static void FilterSaves(SafetyHookContext& regs)
        {
            // fiFindData is a local of the engine's save enumeration function.
            auto filename = reinterpret_cast<char*>(regs.esp + 0x18);
            const auto selected = SaveEpisode();
            if (selected)
            {
                for (const auto& name : selected->saveFiles)
                    if (name == filename) return;
                filename[0] = '.';
            }
            else
            {
                for (const auto& episode : episodes)
                    for (const auto& name : episode.saveFiles)
                        if (name == filename)
                        {
                            filename[0] = '.';
                            return;
                        }
            }
        }

        static int32_t __cdecl NextEpisode(int32_t id)
        {
            auto next = nextEpisode.ccall<int32_t>(id);
            while (Find(next)) next = nextEpisode.ccall<int32_t>(next);
            return next;
        }

        static bool __fastcall EpisodeAvailable(Manager* manager, void* edx, int32_t id)
        {
            // Run the stock lookup first: it initializes the content manager.
            const auto available = episodeAvailable.fastcall<bool>(manager, edx, id);
            const auto episode = Find(id);
            return episode ? episode->mounted : available;
        }

        static void Initialize()
        {
            ReadDefinitions();
            if (episodes.empty()) return;

            auto enable = hook::pattern("53 56 8B F1 E8 ? ? ? ? 8B CE E8 ? ? ? ? 8B 5C 24 0C 85 DB 74 ?");
            auto add = find_pattern("81 EC 6C 01 00 00 53 55 56 8B F1 33 DB 0F B7 8E 50 01 00 00",
                "51 53 8B D9 0F B7 8B 50 01 00 00 55 56 33 ED");
            auto mount = find_pattern("81 EC 04 01 00 00 A1 ? ? ? ? 33 C4 89 84 24 00 01 00 00 53 57 8B BC 24 10 01 00 00 69 FF 68 01 00 00",
                "81 EC 08 01 00 00 A1 ? ? ? ? 33 C4 89 84 24 04 01 00 00 55 8B AC 24 10 01 00 00 56 8B F5 69 F6 68 01 00 00");
            auto pack = find_pattern("81 EC 2C 02 00 00 53 55 8B E9 56 0F B7 95 48 01 00 00",
                "53 55 56 8B 74 24 10 8B E9 57 0F B7 BD 48 01 00 00 33 C0");
            auto filter = hook::pattern("80 7C 24 18 2E 74 ? F6 84 24 28 02 00 00 10 75 ? 8B 0C 9D");
            auto active = find_pattern("A1 ? ? ? ? 48 74 0F 48 74 06 B8 0C 00 00 00 C3",
                "A1 ? ? ? ? 83 E8 01 74 11 83 E8 01 74 06 B8 0C 00 00 00 C3");
            auto requested = hook::pattern("84 C0 74 11 A1 ? ? ? ? C6 05 ? ? ? ? 01 A3");
            const bool ce = !requested.empty();
            if (!ce) requested = hook::pattern("84 C0 5E 74 11 A1 ? ? ? ? C6 05 ? ? ? ? 01 A3");
            auto next = find_pattern("56 8B 74 24 08 46 83 FE 40 7D 1B EB 03 8D 49 00",
                "56 8B 74 24 08 83 C6 01 83 FE 40 7D 1B 8D 49 00");
            auto available = find_pattern("56 57 8B 7C 24 0C 8B F1 85 FF 75 1C 8A 86 69 01 00 00",
                "56 57 8B 7C 24 0C 85 FF 8B F1 75 15 8A 86 69 01 00 00");
            auto filename = hook::pattern("8B 44 24 08 83 F8 10 0F 87 ? ? ? ? FF 24 85");
            if (enable.size() != 1 || add.size() != 1 || mount.size() != 1 || pack.size() != 1 || filter.size() != 1 ||
                active.size() != 1 || requested.size() != 1 || next.size() != 1 || available.size() != 1 || (ce && filename.size() != 1))
            {
                Log("unsupported executable or conflicting hooks; episode support disabled");
                return;
            }
            addMount = reinterpret_cast<decltype(addMount)>(add.get_first());
            mountContent = reinterpret_cast<decltype(mountContent)>(mount.get_first());
            activeEpisode = *active.get_first<int32_t*>(1);
            hasRequestedEpisode = *requested.get_first<uint8_t*>(ce ? 11 : 12);
            requestedEpisode = *requested.get_first<int32_t*>(ce ? 17 : 18);
            if (ce) storageFileName = safetyhook::create_inline(filename.get_first(), StorageFileName);
            filterSaves = safetyhook::create_mid(filter.get_first(), FilterSaves);
            episodeAvailable = safetyhook::create_inline(available.get_first(), EpisodeAvailable);
            nextEpisode = safetyhook::create_inline(next.get_first(), NextEpisode);
            addPack = safetyhook::create_inline(pack.get_first(), AddPack);
            loadSetups = safetyhook::create_inline(injector::GetBranchDestination(enable.get_first(11)).as_int(), LoadSetups);
            enableEpisode = safetyhook::create_inline(enable.get_first(), EnableEpisode);
            installed = (!ce || storageFileName) && filterSaves && episodeAvailable && nextEpisode && addPack && loadSetups && enableEpisode;
            if (!installed)
            {
                enableEpisode.reset();
                loadSetups.reset();
                addPack.reset();
                nextEpisode.reset();
                episodeAvailable.reset();
                filterSaves.reset();
                storageFileName.reset();
                Log("unable to install episode hooks");
            }
        }
    }


    namespace EpisodeMenu
    {
        template<class T> static T& Field(void* object, size_t offset)
        {
            return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
        }
        template<class R = void, class... Args> static R Call(void* object, size_t offset, Args... args)
        {
            return reinterpret_cast<R(__thiscall*)(void*, Args...)>(Field<uintptr_t*>(object, 0)[offset / 4])(object, args...);
        }
        struct Offset { float x; uint32_t relativeX, targetX; float y; uint32_t relativeY, targetY; };
        static SafetyHookInline setup, update, activate, click, hover, next, previous, destroy;
        static void* (__cdecl* allocate)(uint32_t);
        static void* (__thiscall* constructFont)(void*, const char*, const char*);
        static void (__thiscall* setTexture)(void*, int32_t, const char*);
        static int32_t (__cdecl* findDictionary)(const char*);
        static const char** argument;
        static uint8_t *showSelect, *showTitle, *leaveSelect;
        static std::string episodeArgument;
        static void* menu = nullptr;
        static std::array<void*, 2> arrows{};
        static std::vector<Episodes::Episode*> choices;
        static std::array<int32_t, 3> stockModes{};
        static int32_t page = 0;

        static void Visible(void* widget, bool visible, bool interactive = true)
        {
            if (!widget) return;
            Call<bool>(widget, 0x120, visible);
            Call(widget, 0x28, visible && interactive);
            Call(widget, 0x30, visible && interactive);
            Call<bool>(widget, 0x24, visible);
        }
        static void Label(void* widget, const char* text, bool literal)
        {
            Call<bool>(widget, 0x1FC, literal);
            Call(widget, 0x1E0, text, true);
        }
        static void Focus(void* widget)
        {
            Field<void*>(menu, 0x208) = widget;
            Field<void*>(menu, 0x20C) = widget;
        }
        static void* CreateArrow(const char* name, float x)
        {
            auto storage = allocate(0x610);
            if (!storage) return nullptr;
            auto button = constructFont(storage, name, Call<const char*>(menu, 0x48));
            auto bottom = Field<void*>(menu, 0x204);
            const uint32_t color = 0xFFFFFFFF;
            Call<bool>(button, 0x1CC, 35.0f, &color, 0, 2);
            Call(button, 0x104, 1, Call<uint32_t>(bottom, 0x4C), 1, Offset{x, 0, 0, 0, 0, 0});
            Call<bool>(button, 0x118, true);
            Call<bool>(button, 0x200, true);
            Call<uint32_t>(button, 0x170, Call<uint32_t>(menu, 0x4C));
            Call<uint32_t>(button, 0x17C, Call<uint32_t>(menu, 0x4C));
            Call<bool>(button, 0x1DC, Field<float>(bottom, 0x200), Field<float>(bottom, 0x204), true);
            return button;
        }
        static void Render(int32_t target)
        {
            if (page == 0)
                for (size_t i = 0; i < 3; ++i) stockModes[i] = Field<int32_t>(menu, 0x1E0 + i * 4);
            page = target;
            const auto dictionary = findDictionary("select_menu");
            for (int32_t slot = 0; slot < 2; ++slot)
            {
                auto button = Field<void*>(menu, 0x1FC + slot * 4);
                auto texture = Field<void*>(menu, 0x1F0 + slot * 4);
                auto index = (page - 1) * 2 + slot;
                auto episode = page && index < static_cast<int32_t>(choices.size()) ? choices[index] : nullptr;
                Visible(button, page == 0 || episode);
                Visible(texture, page == 0 || episode, false);
                Field<int32_t>(menu, 0x1E0 + slot * 4) = page ? 11 : stockModes[slot];
                if (page)
                {
                    if (episode) Label(button, episode->name.c_str(), true);
                    setTexture(texture, dictionary, "eflc_select");
                }
                else
                {
                    Label(button, "E1_SELECT", false);
                    const auto mode = stockModes[slot];
                    setTexture(texture, dictionary, mode == 0 ? "gta_iv_logo" :
                        mode == 1 || mode == 5 ? "gta_iv_tlad_logo" :
                        mode == 2 || mode == 6 ? "gta_iv_tbogt_logo" : "eflc_select");
                }
            }
            Field<int32_t>(menu, 0x1E8) = page ? 7 : stockModes[2];
            Label(Field<void*>(menu, 0x204), page || stockModes[2] == 7 ? "BACK" : "MO_EXITGAM", false);
        }
        static uintptr_t __fastcall Setup(void* self, void* edx)
        {
            auto result = setup.fastcall<uintptr_t>(self, edx);
            if (menu == self || !Field<void*>(self, 0x204)) return result;
            choices.clear();
            for (auto& episode : Episodes::episodes)
                if (episode.id >= 3 && episode.mounted && !episode.invalid) choices.push_back(&episode);
            if (choices.empty()) return result;
            menu = self;
            page = 0;
            arrows[0] = CreateArrow("FusionFix.PreviousEpisodePage", -400.0f);
            arrows[1] = CreateArrow("FusionFix.NextEpisodePage", 400.0f);
            for (size_t i = 0; i < 2; ++i)
            {
                if (!arrows[i]) continue;
                Label(arrows[i], i ? "    >    " : "    <    ", true);
                Visible(arrows[i], arrows[0] && arrows[1]);
            }
            return result;
        }
        static void __fastcall Update(void* self, void* edx)
        {
            auto hovered = Field<void*>(self, 0x20C);
            update.fastcall<void>(self, edx);
            if (self != menu) return;
            // Stock updates enable both choices for unknown modes. Restore the
            // empty slot on an odd final addon page after that update.
            if (page)
                for (int32_t slot = 0; slot < 2; ++slot)
                {
                    const bool present = (page - 1) * 2 + slot < static_cast<int32_t>(choices.size());
                    Visible(Field<void*>(self, 0x1FC + slot * 4), present);
                    Visible(Field<void*>(self, 0x1F0 + slot * 4), present, false);
                }
            const uint32_t gray = 0xFFAAAAAA;
            for (auto button : arrows)
                if (button && button != hovered && button != Field<void*>(self, 0x208))
                    Call<uint32_t>(button, 0x208, &gray);
        }
        static uintptr_t __fastcall Activate(void* self, void* edx)
        {
            if (self == menu)
            {
                auto focused = Field<void*>(self, 0x208);
                for (size_t i = 0; i < 2; ++i)
                    if (arrows[i] && focused == arrows[i])
                    {
                        const int32_t count = 1 + (static_cast<int32_t>(choices.size()) + 1) / 2;
                        Render((page + (i ? 1 : -1) + count) % count);
                        Focus(arrows[i]);
                        return 0;
                    }
                if (page)
                {
                    if (focused == Field<void*>(self, 0x204))
                    {
                        Render(0);
                        Focus(Field<void*>(self, 0x204));
                        return 0;
                    }
                    for (int32_t i = 0; i < 2; ++i)
                        if (focused == Field<void*>(self, 0x1FC + i * 4))
                        {
                            auto index = (page - 1) * 2 + i;
                            if (index < static_cast<int32_t>(choices.size()))
                            {
                                episodeArgument = std::to_string(choices[index]->id);
                                *argument = episodeArgument.c_str();
                                *showSelect = 0;
                                *showTitle = 1;
                                *leaveSelect = 1;
                            }
                            return 0;
                        }
                }
            }
            return activate.fastcall<uintptr_t>(self, edx);
        }
        static void __fastcall Click(void* self, void* edx, uint32_t id)
        {
            if (self == menu)
                for (auto button : arrows)
                    if (button && Call<uint32_t>(button, 0x4C) == id)
                    {
                        Focus(button);
                        Activate(self, edx);
                        return;
                    }
            click.fastcall<void>(self, edx, id);
        }
        static void __fastcall Hover(void* self, void* edx, uint32_t id)
        {
            if (self == menu)
                for (auto button : arrows)
                    if (button && Call<uint32_t>(button, 0x4C) == id)
                    {
                        Field<void*>(self, 0x20C) = button;
                        return;
                    }
            hover.fastcall<void>(self, edx, id);
        }
        static void* Move(int32_t direction)
        {
            std::array<void*, 5> buttons = { Field<void*>(menu, 0x1FC), Field<void*>(menu, 0x200),
                arrows[0], Field<void*>(menu, 0x204), arrows[1] };
            auto current = std::find(buttons.begin(), buttons.end(), Field<void*>(menu, 0x208));
            auto index = current == buttons.end() ? 0 : static_cast<int32_t>(current - buttons.begin());
            for (int32_t i = 0; i < 5; ++i)
            {
                index = (index + direction + 5) % 5;
                if (buttons[index] && Call<bool>(buttons[index], 0x20))
                {
                    Focus(buttons[index]);
                    return buttons[index];
                }
            }
            return Field<void*>(menu, 0x208);
        }
        static void* __fastcall Next(void* self, void* edx)
        {
            return self == menu ? Move(1) : next.fastcall<void*>(self, edx);
        }
        static void* __fastcall Previous(void* self, void* edx)
        {
            return self == menu ? Move(-1) : previous.fastcall<void*>(self, edx);
        }
        static void __fastcall Destroy(void* self, void* edx)
        {
            if (self == menu)
            {
                menu = nullptr;
                arrows = {};
                choices.clear();
                page = 0;
            }
            destroy.fastcall<void>(self, edx);
        }
        static void Initialize()
        {
            if (!Episodes::installed) return;
            auto setupPattern = hook::pattern("83 EC 54 53 56 57 8B F9 8B 07");
            const bool ce = !setupPattern.empty();
            if (!ce) setupPattern = hook::pattern("83 EC 3C 53 55 56 8B F1 8B 06 8B 90 40 01 00 00 57 FF D2 84 C0 0F 85 ? ? ? ? 8D 44 24 24 6A 02");
            auto activatePattern = hook::pattern(ce ? "3B 87 FC 01 00 00" : "53 56 8B F1 8B 86 08 02 00 00 3B 86 FC 01 00 00 0F 85 ? ? ? ? 8B B6 E0 01 00 00 85 F6 BB 01 00 00 00");
            auto ctor = hook::pattern(ce ? "C7 86 14 02 00 00 FF FF FF FF" : "8B 44 24 08 56 8B F1 8B 4C 24 08 50 51 8B CE E8 ? ? ? ? C7 06 ? ? ? ? 83 C8 FF 89 86 10 02 00 00");
            auto clickPattern = hook::pattern(ce ? "8B 8F FC 01 00 00 85 C9 74 6E" : "56 8B F1 8B 8E FC 01 00 00 85 C9 74 74 83 BE 00 02 00 00 00 74 6B 83 BE 04 02 00 00 00 74 62 8B 01");
            auto hoverPattern = hook::pattern(ce ? "8B 8F FC 01 00 00 85 C9 74 56" : "56 8B F1 8B 8E FC 01 00 00 85 C9 74 5C 83 BE 00 02 00 00 00 74 53 83 BE 04 02 00 00 00 74 4A 8B 01");
            auto nextPattern = hook::pattern(ce ? "8B F9 33 DB 8B 97 08 02 00 00" : "53 55 56 57 8B F9 8B 97 08 02 00 00 33 ED 33 C0 8D 8F FC 01 00 00 3B 11 74 0D 83 C0 01 83 C1 04 83 F8 03 7C ? EB ? 8B E8 8D 75 01");
            auto previousPattern = hook::pattern(ce ? "8B F9 33 ED 8B 97 08 02 00 00" : "53 55 56 57 8B F9 8B 97 08 02 00 00 33 ED 33 C0 8D 8F FC 01 00 00 3B 11 74 0D 83 C0 01 83 C1 04 83 F8 03 7C ? EB ? 8B E8 8D 75 FF");
            auto stock = hook::pattern(ce ? "83 BF E0 01 00 00 01" : "56 57 68 ? ? ? ? 8B F1 E8 ? ? ? ? 8B 8E F0 01 00 00 83 C4 04 8B F8 68 ? ? ? ? 57 E8 ? ? ? ?");
            if (setupPattern.size() != 1 || activatePattern.size() != 1 || ctor.size() != 1 ||
                clickPattern.size() != 1 || hoverPattern.size() != 1 || nextPattern.size() != 1 ||
                previousPattern.size() != 1 || stock.size() != 1)
            {
                Episodes::Log("startup menu patterns unavailable");
                return;
            }
            const auto setupAddress = reinterpret_cast<uintptr_t>(setupPattern.get_first());
            const auto activateAddress = reinterpret_cast<uintptr_t>(activatePattern.get_first(ce ? -9 : 0));
            const auto stockAddress = reinterpret_cast<uintptr_t>(stock.get_first(ce ? -0x88 : 0));
            const auto vtable = *ctor.get_first<uintptr_t*>(ce ? -0xE : 0x16);
            allocate = reinterpret_cast<decltype(allocate)>(injector::GetBranchDestination(setupAddress + (ce ? 0x1BB : 0x17B)).as_int());
            constructFont = reinterpret_cast<decltype(constructFont)>(injector::GetBranchDestination(setupAddress + (ce ? 0x8D8 : 0x881)).as_int());
            findDictionary = reinterpret_cast<decltype(findDictionary)>(injector::GetBranchDestination(stockAddress + 9).as_int());
            // Both stock texture calls have the same signature on all three executables.
            setTexture = reinterpret_cast<decltype(setTexture)>(injector::GetBranchDestination(stockAddress + 0x1F).as_int());
            argument = *reinterpret_cast<const char***>(activateAddress + (ce ? 0x1D : 0x2D));
            showSelect = *reinterpret_cast<uint8_t**>(activateAddress + (ce ? 0x101 : 0x37));
            showTitle = *reinterpret_cast<uint8_t**>(activateAddress + (ce ? 0x108 : 0x3E));
            leaveSelect = *reinterpret_cast<uint8_t**>(activateAddress + (ce ? 0x10F : 0x5A));
            setup = safetyhook::create_inline(setupAddress, Setup);
            activate = safetyhook::create_inline(activateAddress, Activate);
            update = safetyhook::create_inline(vtable[0x148 / 4], Update);
            destroy = safetyhook::create_inline(injector::GetBranchDestination(vtable[2] + 3).as_int(), Destroy);
            click = safetyhook::create_inline(clickPattern.get_first(ce ? -3 : 0), Click);
            hover = safetyhook::create_inline(hoverPattern.get_first(ce ? -3 : 0), Hover);
            next = safetyhook::create_inline(nextPattern.get_first(ce ? -5 : 0), Next);
            previous = safetyhook::create_inline(previousPattern.get_first(ce ? -4 : 0), Previous);
            if (!(setup && activate && update && destroy && click && hover && next && previous))
            {
                setup.reset(); activate.reset(); update.reset(); destroy.reset();
                click.reset(); hover.reset(); next.reset(); previous.reset();
                Episodes::Log("unable to install startup menu hooks");
            }
        }
    }

    namespace Radio
    {
        static void** currentDictionary = nullptr;
        static uint32_t (__cdecl* hashString)(const char*, uint32_t) = nullptr;
        static void* (__thiscall* findTexture)(void*, uint32_t) = nullptr;
        static SafetyHookInline plainIcon, coloredIcon;

        static void FixSaveSize()
        {
            // CE's stats marker makes its loader consume 23 station records,
            // but the writer uses the installed station count. Keep the wire
            // count at 23; the existing serializer supplies empty records for
            // missing stations. The live station count must remain unchanged.
            auto header = hook::pattern("6A 17 E8 ? ? ? ? A1 ? ? ? ? C1 E0 02 50 68 ? ? ? ? E8 ? ? ? ? 83 C4 0C");
            auto writer = hook::pattern("A0 ? ? ? ? 53 8A 1D ? ? ? ? 68 ? ? ? ? A2 ? ? ? ? E8 ? ? ? ? 83 C4 04 88 1D ? ? ? ? 5B C3");
            if (header.size() != 1 || writer.size() != 1) return;
            const auto setter = injector::GetBranchDestination(header.get_first(2)).as_int();
            auto setterPattern = hook::range_pattern(setter, setter + 16, "8A 44 24 04 A2 ? ? ? ? C3");
            if (setterPattern.size() != 1 || *setterPattern.get_first<uintptr_t>(5) != *writer.get_first<uintptr_t>(18)) return;
            std::array<uint8_t, 5> instruction = { 0xB0, 23, 0x90, 0x90, 0x90 };
            injector::WriteMemoryRaw(writer.get_first(), instruction.data(), instruction.size(), true);
        }

        static bool HasTexture(const char* name)
        {
            return name && *name && currentDictionary && *currentDictionary &&
                findTexture(*currentDictionary, hashString(name, 0));
        }

        static char __fastcall LoadPlain(void* station, void* edx)
        {
            if (!HasTexture(static_cast<const char*>(station) + 0x10)) return 0;
            return plainIcon.fastcall<char>(station, edx);
        }

        static char __fastcall LoadColored(void* station, void* edx)
        {
            if (!HasTexture(static_cast<const char*>(station) + 0x10F)) return 0;
            return coloredIcon.fastcall<char>(station, edx);
        }

        static void Initialize()
        {
            FixSaveSize();
            // Shared instruction sequences verified in CE 1.2.0.59, EFLC 1.1.2.0
            // and IV 1.0.8.0. Station definitions still come from game audio data.
            auto plain = hook::pattern("80 79 10 00 8D 41 10 74 ? 50 83 C1 08 E8");
            auto colored = hook::pattern("80 B9 0F 01 00 00 00 8D 81 0F 01 00 00 74 ? 50 83 C1 04 E8");
            if (plain.size() != 1 || colored.size() != 1) return;
            auto loader = injector::GetBranchDestination(plain.get_first(13)).as_int();
            auto lookup = hook::range_pattern(loader, loader + 0xB0,
                "56 8B 35 ? ? ? ? 6A 00 50 E8 ? ? ? ? 83 C4 08 ? ? ? E8");
            if (lookup.size() != 1) return;
            currentDictionary = *lookup.get_first<void**>(3);
            hashString = reinterpret_cast<decltype(hashString)>(injector::GetBranchDestination(lookup.get_first(10)).as_int());
            findTexture = reinterpret_cast<decltype(findTexture)>(injector::GetBranchDestination(lookup.get_first(21)).as_int());
            plainIcon = safetyhook::create_inline(plain.get_first(), LoadPlain);
            coloredIcon = safetyhook::create_inline(colored.get_first(), LoadColored);
            if (!plainIcon || !coloredIcon)
            {
                plainIcon.reset();
                coloredIcon.reset();
            }
        }
    }

    class Installation
    {
    public:
        Installation()
        {
            FusionFix::onInitEvent() += []()
            {
                Radio::Initialize();
                Episodes::Initialize();
                EpisodeMenu::Initialize();
            };
        }
    } installation;
}

export const char* GetAddonSavePrefix()
{
    const auto episode = AddonContent::Episodes::SaveEpisode();
    return episode ? episode->savePrefix.c_str() : nullptr;
}

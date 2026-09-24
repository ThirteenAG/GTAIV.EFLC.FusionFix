// Local fixture only. No OpenProcess, injection, GTA launch or game-code execution.
#define main BaseAdapterTests
#include "adapter_guard_tests.cpp"
#undef main
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <safetyhook.hpp>
#include "../../source/ShadowCasterCE.hpp"

static void __cdecl StubEntry() {}
static void StubMid(SafetyHookContext&) {}
static void __fastcall StubRender(void*, void*) {}

int main(int argc, char** argv)
{
    CHECK(BaseAdapterTests(argc,argv) == 0);
    auto mapped=MapFile(argv[1]);
    auto* image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr, mapped.size(), MEM_RESERVE|MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    CHECK(image != nullptr);
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    std::memcpy(image,mapped.data(),mapped.size());
    const auto pe=ce::Read32(image+0x3C);
    const auto opt=pe+24;
    auto reloc=ce::Read32(image+opt+136);
    const auto end=reloc+ce::Read32(image+opt+140);
    CHECK(end <= mapped.size());
    while(reloc < end)
    {
        const auto page=ce::Read32(image+reloc), length=ce::Read32(image+reloc+4);
        CHECK(length >= 8 && length <= end-reloc);
        for(auto p=reloc+8;p<reloc+length;p+=2)
        {
            const auto entry=Read16(image+p);
            if ((entry>>12)==0) continue;
            CHECK((entry>>12)==3);
            const auto target=page+(entry&4095);
            CHECK(target <= mapped.size()-4);
            const auto value=static_cast<std::uint32_t>(ce::Read32(image+target)-ce::PreferredBase+base);
            std::memcpy(image+target,&value,4);
        }
        reloc+=length;
    }
    namespace allocation=ce::allocation;
    CHECK(ce::casterguard::Validate(image,mapped.size(),base));
    const auto loadedHeaderBase=static_cast<std::uint32_t>(base);
    std::memcpy(image+opt+28,&loadedHeaderBase,4);
    CHECK(ce::casterguard::Validate(image,mapped.size(),base));
    const auto invalidHeaderBase=static_cast<std::uint32_t>(base+0x10000);
    std::memcpy(image+opt+28,&invalidHeaderBase,4);
    CHECK(!ce::casterguard::Validate(image,mapped.size(),base));
    std::memcpy(image+opt+28,&loadedHeaderBase,4);
    // A correct header cannot mask an incorrectly relocated code operand.
    image[0x769F28]^=1;
    CHECK(!ce::casterguard::Validate(image,mapped.size(),base));
    image[0x769F28]^=1;
    CHECK(ce::casterguard::Validate(image,mapped.size(),base));
    {
        // Reproduce old ordering using the real compiled hook library.
        std::memset(image+0x527BD6,0x90,6);
        auto admission=safetyhook::MidHook::create(base+0x527BD4,StubMid);
        CHECK(admission.has_value());
        CHECK(!allocation::ValidateMappedImage(image,mapped.size(),base));
    }
    std::memcpy(image+0x527BD4,mapped.data()+0x527BD4,8); // no absolute operands
    CHECK(ce::casterguard::Validate(image,mapped.size(),base));
    {
        // Candidate 16 ordering. Hook destinations are inert local stubs.
        auto begin=safetyhook::InlineHook::create(base+allocation::SelectionRva,StubEntry,safetyhook::InlineHook::StartDisabled);
        auto collect=safetyhook::MidHook::create(base+allocation::CollectRva,StubMid,safetyhook::MidHook::StartDisabled);
        auto finish=safetyhook::MidHook::create(base+allocation::FinalizeRva,StubMid,safetyhook::MidHook::StartDisabled);
        CHECK(begin && collect && finish);
        CHECK(ce::casterguard::Validate(image,mapped.size(),base)); // prepared but disabled
        CHECK(collect->enable().has_value());
        CHECK(finish->enable().has_value());
        CHECK(begin->enable().has_value());
        std::memset(image+0x527BD6,0x90,6);
        auto admission=safetyhook::MidHook::create(base+0x527BD4,StubMid);
        auto render=safetyhook::InlineHook::create(base+0x977A00,StubRender);
        CHECK(admission && render);
        // Existing MakeInline removes the 25-byte caster branch before hooking.
        std::memset(image+0x6E3736,0x90,25);
        auto caster=safetyhook::MidHook::create(base+0x6E3736,StubMid);
        CHECK(caster.has_value());
        CHECK(image[allocation::SelectionRva]==0xE9);
        CHECK(image[allocation::CollectRva]==0xE9);
        CHECK(image[allocation::FinalizeRva]==0xE9);
        CHECK(image[0x527BD4]==0xE9);
        CHECK(image[0x977A00]==0xE9);
        CHECK(image[0x6E3736]==0xE9);
    }
    CHECK(VirtualFree(image,0,MEM_RELEASE));
    std::cout << "PASS: real x86 hook installation order; " << checks
              << " fixture checks. GTA process untouched; copied game code never called.\n";
}

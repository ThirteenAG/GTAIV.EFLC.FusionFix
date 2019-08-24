#include <misc.h>

void Init()
{
    CIniReader iniReader("");
    bool bRecoilFix = iniReader.ReadInteger("MAIN", "RecoilFix", 1) != 0;
    bool bDefinitionFix = iniReader.ReadInteger("MAIN", "DefinitionFix", 1) != 0;
    bool bEmissiveShaderFix = iniReader.ReadInteger("MAIN", "EmissiveShaderFix", 1) != 0;

    if (bRecoilFix)
    {
        static float fRecMult = 0.65f;
        auto pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB ? E8");
        injector::WriteMemory(pattern.get_first(10), &fRecMult, true);
    }

    if (bDefinitionFix)
    {
        //disable forced definition-off in cutscenes
        auto pattern = hook::pattern("E8 ? ? ? ? F6 D8 1A C0 F6 D0 22 05 ? ? ? ? C3"); //0x88CC6B
        injector::WriteMemory<uint16_t>(pattern.get_first(11), 0xA090, true); //mov al ...
    }

    if (bEmissiveShaderFix)
    {
        // workaround for gta_emissivestrong.fxc lights on patch 6+,
        //"A0 01 00 00 02 00 00 08" replaced in shader files with "A1 01 00 00 02 00 00 08" (5 occurrences)
        auto pattern = hook::pattern("C1 E7 04 C1 E3 04 8B C7 83 F8 04 8D 9B ? ? ? ? 8B CB 72 14 8B 32 3B 31 75 12 83 E8 04");
        struct ShaderTest
        {
            void operator()(injector::reg_pack& regs)
            {
                if (
                    *(uint32_t*)(regs.edx + 0x00) == 0x39D1B717 &&
                    *(uint32_t*)(regs.edx + 0x10) == 0x41000000 && //traffic lights and lamps
                    *(uint32_t*)(regs.edx - 0x10) == 0x3B03126F &&
                    *(uint32_t*)(regs.edx + 0x20) == 0x3E59999A &&
                    *(uint32_t*)(regs.edx + 0x24) == 0x3F372474 &&
                    *(uint32_t*)(regs.edx + 0x28) == 0x3D93A92A
                )
                {

                    auto f_ptr = (uint32_t*)(regs.edx - 0x158);
                    if (f_ptr)
                    {
                        //auto f_00 = f_ptr[3];
                        //auto f_01 = f_ptr[7];
                        auto f_02 = f_ptr[18];
                        auto f_04 = f_ptr[10];
                        auto f_05 = f_ptr[13];
                        //auto f_06 = f_ptr[12];
                        //auto f_07 = f_ptr[17];
                        auto f_08 = f_ptr[19];
                        //auto f_09 = f_ptr[22];
                        //auto f_10 = f_ptr[23];
                        //auto f_11 = f_ptr[24];

                        if (f_05 != 0x7f800001 && f_05 != 0xcdcdcdcd && f_04 == 0x00000000 && f_08 != 0xba8bfc22 && f_02 != 0x3f800000)
                        {
                            *(float*)(regs.edx + 0x00) = -*(float*)(regs.edx + 0x00);
                        }
                    }
                }

                regs.edi = regs.edi << 4;
                regs.ebx = regs.ebx << 4;
            }
        }; injector::MakeInline<ShaderTest>(pattern.count(2).get(1).get<void>(0), pattern.count(2).get(1).get<void>(6)); //417800

        //pattern = hook::pattern("0F B6 29 0F B6 32 2B F5 75 45 83 E8 01 83 C1 01 83 C2 01 85 C0");
        //struct ShaderTest2
        //{
        //    void operator()(injector::reg_pack& regs)
        //    {
        //        *(uint8_t*)&regs.ebp = *(uint8_t*)(regs.ecx + 0x00);
        //        *(uint8_t*)&regs.esi = *(uint8_t*)(regs.edx + 0x00);
        //    }
        //}; injector::MakeInline<ShaderTest2>(pattern.count(6).get(3).get<void>(0), pattern.count(6).get(3).get<void>(6)); //41782D
    }

    //draw distance adjuster
    //static float f_dist_mult = 10.0f;
    //pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 11 05 ? ? ? ? F3 0F 10 86"); //0x95F8AA
    //injector::WriteMemory(pattern.get_first(4), &f_dist_mult, true);
    //injector::WriteMemory(pattern.get_first(-12), &f_dist_mult, true);
}

CEXP void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init, hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB ? E8"));
    });
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    return TRUE;
}
#include "Windows.h"
#include "libmodupdater.h"

int main(int argc, char** argv)
{
    HMODULE hm = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&main, &hm);

    if (muAppendZipFile(argc, argv))
        return 0; // if zip file is appended, exiting

    HICON icon = LoadIconW(hm, MAKEINTRESOURCEW(101));
    muSetInstallerIcon(hm, icon);
    muSetInstallerWindowTitle(hm, "Fusion Fix for GTAIV: The Complete Edition");
    muSetInstallerMainInstruction(hm, "Choose where to install Fusion Fix");
    muSetInstallerContent(hm,
        "Fusion Fix is a comprehensive modification for Grand Theft Auto IV: The Complete Edition that aims "
        "to fix a wide range of technical issues, bugs, and limitations in the game that were left unaddressed in "
        "official updates. This project represents a community-driven effort to restore and enhance the Grand Theft "
        "Auto IV experience for modern systems."
        "\n\n"
        "<a href=\"https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix\">GitHub repository</a>\n"
        "<a href=\"https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/issues\">Report an issue</a>"
    );

    muSetInstallerFooter(hm, "<a href=\"https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix\">https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix</a>");
    muSetRGLAppID(hm, "Grand Theft Auto IV", "");
    muSetSteamAppID(hm, "12210", "GTAIV");

    muSetUpdateURL(hm, "https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFix.zip");
    muInitInstaller();

    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    return main(__argc, __argv);
}
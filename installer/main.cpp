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

    muSetInstallerFooter(hm, "<a href=\"https://fusionfix.io/iv\">fusionfix.io/iv</a>");
    muSetRGLAppID(hm, "Grand Theft Auto IV", "");
    muSetSteamAppID(hm, "12210", "GTAIV");
    // any of them marks a game folder, "Launch" starts the first one found
    muSetInstallerGameExecutable(hm, "GTAIV.exe;EFLC.exe;PlayGTAIV.exe;LaunchGTAIV.exe;LaunchEFLC.exe");
    muSetInstallerIniMode(hm, MU_INI_MERGE, true);

    // modern installer window, "--mu-ui=classic" on the command line shows the TaskDialog version
    muSetInstallerUI(hm, MU_UI_MODERN);
    muSetInstallerString(hm, MU_STR_HEADING, ""); // the logo already shows the name
    muSetInstallerTextBackdropBlur(hm, 10); // the pictures stay sharp, only the parts behind the texts are blurred
    // the menu color of GTA IV: the install button and the checkbox
    muSetInstallerColor(hm, MU_COLOR_BUTTON, RGB(0xF0, 0xA0, 0x00));
    muSetInstallerColor(hm, MU_COLOR_BUTTON_HOVER, RGB(0xFF, 0xB4, 0x1E));
    muSetInstallerColor(hm, MU_COLOR_BUTTON_PRESSED, RGB(0xC0, 0x80, 0x00));
    muSetInstallerColor(hm, MU_COLOR_BUTTON_TEXT, RGB(0x14, 0x12, 0x0E));

    // light and dark look, picked like the Windows app mode ("--mu-theme=light|dark" or muSetInstallerTheme to force one)
    muSetInstallerThemeLogoResource(hm, MU_THEME_DARK, MAKEINTRESOURCEA(102), "PNG");
    muSetInstallerThemeBackgroundResource(hm, MU_THEME_DARK, MAKEINTRESOURCEA(104), "JPG");
    muSetInstallerThemeBackgroundOverlay(hm, MU_THEME_DARK, 40);
    muSetInstallerThemeGradient(hm, MU_THEME_DARK, RGB(0x1E, 0x22, 0x2C), RGB(0x08, 0x09, 0x0C));
    muSetInstallerThemeColor(hm, MU_THEME_DARK, MU_COLOR_PROGRESS, RGB(0x00, 0xDC, 0xDC));
    muSetInstallerThemeColor(hm, MU_THEME_DARK, MU_COLOR_TEXT, RGB(0xFF, 0xFF, 0xFF));             // bright text over the picture
    muSetInstallerThemeColor(hm, MU_THEME_DARK, MU_COLOR_TEXT_SECONDARY, RGB(0xEC, 0xEE, 0xF2));
    muSetInstallerThemeLogoResource(hm, MU_THEME_LIGHT, MAKEINTRESOURCEA(103), "PNG");
    muSetInstallerThemeBackgroundResource(hm, MU_THEME_LIGHT, MAKEINTRESOURCEA(105), "JPG");
    muSetInstallerThemeBackgroundOverlay(hm, MU_THEME_LIGHT, 20); // the picture is misty already
    muSetInstallerThemeGradient(hm, MU_THEME_LIGHT, RGB(0xF4, 0xF5, 0xF8), RGB(0xD9, 0xDD, 0xE4));
    muSetInstallerThemeColor(hm, MU_THEME_LIGHT, MU_COLOR_PROGRESS, RGB(0x00, 0x97, 0xA7));
    muSetInstallerThemeColor(hm, MU_THEME_LIGHT, MU_COLOR_TEXT, RGB(0x00, 0x00, 0x00));            // dark text over the picture
    muSetInstallerThemeColor(hm, MU_THEME_LIGHT, MU_COLOR_TEXT_SECONDARY, RGB(0x12, 0x12, 0x12));
    muSetInstallerThemeColor(hm, MU_THEME_LIGHT, MU_COLOR_LINK, RGB(0x00, 0x14, 0x5A));            // navy, lighter blues are hard to read over the picture
    muSetInstallerThemeColor(hm, MU_THEME_LIGHT, MU_COLOR_LINK_HOVER, RGB(0x00, 0x32, 0xA0));

    muSetUpdateURL(hm, "https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFix.zip");
    return muRunInstaller();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    return main(__argc, __argv);
}
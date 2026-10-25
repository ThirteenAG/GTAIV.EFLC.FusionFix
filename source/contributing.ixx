module;

#include <common.hxx>

export module contributing;

import common;
import settings;

// A starting point for new features: this module does nothing until you add code below.
//
// Adding a setting takes four steps (the settings use ASI Settings from FusionFixUtils; the same description drives
// the in-game menu, FusionFixHub and fusionfix.io):
//
// 1. Describe it in settings/GTAIV.EFLC.FusionFix.json. The id is SECTION.Key; label and description point to texts:
//        {
//          "id": "MISC.MyFeature", "type": "bool", "default": 1,
//          "label": "@MISC.MyFeature", "description": "@MISC.MyFeature.description",
//          "page": "game", "control": [ "switch", "cycler" ],
//          "presets": { "vanilla": 0, "recommended": 1, "enhanced": 1 }
//        }
//    Numbers add "min", "max", "step"; lists use "type": "choice" with "choices". Add "apply": "restart" only when the
//    value can't change while the game runs. "visibility": "editor" keeps it out of the in-game menu (FusionFixHub and
//    fusionfix.io only). The default must equal the Recommended preset; Vanilla is the original game.
//
// 2. Add its texts to settings/GTAIV.EFLC.FusionFix.en.json ("MISC.MyFeature" and "MISC.MyFeature.description") and to
//    the other languages' files; English is the fallback. The build checks the description and texts
//    (asi-settings-validate) and fails on mistakes.
//
// 3. Read it in code. SettingsReader gives the value in effect when called:
//        SettingsReader reader;
//        auto enabled = reader.ReadInteger("MISC", "MyFeature", 1) != 0;
//    Settings apply live by default, so react to changes too:
//        SettingsPlugin().OnChange("MISC.MyFeature", [] { /* apply the new value */ });
//    Off must cost nothing: install hooks once and return early while the setting is off.
//
// 4. To show it as a row in one of the game's own option screens, add AddSetting(screen, "MISC.MyFeature") to the
//    list of rows in CSettings (settings.ixx). Rows that the game's menu stores as preferences also get a PREF_ name in
//    the table there, and FusionFixSettings.GetRef("PREF_MYFEATURE") then gives a reference that follows the menu.

class Contributing
{
public:
    Contributing()
    {
        FusionFix::onInitEvent() += []()
        {
            // Add your code here
        };
    }
} Contributing;
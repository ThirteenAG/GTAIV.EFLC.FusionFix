"""Verify the upstream JSON/PREF/menu contract for Distant Lights."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[1]
schema = json.loads((root/'settings/GTAIV.EFLC.FusionFix.json').read_text(encoding='utf-8'))
choices = [s for s in schema['settings'] if s['id'] == 'MISC.NewDistantLights']
assert len(choices) == 1
setting = choices[0]
assert setting['type'] == 'choice' and setting['default'] == 0
assert setting['page'] == 'game' and setting.get('apply', 'live') != 'restart'
assert [(c['value'], c['label']) for c in setting['choices']] == [(0, '@MO_DEF'), (1, '@NDL_CONSOLE')]
assert setting['label'] == '@Distant Lights'
assert all(v == 0 for v in setting['presets'].values())
for lang in ['en', 'fr', 'de', 'it', 'es', 'ru', 'ja']:
    text = json.loads((root/f'settings/GTAIV.EFLC.FusionFix.{lang}.json').read_text(encoding='utf-8'))
    for key in ['Distant Lights', 'NDL_CONSOLE', 'MISC.NewDistantLights.description', 'MO_DEF']:
        assert text[key].strip(), (lang, key)
settings = (root/'source/settings.ixx').read_text(encoding='utf-8')
assert len(re.findall(r'\{\s*"PREF_NEWDISTANTLIGHTS",\s*"MISC.NewDistantLights"\s*\}', settings)) == 1
assert settings.count('AddSetting(screen, "MISC.NewDistantLights", before);') == 1
assert re.search(r'AddSetting\(screen, "SHADOWS.ExtraNightShadows", before\);\s*AddSetting\(screen, "MISC.NewDistantLights", before\);', settings)
start = settings.index('for (auto [screen, before]')
end = settings.index('// Controls, at the end.', start)
assert 'MenuScreen::Game' in settings[start:end] and 'MenuScreen::TitleGame' in settings[start:end]
assert 'MISC.NewDistantLights' in settings[start:end]
assert 'MISC.NewDistantLights' not in settings[settings.index('// Display:'):settings.index('// Graphics:')]
module = (root/'source/distantlights.ixx').read_text()
assert 'FusionFixSettings.Get("PREF_NEWDISTANTLIGHTS")' in module
assert 'export module distantlights;' in module
height = (root/'source/lodlights.ixx').read_text()
active = '\n'.join(line for line in height.splitlines() if not line.lstrip().startswith('//'))
assert 'onInitEventAsync' not in active and 'fadeStartHeight' not in active
assert 'camera Z > 75' in height
assert 'camera.z <= 0.0f' in module
assert 'WriteMemory<float*>(heightGate+4, &heightStart' in module
assert hashlib.sha256((root/'data/update/pc/data/paths/distantlights_360.dat').read_bytes()).hexdigest() == '81562cfc0f7d5b4477bcbbdd8245aa5bc32889d0f59203bcf5f538344c3c7148'
print('Distant Lights: live 0/1 preference, Game/title placement, seven languages, height policy and Xbox data passed.')

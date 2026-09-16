"""Factory metadata contracts; audible behavior is checked by the DSP renderer."""
import json
from pathlib import Path
import re
import unittest

CATALOG = Path(__file__).resolve().parents[1] / 'resources' / 'factory_presets.json'

class FactoryCuration(unittest.TestCase):
    def test_names_controls_and_expressions(self):
        presets = json.loads(CATALOG.read_text(encoding='utf-8'))
        self.assertEqual(len(presets), len({p['name'].casefold() for p in presets}))
        for preset in presets:
            with self.subTest(preset=preset['name']):
                script = preset['script']
                body = '\n'.join(line for line in script.splitlines() if not line.startswith(('param ', '#')))
                self.assertIsNone(re.search(r';\s*[+*/-]\s*=', body), 'factory arithmetic should use canonical expressions')
                for letter in 'ABCDEF':
                    knob = preset.get('param' + letter)
                    if knob is None:
                        continue
                    self.assertLessEqual(min(knob['min'], knob['max']), knob['default'])
                    self.assertLessEqual(knob['default'], max(knob['min'], knob['max']))
                    self.assertRegex(body, r'\b' + letter.lower() + r'\b', 'unused factory macro')
                text = (preset['name'] + ' ' + preset['description'] + ' ' + script).casefold()
                for artist in ('zatox', 'holypriest', 'holy priest', 'sub zero project', 'rekkt', 'zerosum', 'koren stack', 'priest split', 'sub zero pummel'):
                    self.assertNotIn(artist, text)

if __name__ == '__main__':
    unittest.main()

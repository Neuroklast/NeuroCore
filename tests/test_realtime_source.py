"""Guard known allocating IIR factory calls in runtime coefficient builders."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class RuntimeCoefficientContract(unittest.TestCase):
    def test_coefficient_updates_do_not_construct_reference_counted_objects(self):
        files = [ROOT / 'src/dsl/SignalChain.cpp', *sorted((ROOT / 'src/dsl/blocks').glob('*.cpp'))]
        offenders = []
        for path in files:
            for line, text in enumerate(path.read_text().splitlines(), 1):
                if re.search(r'IIR::Coefficients<float>::make', text):
                    offenders.append(f'{path.relative_to(ROOT)}:{line}')
        self.assertEqual(offenders, [], 'Use preallocated coefficients and ArrayCoefficients: ' + ', '.join(offenders))

class ParameterNotificationContract(unittest.TestCase):
    def test_host_parameter_callback_does_not_persist_global_preferences(self):
        source = (ROOT / 'src/core/PluginProcessor.cpp').read_text()
        body = source.split('void NeuroKoreAudioProcessor::parameterChanged', 1)[1].split('juce::StringArray NeuroKoreAudioProcessor::getPresetNames', 1)[0]
        self.assertNotIn('UiSettings::get().setOversamplingIndex', body)
        self.assertNotIn('UiSettings::get().setPolisherIndex', body)

if __name__ == '__main__':
    unittest.main()

#include "StandaloneAudioSettings.h"
#include <JuceHeader.h>

#if JUCE_MODULE_AVAILABLE_juce_audio_plugin_client
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

void tryOpenStandaloneAudioSettings()
{
#if JUCE_MODULE_AVAILABLE_juce_audio_plugin_client
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->showAudioSettingsDialog();
#else
    juce::ignoreUnused (0);
#endif
}

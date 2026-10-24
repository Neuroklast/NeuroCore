#include "StandaloneAudioSettings.h"
#include <JuceHeader.h>

#if JUCE_MODULE_AVAILABLE_juce_audio_plugin_client
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace
{
#if JUCE_MODULE_AVAILABLE_juce_audio_plugin_client
juce::StandaloneFilterWindow* findStandaloneWindow (juce::Component& editor) noexcept
{
    return dynamic_cast<juce::StandaloneFilterWindow*> (editor.getTopLevelComponent());
}
#endif
}

void tryOpenStandaloneAudioSettings()
{
#if JUCE_MODULE_AVAILABLE_juce_audio_plugin_client
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->showAudioSettingsDialog();
#else
    juce::ignoreUnused (0);
#endif
}

void configureStandaloneWindowChrome (juce::Component& editor)
{
#if JUCE_MODULE_AVAILABLE_juce_audio_plugin_client
    auto* window = findStandaloneWindow (editor);
    if (window == nullptr || window->getTitleBarHeight() == 0)
        return;

    // This changes only the default StandaloneFilterWindow created by JUCE,
    // not the VST3/AU host window. Keep OS-level close and resize shortcuts.
    window->setTitleBarButtonsRequired (0, false);
    window->setTitleBarHeight (0);
    window->setColour (juce::ResizableWindow::backgroundColourId,
                       juce::Colour (0xff000000));

    // Older JUCE wrappers create an "Options" button in their title strip.
    // The same audio device dialog remains available in NEUROKORE Settings.
    for (int i = 0; i < window->getNumChildComponents(); ++i)
        if (auto* button = dynamic_cast<juce::TextButton*> (window->getChildComponent (i)))
            if (button->getButtonText().equalsIgnoreCase ("Options"))
                button->setVisible (false);
#else
    juce::ignoreUnused (editor);
#endif
}

bool performStandaloneWindowAction (juce::Component& editor,
                                    const juce::String& action,
                                    int deltaX, int deltaY)
{
#if JUCE_MODULE_AVAILABLE_juce_audio_plugin_client
    auto* window = findStandaloneWindow (editor);
    if (window == nullptr)
        return false;

    if (action == "minimise")
    {
        window->setMinimised (true);
        return true;
    }
    if (action == "close")
    {
        // Never destroy the JUCE processor/WebView during the native bridge's
        // synchronous callback. Complete the browser request before quitting.
        juce::Component::SafePointer<juce::StandaloneFilterWindow> safe (window);
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
                safe->closeButtonPressed();
        });
        return true;
    }
    if (action == "move")
    {
        if (deltaX < -128 || deltaX > 128 || deltaY < -128 || deltaY > 128)
            return false;
        window->setTopLeftPosition (window->getX() + deltaX,
                                    window->getY() + deltaY);
        return true;
    }
#else
    juce::ignoreUnused (editor, action, deltaX, deltaY);
#endif
    return false;
}

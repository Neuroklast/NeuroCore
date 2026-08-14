#include <JuceHeader.h>
#include "../../src/licensing/LicenseCrypto.h"
#include "LicensePrivateKey.h"
#include <iostream>

namespace
{
juce::String issueToFile (const juce::String& email, const juce::File& dest, juce::String& error)
{
    const auto normalised = LicenseCrypto::normalizeEmail (email);
    if (! LicenseCrypto::looksLikeEmail (normalised))
    {
        error = "Need a valid email address.";
        return {};
    }

    LicensePayload payload;
    payload.email = normalised;
    payload.issued = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");

    const auto text = LicenseCrypto::issue (payload, neurocoreProductPrivateKey());
    if (text.isEmpty() || ! text.contains ("sig="))
    {
        error = "Signing failed.";
        return {};
    }

    LicensePayload check;
    if (! LicenseCrypto::parseAndVerify (text, LicenseCrypto::productPublicKey(), check, error))
    {
        if (error.isEmpty())
            error = "Issued file did not verify.";
        return {};
    }

    auto out = dest;
    if (! out.hasFileExtension (".lic"))
        out = out.withFileExtension (".lic");
    if (! out.replaceWithText (text))
    {
        error = "Could not write " + out.getFullPathName();
        return {};
    }
    return out.getFullPathName();
}

int runCli (const juce::StringArray& args)
{
    if (args.contains ("--write-keys"))
    {
        juce::RSAKey pub, priv;
        juce::RSAKey::createKeyPair (pub, priv, 1024);
        juce::String out;
        out << "PUB\n" << pub.toString() << "\nPRIV\n" << priv.toString() << "\n";
        const auto file = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                              .getSiblingFile ("keys_out.txt");
        file.replaceWithText (out);
        std::cout << out << std::endl;
        return 0;
    }

    if (args.contains ("--selftest"))
    {
        juce::String error;
        const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("neurocore-selftest.lic");
        const auto path = issueToFile ("selftest@neuroklast.net", tmp, error);
        const bool ok = path.isNotEmpty();
        const auto report = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                .getSiblingFile ("issuer_selftest.txt");
        report.replaceWithText (ok ? ("OK " + path) : ("FAIL " + error));
        std::cout << (ok ? "OK " : "FAIL ") << (ok ? path : error) << std::endl;
        tmp.deleteFile();
        return ok ? 0 : 1;
    }

    juce::String email;
    juce::File dest;
    for (int i = 0; i < args.size(); ++i)
    {
        const auto& a = args[i];
        if (a.startsWith ("-"))
            continue;
        if (email.isEmpty() && a.containsChar ('@'))
            email = a;
        else if (dest == juce::File())
            dest = juce::File (a);
    }

    if (email.isEmpty())
        return -1; // tell caller to open the window

    if (dest == juce::File())
    {
        const auto slug = LicenseCrypto::normalizeEmail (email).replaceCharacters ("@.", "__");
        dest = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("NEUROKORE-" + slug + ".lic");
    }

    juce::String error;
    const auto path = issueToFile (email, dest, error);
    if (path.isEmpty())
    {
        std::cerr << error << std::endl;
        return 1;
    }
    std::cout << path << std::endl;
    return 0;
}
}

class IssuerComponent : public juce::Component
{
public:
    IssuerComponent()
    {
        title.setText ("NEUROKORE License", juce::dontSendNotification);
        title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, juce::Colour (0xffff2a2a));
        title.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (title);

        hint.setText ("Enter the tester email. A signed .lic file is written.",
                      juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
        addAndMakeVisible (hint);

        email.setTextToShowWhenEmpty ("name@example.com", juce::Colours::grey);
        email.setFont (juce::FontOptions (16.0f));
        email.setIndents (8, 4);
        addAndMakeVisible (email);

        create.setButtonText ("Create license file");
        create.onClick = [this] { saveLicense(); };
        addAndMakeVisible (create);

        status.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
        addAndMakeVisible (status);

        setSize (520, 220);
        setOpaque (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0a0000));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20);
        title.setBounds (r.removeFromTop (28));
        r.removeFromTop (8);
        hint.setBounds (r.removeFromTop (22));
        r.removeFromTop (12);
        email.setBounds (r.removeFromTop (32));
        r.removeFromTop (12);
        create.setBounds (r.removeFromTop (32).removeFromLeft (200));
        r.removeFromTop (10);
        status.setBounds (r.removeFromTop (28));
    }

private:
    void saveLicense()
    {
        const auto address = LicenseCrypto::normalizeEmail (email.getText());
        if (! LicenseCrypto::looksLikeEmail (address))
        {
            status.setText ("Need a valid email address.", juce::dontSendNotification);
            return;
        }

        const auto slug = address.replaceCharacters ("@.", "__");
        const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile ("NEUROKORE-" + slug + ".lic");
        chooser = std::make_unique<juce::FileChooser> ("Save license", start, "*.lic");
        constexpr int flags = juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting;
        chooser->launchAsync (flags, [this, address] (const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest == juce::File())
                return;
            juce::String error;
            const auto path = issueToFile (address, dest, error);
            status.setText (path.isNotEmpty() ? ("Wrote " + path) : error,
                            juce::dontSendNotification);
        });
    }

    juce::Label title, hint, status;
    juce::TextEditor email;
    juce::TextButton create;
    std::unique_ptr<juce::FileChooser> chooser;
};

class IssuerWindow : public juce::DocumentWindow
{
public:
    IssuerWindow()
        : DocumentWindow ("NEUROKORE License Issuer",
                          juce::Colour (0xff0a0000),
                          DocumentWindow::closeButton | DocumentWindow::minimiseButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new IssuerComponent(), true);
        centreWithSize (520, 220);
        setResizable (false, false);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class IssuerApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "NeuroKoreIssuer"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise (const juce::String&) override
    {
        const int cli = runCli (getCommandLineParameterArray());
        if (cli >= 0)
        {
            setApplicationReturnValue (cli);
            quit();
            return;
        }
        window = std::make_unique<IssuerWindow>();
    }

    void shutdown() override { window.reset(); }

private:
    std::unique_ptr<IssuerWindow> window;
};

START_JUCE_APPLICATION (IssuerApp)

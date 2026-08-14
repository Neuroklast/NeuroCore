#pragma once

#include <JuceHeader.h>
#include "../src/core/Config.h"
#include "../src/licensing/LicenseCrypto.h"
#include "../src/licensing/LicenseManager.h"

class LicenseTest : public juce::UnitTest
{
public:
    LicenseTest() : juce::UnitTest ("LicenseTest", "Licensing") {}

    void runTest() override
    {
        beginTest ("demo window is 20 minutes and licensing is on");
        {
            expect (Config::kEnableLicensing);
            expectWithinAbsoluteError (Config::kDemoDurationSeconds, 20.0 * 60.0, 0.01);
        }

        beginTest ("email normalisation");
        {
            expectEquals (LicenseCrypto::normalizeEmail ("  Ada@NeuroKlast.NET "),
                          juce::String ("ada@neuroklast.net"));
            expect (LicenseCrypto::looksLikeEmail ("ada@neuroklast.net"));
            expect (! LicenseCrypto::looksLikeEmail ("not-an-email"));
            expect (! LicenseCrypto::looksLikeEmail ("missing@tld"));
        }

        beginTest ("signed license verifies and rejects tampering");
        {
            juce::RSAKey pub, priv;
            juce::RSAKey::createKeyPair (pub, priv, 512);

            LicensePayload payload;
            payload.email = "tester@example.com";
            payload.issued = "2026-08-13";
            const auto text = LicenseCrypto::issue (payload, priv);
            expect (text.contains ("NEUROKORE LICENSE"));
            expect (LicenseCrypto::isAcceptedProduct ("NEUROKORE"));
            expect (LicenseCrypto::isAcceptedProduct ("NeuroCore"));
            expect (! LicenseCrypto::isAcceptedProduct ("OtherPlugin"));

            LicensePayload loaded;
            juce::String error;
            expect (LicenseCrypto::parseAndVerify (text, pub, loaded, error), error);
            expectEquals (loaded.email, juce::String ("tester@example.com"));
            expectEquals (loaded.issued, juce::String ("2026-08-13"));

            auto tweaked = text.replace ("tester@example.com", "other@example.com");
            expect (! LicenseCrypto::parseAndVerify (tweaked, pub, loaded, error));

            const auto sigLine = text.fromFirstOccurrenceOf ("sig=", false, false).trim();
            expect (sigLine.isNotEmpty());
            auto broken = text.replace ("sig=" + sigLine, "sig=1");
            expect (! LicenseCrypto::parseAndVerify (broken, pub, loaded, error));
        }

        beginTest ("product public key parses");
        {
            const auto pub = LicenseCrypto::productPublicKey();
            expect (pub.isValid());
        }

        beginTest ("unsigned junk is not a license");
        {
            LicenseManager manager;
            const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("neurocore-bad.lic");
            tmp.replaceWithText ("{ \"email\": \"x@y.com\" }");
            expect (! manager.importLicenseFile (tmp));
            tmp.deleteFile();
        }
    }
};

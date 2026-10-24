#ifndef PRESETLIBRARYPACKTEST_H
#define PRESETLIBRARYPACKTEST_H

#include "../src/core/PluginProcessor.h"
#include "../src/utils/PresetIdentity.h"
#include "../src/utils/PresetLibrary.h"
#include "../src/utils/PresetLibraryIndex.h"
#include "../src/utils/PresetManager.h"
#include "../src/utils/PresetPack.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include <JuceHeader.h>
#include <algorithm>
#include <cstring>

class PresetLibraryPackTest : public juce::UnitTest
{
public:
    PresetLibraryPackTest() : juce::UnitTest ("PresetLibraryPackTest", "Persistence") {}

    void runTest() override
    {
        beginTest ("factory ids are reserved and stable");
        {
            expectEquals (PresetIdentity::factoryId ("Mesa High Gain"),
                          juce::String ("factory:Mesa High Gain"));
            expect (PresetIdentity::isFactoryId ("factory:Mesa High Gain"));
            expectEquals (PresetIdentity::factoryNameFromId ("factory:Mesa High Gain"),
                          juce::String ("Mesa High Gain"));
            expect (! PresetIdentity::isFactoryId ("user-uuid"));
        }

        beginTest ("legacy ids stay stable across scans");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x", err));
            auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-id-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            root.deleteRecursively();
            root.createDirectory();
            const auto nrk = root.getChildFile ("Legacy.nrk");
            expect (mgr.savePreset (nrk, "Legacy"));
            PresetManager::Info info;
            expect (mgr.readInfo (nrk, info));
            const auto written = info.id;
            expect (written.isNotEmpty());
            expect (! PresetIdentity::isFactoryId (written));

            auto stripped = nrk;
            // second scan of a copy without rewriting: catalog path key
            auto copy = root.getChildFile ("Copy.nrk");
            expect (nrk.copyFileTo (copy));
            const auto a = PresetIdentity::legacyIdFor (copy, root);
            const auto b = PresetIdentity::legacyIdFor (copy, root);
            expectEquals (a, b);
            root.deleteRecursively();
        }

        beginTest ("same display name in two packs keeps distinct ids");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x * a", err));
            auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-dup-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            root.deleteRecursively();
            root.createDirectory();
            const auto aFile = root.getChildFile ("A.nrk");
            const auto bFile = root.getChildFile ("B.nrk");
            expect (mgr.savePreset (aFile, "Heavy Drive", "One"));
            expect (mgr.savePreset (bFile, "Heavy Drive", "Two"));
            PackManifest ma, mb;
            ma.name = "Pack One";
            ma.id = "net.test.pack-one";
            mb.name = "Pack Two";
            mb.id = "net.test.pack-two";
            const auto pa = root.getChildFile ("one.nrkpack");
            const auto pb = root.getChildFile ("two.nrkpack");
            juce::String packErr;
            expect (PresetPack::exportNrkpack ({ aFile }, pa, ma, packErr), packErr);
            expect (PresetPack::exportNrkpack ({ bFile }, pb, mb, packErr), packErr);
            auto lib = root.getChildFile ("lib");
            auto r1 = PresetPack::installPreview (PresetPack::analyzeArchive (pa), lib);
            auto r2 = PresetPack::installPreview (PresetPack::analyzeArchive (pb), lib);
            expect (r1.ok, r1.error);
            expect (r2.ok, r2.error);
            PresetLibraryIndex index;
            auto& factory = FactoryPresetLibrary::getInstance();
            index.rebuild (mgr, factory, lib);
            int heavy = 0;
            juce::StringArray ids;
            for (const auto& e : index.entries())
            {
                if (e.displayName != "Heavy Drive")
                    continue;
                ++heavy;
                ids.add (e.presetId);
                expect (e.packId.isNotEmpty());
            }
            expectEquals (heavy, 2);
            expect (ids[0] != ids[1]);
            root.deleteRecursively();
        }

        beginTest ("zip slip and absolute paths are refused");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x", err));
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-zip-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            const auto nrk = work.getChildFile ("ok.nrk");
            expect (mgr.savePreset (nrk, "ok"));

            expect (PresetPack::zipPathSafe ("presets/ok.nrk"));
            expect (! PresetPack::zipPathSafe ("../evil.nrk"));
            expect (! PresetPack::zipPathSafe ("foo/../../x.nrk"));
            expect (! PresetPack::zipPathSafe ("/tmp/x.nrk"));
            expect (! PresetPack::zipPathSafe ("C:/Windows/x.nrk"));
            expect (! PresetPack::zipPathSafe ("\\\\server\\share\\x.nrk"));
            expect (! PresetPack::zipPathSafe ("CON"));
            expect (! PresetPack::zipPathSafe ("presets/NUL.nrk"));
            expect (! PresetPack::zipPathSafe ("COM1.nrk"));

            auto writeZip = [&] (const juce::String& stored, const juce::File& dest)
            {
                juce::ZipFile::Builder b;
                b.addFile (nrk, 9, stored);
                dest.deleteFile();
                juce::FileOutputStream os (dest);
                expect (os.openedOk());
                expect (b.writeToStream (os, nullptr));
            };
            const auto slip = work.getChildFile ("slip.zip");
            writeZip ("../evil.nrk", slip);
            juce::String extractErr;
            auto dest = work.getChildFile ("out");
            dest.createDirectory();
            expect (! PresetPack::extractSafe (slip, dest, extractErr));
            expect (extractErr.containsIgnoreCase ("illegal") || extractErr.containsIgnoreCase ("path"));
            expect (! dest.getParentDirectory().getChildFile ("evil.nrk").existsAsFile());

            const auto absz = work.getChildFile ("abs.zip");
            writeZip ("/tmp/abs.nrk", absz);
            expect (! PresetPack::extractSafe (absz, dest, extractErr));
            work.deleteRecursively();
        }

        beginTest ("checksum mismatch and unknown format version are refused");
        {
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-bad-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            PackManifest m;
            juce::String err;
            expect (! PresetPack::parseManifestText ("{ \"format\": \"neurokore-pack\", \"formatVersion\": 99, \"id\": \"x\", \"name\": \"X\" }", m, err));
            expect (err.containsIgnoreCase ("not supported"));

            expect (PresetPack::parseManifestText (
                "{ \"format\": \"neurokore-pack\", \"formatVersion\": 1, \"id\": \"x\", \"name\": \"X\", \"extra\": { \"ok\": true }, \"presets\": [] }",
                m, err), err);

            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            expect (proc.setFormula ("stage1: y = x", err));
            const auto nrk = work.getChildFile ("A.nrk");
            expect (mgr.savePreset (nrk, "A"));
            auto dir = work.getChildFile ("pack");
            dir.createDirectory();
            nrk.copyFileTo (dir.getChildFile ("A.nrk"));
            dir.getChildFile ("manifest.json").replaceWithText (
                "{ \"format\": \"neurokore-pack\", \"formatVersion\": 1, \"id\": \"net.test.bad\", \"name\": \"Bad\","
                " \"presets\": [ { \"id\": \"A\", \"file\": \"A.nrk\", \"sha256\": \"deadbeef\" } ] }");
            auto preview = PresetPack::analyzeDirectory (dir);
            expect (preview.error.containsIgnoreCase ("checksum"));
            work.deleteRecursively();
        }

        beginTest ("legacy pack.json still installs");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x", err));
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-leg-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            expect (mgr.savePreset (work.getChildFile ("A.nrk"), "A"));
            expect (mgr.savePreset (work.getChildFile ("B.nrk"), "B"));
            work.getChildFile ("pack.json").replaceWithText ("{ \"name\": \"Zardonic Metal Bass\" }");
            auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-leg-dst-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            dest.deleteRecursively();
            const auto r = PresetLibrary::importPathsInto ({ work.getFullPathName() }, dest);
            expectEquals (r.imported, 2);
            expectEquals (r.packName, juce::String ("Zardonic Metal Bass"));
            expect (dest.getChildFile ("Packs").getChildFile ("Zardonic Metal Bass")
                        .getChildFile ("A.nrk").existsAsFile());
            work.deleteRecursively();
            dest.deleteRecursively();
        }

        beginTest ("nrkpack export reimports");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = tanh(x)", err));
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-rt-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            const auto nrk = work.getChildFile ("Tone.nrk");
            expect (mgr.savePreset (nrk, "Tone", "Kay", "Bass"));
            PackManifest m;
            m.name = "Tone Pack";
            m.author = "Kay";
            const auto pack = work.getChildFile ("tone.nrkpack");
            expect (PresetPack::exportNrkpack ({ nrk }, pack, m, err), err);
            expect (pack.existsAsFile());
            auto lib = work.getChildFile ("lib");
            auto inst = PresetPack::installPreview (PresetPack::analyzeArchive (pack), lib);
            expect (inst.ok, inst.error);
            expectEquals (inst.installed, 1);
            NeuroKoreAudioProcessor loaded;
            PresetManager loadedMgr (loaded);
            juce::Array<juce::File> found;
            lib.findChildFiles (found, juce::File::findFiles, true, "*.nrk");
            expectEquals (found.size(), 1);
            expect (loadedMgr.loadPreset (found[0]));
            expect (loaded.getScript().contains ("tanh"));
            work.deleteRecursively();
        }

        beginTest ("uninstall removes only that pack");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x", err));
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-rm-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            const auto user = work.getChildFile ("Mine.nrk");
            expect (mgr.savePreset (user, "Mine"));
            auto lib = work.getChildFile ("lib");
            lib.createDirectory();
            expect (user.copyFileTo (lib.getChildFile ("Mine.nrk")));
            const auto packNrk = work.getChildFile ("P.nrk");
            expect (mgr.savePreset (packNrk, "Packed"));
            PackManifest m;
            m.name = "Temp Pack";
            m.id = "net.test.temp-pack";
            const auto pack = work.getChildFile ("temp.nrkpack");
            expect (PresetPack::exportNrkpack ({ packNrk }, pack, m, err), err);
            auto inst = PresetPack::installPreview (PresetPack::analyzeArchive (pack), lib);
            expect (inst.ok, inst.error);
            expect (PresetPack::uninstallPack (lib, "net.test.temp-pack", err), err);
            expect (lib.getChildFile ("Mine.nrk").existsAsFile());
            juce::Array<juce::File> left;
            lib.getChildFile ("Packs").findChildFiles (left, juce::File::findFiles, true, "*.nrk");
            expectEquals (left.size(), 0);
            work.deleteRecursively();
        }

        beginTest ("failed DSCR apply keeps the previous processor state");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x * a", err));
            proc.setCurrentPresetName ("KeepMe");
            if (auto* p = proc.apvts.getParameter ("a"))
                p->setValueNotifyingHost (0.3f);
            juce::TemporaryFile tmp (".nrk");
            expect (mgr.savePreset (tmp.getFile(), "broken"));
            juce::MemoryBlock raw;
            expect (tmp.getFile().loadFileAsData (raw));
            auto* bytes = static_cast<char*> (raw.getData());
            const juce::String needle ("stage1: y = x * a");
            const juce::String bad    ("this is not dsl!!");
            expectEquals (needle.length(), bad.length());
            auto* found = std::search (bytes, bytes + raw.getSize(),
                                       needle.toRawUTF8(), needle.toRawUTF8() + needle.getNumBytesAsUTF8());
            expect (found != bytes + raw.getSize());
            std::memcpy (found, bad.toRawUTF8(), (size_t) bad.getNumBytesAsUTF8());
            juce::TemporaryFile patched (".nrk");
            expect (patched.getFile().replaceWithData (raw.getData(), raw.getSize()));
            expect (! mgr.loadPreset (patched.getFile()));
            expectEquals (proc.getScript(), juce::String ("stage1: y = x * a"));
            expectEquals (proc.getCurrentPresetName(), juce::String ("KeepMe"));
            if (auto* p = proc.apvts.getParameter ("a"))
                expectWithinAbsoluteError (p->getValue(), 0.3f, 0.02f);
        }

        beginTest ("index search stays usable at 5000 entries");
        {
            PresetLibraryIndex index;
            for (int i = 0; i < 5000; ++i)
            {
                PresetIndexEntry e;
                e.presetId = "id-" + juce::String (i);
                e.displayName = (i % 17 == 0) ? juce::String ("Needle") : ("Tone " + juce::String (i));
                e.category = "User";
                e.sourceType = "user";
                index.append (std::move (e));
            }
            const auto t1 = juce::Time::getMillisecondCounterHiRes();
            auto hit = index.matching ("Needle");
            const auto searched = juce::Time::getMillisecondCounterHiRes() - t1;
            expect (searched < 50.0, "search 5000 entries: " + juce::String (searched) + " ms");
            expect (hit.size() >= 200);
            juce::ignoreUnused (searched);
        }

        beginTest ("export with no .nrk files fails");
        {
            juce::String err;
            const auto dest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("nk-empty-" + juce::String (juce::Random::getSystemRandom().nextInt())
                                                 + ".nrkpack");
            expect (! PresetPack::exportNrkpack ({}, dest, {}, err));
            expect (err.isNotEmpty());
            dest.deleteFile();
        }

        beginTest ("pluginVersionMeets reads numeric prefix");
        {
            expect (PresetPack::pluginVersionMeets ({}, "0.7.2-beta"));
            expect (PresetPack::pluginVersionMeets ("0.7.1", "0.7.2-beta"));
            expect (! PresetPack::pluginVersionMeets ("9.0.0", "0.7.2-beta"));
        }

        beginTest ("invalid pack ids are refused without silent rewrite");
        {
            PackManifest m;
            juce::String err;
            const auto refuse = [&] (const juce::String& id)
            {
                err = {};
                const auto json = "{ \"format\": \"neurokore-pack\", \"formatVersion\": 1, \"id\": \""
                                  + id + "\", \"name\": \"X\" }";
                expect (! PresetPack::parseManifestText (json, m, err), id);
                expect (err.isNotEmpty(), id);
            };
            refuse (".");
            refuse ("..");
            refuse ("...");
            refuse ("foo/bar");
            refuse ("foo\\\\bar");
            refuse ("C:evil");
            refuse ("CON");
            refuse ("com1");
            refuse ("NUL");
            refuse ("LPT9");
            refuse ("COM1.pack");
            refuse ("..evil");
            refuse ("");
            expect (PresetPack::isValidPackId ("net.test.pack-one"));
            expect (PresetPack::isValidPackId ("legacy.pack.zardonic-metal-bass"));
            expect (! PresetPack::isValidPackId (".."));
            expect (! PresetPack::isValidPackId ("CON"));
            expect (PresetPack::parseManifestText (
                "{ \"format\": \"neurokore-pack\", \"formatVersion\": 1, \"id\": \"net.test.pack-one\", \"name\": \"X\" }",
                m, err), err);
            expectEquals (m.id, juce::String ("net.test.pack-one"));
        }

        beginTest ("pack install cannot escape the Packs directory");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x", err));
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-esc-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            const auto nrk = work.getChildFile ("A.nrk");
            expect (mgr.savePreset (nrk, "A"));
            auto lib = work.getChildFile ("lib");
            lib.createDirectory();
            lib.getChildFile ("Packs").createDirectory();
            expect (lib.getChildFile ("keep-lib.txt").replaceWithText ("lib"));
            expect (lib.getChildFile ("Packs").getChildFile ("keep-packs.txt").replaceWithText ("packs"));
            const auto outside = work.getChildFile ("outside.txt");
            expect (outside.replaceWithText ("outside"));

            auto tryId = [&] (const juce::String& id)
            {
                auto dir = work.getChildFile ("src-" + juce::String (juce::Random::getSystemRandom().nextInt()));
                dir.createDirectory();
                expect (nrk.copyFileTo (dir.getChildFile ("A.nrk")));
                PackPreview crafted;
                crafted.source = dir;
                crafted.manifest.id = id;
                crafted.manifest.name = "Crafted";
                PackPresetRef ref;
                ref.id = "A";
                ref.file = "A.nrk";
                crafted.manifest.presets.push_back (ref);
                const auto r = PresetPack::installPreview (crafted, lib);
                expect (! r.ok, id);
                expect (r.error.isNotEmpty(), id);
                dir.deleteRecursively();
            };
            tryId (".");
            tryId ("..");
            tryId ("...");
            tryId ("../evil");
            tryId ("..\\evil");
            tryId ("CON");
            tryId ("foo/../../x");

            expect (lib.isDirectory());
            expect (lib.getChildFile ("keep-lib.txt").loadFileAsString() == "lib");
            expect (lib.getChildFile ("Packs").getChildFile ("keep-packs.txt").loadFileAsString() == "packs");
            expect (outside.loadFileAsString() == "outside");
            expect (! work.getChildFile ("evil").exists());
            expect (! lib.getChildFile ("A.nrk").existsAsFile());
            work.deleteRecursively();
        }

        beginTest ("failed install leaves the previous pack readable");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = tanh(x)", err));
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-tx-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            const auto goodNrk = work.getChildFile ("Good.nrk");
            expect (mgr.savePreset (goodNrk, "Good"));
            PackManifest m;
            m.name = "Keep Pack";
            m.id = "net.test.keep-pack";
            const auto pack = work.getChildFile ("keep.nrkpack");
            expect (PresetPack::exportNrkpack ({ goodNrk }, pack, m, err), err);
            auto lib = work.getChildFile ("lib");
            auto inst = PresetPack::installPreview (PresetPack::analyzeArchive (pack), lib);
            expect (inst.ok, inst.error);
            const auto dest = lib.getChildFile ("Packs").getChildFile ("net.test.keep-pack");
            juce::Array<juce::File> installed;
            dest.findChildFiles (installed, juce::File::findFiles, true, "*.nrk");
            expectEquals (installed.size(), 1);
            const auto sentinel = dest.getChildFile ("user-note.txt");
            expect (sentinel.replaceWithText ("do-not-lose"));
            const auto before = dest.getChildFile ("manifest.json").loadFileAsString();

            auto badDir = work.getChildFile ("bad");
            badDir.createDirectory();
            expect (goodNrk.copyFileTo (badDir.getChildFile ("Good.nrk")));
            badDir.getChildFile ("manifest.json").replaceWithText (
                "{ \"format\": \"neurokore-pack\", \"formatVersion\": 1, \"id\": \"net.test.keep-pack\", \"name\": \"Keep Pack\","
                " \"presets\": [ { \"id\": \"Good\", \"file\": \"Missing.nrk\" } ] }");
            auto missing = PresetPack::installPreview (PresetPack::analyzeDirectory (badDir), lib);
            expect (! missing.ok);
            expect (missing.error.containsIgnoreCase ("missing"));
            expectEquals (sentinel.loadFileAsString(), juce::String ("do-not-lose"));
            expectEquals (dest.getChildFile ("manifest.json").loadFileAsString(), before);

            badDir.getChildFile ("manifest.json").replaceWithText (
                "{ \"format\": \"neurokore-pack\", \"formatVersion\": 1, \"id\": \"net.test.keep-pack\", \"name\": \"Keep Pack\","
                " \"presets\": [ { \"id\": \"Good\", \"file\": \"Good.nrk\", \"sha256\": \"deadbeef\" } ] }");
            auto checksum = PresetPack::installPreview (PresetPack::analyzeDirectory (badDir), lib);
            expect (! checksum.ok);
            expect (checksum.error.containsIgnoreCase ("checksum"));
            expectEquals (sentinel.loadFileAsString(), juce::String ("do-not-lose"));
            expectEquals (dest.getChildFile ("manifest.json").loadFileAsString(), before);

            PackPreview crafted;
            crafted.source = dest;
            crafted.manifest.id = "net.test.keep-pack";
            crafted.manifest.name = "Keep Pack";
            PackPresetRef badRef;
            badRef.id = "Good";
            badRef.file = installed[0].getRelativePathFrom (dest).replaceCharacter ('\\', '/');
            badRef.sha256 = "deadbeef";
            crafted.manifest.presets.push_back (badRef);
            auto liveFail = PresetPack::installPreview (crafted, lib);
            expect (! liveFail.ok);
            expect (liveFail.error.containsIgnoreCase ("checksum"));
            expectEquals (sentinel.loadFileAsString(), juce::String ("do-not-lose"));
            expectEquals (dest.getChildFile ("manifest.json").loadFileAsString(), before);

            NeuroKoreAudioProcessor loaded;
            PresetManager loadedMgr (loaded);
            expect (loadedMgr.loadPreset (installed[0]));
            expect (loaded.getScript().contains ("tanh"));
            work.deleteRecursively();
        }

        beginTest ("same-version reinstall commits only a complete pack");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x * a", err));
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-re-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            const auto nrk = work.getChildFile ("Tone.nrk");
            expect (mgr.savePreset (nrk, "Tone"));
            PackManifest m;
            m.name = "Tone Pack";
            m.id = "net.test.tone-pack";
            m.version = "1.0.0";
            const auto pack = work.getChildFile ("tone.nrkpack");
            expect (PresetPack::exportNrkpack ({ nrk }, pack, m, err), err);
            auto lib = work.getChildFile ("lib");
            auto first = PresetPack::installPreview (PresetPack::analyzeArchive (pack), lib);
            expect (first.ok, first.error);
            auto again = PresetPack::installPreview (PresetPack::analyzeArchive (pack), lib);
            expect (again.ok, again.error);
            expectEquals (again.installed, 1);
            const auto dest = lib.getChildFile ("Packs").getChildFile ("net.test.tone-pack");
            expect (dest.getChildFile ("manifest.json").existsAsFile());
            PackManifest parsed;
            expect (PresetPack::parseManifestText (dest.getChildFile ("manifest.json").loadFileAsString(), parsed, err), err);
            expectEquals (parsed.id, juce::String ("net.test.tone-pack"));
            juce::Array<juce::File> found;
            dest.findChildFiles (found, juce::File::findFiles, true, "*.nrk");
            expectEquals (found.size(), 1);
            work.deleteRecursively();
        }

        beginTest ("export keeps colliding filenames as distinct archive entries");
        {
            NeuroKoreAudioProcessor proc;
            PresetManager mgr (proc);
            juce::String err;
            expect (proc.setFormula ("stage1: y = x * a", err));
            auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("nk-col-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            work.deleteRecursively();
            work.createDirectory();
            const auto dirA = work.getChildFile ("A");
            const auto dirB = work.getChildFile ("B");
            expect (dirA.createDirectory());
            expect (dirB.createDirectory());
            const auto aFile = dirA.getChildFile ("Drive.nrk");
            const auto bFile = dirB.getChildFile ("Drive.nrk");
            expect (mgr.savePreset (aFile, "Drive", "One", "Bass", "heavy"));
            expect (proc.setFormula ("stage1: y = tanh(x)", err));
            expect (mgr.savePreset (bFile, "Drive", "Two", "Lead", "bright"));
            PresetManager::Info ia, ib;
            expect (PresetManager::readInfo (aFile, ia));
            expect (PresetManager::readInfo (bFile, ib));
            expect (ia.id.isNotEmpty() && ib.id.isNotEmpty());
            expect (ia.id != ib.id);

            PackManifest m;
            m.name = "Collision Pack";
            m.id = "net.test.collision-pack";
            const auto pack = work.getChildFile ("collision.nrkpack");
            expect (PresetPack::exportNrkpack ({ aFile, bFile }, pack, m, err), err);
            auto preview = PresetPack::analyzeArchive (pack);
            expect (preview.error.isEmpty(), preview.error);
            expectEquals ((int) preview.manifest.presets.size(), 2);
            expect (preview.manifest.presets[0].id == ia.id);
            expect (preview.manifest.presets[1].id == ib.id);
            expect (preview.manifest.presets[0].file != preview.manifest.presets[1].file);
            expect (preview.manifest.presets[0].file.startsWith ("presets/"));
            expect (preview.manifest.presets[1].file.startsWith ("presets/"));
            expectEquals (preview.manifest.presets[0].category, juce::String ("Bass"));
            expectEquals (preview.manifest.presets[1].category, juce::String ("Lead"));

            auto lib = work.getChildFile ("lib");
            auto inst = PresetPack::installPreview (preview, lib);
            expect (inst.ok, inst.error);
            expectEquals (inst.installed, 2);
            juce::Array<juce::File> installed;
            lib.findChildFiles (installed, juce::File::findFiles, true, "*.nrk");
            expectEquals (installed.size(), 2);
            PresetLibraryIndex index;
            index.rebuild (mgr, FactoryPresetLibrary::getInstance(), lib);
            const auto* ea = index.findById (ia.id);
            const auto* eb = index.findById (ib.id);
            expect (ea != nullptr && eb != nullptr);
            expectEquals (ea->displayName, juce::String ("Drive"));
            expectEquals (eb->displayName, juce::String ("Drive"));
            expect (ea->presetId != eb->presetId);

            const auto pack2 = work.getChildFile ("collision2.nrkpack");
            expect (PresetPack::exportNrkpack ({ aFile, bFile }, pack2, m, err), err);
            auto preview2 = PresetPack::analyzeArchive (pack2);
            expectEquals ((int) preview2.manifest.presets.size(), 2);
            expect (preview2.manifest.presets[0].id == preview.manifest.presets[0].id);
            expect (preview2.manifest.presets[1].id == preview.manifest.presets[1].id);
            expect (preview2.manifest.presets[0].file == preview.manifest.presets[0].file);
            expect (preview2.manifest.presets[1].file == preview.manifest.presets[1].file);

            NeuroKoreAudioProcessor loaded;
            PresetManager loadedMgr (loaded);
            bool sawTanh = false, sawMul = false;
            for (auto& f : installed)
            {
                expect (loadedMgr.loadPreset (f));
                if (loaded.getScript().contains ("tanh"))
                    sawTanh = true;
                if (loaded.getScript().contains ("x * a"))
                    sawMul = true;
            }
            expect (sawTanh && sawMul);
            work.deleteRecursively();
        }
    }
};

#endif

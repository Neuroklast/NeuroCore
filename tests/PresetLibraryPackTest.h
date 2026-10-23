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
    }
};

#endif

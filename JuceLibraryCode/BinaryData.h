/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   outerKnob_png;
    const int            outerKnob_pngSize = 41283;

    extern const char*   knob_lights_png;
    const int            knob_lights_pngSize = 95148;

    extern const char*   overknob_png;
    const int            overknob_pngSize = 85891;

    extern const char*   innerknob_png;
    const int            innerknob_pngSize = 537203;

    extern const char*   de_txt;
    const int            de_txtSize = 979;

    extern const char*   en_txt;
    const int            en_txtSize = 908;

    extern const char*   optimizations_txt;
    const int            optimizations_txtSize = 102;

    extern const char*   templates_json;
    const int            templates_jsonSize = 234;

    extern const char*   install_linux_deps_sh;
    const int            install_linux_deps_shSize = 190;

    extern const char*   NeuroCore_Tests_jucer;
    const int            NeuroCore_Tests_jucerSize = 1786;

    extern const char*   AGENTS_md;
    const int            AGENTS_mdSize = 1241;

    extern const char*   CMakeLists_txt;
    const int            CMakeLists_txtSize = 5142;

    extern const char*   NeuroCore_jucer;
    const int            NeuroCore_jucerSize = 16595;

    extern const char*   README_md;
    const int            README_mdSize = 5802;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 14;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}

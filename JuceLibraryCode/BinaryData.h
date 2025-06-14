/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   knob_png;
    const int            knob_pngSize = 381724;

    extern const char*   mockup_png;
    const int            mockup_pngSize = 1267316;

    extern const char*   de_txt;
    const int            de_txtSize = 248;

    extern const char*   en_txt;
    const int            en_txtSize = 225;

    extern const char*   optimizations_txt;
    const int            optimizations_txtSize = 102;

    extern const char*   templates_json;
    const int            templates_jsonSize = 234;

    extern const char*   NeuroCore_Tests_jucer;
    const int            NeuroCore_Tests_jucerSize = 1786;

    extern const char*   README_md;
    const int            README_mdSize = 4278;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 8;

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

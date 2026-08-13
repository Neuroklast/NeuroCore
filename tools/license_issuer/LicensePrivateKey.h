#pragma once

#include <JuceHeader.h>

/** Private RSA key. Compile this into NeuroCoreIssuer only — never the plugin. */
inline juce::RSAKey neurocoreProductPrivateKey()
{
    return juce::RSAKey (
        "59f569dc5c2d2951b7763c56ee3845048ecc46e85ffc5627bec5c929e052640518b1dd3ec471b9ad41980736fead6348ba36d3e02f93dfba3356c917b53cc206fe3359e93d36d50d6b15eb9804e030005f473e0fe7d74dea0ecb88f191a4193353022204796199c8ff397030bd1ef39ec8a213e920f29261f76a531b732a4ef1,"
        "bf2980f443dff7cda5db4038ba3792a9af7216adcbf8371475644b78fcaf148ad479f6256171aa902b630f54dd3072fa8bb4823c651a3b6bad186b5261211c509936951dcc0dd93375b1d416e2f73fad4beec3a4271b30e7b1394e359d7fd54ca04777caf2afa5e742f84b484bfee4ee0f50980aab125578a795e7f5958bd1dd");
}

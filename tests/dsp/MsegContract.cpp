#include "../../src/dsp/MsegDSP.h"
#include <cassert>
#include <cmath>
#include <limits>

int main()
{
    using namespace nk;
    MsegDSP curve;
    const MsegDSP::Point ramp[] {{0., 0.f, 0.f}, {1., 1.f, 0.f}};
    assert (curve.configure (ramp, 2));
    curve.reset (true);
    for (int i=0; i<50; ++i) curve.tick (0.01, true, true, false, -1, 0, 1, true);
    assert (std::abs (curve.value() - 0.5f) < 1.e-5f);
    const MsegDSP::Point shaped[] {{0.,0.f,2.f},{0.5,1.f,0.f},{1.,0.f,0.f}};
    assert (curve.configure (shaped,3));
    assert (std::abs (curve.at (0.25) - 0.0625f) < 1.e-5f);
    curve.reset (true);
    for (int i=0;i<100;++i) curve.tick (0.01,true,false,false,1,0,2,true);
    assert (std::abs (curve.value()-1.f) < 1.e-5f); // sustain
    for (int i=0;i<50;++i) curve.tick (0.01,false,false,false,1,0,2,true);
    assert (std::abs (curve.value()) < 1.e-5f); // remaining release segments
    curve.reset (true);
    for (int i=0;i<175;++i) curve.tick (0.01,true,true,true,-1,0,1,true);
    assert (std::abs (curve.phase()-0.25) < 1.e-8); // bounded loop [0,0.5)
    curve.reset (false);
    curve.tick (0.1,false,false,false,-1,0,2,true);
    assert (!curve.running());
    curve.tick (0.1,true,false,false,-1,0,2,true);
    const auto first=curve.phase();
    curve.tick (0.1,false,false,false,-1,0,2,true);
    curve.tick (0.1,true,false,false,-1,0,2,true);
    assert (std::abs(curve.phase()-first)<1.e-8); // retrigger
    const MsegDSP::Point invalid[] {{0.,0.f,0.f},{0.,1.f,0.f}};
    assert (!curve.configure(invalid,2));
    assert (!curve.configure(ramp,65));
    auto bad=ramp[1];bad.value=std::numeric_limits<float>::quiet_NaN();
    const MsegDSP::Point nonfinite[]{ramp[0],bad};
    assert (!curve.configure(nonfinite,2));
    // Tiny loops and large increments are bounded, with no iteration per wrap.
    curve.reset(true);
    curve.tick(1.e12,true,true,true,-1,0,1,true);
    assert(std::isfinite(curve.value()) && curve.phase()>=0 && curve.phase()<0.5);
}

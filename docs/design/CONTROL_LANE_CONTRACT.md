# Control-lane integration contract

The old graph guard conflates audio Join and modulation A/B inputs, while ModMath, ModMix and SampleHold read raw lanes and lose upstream depth/polarity. Destination port kind owns connection validation, and every lane consumer reads the same shaped-source accessor exactly once. Native GraphOp/AST round-trip and nested Random chains, plus hydrated Circuit A/B port-kind contracts, reproduce the failures before and verify the correction after.

Raw lanes stay internal to each source. A source's published scalar is already shaped. The shared accessor shapes raw lane samples but returns the published scalar unchanged when a lane is absent. This applies equally to a direct parameter destination and a later control processor.

Implicit control defaults must be editable even in older short-form ModMix/MSEG scripts. Inspect resolves the two expanded control blocks from their catalog defaults plus explicit arguments, with MSEG loop end derived from its actual point count. Model contracts and browser control geometry verify that omitted settings appear with the values used by native compilation.

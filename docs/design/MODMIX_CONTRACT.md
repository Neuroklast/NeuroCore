# ModMix control contract

ModMix currently only adds positive weights and lacks offset, limiting and normalization. The model is `clamp(mix * weightedSum / divisor + offset)`, with signed A/B gains, a divisor of one unless normalization is enabled, and only connected inputs contributing absolute weights to that divisor. Native contracts exercise A, B, A+B, cancellation, missing sources, offset, both clamp ranges and AST restore; the Circuit uses these same keys and real published control values.

Defaults preserve existing preset output. Normalize defaults off; clamp defaults off. `polarity` and `depth` remain the common control-source stage after the mix. Normalize with zero total weight yields zero before offset. Offset follows master mix, allowing a DC control bias even with mix zero.

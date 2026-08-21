#pragma once

#include "ExprTape.h"

/** Native function emitted at parse/load. Audio thread only calls `fn`. */
struct ExprJitCode
{
    using Fn = float (*) (ExprTape*, const float*);
    Fn fn { nullptr };
};

void exprTapeJitRelease (ExprJitCode& code) noexcept;
bool exprTapeJitCompile (ExprJitCode& code, const ExprTape& tape) noexcept;

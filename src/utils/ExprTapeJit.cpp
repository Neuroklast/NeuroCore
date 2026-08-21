#include "ExprTapeJit.h"

#if defined(NK_HAS_EXPR_JIT) && NK_HAS_EXPR_JIT
#include <asmjit/x86.h>
#include <mutex>

using namespace asmjit;

namespace
{
JitRuntime& jitRuntime() noexcept
{
    static JitRuntime rt;
    return rt;
}

std::mutex& jitLock() noexcept
{
    static std::mutex m;
    return m;
}
} // namespace

void exprTapeJitRelease (ExprJitCode& code) noexcept
{
    if (code.fn == nullptr)
        return;
    std::lock_guard<std::mutex> g (jitLock());
    jitRuntime().release (code.fn);
    code.fn = nullptr;
}

bool exprTapeJitCompile (ExprJitCode& code, const ExprTape& tape) noexcept
{
    exprTapeJitRelease (code);
    if (tape.n <= 0 || tape.n > ExprTape::kMaxOps)
        return false;

    for (int i = 0; i < tape.n; ++i)
    {
        switch (tape.op[i])
        {
            case ExprOp::LoadImm:
            case ExprOp::LoadVar:
            case ExprOp::Add:
            case ExprOp::Sub:
            case ExprOp::Mul:
            case ExprOp::Neg:
            case ExprOp::End:
                break;
            default:
                return false;
        }
    }

    ExprJitCode::Fn added = nullptr;
    try
    {
        std::lock_guard<std::mutex> g (jitLock());
        CodeHolder holder;
        if (holder.init (jitRuntime().environment(), jitRuntime().cpu_features()) != Error::kOk)
            return false;

        x86::Compiler cc (&holder);
        FuncNode* func = cc.add_func (FuncSignature::build<float, ExprTape*, const float*>());
        x86::Gp pTape = cc.new_gp_ptr();
        x86::Gp pVars = cc.new_gp_ptr();
        func->set_arg (0, pTape);
        func->set_arg (1, pVars);
        (void) pTape;

        x86::Vec s[ExprTape::kMaxSlots];
        for (int i = 0; i < ExprTape::kMaxSlots; ++i)
            s[i] = cc.new_xmm_ss();

        auto slotOk = [] (uint8_t v) noexcept
        {
            return v < (uint8_t) ExprTape::kMaxSlots;
        };

        bool ok = true;
        for (int i = 0; i < tape.n && ok; ++i)
        {
            const uint8_t ds = tape.dst[i];
            if (! slotOk (ds))
                continue;
            const uint8_t a = tape.a[i];
            const uint8_t b = tape.b[i];
            switch (tape.op[i])
            {
                case ExprOp::LoadImm:
                    cc.movss (s[ds], cc.new_float_const (ConstPoolScope::kLocal, tape.imm[i]));
                    break;
                case ExprOp::LoadVar:
                    if (a >= (uint8_t) ExprTape::kMaxVars)
                        cc.xorps (s[ds], s[ds]);
                    else
                        cc.movss (s[ds], x86::dword_ptr (pVars, (int) a * (int) sizeof (float)));
                    break;
                case ExprOp::Add:
                    if (! slotOk (a) || ! slotOk (b)) { ok = false; break; }
                    cc.movss (s[ds], s[a]);
                    cc.addss (s[ds], s[b]);
                    break;
                case ExprOp::Sub:
                    if (! slotOk (a) || ! slotOk (b)) { ok = false; break; }
                    cc.movss (s[ds], s[a]);
                    cc.subss (s[ds], s[b]);
                    break;
                case ExprOp::Mul:
                    if (! slotOk (a) || ! slotOk (b)) { ok = false; break; }
                    cc.movss (s[ds], s[a]);
                    cc.mulss (s[ds], s[b]);
                    break;
                case ExprOp::Neg:
                    if (! slotOk (a)) { ok = false; break; }
                    cc.movss (s[ds], s[a]);
                    cc.mulss (s[ds], cc.new_float_const (ConstPoolScope::kLocal, -1.0f));
                    break;
                case ExprOp::End:
                    i = tape.n;
                    break;
                default:
                    ok = false;
                    break;
            }
        }

        if (! ok)
            return false;

        const uint8_t rs = tape.resultSlot;
        if (rs >= (uint8_t) ExprTape::kMaxSlots)
            return false;
        cc.ret (s[rs]);
        cc.end_func();

        if (cc.finalize() != Error::kOk)
            return false;

        if (jitRuntime().add (&added, &holder) != Error::kOk || added == nullptr)
            return false;
        code.fn = added;
        return true;
    }
    catch (...)
    {
        if (added != nullptr)
        {
            std::lock_guard<std::mutex> g (jitLock());
            jitRuntime().release (added);
        }
        code.fn = nullptr;
        return false;
    }
}

#else

void exprTapeJitRelease (ExprJitCode& code) noexcept
{
    code.fn = nullptr;
}

bool exprTapeJitCompile (ExprJitCode&, const ExprTape&) noexcept
{
    return false;
}

#endif

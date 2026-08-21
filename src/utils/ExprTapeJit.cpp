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

        x86::Vec s[ExprTape::kMaxSlots];
        for (int i = 0; i < ExprTape::kMaxSlots; ++i)
            s[i] = cc.new_xmm_ss();

        auto slotOk = [] (uint8_t v) noexcept
        {
            return v < (uint8_t) ExprTape::kMaxSlots;
        };

        auto callF2 = [&] (float (*fn)(float, float), uint8_t da, uint8_t db, uint8_t ds) -> bool
        {
            if (! slotOk (da) || ! slotOk (db) || fn == nullptr)
                return false;
            InvokeNode* inv = nullptr;
            if (cc.invoke (Out (inv), (uint64_t) (uintptr_t) fn,
                           FuncSignature::build<float, float, float>()) != Error::kOk
                || inv == nullptr)
                return false;
            inv->set_arg (0, s[da]);
            inv->set_arg (1, s[db]);
            inv->set_ret (0, s[ds]);
            return true;
        };

        auto callU32F = [&] (float (*fn)(uint32_t, float), uint32_t id, uint8_t da, uint8_t ds) -> bool
        {
            if (! slotOk (da) || fn == nullptr)
                return false;
            x86::Gp gid = cc.new_gp32();
            cc.mov (gid, id);
            InvokeNode* inv = nullptr;
            if (cc.invoke (Out (inv), (uint64_t) (uintptr_t) fn,
                           FuncSignature::build<float, uint32_t, float>()) != Error::kOk
                || inv == nullptr)
                return false;
            inv->set_arg (0, gid);
            inv->set_arg (1, s[da]);
            inv->set_ret (0, s[ds]);
            return true;
        };

        auto callU32FF = [&] (float (*fn)(uint32_t, float, float), uint32_t id,
                              uint8_t da, uint8_t db, uint8_t ds) -> bool
        {
            if (! slotOk (da) || ! slotOk (db) || fn == nullptr)
                return false;
            x86::Gp gid = cc.new_gp32();
            cc.mov (gid, id);
            InvokeNode* inv = nullptr;
            if (cc.invoke (Out (inv), (uint64_t) (uintptr_t) fn,
                           FuncSignature::build<float, uint32_t, float, float>()) != Error::kOk
                || inv == nullptr)
                return false;
            inv->set_arg (0, gid);
            inv->set_arg (1, s[da]);
            inv->set_arg (2, s[db]);
            inv->set_ret (0, s[ds]);
            return true;
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
                case ExprOp::Div:
                    ok = callF2 (exprTapeDiv, a, b, ds);
                    break;
                case ExprOp::Pow:
                    ok = callF2 (exprTapePow, a, b, ds);
                    break;
                case ExprOp::Neg:
                    if (! slotOk (a)) { ok = false; break; }
                    cc.movss (s[ds], s[a]);
                    cc.mulss (s[ds], cc.new_float_const (ConstPoolScope::kLocal, -1.0f));
                    break;
                case ExprOp::Call1:
                    ok = callU32F (exprTapeCall1, tape.fn[i], a, ds);
                    break;
                case ExprOp::Call2:
                    ok = callU32FF (exprTapeCall2, tape.fn[i], a, b, ds);
                    break;
                case ExprOp::Call3:
                {
                    if (! slotOk (a) || ! slotOk (b) || ! slotOk (tape.c[i]))
                    { ok = false; break; }
                    x86::Gp gid = cc.new_gp32();
                    cc.mov (gid, (uint32_t) tape.fn[i]);
                    InvokeNode* inv = nullptr;
                    if (cc.invoke (Out (inv), (uint64_t) (uintptr_t) exprTapeCall3,
                                   FuncSignature::build<float, uint32_t, float, float, float>()) != Error::kOk
                        || inv == nullptr)
                    { ok = false; break; }
                    inv->set_arg (0, gid);
                    inv->set_arg (1, s[a]);
                    inv->set_arg (2, s[b]);
                    inv->set_arg (3, s[tape.c[i]]);
                    inv->set_ret (0, s[ds]);
                    break;
                }
                case ExprOp::Call5:
                {
                    if (! slotOk (a) || ! slotOk (b) || ! slotOk (tape.c[i])
                        || ! slotOk (tape.d[i]) || ! slotOk (tape.e[i]))
                    { ok = false; break; }
                    InvokeNode* inv = nullptr;
                    if (cc.invoke (Out (inv), (uint64_t) (uintptr_t) exprTapeCall5,
                                   FuncSignature::build<float, float, float, float, float, float>()) != Error::kOk
                        || inv == nullptr)
                    { ok = false; break; }
                    inv->set_arg (0, s[a]);
                    inv->set_arg (1, s[b]);
                    inv->set_arg (2, s[tape.c[i]]);
                    inv->set_arg (3, s[tape.d[i]]);
                    inv->set_arg (4, s[tape.e[i]]);
                    inv->set_ret (0, s[ds]);
                    break;
                }
                case ExprOp::Adaa2:
                {
                    if (! slotOk (a) || ! slotOk (b))
                    { ok = false; break; }
                    x86::Gp gid = cc.new_gp32();
                    x86::Gp gst = cc.new_gp32();
                    cc.mov (gid, (uint32_t) tape.fn[i]);
                    cc.mov (gst, (uint32_t) tape.d[i]);
                    InvokeNode* inv = nullptr;
                    if (cc.invoke (Out (inv), (uint64_t) (uintptr_t) exprTapeCallAdaa,
                                   FuncSignature::build<float, ExprTape*, uint32_t, float, float, uint32_t>()) != Error::kOk
                        || inv == nullptr)
                    { ok = false; break; }
                    inv->set_arg (0, pTape);
                    inv->set_arg (1, gid);
                    inv->set_arg (2, s[a]);
                    inv->set_arg (3, s[b]);
                    inv->set_arg (4, gst);
                    inv->set_ret (0, s[ds]);
                    break;
                }
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

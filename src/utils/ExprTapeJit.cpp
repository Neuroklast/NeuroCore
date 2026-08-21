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

x86::Mem fconst (x86::Compiler& cc, float v)
{
    return cc.new_float_const (ConstPoolScope::kLocal, v);
}

bool emitInvoke (x86::Compiler& cc, InvokeNode*& inv, uint64_t target, const FuncSignature& sig)
{
    inv = nullptr;
    const Error e = cc.invoke (Out (inv), imm (target), sig);
    return e == Error::kOk && inv != nullptr;
}

bool emitCall1 (x86::Compiler& cc, x86::Vec dst, uint32_t fn, const x86::Vec& x)
{
    x86::Gp fnr = cc.new_gp32();
    cc.mov (fnr, fn);
    x86::Vec tmp = cc.new_xmm_ss();
    InvokeNode* inv = nullptr;
    if (! emitInvoke (cc, inv, (uint64_t) (void*) &exprTapeCall1,
                      FuncSignature::build<float, uint32_t, float>()))
        return false;
    inv->set_arg (0, fnr);
    inv->set_arg (1, x);
    inv->set_ret (0, tmp);
    cc.movss (dst, tmp);
    return true;
}

bool emitCall2 (x86::Compiler& cc, x86::Vec dst, uint32_t fn, const x86::Vec& a, const x86::Vec& b)
{
    x86::Gp fnr = cc.new_gp32();
    cc.mov (fnr, fn);
    x86::Vec tmp = cc.new_xmm_ss();
    InvokeNode* inv = nullptr;
    if (! emitInvoke (cc, inv, (uint64_t) (void*) &exprTapeCall2,
                      FuncSignature::build<float, uint32_t, float, float>()))
        return false;
    inv->set_arg (0, fnr);
    inv->set_arg (1, a);
    inv->set_arg (2, b);
    inv->set_ret (0, tmp);
    cc.movss (dst, tmp);
    return true;
}

bool emitCall3 (x86::Compiler& cc, x86::Vec dst, uint32_t fn,
                const x86::Vec& a, const x86::Vec& b, const x86::Vec& c)
{
    x86::Gp fnr = cc.new_gp32();
    cc.mov (fnr, fn);
    x86::Vec tmp = cc.new_xmm_ss();
    InvokeNode* inv = nullptr;
    if (! emitInvoke (cc, inv, (uint64_t) (void*) &exprTapeCall3,
                      FuncSignature::build<float, uint32_t, float, float, float>()))
        return false;
    inv->set_arg (0, fnr);
    inv->set_arg (1, a);
    inv->set_arg (2, b);
    inv->set_arg (3, c);
    inv->set_ret (0, tmp);
    cc.movss (dst, tmp);
    return true;
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

    // Native path is arithmetic + loads. Call/ADAA/div/pow stay on the tape
    // interpreter — invoke ABI is the crash surface on factory formulas.
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

    try
    {
    std::lock_guard<std::mutex> g (jitLock());
    CodeHolder holder;
    const Error initErr = holder.init (jitRuntime().environment(), jitRuntime().cpu_features());
    if (initErr != Error::kOk)
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

    bool ok = true;
    for (int i = 0; i < tape.n && ok; ++i)
    {
        const uint8_t ds = tape.dst[i];
        if (ds >= ExprTape::kMaxSlots)
            continue;
        const uint8_t a = tape.a[i];
        const uint8_t b = tape.b[i];
        auto slotOk = [] (uint8_t v) noexcept { return v < (uint8_t) ExprTape::kMaxSlots; };
        switch (tape.op[i])
        {
            case ExprOp::LoadImm:
                cc.movss (s[ds], fconst (cc, tape.imm[i]));
                break;
            case ExprOp::LoadVar:
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
            {
                InvokeNode* inv = nullptr;
                x86::Vec tmp = cc.new_xmm_ss();
                if (! emitInvoke (cc, inv, (uint64_t) (void*) &exprTapeDiv,
                                  FuncSignature::build<float, float, float>()))
                { ok = false; break; }
                inv->set_arg (0, s[a]);
                inv->set_arg (1, s[b]);
                inv->set_ret (0, tmp);
                cc.movss (s[ds], tmp);
                break;
            }
            case ExprOp::Pow:
            {
                InvokeNode* inv = nullptr;
                x86::Vec tmp = cc.new_xmm_ss();
                if (! emitInvoke (cc, inv, (uint64_t) (void*) &exprTapePow,
                                  FuncSignature::build<float, float, float>()))
                { ok = false; break; }
                inv->set_arg (0, s[a]);
                inv->set_arg (1, s[b]);
                inv->set_ret (0, tmp);
                cc.movss (s[ds], tmp);
                break;
            }
            case ExprOp::Neg:
                if (! slotOk (a)) { ok = false; break; }
                cc.movss (s[ds], s[a]);
                cc.xorps (s[ds], fconst (cc, -0.0f));
                break;
            case ExprOp::Call1:
                if (! emitCall1 (cc, s[ds], tape.fn[i], s[a]))
                    ok = false;
                break;
            case ExprOp::Call2:
                if (! emitCall2 (cc, s[ds], tape.fn[i], s[a], s[b]))
                    ok = false;
                break;
            case ExprOp::Call3:
                if (! emitCall3 (cc, s[ds], tape.fn[i], s[a], s[b], s[tape.c[i]]))
                    ok = false;
                break;
            case ExprOp::Call5:
            {
                InvokeNode* inv = nullptr;
                x86::Vec tmp = cc.new_xmm_ss();
                if (! emitInvoke (cc, inv, (uint64_t) (void*) &exprTapeCall5,
                                  FuncSignature::build<float, float, float, float, float, float>()))
                { ok = false; break; }
                inv->set_arg (0, s[a]);
                inv->set_arg (1, s[b]);
                inv->set_arg (2, s[tape.c[i]]);
                inv->set_arg (3, s[tape.d[i]]);
                inv->set_arg (4, s[tape.e[i]]);
                inv->set_ret (0, tmp);
                cc.movss (s[ds], tmp);
                break;
            }
            case ExprOp::Adaa2:
            {
                InvokeNode* inv = nullptr;
                x86::Vec tmp = cc.new_xmm_ss();
                x86::Gp fnr = cc.new_gp32();
                x86::Gp str = cc.new_gp32();
                cc.mov (fnr, (uint32_t) tape.fn[i]);
                cc.mov (str, (uint32_t) tape.d[i]);
                if (! emitInvoke (cc, inv, (uint64_t) (void*) &exprTapeCallAdaa,
                                  FuncSignature::build<float, ExprTape*, uint32_t, float, float, uint32_t>()))
                { ok = false; break; }
                inv->set_arg (0, pTape);
                inv->set_arg (1, fnr);
                inv->set_arg (2, s[a]);
                inv->set_arg (3, s[b]);
                inv->set_arg (4, str);
                inv->set_ret (0, tmp);
                cc.movss (s[ds], tmp);
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
    x86::Vec y = (rs < ExprTape::kMaxSlots) ? s[rs] : cc.new_xmm_ss();
    if (rs >= ExprTape::kMaxSlots)
        cc.xorps (y, y);

    InvokeNode* fin = nullptr;
    x86::Vec ret = cc.new_xmm_ss();
    if (! emitInvoke (cc, fin, (uint64_t) (void*) &exprTapeFinite,
                      FuncSignature::build<float, float>()))
        return false;
    fin->set_arg (0, y);
    fin->set_ret (0, ret);
    cc.ret (ret);
    cc.end_func();

    if (cc.finalize() != Error::kOk)
        return false;

    ExprJitCode::Fn fn = nullptr;
    if (jitRuntime().add (&fn, &holder) != Error::kOk || fn == nullptr)
        return false;
    code.fn = fn;
    return true;
    }
    catch (...)
    {
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

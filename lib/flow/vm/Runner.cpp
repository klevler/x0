#include <x0/flow/vm/Runner.h>
#include <x0/flow/vm/Params.h>
#include <x0/flow/vm/NativeCallback.h>
#include <x0/flow/vm/Handler.h>
#include <x0/flow/vm/Program.h>
#include <x0/flow/vm/Match.h>
#include <x0/flow/vm/Instruction.h>
#include <vector>
#include <utility>
#include <memory>
#include <new>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <x0/sysconfig.h>

namespace x0 {
namespace FlowVM {

std::unique_ptr<Runner> Runner::create(Handler* handler)
{
    Runner* p = (Runner*) malloc(sizeof(Runner) + handler->registerCount() * sizeof(uint64_t));
    new (p) Runner(handler);
    return std::unique_ptr<Runner>(p);
}

static FlowString* t = nullptr;

Runner::Runner(Handler* handler) :
    handler_(handler),
    program_(handler->program()),
    userdata_(nullptr),
    stringGarbage_()
{
    // initialize emptyString()
    t = newString("");

    // initialize registers
    memset(data_, 0, sizeof(Register) * handler_->registerCount());
}

Runner::~Runner()
{
}

void Runner::operator delete (void* p)
{
    free(p);
}

FlowString* Runner::newString(const std::string& value)
{
    stringGarbage_.push_back(Buffer(value.c_str(), value.size()));
    return &stringGarbage_.back();
}

FlowString* Runner::newString(const char* p, size_t n)
{
    stringGarbage_.push_back(Buffer(p, n));
    return &stringGarbage_.back();
}

FlowString* Runner::catString(const FlowString& a, const FlowString& b)
{
    Buffer s(a.size() + b.size() + 1);
    s.push_back(a);
    s.push_back(b);

    stringGarbage_.push_back(std::move(s));

    return &stringGarbage_.back();
}

bool Runner::run()
{
    const Program* program = handler_->program();
    uint64_t ticks = 0;

    #define OP opcode((Instruction) *pc)
    #define A  operandA((Instruction) *pc)
    #define B  operandB((Instruction) *pc)
    #define C  operandC((Instruction) *pc)

    #define toString(R)     (*(FlowString*) data_[R])
    #define toIPAddress(R)  (*(IPAddress*) data_[R])
    #define toCidr(R)       (*(Cidr*) data_[R])
    #define toRegExp(R)     (*(RegExp*) data_[R])
    #define toNumber(R)     ((FlowNumber) data_[R])

    #define toStringPtr(R)  ((FlowString*) data_[R])
    #define toCidrPtr(R)    ((Cidr*) data_[R])

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
    auto& code = handler_->directThreadedCode();
    const size_t instructionSize = 2;

    #define instr(name) \
        l_##name: \
        ++pc; \
        ++ticks;

    #define vm_start goto **pc
    #define next goto **++pc
#else
    const auto& code = handler_->code();
    const size_t instructionSize = 1;

    #define instr(name) \
        l_##name: \
        ++ticks;

    #define vm_start goto *ops[OP]
    #define next goto *ops[opcode(*++pc)]
#endif

    // {{{ jump table
    #define label(opcode) && l_##opcode
    static const void* const ops[] = {
        // misc
        label(NOP),

        // control
        label(EXIT),
        label(JMP),
        label(JN),
        label(JZ),

        // debug
        label(NTICKS),
        label(NDUMPN),

        // copy
        label(MOV),

        // array
        label(ITCONST),
        label(STCONST),
        label(PTCONST),
        label(CTCONST),

        // numerical
        label(IMOV),
        label(NCONST),
        label(NNEG),
        label(NADD),
        label(NSUB),
        label(NMUL),
        label(NDIV),
        label(NREM),
        label(NSHL),
        label(NSHR),
        label(NPOW),
        label(NAND),
        label(NOR),
        label(NXOR),
        label(NCMPZ),
        label(NCMPEQ),
        label(NCMPNE),
        label(NCMPLE),
        label(NCMPGE),
        label(NCMPLT),
        label(NCMPGT),

        // numerical (reg, imm)
        label(NIADD),
        label(NISUB),
        label(NIMUL),
        label(NIDIV),
        label(NIREM),
        label(NISHL),
        label(NISHR),
        label(NIPOW),
        label(NIAND),
        label(NIOR),
        label(NIXOR),
        label(NICMPEQ),
        label(NICMPNE),
        label(NICMPLE),
        label(NICMPGE),
        label(NICMPLT),
        label(NICMPGT),

        // boolean op
        label(BNOT),
        label(BAND),
        label(BOR),
        label(BXOR),

        // string op
        label(SCONST),
        label(SADD),
        label(SADDMULTI),
        label(SSUBSTR),
        label(SCMPEQ),
        label(SCMPNE),
        label(SCMPLE),
        label(SCMPGE),
        label(SCMPLT),
        label(SCMPGT),
        label(SCMPBEG),
        label(SCMPEND),
        label(SCONTAINS),
        label(SLEN),
        label(SISEMPTY),
        label(SPRINT),
        label(SMATCHEQ),
        label(SMATCHBEG),
        label(SMATCHEND),
        label(SMATCHR),

        // ipaddr
        label(PCONST),
        label(PCMPEQ),
        label(PCMPNE),
        label(PINCIDR),

        // cidr
        label(CCONST),

        // regex
        label(SREGMATCH),
        label(SREGGROUP),

        // conversion
        label(I2S),
        label(P2S),
        label(C2S),
        label(R2S),
        label(S2I),
        label(SURLENC),
        label(SURLDEC),

        // invokation
        label(CALL),
        label(HANDLER),
    };
    // }}}
    // {{{ direct threaded code initialization
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
    if (code.empty()) {
        const auto& source = handler_->code();
        code.resize(source.size() * instructionSize);

        const void** pc = code.data();
        for (size_t i = 0, e = source.size(); i != e; ++i) {
            Instruction instr = source[i];

            *pc++ = ops[opcode(instr)];
            *pc++ = (void*) instr;
        }
    }
    //const void** pc = code.data();
#endif
    // }}}

    const auto* pc = code.data();

    vm_start;

    // {{{ misc
    instr (NOP) {
        next;
    }
    // }}}
    // {{{ control
    instr (EXIT) {
        return A != 0;
    }

    instr (JMP) {
        pc = code.data() + A * instructionSize;
        goto *ops[OP];
    }

    instr (JN) {
        if (data_[A] != 0) {
            pc = code.data() + B * instructionSize;
            goto *ops[OP];
        } else {
            next;
        }
    }

    instr (JZ) {
        if (data_[A] == 0) {
            pc = code.data() + B * instructionSize;
            goto *ops[OP];
        } else {
            next;
        }
    }
    // }}}
    // {{{ copy
    instr (MOV) {
        data_[A] = data_[B];
        next;
    }
    // }}}
    // {{{ array
    instr (ITCONST) {
        data_[A] = reinterpret_cast<Register>(&program->constants().getIntArray(B));
        next;
    }
    instr (STCONST) {
        data_[A] = reinterpret_cast<Register>(&program->constants().getStringArray(B));
        next;
    }
    instr (PTCONST) {
        data_[A] = reinterpret_cast<Register>(&program->constants().getIPAddressArray(B));
        next;
    }
    instr (CTCONST) {
        data_[A] = reinterpret_cast<Register>(&program->constants().getCidrArray(B));
        next;
    }
    // }}}
    // {{{ debug
    instr (NTICKS) {
        data_[A] = ticks;
        next;
    }

    instr (NDUMPN) {
        printf("regdump: ");
        for (int i = 0; i < B; ++i) {
            if (i) printf(", ");
            printf("r%d = %li", A + i, (int64_t)data_[A + i]);
        }
        if (B) printf("\n");
        next;
    }
    // }}}
    // {{{ numerical
    instr (IMOV) {
        data_[A] = B;
        next;
    }

    instr (NCONST) {
        data_[A] = program->constants().getInteger(B);
        next;
    }

    instr (NNEG) {
        data_[A] = (Register) (-toNumber(B));
        next;
    }

    instr (NADD) {
        data_[A] = static_cast<Register>(toNumber(B) + toNumber(C));
        next;
    }

    instr (NSUB) {
        data_[A] = static_cast<Register>(toNumber(B) - toNumber(C));
        next;
    }

    instr (NMUL) {
        data_[A] = static_cast<Register>(toNumber(B) * toNumber(C));
        next;
    }

    instr (NDIV) {
        data_[A] = static_cast<Register>(toNumber(B) / toNumber(C));
        next;
    }

    instr (NREM) {
        data_[A] = static_cast<Register>(toNumber(B) % toNumber(C));
        next;
    }

    instr (NSHL) {
        data_[A] = static_cast<Register>(toNumber(B) << toNumber(C));
        next;
    }

    instr (NSHR) {
        data_[A] = static_cast<Register>(toNumber(B) >> toNumber(C));
        next;
    }

    instr (NPOW) {
        data_[A] = static_cast<Register>(powl(toNumber(B), toNumber(C)));
        next;
    }

    instr (NAND) {
        data_[A] = data_[B] & data_[C];
        next;
    }

    instr (NOR) {
        data_[A] = data_[B] | data_[C];
        next;
    }

    instr (NXOR) {
        data_[A] = data_[B] ^ data_[C];
        next;
    }

    instr (NCMPZ) {
        data_[A] = static_cast<Register>(toNumber(B) == 0);
        next;
    }

    instr (NCMPEQ) {
        data_[A] = static_cast<Register>(toNumber(B) == toNumber(C));
        next;
    }

    instr (NCMPNE) {
        data_[A] = static_cast<Register>(toNumber(B) != toNumber(C));
        next;
    }

    instr (NCMPLE) {
        data_[A] = static_cast<Register>(toNumber(B) <= toNumber(C));
        next;
    }

    instr (NCMPGE) {
        data_[A] = static_cast<Register>(toNumber(B) >= toNumber(C));
        next;
    }

    instr (NCMPLT) {
        data_[A] = static_cast<Register>(toNumber(B) < toNumber(C));
        next;
    }

    instr (NCMPGT) {
        data_[A] = static_cast<Register>(toNumber(B) > toNumber(C));
        next;
    }
    // }}}
    // {{{ numerical binary (reg, imm)
    instr (NIADD) {
        data_[A] = static_cast<Register>(toNumber(B) + C);
        next;
    }

    instr (NISUB) {
        data_[A] = static_cast<Register>(toNumber(B) - C);
        next;
    }

    instr (NIMUL) {
        data_[A] = static_cast<Register>(toNumber(B) * C);
        next;
    }

    instr (NIDIV) {
        data_[A] = static_cast<Register>(toNumber(B) / C);
        next;
    }

    instr (NIREM) {
        data_[A] = static_cast<Register>(toNumber(B) % C);
        next;
    }

    instr (NISHL) {
        data_[A] = static_cast<Register>(toNumber(B) << C);
        next;
    }

    instr (NISHR) {
        data_[A] = static_cast<Register>(toNumber(B) >> C);
        next;
    }

    instr (NIPOW) {
        data_[A] = static_cast<Register>(powl(toNumber(B), C));
        next;
    }

    instr (NIAND) {
        data_[A] = data_[B] & C;
        next;
    }

    instr (NIOR) {
        data_[A] = data_[B] | C;
        next;
    }

    instr (NIXOR) {
        data_[A] = data_[B] ^ C;
        next;
    }

    instr (NICMPEQ) {
        data_[A] = static_cast<Register>(toNumber(B) == C);
        next;
    }

    instr (NICMPNE) {
        data_[A] = static_cast<Register>(toNumber(B) != C);
        next;
    }

    instr (NICMPLE) {
        data_[A] = static_cast<Register>(toNumber(B) <= C);
        next;
    }

    instr (NICMPGE) {
        data_[A] = static_cast<Register>(toNumber(B) >= C);
        next;
    }

    instr (NICMPLT) {
        data_[A] = static_cast<Register>(toNumber(B) < C);
        next;
    }

    instr (NICMPGT) {
        data_[A] = static_cast<Register>(toNumber(B) > C);
        next;
    }
    // }}}
    // {{{ boolean
    instr (BNOT) {
        data_[A] = (Register) (!toNumber(B));
        next;
    }

    instr (BAND) {
        data_[A] = toNumber(B) && toNumber(C);
        next;
    }

    instr (BOR) {
        data_[A] = toNumber(B) || toNumber(C);
        next;
    }

    instr (BXOR) {
        data_[A] = toNumber(B) ^ toNumber(C);
        next;
    }
    // }}}
    // {{{ string
    instr (SCONST) { // A = stringConstTable[B]
        data_[A] = reinterpret_cast<Register>(&program->constants().getString(B));
        next;
    }

    instr (SADD) { // A = concat(B, C)
        data_[A] = (Register) catString(toString(B), toString(C));
        next;
    }

    instr (SSUBSTR) { // A = substr(B, C /*offset*/, C+1 /*count*/)
        data_[A] = (Register) newString(toString(B).substr(data_[C], data_[C + 1]));
        next;
    }

    instr (SADDMULTI) { // TODO: A = concat(B /*rbase*/, C /*count*/)
        next;
    }

    instr (SCMPEQ) {
        data_[A] = toString(B) == toString(C);
        next;
    }

    instr (SCMPNE) {
        data_[A] = toString(B) != toString(C);
        next;
    }

    instr (SCMPLE) {
        data_[A] = toString(B) <= toString(C);
        next;
    }

    instr (SCMPGE) {
        data_[A] = toString(B) >= toString(C);
        next;
    }

    instr (SCMPLT) {
        data_[A] = toString(B) < toString(C);
        next;
    }

    instr (SCMPGT) {
        data_[A] = toString(B) > toString(C);
        next;
    }

    instr (SCMPBEG) {
        const auto& b = toString(B);
        const auto& c = toString(C);
        data_[A] = b.begins(c);
        next;
    }

    instr (SCMPEND) {
        const auto& b = toString(B);
        const auto& c = toString(C);
        data_[A] = b.ends(c);
        next;
    }

    instr (SCONTAINS) {
        data_[A] = toString(B).find(toString(C)) != FlowString::npos;
        next;
    }

    instr (SLEN) {
        data_[A] = toString(B).size();
        next;
    }

    instr (SISEMPTY) {
        data_[A] = toString(B).empty();
        next;
    }

    instr (SPRINT) {
        printf("%s\n", toString(A).str().c_str());
        next;
    }

    instr (SMATCHEQ) {
        auto result = program_->match(B)->evaluate(toStringPtr(A), this);
        pc = code.data() + result * instructionSize;
        goto *ops[OP];
    }

    instr (SMATCHBEG) {
        auto result = program_->match(B)->evaluate(toStringPtr(A), this);
        pc = code.data() + result * instructionSize;
        goto *ops[OP];
    }

    instr (SMATCHEND) {
        auto result = program_->match(B)->evaluate(toStringPtr(A), this);
        pc = code.data() + result * instructionSize;
        goto *ops[OP];
    }

    instr (SMATCHR) {
        auto result = program_->match(B)->evaluate(toStringPtr(A), this);
        pc = code.data() + result * instructionSize;
        goto *ops[OP];
    }
    // }}}
    // {{{ ipaddr
    instr (PCONST) {
        data_[A] = reinterpret_cast<Register>(&program->constants().getIPAddress(B));
        next;
    }

    instr (PCMPEQ) {
        data_[A] = toIPAddress(B) == toIPAddress(C);
        next;
    }

    instr (PCMPNE) {
        data_[A] = toIPAddress(B) != toIPAddress(C);
        next;
    }

    instr (PINCIDR) {
        const IPAddress& ipaddr = toIPAddress(B);
        const Cidr& cidr = toCidr(C);
        data_[A] = cidr.contains(ipaddr);
        next;
    }
    // }}}
    // {{{ cidr
    instr (CCONST) {
        data_[A] = reinterpret_cast<Register>(&program->constants().getCidr(B));
        next;
    }
    // }}}
    // {{{ regex
    instr (SREGMATCH) { // A = B =~ C
        RegExpContext* cx = (RegExpContext*) userdata();
        data_[A] = program_->constants().getRegExp(C).match(toString(B), cx ? cx->regexMatch() : nullptr);

        next;
    }

    instr (SREGGROUP) { // A = regex.group(B)
        FlowNumber position = toNumber(B);
        RegExpContext* cx = (RegExpContext*) userdata();
        RegExp::Result* rr = cx->regexMatch();
        const auto& match = rr->at(position);

        data_[A] = (Register) newString(match.first, match.second);

        next;
    }
    // }}}
    // {{{ conversion
    instr (S2I) { // A = atoi(B)
        data_[A] = toString(B).toInt();
        next;
    }

    instr (I2S) { // A = itoa(B)
        char buf[64];
        if (snprintf(buf, sizeof(buf), "%li", (int64_t) data_[B]) > 0) {
            data_[A] = (Register) newString(buf);
        } else {
            data_[A] = (Register) emptyString();
        }
        next;
    }

    instr (P2S) { // A = ip(B).toString()
        const IPAddress& ipaddr = toIPAddress(B);
        data_[A] = (Register) newString(ipaddr.str());
        next;
    }

    instr (C2S) { // A = cidr(B).toString()
        const Cidr& cidr = toCidr(B);
        data_[A] = (Register) newString(cidr.str());
        next;
    }

    instr (R2S) { // A = regex(B).toString()
        const RegExp& re = toRegExp(B);
        data_[A] = (Register) newString(re.pattern());
        next;
    }

    instr (SURLENC) { // A = urlencode(B)
        // TODO
        next;
    }

    instr (SURLDEC) { // B = urldecode(B)
        // TODO
        next;
    }
    // }}}
    // {{{ invokation
    instr (CALL) { // IIR
        size_t id = A;
        int argc = B;
        Register* argv = &data_[C];

        Params args(argc, argv, this);
        handler_->program()->nativeFunction(id)->invoke(args);

        next;
    }

    instr (HANDLER) { // IIR
        size_t id = A;
        int argc = B;
        Value* argv = &data_[C];

        Params args(argc, argv, this);
        handler_->program()->nativeHandler(id)->invoke(args);
        const bool handled = (bool) argv[0];

        if (handled) {
            return true;
        }

        next;
    }
    // }}}
}

} // namespace FlowVM
} // namespace x0

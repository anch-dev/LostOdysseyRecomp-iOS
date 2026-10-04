#include "lo_semantics/legacy_utf16_line_token_number.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_utf16_line_token_number;
constexpr GuestAddress Text = 0x20000u, Cursor = 0x30000u;
constexpr GuestAddress Output = 0x40000u, Name = 0x50000u;
constexpr GuestAddress Stack = 0x80000u, Classes = 0x60000u;
constexpr std::uint32_t Sentinel = 0xaabbccddu;
constexpr std::array<test::Region, 2> Regions{{{0u, 0x90000u},
    {0x83214000u, 0x2000u}}};

enum class Kind {Empty, Plain, QuotedComment, Limit, Miss, Number,
    Negative, SignedWhite};
constexpr std::array<Kind, 8> Cases{{Kind::Empty, Kind::Plain,
    Kind::QuotedComment, Kind::Limit, Kind::Miss, Kind::Number,
    Kind::Negative, Kind::SignedWhite}};

bool IsLine(Kind kind)
{ return kind <= Kind::Limit; }

struct Services final : CrtThreadDataServices, InvalidParameterServices
{
    std::uint64_t GetTlsValue(std::uint32_t) override
    { throw std::runtime_error("unexpected TLS lookup"); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { throw std::runtime_error("unexpected TLS write"); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected thread data getter"); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected thread data allocation"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected thread data binding"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unexpected thread data free"); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid parameter handler"); }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid parameter trap"); }
};

std::array<PPCRegister*, 32> Fields(PPCContext& context)
{
    return {&context.r0, &context.r1, &context.r2, &context.r3,
        &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11,
        &context.r12, &context.r13, &context.r14, &context.r15,
        &context.r16, &context.r17, &context.r18, &context.r19,
        &context.r20, &context.r21, &context.r22, &context.r23,
        &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
}

family::Registers FromPpc(PPCContext& context)
{
    family::Registers state{};
    const auto fields = Fields(context);
    for (unsigned index = 0; index < fields.size(); ++index)
        state.r[index] = index == 1u ? 0u : fields[index]->u64;
    state.sp = context.r1.u64;
    state.lr = context.lr;
    state.ctr = context.ctr.u64;
    state.xer_so = context.xer.so;
    state.xer_ca = context.xer.ca;
    state.cr0 = {std::uint8_t(context.cr0.lt),
        std::uint8_t(context.cr0.gt), std::uint8_t(context.cr0.eq),
        std::uint8_t(context.cr0.so)};
    state.cr6 = {std::uint8_t(context.cr6.lt),
        std::uint8_t(context.cr6.gt), std::uint8_t(context.cr6.eq),
        std::uint8_t(context.cr6.so)};
    return state;
}

bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }

bool SameLine(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so &&
        a.xer_ca == b.xer_ca && SameCondition(a.cr0, b.cr0) &&
        SameCondition(a.cr6, b.cr6);
}

bool SameNumber(const family::Registers& a, const family::Registers& b)
{
    // The accepted CRT decimal parser exposes r3, r13, LR and nonvolatile
    // frame registers. The outer body's post-call r11/output are also live.
    if (a.sp != b.sp || a.lr != b.lr || a.r[3] != b.r[3] ||
        a.r[11] != b.r[11] || a.r[13] != b.r[13]) return false;
    for (unsigned index = 29u; index <= 31u; ++index)
        if (a.r[index] != b.r[index]) return false;
    return true;
}

void WriteText(GuestMemory& memory, GuestAddress address, const char* text)
{
    do
    {
        memory.WriteU16(address, static_cast<unsigned char>(*text));
        address += 2u;
    } while (*text++);
}

void Seed(test::GuestWindow& window, Kind kind)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(Cursor, Text);
    memory.WriteU32(Output, Sentinel);
    memory.WriteU32(0x83215b40u, Classes);
    for (const auto ch : {' ', '\t', '\n', '\r'})
        memory.WriteU16(Classes + 2u * static_cast<unsigned>(ch), 8u);
    switch (kind)
    {
    case Kind::Empty: WriteText(memory, Text, ""); break;
    case Kind::Plain: WriteText(memory, Text, "abc\nnext"); break;
    case Kind::QuotedComment:
        WriteText(memory, Text, "\"a|b\"//ignored\nZ"); break;
    case Kind::Limit:
        for (unsigned index = 0; index < 256u; ++index)
            memory.WriteU16(Text + 2u * index, 'Q');
        memory.WriteU16(Text + 512u, '\n');
        memory.WriteU16(Text + 514u, 0u);
        break;
    case Kind::Miss:
        WriteText(memory, Text, "alpha=42");
        WriteText(memory, Name, "beta");
        break;
    case Kind::Number:
        WriteText(memory, Text, "VAL123");
        WriteText(memory, Name, "val");
        break;
    case Kind::Negative:
        WriteText(memory, Text, "val-42");
        WriteText(memory, Name, "VAL");
        break;
    case Kind::SignedWhite:
        WriteText(memory, Text, "vAl  +17");
        WriteText(memory, Name, "val");
        break;
    }
}

PPCContext Initial(Kind kind)
{
    PPCContext context{};
    const auto fields = Fields(context);
    for (unsigned index = 0; index < fields.size(); ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    context.r1.u64 = 0x1234567800000000ull | Stack;
    context.r3.u64 = 0x9988776600000000ull |
        (IsLine(kind) ? Cursor : Text);
    context.r4.u64 = 0x8877665500000000ull |
        (IsLine(kind) ? Output : Name);
    context.r5.u64 = 0x7766554400000000ull |
        (IsLine(kind) ? 0x13579u : Output);
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

void CheckExpected(Kind kind, const GuestMemory& memory,
    const family::Registers& state)
{
    if (IsLine(kind))
    {
        const auto expected_status = kind == Kind::Empty ? 0u : 1u;
        const auto expected_cursor = kind == Kind::Empty ? Text :
            kind == Kind::Plain ? Text + 8u :
            kind == Kind::QuotedComment ? Text + 30u : Text + 510u;
        const unsigned output_length = kind == Kind::Empty ? 0u :
            kind == Kind::Plain ? 3u :
            kind == Kind::QuotedComment ? 5u : 255u;
        if (state.r[3] != expected_status ||
            memory.ReadU32(Cursor) != expected_cursor ||
            memory.ReadU16(Output + 2u * output_length) != 0u)
            throw std::runtime_error("independent UTF-16 line status/cursor");
        const char* output = kind == Kind::Plain ? "abc" : "\"a|b\"";
        if (kind == Kind::Plain || kind == Kind::QuotedComment)
            for (unsigned index = 0; index < output_length; ++index)
                if (memory.ReadU16(Output + 2u * index) !=
                    static_cast<unsigned char>(output[index]))
                    throw std::runtime_error("independent UTF-16 line text");
        if (kind == Kind::Limit)
            for (unsigned index = 0; index < 255u; ++index)
                if (memory.ReadU16(Output + 2u * index) != 'Q')
                    throw std::runtime_error("independent UTF-16 line limit");
        return;
    }
    const auto expected = kind == Kind::Miss ? Sentinel :
        kind == Kind::Number ? 123u :
        kind == Kind::Negative ? 0xffffffd6u : 17u;
    if (memory.ReadU32(Output) != expected ||
        (kind != Kind::Miss && state.r[3] != 1u))
        throw std::runtime_error("independent named decimal result");
}

void Check(Kind kind)
{
    test::GuestWindow original(Regions), recovered(Regions);
    Seed(original, kind);
    Seed(recovered, kind);
    auto context = Initial(kind);
    auto state = FromPpc(context);
    if (IsLine(kind)) __imp__sub_82295EE0(context, original.Bytes());
    else __imp__sub_822988E0(context, original.Bytes());
    Services services;
    auto memory = recovered.Memory();
    if (!family::Apply(IsLine(kind) ? 0x82295ee0u : 0x822988e0u,
        memory, services, services, state))
        throw std::runtime_error("UTF-16 line/number entry missing");
    const auto observed = FromPpc(context);
    const bool state_equal = IsLine(kind) ?
        SameLine(observed, state) : SameNumber(observed, state);
    if (!state_equal || !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr, "FAIL UTF-16 case=%u state=%u RAM=%u "
            "r3=%llX/%llX r11=%llX/%llX\n",
            static_cast<unsigned>(kind), state_equal,
            original.EqualCommitted(recovered),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.r[11]),
            static_cast<unsigned long long>(state.r[11]));
        throw std::runtime_error("UTF-16 line/number PPC mismatch");
    }
    CheckExpected(kind, original.Memory(), observed);
}
} // namespace

void OriginalSave23(PPCContext& context, std::uint8_t* base)
{
    const auto fields = Fields(context);
    for (unsigned index = 23u; index <= 31u; ++index)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - index),
            fields[index]->u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}
void OriginalRestore23(PPCContext& context, std::uint8_t* base)
{
    const auto fields = Fields(context);
    for (unsigned index = 23u; index <= 31u; ++index)
        fields[index]->u64 = PPC_LOAD_U64(
            context.r1.u32 - 8u * (33u - index));
    context.r12.u64 = PPC_LOAD_U32(context.r1.u32 - 8u);
    context.lr = context.r12.u64;
}
void OriginalSave29(PPCContext& context, std::uint8_t* base)
{
    const auto fields = Fields(context);
    for (unsigned index = 29u; index <= 31u; ++index)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - index),
            fields[index]->u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}
void OriginalRestore29(PPCContext& context, std::uint8_t* base)
{
    const auto fields = Fields(context);
    for (unsigned index = 29u; index <= 31u; ++index)
        fields[index]->u64 = PPC_LOAD_U64(
            context.r1.u32 - 8u * (33u - index));
    context.r12.u64 = PPC_LOAD_U32(context.r1.u32 - 8u);
    context.lr = context.r12.u64;
}
void OriginalSave25(PPCContext& context, std::uint8_t* base)
{
    const auto fields = Fields(context);
    for (unsigned index = 25u; index <= 31u; ++index)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - index),
            fields[index]->u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}
void OriginalRestore25(PPCContext& context, std::uint8_t* base)
{
    const auto fields = Fields(context);
    for (unsigned index = 25u; index <= 31u; ++index)
        fields[index]->u64 = PPC_LOAD_U64(
            context.r1.u32 - 8u * (33u - index));
    context.r12.u64 = PPC_LOAD_U32(context.r1.u32 - 8u);
    context.lr = context.r12.u64;
}
void OriginalInvalidArgument(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected invalid argument guest call"); }
void OriginalInvalidParameter(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected invalid parameter guest call"); }

int main()
{
    try
    {
        for (const auto kind : Cases) Check(kind);
        std::printf("PASS legacy-utf16-line-token-number %zu PPC cases\n",
            Cases.size());
        std::puts("LIMIT decimal lower exposes selected frame/return state; volatile parser scratch, invalid paths, other inputs, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}

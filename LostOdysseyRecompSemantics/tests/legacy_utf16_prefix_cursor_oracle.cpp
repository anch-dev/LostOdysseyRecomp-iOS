// Appended after 82296740 and its four complete PPC callees by the runner.
// Only 82296740 is new mapping credit; the lower entries were accepted earlier.
#include "lo_semantics/legacy_utf16_prefix_cursor.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_utf16_prefix_cursor;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Cursor = 0x20000u;
constexpr GuestAddress Text = 0x30000u;
constexpr GuestAddress Token = 0x40000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::array<Region, 1> Regions{{{0u, 0x90000u}}};

enum class Mode { MatchBoundary, MatchRewindLetter, MatchRewindDigit,
    Mismatch, ExactEnd, EmptyToken };
constexpr std::array<Mode, 6> Cases{{Mode::MatchBoundary,
    Mode::MatchRewindLetter, Mode::MatchRewindDigit, Mode::Mismatch,
    Mode::ExactEnd, Mode::EmptyToken}};

struct Services final : CrtThreadDataServices, InvalidParameterServices
{
    std::uint64_t GetTlsValue(std::uint32_t) override
    { throw std::runtime_error("unexpected TLS read"); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { throw std::runtime_error("unexpected TLS write"); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data getter"); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected thread-data allocation"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data binding"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data free"); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter handler"); }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter trap"); }
};

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    const PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    for (unsigned index = 0; index < 32u; ++index)
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

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr && a.ctr == b.ctr &&
        a.xer_so == b.xer_so && a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so;
}

void WriteText(GuestMemory& memory, GuestAddress address, const char* value)
{
    do
    {
        memory.WriteU16(address, static_cast<unsigned char>(*value));
        address += 2u;
    } while (*value++);
}

void Seed(GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(Cursor, Text);
    switch (mode)
    {
    case Mode::MatchBoundary:
        WriteText(memory, Text, " \tAb \t!");
        WriteText(memory, Token, "ab");
        break;
    case Mode::MatchRewindLetter:
        WriteText(memory, Text, "AbC");
        WriteText(memory, Token, "ab");
        break;
    case Mode::MatchRewindDigit:
        WriteText(memory, Text, "aB5");
        WriteText(memory, Token, "Ab");
        break;
    case Mode::Mismatch:
        WriteText(memory, Text, "xAb");
        WriteText(memory, Token, "ab");
        break;
    case Mode::ExactEnd:
        WriteText(memory, Text, "AB");
        WriteText(memory, Token, "ab");
        break;
    case Mode::EmptyToken:
        WriteText(memory, Text, "?");
        WriteText(memory, Token, "");
        break;
    }
}

PPCContext Initial()
{
    PPCContext context{};
    PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    context.r1.u64 = 0x1234567800000000ull | Stack;
    context.r3.u64 = 0x1234567800000000ull | Cursor;
    context.r4.u64 = 0x8765432100000000ull | Token;
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

bool ExpectedPath(Mode mode, const PPCContext& context,
    const GuestWindow& window)
{
    GuestAddress cursor = Text;
    bool result = false;
    switch (mode)
    {
    case Mode::MatchBoundary: cursor += 12u; result = true; break;
    case Mode::MatchRewindLetter: break;
    case Mode::MatchRewindDigit: break;
    case Mode::Mismatch: break;
    case Mode::ExactEnd: cursor += 4u; result = true; break;
    case Mode::EmptyToken: result = true; break;
    }
    const auto memory = window.Memory();
    return memory.ReadU32(Cursor) == cursor &&
        context.r3.u64 == (result ? 1u : 0u);
}

bool Check(Mode mode)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode);
    Seed(recovered, mode);
    auto raw = Initial();
    auto state = FromPpc(raw);
    Services actual;
    __imp__sub_82296740(raw, original.Bytes());
    if (!ExpectedPath(mode, raw, original))
        throw std::runtime_error("prefix cursor fixture missed PPC path");
    auto memory = recovered.Memory();
    if (!family::Apply(0x82296740u, memory, actual, actual, state))
        throw std::runtime_error("prefix cursor recovered entry missing");
    const auto observed = FromPpc(raw);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!Same(observed, state) || !ram_equal)
    {
        std::fprintf(stderr, "FAIL prefix mode=%u r3=%llX/%llX "
            "cursor=%08X/%08X RAM=%u CR6=%u%u%u%u/%u%u%u%u "
            "CA=%u/%u\n", static_cast<unsigned>(mode),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            original.Memory().ReadU32(Cursor), memory.ReadU32(Cursor),
            ram_equal, observed.cr6.lt, observed.cr6.gt, observed.cr6.eq,
            observed.cr6.so, state.cr6.lt, state.cr6.gt, state.cr6.eq,
            state.cr6.so, observed.xer_ca, state.xer_ca);
        for (unsigned index = 0; index < 32u; ++index)
            if (observed.r[index] != state.r[index])
                std::fprintf(stderr, " r%u=%llX/%llX", index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        std::fprintf(stderr, "\n");
        return false;
    }
    return true;
}
} // namespace

void LegacyPrefixInvalidArgument(PPCContext&, std::uint8_t*)
{
    throw std::runtime_error("unexpected original invalid argument");
}

void LegacyPrefixInvalidParameter(PPCContext&, std::uint8_t*)
{
    throw std::runtime_error("unexpected original invalid parameter");
}

int main()
{
    try
    {
        for (const auto mode : Cases)
            if (!Check(mode)) return 1;
        std::printf("PASS legacy-utf16-prefix-cursor %zu composed PPC cases\n",
            Cases.size());
        std::puts("LIMIT accepted lower callees; invalid args, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

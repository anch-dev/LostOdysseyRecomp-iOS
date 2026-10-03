// Appended after the pinned PPC body by semantic_recovery.py.
#include "lo_semantics/legacy_character_cursor.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_character_cursor;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Cursor = 0x20000u;
constexpr GuestAddress Text = 0x30000u;
constexpr GuestAddress Output = 0x40000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::array<Region, 1> Regions{{{0u, 0x90000u}}};

enum class Mode { Empty, Ordinary, LeadingSpaceTab, Quoted,
    EmptyQuoted, QuotedEscape, EscapeTerminal, LiteralBackslash,
    BufferLimit, QuotedBufferLimit };
constexpr std::array<Mode, 10> Cases{{Mode::Empty, Mode::Ordinary,
    Mode::LeadingSpaceTab, Mode::Quoted, Mode::EmptyQuoted,
    Mode::QuotedEscape, Mode::EscapeTerminal, Mode::LiteralBackslash,
    Mode::BufferLimit, Mode::QuotedBufferLimit}};

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

void Seed(GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(Cursor, Text);
    for (unsigned index = 0; index < 12u; ++index)
        memory.WriteU16(Output + 2u * index, 0xeeeeu);
    switch (mode)
    {
    case Mode::Empty:
        memory.WriteU16(Text, 0u);
        break;
    case Mode::Ordinary:
        memory.WriteU16(Text, 'A');
        memory.WriteU16(Text + 2u, 'B');
        memory.WriteU16(Text + 4u, 'C');
        break;
    case Mode::LeadingSpaceTab:
        memory.WriteU16(Text, ' ');
        memory.WriteU16(Text + 2u, '\t');
        memory.WriteU16(Text + 4u, 'H');
        memory.WriteU16(Text + 6u, 'i');
        break;
    case Mode::Quoted:
        memory.WriteU16(Text, '"');
        memory.WriteU16(Text + 2u, 'A');
        memory.WriteU16(Text + 4u, 'B');
        memory.WriteU16(Text + 6u, '"');
        break;
    case Mode::EmptyQuoted:
        memory.WriteU16(Text, '"');
        memory.WriteU16(Text + 2u, '"');
        break;
    case Mode::QuotedEscape:
        memory.WriteU16(Text, '"');
        memory.WriteU16(Text + 2u, 'A');
        memory.WriteU16(Text + 4u, '\\');
        memory.WriteU16(Text + 6u, '"');
        memory.WriteU16(Text + 8u, 'B');
        memory.WriteU16(Text + 10u, '"');
        break;
    case Mode::EscapeTerminal:
        memory.WriteU16(Text, '"');
        memory.WriteU16(Text + 2u, 'A');
        memory.WriteU16(Text + 4u, '\\');
        break;
    case Mode::LiteralBackslash:
        memory.WriteU16(Text, '"');
        memory.WriteU16(Text + 2u, 'A');
        memory.WriteU16(Text + 4u, '\\');
        memory.WriteU16(Text + 6u, 'B');
        memory.WriteU16(Text + 8u, '"');
        break;
    case Mode::BufferLimit:
        memory.WriteU16(Text, 'A');
        memory.WriteU16(Text + 2u, 'B');
        memory.WriteU16(Text + 4u, 'C');
        memory.WriteU16(Text + 6u, 'D');
        memory.WriteU16(Text + 8u, 'E');
        memory.WriteU16(Text + 10u, ' ');
        break;
    case Mode::QuotedBufferLimit:
        memory.WriteU16(Text, '"');
        memory.WriteU16(Text + 2u, 'A');
        memory.WriteU16(Text + 4u, 'B');
        memory.WriteU16(Text + 6u, 'C');
        memory.WriteU16(Text + 8u, 'D');
        memory.WriteU16(Text + 10u, '"');
        break;
    }
}

PPCContext Initial(Mode mode)
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
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.r3.u64 = 0x1234567800000000ull | Cursor;
    context.r4.u64 = 0x8765432100000000ull | Output;
    context.r5.u64 = 0xdeadbeef00000000ull |
        ((mode == Mode::BufferLimit || mode == Mode::QuotedBufferLimit) ?
            3u : 16u);
    context.r6.u64 = (mode == Mode::QuotedEscape ||
        mode == Mode::EscapeTerminal) ?
        0x1234567800000001ull : 0x1234567800000000ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

bool ExpectedPath(Mode mode, const PPCContext& context,
    const GuestWindow& window)
{
    const auto memory = window.Memory();
    GuestAddress cursor = Text;
    const char* result = "";
    switch (mode)
    {
    case Mode::Empty: break;
    case Mode::Ordinary: cursor += 6u; result = "ABC"; break;
    case Mode::LeadingSpaceTab: cursor += 8u; result = "Hi"; break;
    case Mode::Quoted: cursor += 8u; result = "AB"; break;
    case Mode::EmptyQuoted: cursor += 4u; break;
    case Mode::QuotedEscape: cursor += 12u; result = "A\"B"; break;
    case Mode::EscapeTerminal: cursor += 8u; result = "A"; break;
    case Mode::LiteralBackslash: cursor += 10u; result = "A\\B"; break;
    case Mode::BufferLimit: cursor += 10u; result = "AB"; break;
    case Mode::QuotedBufferLimit: cursor += 6u; result = "AB"; break;
    }
    if (memory.ReadU32(Cursor) != cursor ||
        context.r3.u64 != (result[0] ? 1u : 0u))
        return false;
    unsigned index = 0;
    while (result[index])
    {
        if (memory.ReadU16(Output + 2u * index) !=
            static_cast<unsigned char>(result[index]))
            return false;
        ++index;
    }
    return memory.ReadU16(Output + 2u * index) == 0u &&
        memory.ReadU16(Output + 2u * (index + 1u)) == 0xeeeeu;
}

bool Check(Mode mode)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode);
    Seed(recovered, mode);
    auto raw = Initial(mode);
    auto state = FromPpc(raw);
    std::fprintf(stderr, "cursor original mode=%u\n",
        static_cast<unsigned>(mode));
    __imp__sub_822969A0(raw, original.Bytes());
    if (!ExpectedPath(mode, raw, original))
    {
        const auto memory = original.Memory();
        std::fprintf(stderr,
            "cursor original path mode=%u r3=%llX cursor=%08X out=%04X %04X %04X\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            memory.ReadU32(Cursor), memory.ReadU16(Output),
            memory.ReadU16(Output + 2u), memory.ReadU16(Output + 4u));
        throw std::runtime_error("cursor fixture missed original path");
    }
    auto recovered_memory = recovered.Memory();
    if (!family::Apply(0x822969a0u, recovered_memory, state))
        throw std::runtime_error("cursor recovered entry missing");
    const auto observed = FromPpc(raw);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!Same(observed, state) || !ram_equal)
    {
        std::fprintf(stderr,
            "FAIL cursor mode=%u r3=%llX/%llX sp=%llX/%llX "
            "lr=%llX/%llX ctr=%llX/%llX RAM=%u "
            "CR6=%u%u%u%u/%u%u%u%u CA=%u/%u\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(observed.ctr),
            static_cast<unsigned long long>(state.ctr), ram_equal,
            observed.cr6.lt, observed.cr6.gt, observed.cr6.eq,
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

int main()
{
    try
    {
        for (const auto mode : Cases)
            if (!Check(mode)) return 1;
        std::printf("PASS legacy-character-cursor %zu focused original PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected ABI/RAM; other inputs, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

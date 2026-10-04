// Appended after the complete 82479188 and 823F7BF8 PPC bodies.
#include "lo_semantics/legacy_numeric_text_shape.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_numeric_text_shape;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Record = 0x30000u;
constexpr GuestAddress Text = 0x40000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::array<Region, 1> Regions{{{0u, 0x90000u}}};

struct Case
{
    const char* name;
    const char* text;
    std::uint32_t length;
    GuestAddress entry;
    std::uint64_t expected;
};
constexpr std::array Cases{
    Case{"empty", "", 1u, 0x82479188u, 0u},
    Case{"zero-length", "0", 0u, 0x82479188u, 0u},
    Case{"one-digit", "7", 2u, 0x82479188u, 1u},
    Case{"minus-alone", "-", 2u, 0x82479188u, 1u},
    Case{"decimal", "12.3", 5u, 0x82479188u, 1u},
    Case{"second-dot", "1..2", 5u, 0x82479188u, 0u},
    Case{"plus-prefix", "+12", 4u, 0x82479188u, 0u},
    Case{"letter-tail", "12x", 4u, 0x82479188u, 0u},
    Case{"leaf-zero", "", 0u, 0x823f7bf8u, 0u},
    Case{"leaf-four", "123", 4u, 0x823f7bf8u, 3u},
};

PPCRegister* Gpr(PPCContext& context, unsigned index)
{
    PPCRegister* const fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16,
        &context.r17, &context.r18, &context.r19, &context.r20,
        &context.r21, &context.r22, &context.r23, &context.r24,
        &context.r25, &context.r26, &context.r27, &context.r28,
        &context.r29, &context.r30, &context.r31};
    return fields[index];
}

family::Registers FromPpc(PPCContext& context)
{
    family::Registers state{};
    for (unsigned i = 0; i < 32; ++i) state.r[i] = Gpr(context, i)->u64;
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

void Seed(GuestWindow& window, const Case& item)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(Record, Text);
    memory.WriteU32(Record + 4u, item.length);
    auto address = Text;
    const char* cursor = item.text;
    do
    {
        memory.WriteU16(address, static_cast<unsigned char>(*cursor));
        address += 2u;
    } while (*cursor++);
}

PPCContext Initial()
{
    PPCContext context{};
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = 0x1122334400000000ull + i;
    context.r1.u64 = 0x8877665500000000ull | Stack;
    context.r3.u64 = 0x9988776600000000ull | Record;
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.lt = 1;
    context.cr6.gt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so &&
        a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so;
}

void Check(const Case& item)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, item);
    Seed(recovered, item);
    auto context = Initial();
    auto state = FromPpc(context);
    if (item.entry == 0x82479188u)
        __imp__sub_82479188(context, original.Bytes());
    else __imp__sub_823F7BF8(context, original.Bytes());
    if (context.r3.u64 != item.expected)
    {
        std::fprintf(stderr, "EXPECT %s r3=%llX/%llX\n", item.name,
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(item.expected));
        throw std::runtime_error("independent numeric text result");
    }
    auto memory = recovered.Memory();
    if (!family::Apply(item.entry, memory, state))
        throw std::runtime_error("numeric text entry missing");
    const auto observed = FromPpc(context);
    const bool state_equal = Same(observed, state);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!state_equal || !ram_equal)
    {
        std::fprintf(stderr, "FAIL %s state=%u RAM=%u "
            "r3=%llX/%llX SP=%llX/%llX LR=%llX/%llX CTR=%llX/%llX "
            "CR6=%u%u%u%u/%u%u%u%u CA=%u/%u\n", item.name,
            state_equal, ram_equal,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(observed.ctr),
            static_cast<unsigned long long>(state.ctr),
            observed.cr6.lt, observed.cr6.gt, observed.cr6.eq,
            observed.cr6.so, state.cr6.lt, state.cr6.gt, state.cr6.eq,
            state.cr6.so, observed.xer_ca, state.xer_ca);
        for (unsigned i = 0; i < 32; ++i)
            if (observed.r[i] != state.r[i])
                std::fprintf(stderr, " r%u=%llX/%llX", i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        std::fputc('\n', stderr);
        throw std::runtime_error("numeric text PPC mismatch");
    }
}
} // namespace

void OriginalSaveShape(PPCContext& context, std::uint8_t* base)
{
    for (unsigned i = 29; i <= 31; ++i)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - i),
            Gpr(context, i)->u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}

void OriginalRestoreShape(PPCContext& context, std::uint8_t* base)
{
    for (unsigned i = 29; i <= 31; ++i)
        Gpr(context, i)->u64 = PPC_LOAD_U64(
            context.r1.u32 - 8u * (33u - i));
    context.r12.u64 = PPC_LOAD_U32(context.r1.u32 - 8u);
    context.lr = context.r12.u64;
}

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-numeric-text-shape %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT bounded mapped RAM; faults/MMIO/concurrent mutation/runtime");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

#include "lo_semantics/legacy_descriptor_array_lookup.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_array_lookup;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Owner = 0x10000u;
constexpr GuestAddress Previous = 0x20000u;
constexpr GuestAddress Values = 0x30000u;
constexpr GuestAddress Existing = 0x40000u;
constexpr GuestAddress Large = 0x50000u;
constexpr GuestAddress Pool = 0x60000u;
constexpr GuestAddress NextLink = 0x51000u;
constexpr GuestAddress PriorLink = 0x52000u;
constexpr GuestAddress SelectorTable = 0x821712a0u;
constexpr GuestAddress FloatZero = 0x82000e50u;
constexpr std::array<test::Region, 4> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x1000u},
    {0x82171000u, 0x1000u}, {0x83245000u, 0x2000u}
}};
// image_disc1.bin VA 0x821712A0..C9, file offsets 0x1712A0..C9.
constexpr std::array<std::uint8_t, 42> SelectorBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};

enum class Path { Empty, Match, Mismatch, TombCache, TombFree,
    SmallReturn, LargeReturn };
struct Case
{
    const char* name;
    GuestAddress entry;
    Path path;
    std::uint32_t count, flags, value0, value1;
};
constexpr std::array<Case, 8> Cases{{
    {"empty-zero", 0x83058ad8u, Path::Empty, 0u, 0u, 0u, 0u},
    {"empty-two-float-bits", 0x83058ad8u, Path::Empty,
        2u, 0x12u, 0x3f800000u, 0xc0000000u},
    {"existing-two-word-match", 0x83058ad8u, Path::Match,
        2u, 0x34u, 0x11111111u, 0x22222222u},
    {"existing-value-mismatch", 0x83058ad8u, Path::Mismatch,
        1u, 0x01u, 0x3fc00000u, 0u},
    {"tombstone-cache-reuse", 0x83058ad8u, Path::TombCache,
        1u, 0x77u, 0x40400000u, 0u},
    {"tombstone-small-free", 0x83058ad8u, Path::TombFree,
        1u, 0x21u, 0xbfc00000u, 0u},
    {"standalone-small-return", 0x82fac980u, Path::SmallReturn,
        0u, 0u, 0u, 0u},
    {"standalone-large-tail", 0x82fac980u, Path::LargeReturn,
        0u, 0u, 0u, 0u}
}};

unsigned selector_calls = 0, mutation_calls = 0, allocation_calls = 0;
unsigned constructor_calls = 0, return_calls = 0, fill_calls = 0;
unsigned dispatch_calls = 0, general_calls = 0, heap_general_calls = 0;
using Event = std::array<std::uint64_t, 5>;
std::vector<Event> original_events;

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}

family::Registers FromPpc(PPCContext& c)
{
    family::Registers state{};
    const auto fields = Fields(c);
    auto& g = state.integer;
    for (unsigned index = 0; index < 32u; ++index)
        g.r[index] = index == 1u ? 0u : fields[index]->u64;
    g.sp = c.r1.u64; g.lr = c.lr; g.ctr = c.ctr.u64;
    g.xer_so = c.xer.so; g.xer_ca = c.xer.ca;
    g.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    g.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.f0_bits = c.f0.u64; state.f12_bits = c.f12.u64;
    state.f13_bits = c.f13.u64;
    state.cached_fp_control = c.fpscr.csr;
    return state;
}

bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }

bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.integer;
    const auto& y = b.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr && x.ctr == y.ctr &&
        x.xer_so == y.xer_so && x.xer_ca == y.xer_ca &&
        SameCondition(x.cr0, y.cr0) && SameCondition(x.cr6, y.cr6) &&
        a.f0_bits == b.f0_bits && a.f12_bits == b.f12_bits &&
        a.f13_bits == b.f13_bits &&
        a.cached_fp_control == b.cached_fp_control;
}

GuestAddress Bucket(const Case& item)
{
    const std::uint32_t sum = (item.flags & 0xffu) +
        (item.count >= 1u ? item.value0 : 0u) +
        (item.count >= 2u ? item.value1 : 0u);
    return Owner + ((sum % 7u) + 15u) * 4u;
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0x1234567800000000ull | Owner;
    c.r4.u64 = item.entry == 0x83058ad8u ?
        0x5566778800000000ull | item.count :
        (item.path == Path::LargeReturn ? Large : Existing);
    c.r5.u64 = item.entry == 0x83058ad8u ?
        0x99aabbcc00000000ull | Values :
        (item.path == Path::LargeReturn ? 136u : 40u);
    c.r6.u64 = 0xddeeffaa00000000ull | item.flags;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    c.f0.u64 = std::bit_cast<std::uint64_t>(5.0);
    c.f12.u64 = std::bit_cast<std::uint64_t>(7.0);
    c.f13.u64 = std::bit_cast<std::uint64_t>(9.0);
    c.fpscr.csr = 0x1f80u;
    return c;
}

void Seed(GuestMemory& memory, const Case& item)
{
    for (unsigned index = 0; index < SelectorBytes.size(); ++index)
        memory.WriteU8(SelectorTable + index, SelectorBytes[index]);
    // image_disc1.bin VA 0x82000E50, file offset 0xE50: 00 00 00 00.
    memory.WriteU32(FloatZero, 0u);
    memory.WriteU32(Owner + 4u, Owner);
    memory.WriteU32(Owner + 16u, Previous);
    memory.WriteU32(Previous, 0u);
    memory.WriteU32(Owner + 40u,
        item.path == Path::TombCache ? 0x1000u : 0u);
    memory.WriteU32(Owner + 88u, 0u);
    memory.WriteU32(Owner + 540u, 0u);
    memory.WriteU32(Owner + 912u, Pool);
    memory.WriteU32(Owner + 916u, Pool);
    for (std::uint32_t count = 0u; count <= 2u; ++count)
        memory.WriteU32(Owner + 772u + 40u + 4u * count - 4u, 0u);
    memory.WriteU32(Values, item.value0);
    memory.WriteU32(Values + 4u, item.value1);
    if (item.entry == 0x83058ad8u)
    {
        const auto path = item.path;
        memory.WriteU32(Bucket(item), path == Path::Empty ? 0u : Existing);
        memory.WriteU32(Existing + 28u, 0u);
        const auto type = path == Path::TombCache || path == Path::TombFree ?
            114u : 124u;
        memory.WriteU32(Existing + 8u,
            (type << 7u) | (item.count << 14u));
        memory.WriteU32(Existing + 16u, (item.flags & 0xffu) << 14u);
        memory.WriteU32(Existing + 40u,
            path == Path::Mismatch ? item.value0 ^ 1u : item.value0);
        memory.WriteU32(Existing + 44u, item.value1);
    }
    if (item.path == Path::LargeReturn)
    {
        memory.WriteU32(Large - 12u, NextLink);
        memory.WriteU32(Large - 8u, PriorLink);
        memory.WriteU32(NextLink, Large - 12u);
        memory.WriteU32(PriorLink, Large - 12u);
        memory.WriteU32(0x83245708u, 0x70000u);
    }
    if (item.path == Path::SmallReturn)
        memory.WriteU32(Existing, 0u);
}

class AllocationServices final : public legacy_descriptor_array_allocation::Services
{
public:
    unsigned native = 0, fp = 0;
    void AllocateFromPool(GuestMemory&,
        crt_stream_operations::Registers&) override
    { ++native; throw std::runtime_error("unexpected pool exhaustion"); }
    void SetHostFpControl(std::uint32_t control) override
    { ++fp; simde_mm_setcsr(control); }
};

class FreeServices final : public family::FreeTailServices
{
public:
    std::vector<Event> events;
    void Call(GuestAddress target, GuestMemory& memory,
        crt_stream_operations::Registers& integer) override
    {
        std::fprintf(stderr,
            "recovered free tail target=%08X heapglobal=%08X r3=%016llX r5=%016llX\n",
            target, memory.ReadU32(0x83245708u),
            static_cast<unsigned long long>(integer.r[3]),
            static_cast<unsigned long long>(integer.r[5]));
        std::fflush(stderr);
        events.push_back({target, integer.r[3], integer.r[4],
            integer.r[5], integer.lr});
        if (target != 0x823ade28u)
            throw std::runtime_error("unexpected free tail boundary");
        integer.r[3] = 1u;
        integer.r[9] = 0x1234u;
        memory.WriteU32(Owner + 200u, 0xabcdu);
    }
};

void CallOriginal(const Case& item, PPCContext& c, std::uint8_t* base)
{
    if (item.entry == 0x83058ad8u)
        __imp__sub_83058AD8(c, base);
    else __imp__sub_82FAC980(c, base);
}

void Check(const Case& item)
{
    std::fprintf(stderr, "case %s: seed\n", item.name);
    std::fflush(stderr);
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial(item);
    auto state = FromPpc(context);
    AllocationServices allocator;
    FreeServices free;
    const auto previous_control = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    selector_calls = mutation_calls = allocation_calls = 0;
    constructor_calls = return_calls = fill_calls = 0;
    dispatch_calls = general_calls = heap_general_calls = 0;
    original_events.clear();
    std::fprintf(stderr, "case %s: original start\n", item.name);
    std::fflush(stderr);
    CallOriginal(item, context, original.Bytes());
    std::fprintf(stderr, "case %s: original done\n", item.name);
    std::fflush(stderr);
    const auto original_control = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    std::fprintf(stderr, "case %s: recovered start\n", item.name);
    std::fflush(stderr);
    if (!family::Apply(item.entry, right, {allocator, free}, state))
        throw std::runtime_error("recovered descriptor lookup entry missing");
    std::fprintf(stderr, "case %s: recovered done\n", item.name);
    std::fflush(stderr);
    const auto recovered_control = simde_mm_getcsr();
    simde_mm_setcsr(previous_control);
    const auto observed = FromPpc(context);
    const bool main = item.entry == 0x83058ad8u;
    const bool creates = main && item.path != Path::Match;
    const bool tomb_free = item.path == Path::TombFree;
    const bool large = item.path == Path::LargeReturn;
    // Direct entry invocation bypasses the OriginalReturnRecord call wrapper.
    const bool returns = tomb_free;
    if (selector_calls != unsigned(creates) + unsigned(tomb_free) ||
        mutation_calls != unsigned(creates) ||
        allocation_calls != unsigned(creates) ||
        constructor_calls != unsigned(creates) ||
        return_calls != unsigned(returns) || fill_calls != 0u ||
        dispatch_calls != unsigned(large) ||
        general_calls != unsigned(large) ||
        heap_general_calls != unsigned(large) ||
        free.events != original_events ||
        allocator.native != 0u || allocator.fp != 0u)
    {
        std::fprintf(stderr,
            "fixture path %s: selector=%u/%u mutation=%u/%u allocation=%u/%u constructor=%u/%u return=%u/%u fill=%u/0 dispatch=%u/%u general=%u/%u heap=%u/%u events=%zu/%zu native=%u fp=%u\n",
            item.name, selector_calls,
            unsigned(creates) + unsigned(tomb_free),
            mutation_calls, unsigned(creates), allocation_calls,
            unsigned(creates), constructor_calls, unsigned(creates),
            return_calls, unsigned(returns), fill_calls, dispatch_calls,
            unsigned(large), general_calls, unsigned(large),
            heap_general_calls, unsigned(large), original_events.size(),
            free.events.size(), allocator.native, allocator.fp);
        throw std::runtime_error("original descriptor lookup fixture path");
    }
    if (!Same(observed, state) || original_control != recovered_control ||
        !original.EqualCommitted(recovered))
    {
        const auto& a = observed.integer;
        const auto& b = state.integer;
        std::fprintf(stderr,
            "mismatch %s entry=%08X r3=%016llX/%016llX SP=%016llX/%016llX LR=%016llX/%016llX CTR=%016llX/%016llX CA=%u/%u CR0=%u,%u,%u/%u,%u,%u CR6=%u,%u,%u/%u,%u,%u f0=%016llX/%016llX f12=%016llX/%016llX fp=%08X/%08X host=%08X/%08X\n",
            item.name, item.entry,
            static_cast<unsigned long long>(a.r[3]),
            static_cast<unsigned long long>(b.r[3]),
            static_cast<unsigned long long>(a.sp),
            static_cast<unsigned long long>(b.sp),
            static_cast<unsigned long long>(a.lr),
            static_cast<unsigned long long>(b.lr),
            static_cast<unsigned long long>(a.ctr),
            static_cast<unsigned long long>(b.ctr), a.xer_ca, b.xer_ca,
            a.cr0.lt, a.cr0.gt, a.cr0.eq, b.cr0.lt, b.cr0.gt, b.cr0.eq,
            a.cr6.lt, a.cr6.gt, a.cr6.eq, b.cr6.lt, b.cr6.gt, b.cr6.eq,
            static_cast<unsigned long long>(observed.f0_bits),
            static_cast<unsigned long long>(state.f0_bits),
            static_cast<unsigned long long>(observed.f12_bits),
            static_cast<unsigned long long>(state.f12_bits),
            observed.cached_fp_control, state.cached_fp_control,
            original_control, recovered_control);
        for (unsigned index = 0; index < 32u; ++index)
            if (a.r[index] != b.r[index])
                std::fprintf(stderr, "r%u=%016llX/%016llX\n", index,
                    static_cast<unsigned long long>(a.r[index]),
                    static_cast<unsigned long long>(b.r[index]));
        for (const auto region : Regions)
            for (std::size_t offset = 0; offset < region.size; ++offset)
            {
                const auto address = static_cast<std::size_t>(region.base) +
                    offset;
                if (original.Bytes()[address] != recovered.Bytes()[address])
                {
                    std::fprintf(stderr,
                        "RAM first difference %08zX=%02X/%02X\n", address,
                        original.Bytes()[address], recovered.Bytes()[address]);
                    throw std::runtime_error(item.name);
                }
            }
        throw std::runtime_error(item.name);
    }
    if (item.path == Path::SmallReturn &&
        (left.ReadU32(Owner + 808u) != Existing ||
         right.ReadU32(Owner + 808u) != Existing ||
         left.ReadU32(Existing) != 0u ||
         right.ReadU32(Existing) != 0u))
        throw std::runtime_error("standalone small return free head");
}
} // namespace

void OriginalSelector(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82FAC238(c, base); ++selector_calls; }
void OriginalMutation(PPCContext& c, std::uint8_t* base)
{ __imp__sub_83056568(c, base); ++mutation_calls; }
void OriginalAllocate(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82FB36B0(c, base); ++allocation_calls; }
void OriginalConstructor(PPCContext& c, std::uint8_t* base)
{ __imp__sub_83058590(c, base); ++constructor_calls; }
void OriginalReturnRecord(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82FAC980(c, base); ++return_calls; }
void OriginalFill(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82B7BC40(c, base); ++fill_calls; }
void OriginalDispatch(PPCContext& c, std::uint8_t* base)
{
    std::fprintf(stderr, "original free dispatch r3=%016llX r4=%016llX\n",
        static_cast<unsigned long long>(c.r3.u64),
        static_cast<unsigned long long>(c.r4.u64));
    std::fflush(stderr);
    __imp__sub_827C9DB0(c, base); ++dispatch_calls;
}
void OriginalGeneral(PPCContext& c, std::uint8_t* base)
{
    std::fprintf(stderr, "original free general r3=%016llX r4=%016llX\n",
        static_cast<unsigned long long>(c.r3.u64),
        static_cast<unsigned long long>(c.r4.u64));
    std::fflush(stderr);
    __imp__sub_827CA0E8(c, base); ++general_calls;
}
void OriginalHeapGeneral(PPCContext& c, std::uint8_t* base)
{
    std::fprintf(stderr, "original free heap r3=%016llX r4=%016llX\n",
        static_cast<unsigned long long>(c.r3.u64),
        static_cast<unsigned long long>(c.r4.u64));
    std::fflush(stderr);
    __imp__sub_827CAD80(c, base); ++heap_general_calls;
}
void OriginalHeapGlobal(PPCContext& c, std::uint8_t* base)
{
    std::fprintf(stderr, "original heap global=%08X\n",
        PPC_LOAD_U32(0x83245708u));
    std::fflush(stderr);
    __imp__sub_823ACC98(c, base);
}
void OriginalFreeTail(PPCContext& c, std::uint8_t* base)
{
    std::fprintf(stderr, "original free tail LR=%016llX r3=%016llX r5=%016llX\n",
        static_cast<unsigned long long>(c.lr),
        static_cast<unsigned long long>(c.r3.u64),
        static_cast<unsigned long long>(c.r5.u64));
    std::fflush(stderr);
    if (c.lr != 0x827cada4u)
        throw std::runtime_error("unexpected original free tail LR");
    original_events.push_back({0x823ade28u, c.r3.u64, c.r4.u64,
        c.r5.u64, c.lr});
    c.r3.u64 = 1u;
    c.r9.u64 = 0x1234u;
    PPC_STORE_U32(Owner + 200u, 0xabcdu);
}
void OriginalSpecialFree(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected special free boundary"); }
void OriginalPhysicalLower(PPCContext& c, std::uint8_t* base)
{ __imp__sub_827C9EB8(c, base); }
void OriginalPhysicalFree(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected physical free boundary"); }
void OriginalNativePool(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected pool exhaustion"); }
void OriginalSave24(PPCContext& c, std::uint8_t* base)
{
    const auto fields = Fields(c);
    for (unsigned index = 24u; index <= 31u; ++index)
        PPC_STORE_U64(c.r1.u32 - (33u - index) * 8u,
            fields[index]->u64);
    PPC_STORE_U32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore24(PPCContext& c, std::uint8_t* base)
{
    const auto fields = Fields(c);
    for (unsigned index = 24u; index <= 31u; ++index)
        fields[index]->u64 = PPC_LOAD_U64(
            c.r1.u32 - (33u - index) * 8u);
    c.r12.u64 = PPC_LOAD_U32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalSave27(PPCContext& c, std::uint8_t* base)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        PPC_STORE_U64(c.r1.u32 - (33u - index) * 8u,
            fields[index]->u64);
    PPC_STORE_U32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t* base)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        fields[index]->u64 = PPC_LOAD_U64(
            c.r1.u32 - (33u - index) * 8u);
    c.r12.u64 = PPC_LOAD_U32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-array-lookup %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size());
        std::puts("LIMIT selected ordinary RAM/finite FP; deep heap/physical release native boundary; faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}

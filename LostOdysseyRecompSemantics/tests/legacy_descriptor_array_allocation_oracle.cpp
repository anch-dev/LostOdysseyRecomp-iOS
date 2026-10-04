#include "lo_semantics/legacy_descriptor_array_allocation.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_array_allocation;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Owner = 0x10000u;
constexpr GuestAddress Previous = 0x20000u;
constexpr GuestAddress Values = 0x30000u;
constexpr GuestAddress Pool = 0x60000u;
constexpr GuestAddress Free = 0x70000u;
constexpr GuestAddress Native = 0x72000u;
constexpr GuestAddress SelectorTable = 0x821712a0u;
constexpr GuestAddress FloatZero = 0x82000e50u;
constexpr std::array<test::Region, 3> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x1000u},
    {0x82171000u, 0x1000u}
}};

// image_disc1.bin VA 0x821712A0..C9, file offsets 0x1712A0..C9.
constexpr std::array<std::uint8_t, 42> SelectorBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};

enum class Path { Pool, FreeList, NativeBoundary };
struct Case
{
    const char* name;
    GuestAddress entry;
    Path path;
    std::uint32_t count, flags;
    std::uint32_t value0, value1;
    bool low_bit_head, skip_status;
};
constexpr std::array<Case, 8> Cases{{
    {"constructor-empty", 0x83058590u, Path::Pool,
        0u, 0u, 0u, 0u, false, false},
    {"constructor-positive-fraction", 0x83058590u, Path::Pool,
        1u, 0x12u, 0x3fc00000u, 0u, false, false},
    {"constructor-negative-integer", 0x83058590u, Path::Pool,
        1u, 0u, 0xc0400000u, 0u, true, false},
    {"constructor-free-list-two-values", 0x83058590u, Path::FreeList,
        2u, 0x81u, 0x40400000u, 0xbfc00000u, false, false},
    {"constructor-status-skipped", 0x83058590u, Path::Pool,
        2u, 0x55u, 0x3f800000u, 0x00000000u, false, true},
    {"allocator-contiguous", 0x82fb36b0u, Path::Pool,
        1u, 0u, 0u, 0u, false, false},
    {"allocator-free-list", 0x82fb36b0u, Path::FreeList,
        1u, 0u, 0u, 0u, false, false},
    {"allocator-native-boundary", 0x82fb36b0u, Path::NativeBoundary,
        1u, 0u, 0u, 0u, false, false}
}};

unsigned selector_calls = 0, mutation_calls = 0, allocator_calls = 0;
unsigned fill_calls = 0, native_calls = 0;

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

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0x1234567800000000ull | Owner;
    // 83058590 takes the element count in r4 and float-source pointer in r5.
    c.r4.u64 = item.entry == 0x83058590u ?
        0x5566778800000000ull | item.count : 44u;
    c.r5.u64 = item.entry == 0x83058590u ?
        0x99aabbcc00000000ull | Values : 34u;
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
    memory.WriteU32(Owner + 4u, Owner | (item.low_bit_head ? 1u : 0u));
    memory.WriteU32(Owner + 16u, Previous);
    memory.WriteU32(16u, Previous);
    memory.WriteU32(Previous, 0x0a0b0c0du);
    memory.WriteU32(Owner + 40u, item.skip_status ? 0x4000u : 0u);
    memory.WriteU32(Owner + 912u,
        item.path == Path::Pool ? Pool : Pool - 4096u);
    memory.WriteU32(Owner + 916u, Pool);
    memory.WriteU32(Values, item.value0);
    memory.WriteU32(Values + 4u, item.value1);
    const auto bytes = item.entry == 0x83058590u ?
        40u + 4u * item.count : 44u;
    // 82FB36B0 computes this exact size-class head on its exhausted-pool
    // branch. Seed it on every path so an unexpected branch fails the path
    // assertion instead of dereferencing the fill pattern.
    memory.WriteU32(Owner + 772u + bytes - 4u,
        item.path == Path::FreeList ? Free : 0u);
    if (item.path == Path::FreeList) memory.WriteU32(Free, 0u);
}

class MockServices final : public family::Services
{
public:
    unsigned native = 0, fp = 0;
    void AllocateFromPool(GuestMemory& memory,
        crt_stream_operations::Registers& integer) override
    {
        ++native;
        integer.r[3] = Native +
            (static_cast<std::uint32_t>(integer.r[4]) & 0xffu) * 4u;
        integer.r[9] = 0x1234u;
        memory.WriteU32(static_cast<GuestAddress>(integer.r[3]), 0xabcd1234u);
    }
    void SetHostFpControl(std::uint32_t control) override
    { ++fp; simde_mm_setcsr(control); }
};

void CallOriginal(GuestAddress entry, PPCContext& c, std::uint8_t* base)
{
    switch (entry)
    {
    case 0x83058590u: __imp__sub_83058590(c, base); return;
    case 0x82fb36b0u: __imp__sub_82FB36B0(c, base); return;
    default: throw std::runtime_error("unknown descriptor array entry");
    }
}

void Check(const Case& item)
{
    std::fprintf(stderr, "case %s begin\n", item.name);
    std::fflush(stderr);
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    std::fprintf(stderr, "case %s seed complete\n", item.name);
    std::fflush(stderr);
    auto context = Initial(item);
    auto state = FromPpc(context);
    MockServices services;
    const auto previous_control = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    selector_calls = 0; mutation_calls = 0; allocator_calls = 0;
    fill_calls = 0; native_calls = 0;
    std::fprintf(stderr, "case %s original begin\n", item.name);
    std::fflush(stderr);
    CallOriginal(item.entry, context, original.Bytes());
    std::fprintf(stderr, "case %s original complete\n", item.name);
    std::fflush(stderr);
    const auto original_control = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    std::fprintf(stderr, "case %s recovered begin\n", item.name);
    std::fflush(stderr);
    if (!family::Apply(item.entry, right, services, state))
        throw std::runtime_error("recovered descriptor array entry missing");
    std::fprintf(stderr, "case %s recovered complete\n", item.name);
    std::fflush(stderr);
    const auto recovered_control = simde_mm_getcsr();
    simde_mm_setcsr(previous_control);
    const auto observed = FromPpc(context);
    const auto main = item.entry == 0x83058590u;
    if (selector_calls != (main ? 1u : 0u) ||
        mutation_calls != (main ? 1u : 0u) ||
        allocator_calls != (main ? 1u : 0u) ||
        fill_calls != (item.path == Path::FreeList ? 1u : 0u) ||
        native_calls != (item.path == Path::NativeBoundary ? 1u : 0u) ||
        services.native != native_calls || services.fp != 0u)
        throw std::runtime_error("original descriptor array fixture path");
    if (!Same(observed, state) || original_control != recovered_control ||
        !original.EqualCommitted(recovered))
    {
        const auto& a = observed.integer;
        const auto& b = state.integer;
        std::fprintf(stderr,
            "mismatch %s entry=%08X r3=%016llX/%016llX r11=%016llX/%016llX SP=%016llX/%016llX LR=%016llX/%016llX CTR=%016llX/%016llX CA=%u/%u CR0=%u,%u,%u/%u,%u,%u CR6=%u,%u,%u/%u,%u,%u f0=%016llX/%016llX f12=%016llX/%016llX f13=%016llX/%016llX fp=%08X/%08X host=%08X/%08X\n",
            item.name, item.entry,
            static_cast<unsigned long long>(a.r[3]),
            static_cast<unsigned long long>(b.r[3]),
            static_cast<unsigned long long>(a.r[11]),
            static_cast<unsigned long long>(b.r[11]),
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
            static_cast<unsigned long long>(observed.f13_bits),
            static_cast<unsigned long long>(state.f13_bits),
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
}
} // namespace

void OriginalRecordSelector(PPCContext& c, std::uint8_t* base)
{
    std::fprintf(stderr, "original selector r4=%08X r5=%08X r6=%08X\n",
        c.r4.u32, c.r5.u32, c.r6.u32);
    std::fflush(stderr);
    if (c.lr != 0x830585c0u)
        throw std::runtime_error("actual 82FAC238 call LR");
    __imp__sub_82FAC238(c, base);
    std::fprintf(stderr, "original selector result=%08X\n", c.r3.u32);
    std::fflush(stderr);
    ++selector_calls;
}
void OriginalMutate(PPCContext& c, std::uint8_t* base)
{
    std::fprintf(stderr, "original mutation r3=%08X r4=%08X r5=%08X\n",
        c.r3.u32, c.r4.u32, c.r5.u32);
    std::fflush(stderr);
    if (c.lr != 0x83058600u)
        throw std::runtime_error("actual 83056568 call LR");
    __imp__sub_83056568(c, base);
    std::fprintf(stderr, "original mutation complete\n");
    std::fflush(stderr);
    ++mutation_calls;
}
void OriginalAllocateRecord(PPCContext& c, std::uint8_t* base)
{
    std::fprintf(stderr, "original pool r3=%08X bytes=%08X end=%08X cursor=%08X head=%08X\n",
        c.r3.u32, c.r4.u32,
        PPC_LOAD_U32(c.r3.u32 + 912u),
        PPC_LOAD_U32(c.r3.u32 + 916u),
        PPC_LOAD_U32(c.r3.u32 + 772u + c.r4.u32 - 4u));
    std::fflush(stderr);
    if (c.lr != 0x830585d0u)
        throw std::runtime_error("actual 82FB36B0 call LR");
    __imp__sub_82FB36B0(c, base);
    std::fprintf(stderr, "original pool result=%08X\n", c.r3.u32);
    std::fflush(stderr);
    ++allocator_calls;
}
void OriginalFill(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x82fb371cu)
        throw std::runtime_error("actual 82B7BC40 call LR");
    __imp__sub_82B7BC40(c, base);
    ++fill_calls;
}
void OriginalNativeAllocate(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x82fb372cu)
        throw std::runtime_error("82FAC428 native boundary LR");
    c.r3.u64 = Native + (c.r4.u32 & 0xffu) * 4u;
    c.r9.u64 = 0x1234u;
    PPC_STORE_U32(c.r3.u32, 0xabcd1234u);
    ++native_calls;
}
void OriginalSave27(PPCContext& c, std::uint8_t* base)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        PPC_STORE_U64(c.r1.u32 - 48u + (index - 27u) * 8u,
            fields[index]->u64);
    PPC_STORE_U32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t* base)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        fields[index]->u64 = PPC_LOAD_U64(
            c.r1.u32 - 48u + (index - 27u) * 8u);
    c.r12.u64 = PPC_LOAD_U32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-array-allocation %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size());
        std::puts("LIMIT selected ordinary RAM/finite FP; 82FAC428 is a variable native boundary; faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}

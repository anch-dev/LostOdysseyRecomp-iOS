#include "lo_semantics/metadata_descriptor_lookup.h"
#include "lo_semantics/metadata_name_index.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::metadata_descriptor_lookup;
namespace name = lo::semantic::gpu::metadata_name_lookup;
namespace record = lo::semantic::gpu::metadata_name_record;
namespace index = lo::semantic::gpu::metadata_name_index;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Output = 0x20000u;
constexpr GuestAddress Descriptor = 0x30000u;
constexpr GuestAddress Next = 0x31000u;
constexpr GuestAddress Type = 0x34000u;
constexpr GuestAddress Ancestor = 0x34100u;
constexpr GuestAddress NameNode = 0x40000u;
constexpr GuestAddress Name = 0x8218d870u;
constexpr GuestAddress Owned = 0x832f2568u;
constexpr GuestAddress Global = 0x832fa568u;
constexpr GuestAddress NameBuckets = 0x832ee568u;
constexpr std::uint64_t Pair = (std::uint64_t{42} << 32u);
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000}, {0x82000000u, 0x190000},
    {0x83214000u, 0x2000}, {0x83246000u, 0x1000},
    {0x832ee000u, 0x6000}, {0x832f2000u, 0x12000},
    {0x8330b000u, 0x1000}, {0x83369000u, 0x1000}};
enum class Mode { OwnedExact, GlobalHit, OwnedAncestor, GlobalMaskRejectNext,
    AllMaskReject, OwnedSentinel, ParentAdvance, ParentDefaultSentinel };
constexpr Mode Cases[] = {Mode::OwnedExact, Mode::GlobalHit,
    Mode::OwnedAncestor, Mode::GlobalMaskRejectNext,
    Mode::AllMaskReject, Mode::OwnedSentinel,
    Mode::ParentAdvance, Mode::ParentDefaultSentinel};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve descriptor guest RAM");
        for (const Region region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit descriptor guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Services final : record::Services, ArrayResizeServices,
    CrtThreadDataServices, InvalidParameterServices
{
    GuestMemory memory;
    explicit Services(std::uint8_t* bytes)
        : memory(0, std::span<std::uint8_t>(bytes, Space)) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected manager construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected manager fallback"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected descriptor resize"); }
    std::uint64_t AllocateRecord(GuestAddress, GuestMemory&,
        std::uint64_t, std::uint64_t, std::uint64_t,
        std::uint64_t, record::FrameRegisters&) override
    { throw std::runtime_error("unexpected record allocation"); }
    std::uint64_t GetTlsValue(std::uint32_t) override
    { throw std::runtime_error("unexpected CRT TLS read"); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { throw std::runtime_error("unexpected CRT TLS write"); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected CRT getter"); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected CRT allocation"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected CRT bind"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unexpected CRT free"); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter handler"); }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter trap"); }
};
Services* active = nullptr;

void WritePair(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

void SeedName(GuestMemory& memory)
{
    memory.WriteU16(Name, 'Q');
    memory.WriteU16(Name + 2u, 0);
    memory.WriteU32(0x83215b40u, 0x50000u);
    memory.WriteU16(0x50000u + 2u * 'Q', 0);
    for (std::uint32_t i = 0; i < 256u; ++i)
        memory.WriteU32(0x832ee168u + 4u * i, 0);
    index::FrameRegisters frame{};
    const auto hash = index::HashName(memory, Name, Stack - 0x1000u, frame);
    memory.WriteU32(NameBuckets +
        ((static_cast<std::uint32_t>(hash) << 2u) & 0x3ffcu), NameNode);
    memory.WriteU32(NameNode, 42u);
    memory.WriteU16(NameNode + 16u, 'Q');
    memory.WriteU16(NameNode + 18u, 0);
}

void SeedDescriptor(GuestMemory& memory, GuestAddress address,
    std::uint32_t low, std::uint32_t owner, std::uint32_t type)
{
    WritePair(memory, address + 44u, Pair | low);
    memory.WriteU32(address + 40u, owner);
    memory.WriteU32(address + 52u, type);
}

void Seed(Window& window, Mode mode)
{
    for (const Region region : Regions)
        std::memset(window.bytes + region.start, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x83246260u, 1u); // NameLookup ready; no cold registry replay.
    SeedName(memory);
    const bool parent = mode == Mode::ParentAdvance ||
        mode == Mode::ParentDefaultSentinel;
    const bool global = mode == Mode::GlobalHit ||
        mode == Mode::GlobalMaskRejectNext ||
        mode == Mode::AllMaskReject || parent;
    const std::uint32_t owner = global ? 0u : 7u;
    const std::uint32_t low = mode == Mode::ParentAdvance ? 1u : 0u;
    SeedDescriptor(memory, Descriptor, low, owner,
        mode == Mode::OwnedAncestor ? Type : Ancestor);
    memory.WriteU32(Type + 60u, Ancestor);
    if (mode == Mode::OwnedSentinel || mode == Mode::ParentDefaultSentinel)
        memory.WriteU32(Descriptor + 4u, 0xffffffffu);
    if (mode == Mode::GlobalMaskRejectNext)
    {
        WritePair(memory, Descriptor + 8u, 0x20u);
        memory.WriteU32(Descriptor + 16u, Next);
        SeedDescriptor(memory, Next, 0u, 0u, Ancestor);
    }
    if (mode != Mode::ParentDefaultSentinel)
    {
        const std::uint32_t hash = 42u ^ low ^ owner;
        memory.WriteU32((global ? Global : Owned) +
            ((hash << 2u) & 0x7ffcu), Descriptor);
    }
    memory.WriteU32(Output, 0xfeedfaceu);
    memory.WriteU32(Output + 4u, 0xfacefeedu);
}

PPCContext Initial(Mode mode)
{
    PPCContext context{};
    context.r1.u64 = Stack;
    const bool parent = mode == Mode::ParentAdvance ||
        mode == Mode::ParentDefaultSentinel;
    const bool global = mode == Mode::GlobalHit ||
        mode == Mode::GlobalMaskRejectNext ||
        mode == Mode::AllMaskReject || parent;
    context.r3.u64 = parent ? 0xabcdef0000020000ull :
        mode == Mode::OwnedExact ? 0xabcdef0000034100ull :
        mode == Mode::OwnedAncestor ? 0xabcdef0000034100ull : 0;
    context.r4.u64 = global ? 0xabcdef0000000000ull :
        0xabcdef0000000007ull;
    context.r5.u64 = parent ? 0xabcdef0000030000ull : Pair;
    context.r6.u64 = parent ? (mode == Mode::ParentAdvance ? Pair : 0u) :
        mode == Mode::OwnedExact ? 1u : 0u;
    context.r7.u64 = 0;
    context.r8.u64 = mode == Mode::GlobalMaskRejectNext ? 0x20u :
        mode == Mode::AllMaskReject ? ~std::uint64_t{0} : 0u;
    context.lr = 0x1122334455667788ull;
    context.r13.u64 = 0xabcdef0000013000ull;
    context.r23.u64 = 0x1234567800000023ull;
    const std::array<PPCRegister*, 8> saved = {&context.r24, &context.r25,
        &context.r26, &context.r27, &context.r28, &context.r29,
        &context.r30, &context.r31};
    for (unsigned i = 0; i < saved.size(); ++i)
        saved[i]->u64 = 0x1234567800000000ull | (i + 24u);
    return context;
}

bool SameMemory(const Window& a, const Window& b, std::uint64_t& first)
{
    for (const Region region : Regions)
        for (std::size_t i = 0; i < region.size; ++i)
            if (a.bytes[std::size_t{region.start} + i] !=
                b.bytes[std::size_t{region.start} + i])
            {
                first = std::size_t{region.start} + i;
                return false;
            }
    return true;
}

bool Compare(Mode mode)
{
    Window original, recovered;
    Seed(original, mode);
    Seed(recovered, mode);
    PPCContext raw = Initial(mode);
    const PPCContext initial = raw;
    Services expected(original.bytes);
    active = &expected;
    const bool parent = mode == Mode::ParentAdvance ||
        mode == Mode::ParentDefaultSentinel;
    if (parent) __imp__sub_82400A30(raw, original.bytes);
    else __imp__sub_8229D160(raw, original.bytes);

    Services actual(recovered.bytes);
    family::FrameRegisters frame{initial.lr, initial.r13.u64,
        initial.r23.u64, initial.r24.u64, initial.r25.u64,
        initial.r26.u64, initial.r27.u64, initial.r28.u64,
        initial.r29.u64, initial.r30.u64, initial.r31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(parent ? 0x82400a30u : 0x8229d160u,
            actual.memory, actual, actual, actual, actual,
            initial.r3.u64, initial.r4.u64, initial.r5.u64,
            initial.r6.u64, initial.r7.u64, initial.r8.u64,
            initial.r1.u64, frame, result))
        throw std::runtime_error("descriptor entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && raw.r3.u64 == result &&
        raw.r1.u64 == Stack && raw.lr == frame.lr &&
        raw.r13.u64 == frame.r13 && raw.r23.u64 == frame.r23 &&
        raw.r24.u64 == frame.r24 && raw.r25.u64 == frame.r25 &&
        raw.r26.u64 == frame.r26 && raw.r27.u64 == frame.r27 &&
        raw.r28.u64 == frame.r28 && raw.r29.u64 == frame.r29 &&
        raw.r30.u64 == frame.r30 && raw.r31.u64 == frame.r31;
    if (!same)
        std::fprintf(stderr,
            "FAIL descriptor mode=%u r3 %llx/%llx lr %llx/%llx "
            "r31 %llx/%llx memory %llx:%02x/%02x\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(frame.r31),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

void OriginalNameLookup(PPCContext& ctx, std::uint8_t*)
{
    name::FrameRegisters frame{ctx.lr, ctx.r13.u64,
        ctx.r23.u64, ctx.r24.u64, ctx.r25.u64,
        ctx.r26.u64, ctx.r27.u64, ctx.r28.u64,
        ctx.r29.u64, ctx.r30.u64, ctx.r31.u64,
        ctx.r0.u64, ctx.ctr.u64, ctx.r8.u64,
        ctx.r9.u64, ctx.r10.u64};
    std::uint64_t result = 0;
    (void)name::Apply(0x82296d30u, active->memory,
        *active, *active, *active, *active,
        ctx.r3.u64, ctx.r4.u64, ctx.r5.u64,
        ctx.r6.u64, ctx.r7.u64, ctx.r1.u64, frame, result);
    ctx.r3.u64 = result;
    ctx.lr = frame.lr;
    ctx.r13.u64 = frame.r13;
    ctx.r23.u64 = frame.r23;
    const std::array<PPCRegister*, 8> target = {&ctx.r24, &ctx.r25,
        &ctx.r26, &ctx.r27, &ctx.r28, &ctx.r29, &ctx.r30, &ctx.r31};
    const std::array<const std::uint64_t*, 8> source = {&frame.r24,
        &frame.r25, &frame.r26, &frame.r27, &frame.r28, &frame.r29,
        &frame.r30, &frame.r31};
    for (unsigned i = 0; i < target.size(); ++i)
        target[i]->u64 = *source[i];
    ctx.r0.u64 = frame.r0;
    ctx.ctr.u64 = frame.ctr;
    ctx.r8.u64 = frame.r8;
    ctx.r9.u64 = frame.r9;
    ctx.r10.u64 = frame.r10;
}

int main()
{
    try
    {
        for (const Mode mode : Cases) if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes);
        family::FrameRegisters frame{};
        frame.lr = 9;
        std::uint64_t result = 7;
        if (family::Apply(0xffffffffu, service.memory,
                service, service, service, service,
                1, 2, 3, 4, 5, 6, Stack, frame, result) ||
            result != 7 || frame.lr != 9)
            throw std::runtime_error("unknown descriptor target changed state");
        std::printf("PASS metadata-descriptor-lookup %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT real original descriptor bodies and recovered NameLookup; lower dynamic services, generic lower volatile ABI, faults/MMIO excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

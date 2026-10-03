#include "lo_semantics/metadata_descriptor_membership.h"
#include "lo_semantics/registered_metadata_words.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = metadata_descriptor_membership;
constexpr std::size_t Space = std::size_t{1} << 32u;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Owner = 0x20000u;
constexpr GuestAddress Owner2 = 0x21000u;
constexpr GuestAddress Member = 0x30000u;
constexpr GuestAddress Slots = 0x40000u;
constexpr GuestAddress Slots2 = 0x41000u;
constexpr GuestAddress Owners = 0x42000u;
constexpr GuestAddress Manager = 0x50000u;
constexpr GuestAddress Manager2 = 0x51000u;
constexpr GuestAddress Vtable = 0x52000u;
constexpr GuestAddress Observer = 0x83315f78u;
constexpr GuestAddress Registry = 0x8336913cu;
constexpr GuestAddress Notify4 = 0x6007u;
constexpr GuestAddress Notify8 = 0x6017u;
constexpr GuestAddress Notify12 = 0x6027u;
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000u}, {0x83315000u, 0x2000u},
    {0x83369000u, 0x2000u}};
enum class Mode { RemoveInvalid, RemoveOccupied, RemoveLastDuplicates,
    ChangeSame, ChangeBlocked, ChangeInsertFirst, ChangeMoveAlias };
constexpr Mode Cases[] = {Mode::RemoveInvalid, Mode::RemoveOccupied,
    Mode::RemoveLastDuplicates, Mode::ChangeSame, Mode::ChangeBlocked,
    Mode::ChangeInsertFirst, Mode::ChangeMoveAlias};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve membership guest RAM");
        for (const auto region : Regions)
        {
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit membership guest RAM");
            std::memset(bytes + region.start, 0, region.size);
        }
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
void Write64(GuestMemory& memory, GuestAddress where, std::uint64_t value)
{
    memory.WriteU32(where, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(where + 4u, static_cast<std::uint32_t>(value));
}
struct Services final : family::Services
{
    GuestMemory memory;
    Mode mode;
    std::vector<std::array<std::uint64_t, 6>> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected membership manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected membership resize"); }
    std::uint64_t Dispatch(GuestAddress target, std::uint64_t receiver,
        std::uint64_t argument, std::uint64_t sp,
        family::FrameRegisters& frame) override
    {
        events.push_back({target, receiver, argument, sp, frame.lr, frame.ctr});
        if (target != (Notify4 & ~3u) && target != (Notify8 & ~3u) &&
            target != (Notify12 & ~3u))
            throw std::runtime_error("incorrect membership observer target");
        if (target == (Notify12 & ~3u))
            memory.WriteU32(Observer, Manager2);
        if (target == (Notify8 & ~3u) && mode == Mode::ChangeMoveAlias)
        {
            // This is the nested removal's actual saved r31. Restoring it
            // redirects the parent's following slot/count reads to Owner2.
            Write64(memory, static_cast<GuestAddress>(sp + 112u),
                0xabcdef0000021000ull);
        }
        if (target == (Notify4 & ~3u) ||
            (target == (Notify8 & ~3u) && mode == Mode::RemoveLastDuplicates))
        {
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 8u, 0xcafebabeu);
            Write64(memory, static_cast<GuestAddress>(Stack) - 16u,
                0x1122334455667788ull);
        }
        return 0xaabbccdd00000000ull | events.size();
    }
};
Services* active = nullptr;

bool IsChange(Mode mode)
{
    return mode == Mode::ChangeSame || mode == Mode::ChangeBlocked ||
        mode == Mode::ChangeInsertFirst || mode == Mode::ChangeMoveAlias;
}
void Seed(Services& services)
{
    auto& m = services.memory;
    const auto mode = services.mode;
    m.WriteU32(Observer, Manager);
    m.WriteU32(Manager, Vtable);
    m.WriteU32(Manager2, Vtable);
    m.WriteU32(Vtable + 4u, Notify4);
    m.WriteU32(Vtable + 8u, Notify8);
    m.WriteU32(Vtable + 12u, Notify12);
    m.WriteU32(Owner + 112u, Slots);
    m.WriteU32(Owner + 116u, 4u);
    m.WriteU32(Owner + 124u, mode == Mode::ChangeInsertFirst ? 0u :
        mode == Mode::RemoveOccupied ? 2u : 1u);
    m.WriteU32(Owner + 148u, mode == Mode::ChangeBlocked ? 4u : 0u);
    m.WriteU32(Owner2 + 112u, Slots2);
    m.WriteU32(Owner2 + 116u, 4u);
    m.WriteU32(Member + 36u, mode == Mode::RemoveInvalid ||
        mode == Mode::ChangeInsertFirst ? 0xffffffffu : 0u);
    m.WriteU32(Member + 40u, Owner);
    if (mode != Mode::ChangeInsertFirst) m.WriteU32(Slots, Member);
    m.WriteU32(Registry, Owners);
    m.WriteU32(Registry + 4u, mode == Mode::RemoveLastDuplicates ||
        mode == Mode::ChangeMoveAlias ? 3u : 0u);
    m.WriteU32(Registry + 8u, 4u);
    m.WriteU32(Owners, Owner);
    m.WriteU32(Owners + 4u, 0x22222u);
    m.WriteU32(Owners + 8u, Owner);
}
family::FrameRegisters FrameFrom(const PPCContext& ctx)
{
    return {ctx.lr, ctx.r27.u64, ctx.r28.u64, ctx.r29.u64,
        ctx.r30.u64, ctx.r31.u64, ctx.ctr.u64};
}

bool Compare(Mode mode)
{
    Window original, recovered;
    Services expected(original.bytes, mode), actual(recovered.bytes, mode);
    Seed(expected);
    Seed(actual);
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000000000ull | (IsChange(mode) ? Member : Owner);
    raw.r4.u64 = IsChange(mode) ? 0xabcdef0000000000ull |
        (mode == Mode::ChangeSame ? 0u : 1u) :
        0xabcdef0000000000ull | Member;
    raw.lr = 0x9988776655443322ull;
    raw.r27.u64 = 0xabcdef0000000027ull;
    raw.r28.u64 = 0xabcdef0000000028ull;
    raw.r29.u64 = 0xabcdef0000000029ull;
    raw.r30.u64 = 0xabcdef0000000030ull;
    raw.r31.u64 = 0xabcdef0000000031ull;
    raw.ctr.u64 = 0xabcdef0000000042ull;
    const PPCContext initial = raw;
    active = &expected;
    if (IsChange(mode)) __imp__sub_823AAE00(raw, original.bytes);
    else __imp__sub_824080A8(raw, original.bytes);
    auto frame = FrameFrom(initial);
    std::uint64_t result = 0;
    if (!family::Apply(IsChange(mode) ? 0x823aae00u : 0x824080a8u,
            actual.memory, actual, initial.r3.u64, initial.r4.u64,
            Stack, frame, result))
        throw std::runtime_error("membership entry missing");
    bool same = expected.events == actual.events && raw.r3.u64 == result &&
        raw.r1.u64 == Stack && raw.lr == frame.lr && raw.r27.u64 == frame.r27 &&
        raw.r28.u64 == frame.r28 && raw.r29.u64 == frame.r29 &&
        raw.r30.u64 == frame.r30 && raw.r31.u64 == frame.r31 &&
        raw.ctr.u64 == frame.ctr;
    std::uint64_t first = 0;
    for (const auto region : Regions)
    {
        if (std::memcmp(original.bytes + region.start,
                recovered.bytes + region.start, region.size) != 0)
        {
            same = false;
            for (std::size_t i = 0; i < region.size; ++i)
                if (original.bytes[region.start + i] != recovered.bytes[region.start + i])
                { first = region.start + i; break; }
            break;
        }
    }
    if (!same)
        std::fprintf(stderr, "FAIL membership mode=%u r3=%llx/%llx first=%llx events=%zu/%zu\n",
            static_cast<unsigned>(mode), static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result), static_cast<unsigned long long>(first),
            expected.events.size(), actual.events.size());
    return same;
}
} // namespace

PPC_FUNC(sub_82298AF8)
{
    RemoveArrayRange(active->memory, *active, ctx.r3.u32,
        ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r1.u32 - 128u);
}
PPC_FUNC(sub_825F41E8)
{
    ctx.r3.u64 = registered_metadata_words::AppendMetadataWord(active->memory,
        *active, ctx.r3.u32, ctx.r4.u32);
}
void OriginalMembershipCall(PPCContext& ctx, std::uint8_t*, std::uint32_t target)
{
    auto frame = FrameFrom(ctx);
    ctx.r3.u64 = active->Dispatch(target, ctx.r3.u64, ctx.r4.u64, ctx.r1.u64, frame);
    ctx.lr = frame.lr;
    ctx.r27.u64 = frame.r27;
    ctx.r28.u64 = frame.r28;
    ctx.r29.u64 = frame.r29;
    ctx.r30.u64 = frame.r30;
    ctx.r31.u64 = frame.r31;
    ctx.ctr.u64 = frame.ctr;
}

int main()
{
    try
    {
        for (auto mode : Cases) if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::RemoveInvalid);
        family::FrameRegisters frame{1, 2, 3, 4, 5, 6, 7};
        std::uint64_t result = 8;
        if (family::Apply(0xffffffffu, service.memory, service, 9, 10,
                Stack, frame, result) || result != 8 || frame.lr != 1 ||
            frame.r27 != 2 || frame.r28 != 3 || frame.r29 != 4 ||
            frame.r30 != 5 || frame.r31 != 6 || frame.ctr != 7 ||
            !service.events.empty() || spare.bytes[Owner] != 0)
            throw std::runtime_error("unknown membership entry changed state");
        std::printf("PASS metadata-descriptor-membership %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT dynamic observer internals and existing lower generic ABI/frame/MMIO external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

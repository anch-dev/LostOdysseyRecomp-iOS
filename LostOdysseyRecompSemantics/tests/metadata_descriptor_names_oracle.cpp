#include "lo_semantics/metadata_descriptor_names.h"
#include "lo_semantics/metadata_utf16_buffer.h"
#include "lo_semantics/registered_metadata_string.h"

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
namespace family = lo::semantic::gpu::metadata_descriptor_names;
namespace slice = lo::semantic::gpu::metadata_utf16_slice;
namespace buffer = lo::semantic::gpu::metadata_utf16_buffer;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Destination = 0x20000u;
constexpr GuestAddress RedirectedDestination = 0x21000u;
constexpr GuestAddress Owner = 0x23000u;
constexpr GuestAddress Parent = 0x24000u;
constexpr GuestAddress Stop = 0x25000u;
constexpr GuestAddress Packed = 0x26000u;
constexpr GuestAddress RecordTable = 0x10000u;
constexpr GuestAddress Record0 = 0x11000u;
constexpr GuestAddress Record1 = 0x12000u;
constexpr GuestAddress Manager = 0x50000u;
constexpr GuestAddress Vtable = 0x50100u;
constexpr GuestAddress ResizeMethod = 0x7300u;
constexpr GuestAddress ReleaseMethod = 0x7400u;
enum class Mode { PackedZero, PackedSuffix, ParentStop, ParentLeaf,
    ParentChain, ParentAlias };
struct Case { GuestAddress address; PPCFunc* original; Mode mode; };
constexpr Case Cases[] = {
    {0x822a9668u, __imp__sub_822A9668, Mode::PackedZero},
    {0x822a9668u, __imp__sub_822A9668, Mode::PackedSuffix},
    {0x823ac8e0u, __imp__sub_823AC8E0, Mode::ParentStop},
    {0x823ac8e0u, __imp__sub_823AC8E0, Mode::ParentLeaf},
    {0x823ac8e0u, __imp__sub_823AC8E0, Mode::ParentChain},
    {0x823ac8e0u, __imp__sub_823AC8E0, Mode::ParentAlias},
};
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000}, {0x82000000u, 0x1b0000u},
    {0x8330b000u, 0x1000}, {0x83369000u, 0x1000}};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve descriptor-name RAM");
        for (const Region region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit descriptor-name RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    char kind;
    std::array<std::uint64_t, 7> args;
    bool operator==(const Event&) const = default;
};
struct Services final : ArrayResizeServices, ManagerFacadeServices,
    slice::VirtualServices
{
    GuestMemory memory;
    Mode mode;
    GuestAddress next = 0x40000u;
    bool aliased = false;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected init virtual method"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected standalone allocation"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({'Z', {method, manager, old, bytes, argument}});
        if (method != ResizeMethod || manager != Manager ||
            argument != 8u || bytes > 0x1000u)
            throw std::runtime_error("incorrect name resize boundary");
        if (bytes == 0u) return 0;
        const GuestAddress fresh = next;
        next += 0x1000u;
        if (old != 0u)
            for (std::uint32_t i = 0; i < 64u; ++i)
                memory.WriteU8(fresh + i, memory.ReadU8(old + i));
        return fresh;
    }
    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t buffer_register) override
    {
        events.push_back({'F', {method, manager, buffer_register}});
        if (method != ReleaseMethod || manager != Manager)
            throw std::runtime_error("incorrect name release boundary");
        return 0xabcdef0000000000ull;
    }
    std::uint64_t CallMethod(GuestAddress method, GuestMemory& caller_memory,
        std::uint64_t r3, std::uint64_t r4, std::uint64_t r5,
        std::uint64_t r6, std::uint64_t caller_sp,
        slice::FrameRegisters& frame) override
    {
        events.push_back({'V', {method, r3, r4, r5, r6,
            caller_sp, frame.lr}});
        if (&caller_memory != &memory || r3 != Manager ||
            (method != ResizeMethod && method != ReleaseMethod))
            throw std::runtime_error("incorrect name virtual boundary");
        if (mode == Mode::ParentAlias && !aliased)
        {
            // PackedName restores its saved r30 into OwnerName's live r30.
            const GuestAddress slot = static_cast<GuestAddress>(Stack) - 184u;
            memory.WriteU32(slot, 0x12345678u);
            memory.WriteU32(slot + 4u, RedirectedDestination);
            aliased = true;
        }
        return method == ResizeMethod ? 0u : 0xabcdef00000000ffull;
    }
};
Services* active = nullptr;

void WriteText(GuestMemory& memory, GuestAddress address, const char* text)
{
    std::size_t i = 0;
    do { memory.WriteU16(address + static_cast<GuestAddress>(i * 2u),
        static_cast<unsigned char>(text[i])); }
    while (text[i++] != '\0');
}
void Write64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}
std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}
void Seed(Window& window, Mode mode)
{
    for (const Region region : Regions)
        std::memset(window.bytes + region.start, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(Vtable + 12u, ReleaseMethod | 3u);
    memory.WriteU32(0x833690d0u, RecordTable);
    memory.WriteU32(RecordTable, Record0);
    memory.WriteU32(RecordTable + 4u, Record1);
    WriteText(memory, Record0 + 16u, "Hero");
    WriteText(memory, Record1 + 16u, "Root");
    WriteText(memory, 0x821a83d0u, "");
    WriteText(memory, 0x8200375cu, "#");
    WriteText(memory, 0x82000e00u, "/");
    WriteText(memory, 0x8218d870u, "unnamed");
    WriteText(memory, 0x8201f9f4u, "stopped");
    constexpr std::array<GuestAddress, 10> digits = {
        0x820009f8u, 0x82000b90u, 0x821a6b78u, 0x821a83bcu,
        0x82000cd4u, 0x82000cd0u, 0x821a83c0u, 0x821a83c4u,
        0x82000cc4u, 0x82000cccu};
    for (unsigned i = 0; i < digits.size(); ++i)
    {
        const char text[] = {static_cast<char>('0' + i), 0};
        WriteText(memory, digits[i], text);
    }
    memory.WriteU32(Packed, 0u);
    memory.WriteU32(Packed + 4u, mode == Mode::PackedSuffix ? 2u : 0u);
    memory.WriteU32(Owner + 4u, 0u);
    memory.WriteU32(Owner + 44u, 0u);
    memory.WriteU32(Owner + 48u, mode == Mode::ParentAlias ? 2u : 0u);
    memory.WriteU32(Parent + 4u, 0u);
    memory.WriteU32(Parent + 44u, 1u);
    memory.WriteU32(Parent + 48u, 0u);
    memory.WriteU32(Owner + 40u,
        mode == Mode::ParentChain ? Parent : 0u);
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
PPCContext Initial(const Case& test)
{
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000020000ull;
    raw.r4.u64 = test.address == 0x822a9668u ?
        0x1234567800026000ull :
        (test.mode == Mode::ParentStop ?
            0x1234567800025000ull : 0x1234567800023000ull);
    raw.r5.u64 = test.mode == Mode::ParentStop ? Stop :
        0x1234567800025000ull;
    raw.lr = 0x8877665544332211ull;
    const std::array<PPCRegister*, 7> saved = {&raw.r25, &raw.r26,
        &raw.r27, &raw.r28, &raw.r29, &raw.r30, &raw.r31};
    for (unsigned i = 0; i < saved.size(); ++i)
        saved[i]->u64 = 0x1234567800000000ull | (i + 25u);
    return raw;
}
bool Compare(const Case& test)
{
    Window original, recovered;
    Seed(original, test.mode);
    Seed(recovered, test.mode);
    PPCContext raw = Initial(test);
    const PPCContext initial = raw;
    Services expected(original.bytes, test.mode);
    active = &expected;
    test.original(raw, original.bytes);
    Services actual(recovered.bytes, test.mode);
    family::FrameRegisters frame{initial.lr, initial.r25.u64,
        initial.r26.u64, initial.r27.u64, initial.r28.u64,
        initial.r29.u64, initial.r30.u64, initial.r31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(test.address, actual.memory, actual, actual,
            actual, initial.r3.u64, initial.r4.u64, initial.r5.u64,
            initial.r1.u64, frame, result))
        throw std::runtime_error("descriptor-name entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && expected.events == actual.events &&
        raw.r3.u64 == result && raw.r1.u64 == Stack && raw.lr == frame.lr &&
        raw.r25.u64 == frame.r25 && raw.r26.u64 == frame.r26 &&
        raw.r27.u64 == frame.r27 && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31 && expected.aliased == actual.aliased;
    if (!same)
        std::fprintf(stderr,
            "FAIL names mode=%u r3 %llx/%llx lr %llx/%llx "
            "events %zu/%zu first %llx:%02x/%02x\n",
            static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

PPC_FUNC(sub_8232CED8)
{
    slice::FrameRegisters frame{ctx.lr, ctx.r25.u64, ctx.r26.u64,
        ctx.r27.u64, ctx.r28.u64, ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
    std::uint64_t result = 0;
    (void)slice::Apply(0x8232ced8u, active->memory, *active, *active,
        *active, ctx.r3.u64, ctx.r4.u64, ctx.r5.u64, ctx.r6.u64,
        ctx.r1.u64, frame, result);
    ctx.r3.u64 = result;
    ctx.lr = frame.lr;
    ctx.r25.u64 = frame.r25; ctx.r26.u64 = frame.r26;
    ctx.r27.u64 = frame.r27; ctx.r28.u64 = frame.r28;
    ctx.r29.u64 = frame.r29; ctx.r30.u64 = frame.r30;
    ctx.r31.u64 = frame.r31;
}
PPC_FUNC(sub_8229C8B0)
{
    const GuestAddress sp = ctx.r1.u32;
    active->memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    Write64(active->memory, sp - 24u, ctx.r30.u64);
    Write64(active->memory, sp - 16u, ctx.r31.u64);
    active->memory.WriteU32(sp - 112u, sp);
    ctx.r3.u64 = registered_metadata_string::InitializeString(
        active->memory, *active, ctx.r3.u64, ctx.r4.u64, sp);
    ctx.lr = active->memory.ReadU32(sp - 8u);
    ctx.r30.u64 = Read64(active->memory, sp - 24u);
    ctx.r31.u64 = Read64(active->memory, sp - 16u);
}
PPC_FUNC(sub_8229F5E0)
{
    const GuestAddress sp = ctx.r1.u32;
    active->memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    Write64(active->memory, sp - 24u, ctx.r30.u64);
    Write64(active->memory, sp - 16u, ctx.r31.u64);
    active->memory.WriteU32(sp - 112u, sp);
    ctx.r3.u64 = registered_metadata_string::AssignString(
        active->memory, *active, ctx.r3.u64, ctx.r4.u64, sp);
    ctx.lr = active->memory.ReadU32(sp - 8u);
    ctx.r30.u64 = Read64(active->memory, sp - 24u);
    ctx.r31.u64 = Read64(active->memory, sp - 16u);
}
void OriginalBufferCall(GuestAddress address, PPCContext& ctx)
{
    buffer::FrameRegisters frame{ctx.lr, ctx.r28.u64, ctx.r29.u64,
        ctx.r30.u64, ctx.r31.u64};
    std::uint64_t result = 0;
    (void)buffer::Apply(address, active->memory, *active, *active,
        ctx.r3.u64, ctx.r4.u64, ctx.r5.u64, ctx.r1.u64,
        frame, result);
    ctx.r3.u64 = result;
    ctx.lr = frame.lr;
    ctx.r28.u64 = frame.r28; ctx.r29.u64 = frame.r29;
    ctx.r30.u64 = frame.r30; ctx.r31.u64 = frame.r31;
}
PPC_FUNC(sub_8232D378) { OriginalBufferCall(0x8232d378u, ctx); }
PPC_FUNC(sub_8232D418) { OriginalBufferCall(0x8232d418u, ctx); }
PPC_FUNC(sub_82298938)
{
    const GuestAddress sp = ctx.r1.u32;
    active->memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    Write64(active->memory, sp - 16u, ctx.r31.u64);
    active->memory.WriteU32(sp - 96u, sp);
    ctx.r3.u64 = ResetTwoByteArray(active->memory, *active,
        ctx.r3.u32, sp);
    ctx.lr = active->memory.ReadU32(sp - 8u);
    ctx.r31.u64 = Read64(active->memory, sp - 16u);
}
PPC_FUNC(sub_822A9668) { __imp__sub_822A9668(ctx, base); }
PPC_FUNC(sub_823AC8E0) { __imp__sub_823AC8E0(ctx, base); }

int main()
{
    try
    {
        for (const Case& test : Cases) if (!Compare(test)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::PackedZero);
        family::FrameRegisters frame{9u, 10u, 11u, 12u, 13u,
            14u, 15u, 16u};
        std::uint64_t result = 7u;
        spare.bytes[Destination] = 0x5au;
        if (family::Apply(0xffffffffu, service.memory, service, service,
                service, 1, 2, 3, Stack, frame, result) || result != 7u ||
            frame.lr != 9u || frame.r31 != 16u ||
            spare.bytes[Destination] != 0x5au || !service.events.empty())
            throw std::runtime_error("unknown name entry changed state");
        std::printf("PASS metadata-descriptor-names %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT accepted UTF16 and manager models; virtual method internals, generic lower ABI/frame, MMIO, cyclic owners external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

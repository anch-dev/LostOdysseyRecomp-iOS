#include "lo_semantics/metadata_name_dispatch.h"
#include "lo_semantics/metadata_name_index.h"

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
namespace family = lo::semantic::gpu::metadata_name_dispatch;
namespace index = lo::semantic::gpu::metadata_name_index;
namespace record = lo::semantic::gpu::metadata_name_record;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Source = 0x10000u;
constexpr GuestAddress Object = 0x20000u;
constexpr GuestAddress Node = 0x30000u;
constexpr GuestAddress Receiver = 0x60000u;
constexpr GuestAddress Vtable = 0x61000u;
constexpr GuestAddress Buckets = 0x832ee568u;
constexpr GuestAddress Ready = 0x83246260u;
constexpr GuestAddress Method = 0x7345u;
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000}, {0x83214000u, 0x2000},
    {0x83246000u, 0x1000}, {0x832ee000u, 0x6000}};
enum class Mode { Found, ZeroReceiver, SavedAlias };
constexpr Mode Cases[] = {Mode::Found, Mode::ZeroReceiver,
    Mode::SavedAlias};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve dispatch guest RAM");
        for (const Region region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit dispatch guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    std::array<std::uint64_t, 8> values;
    bool operator==(const Event&) const = default;
};

struct Services final : record::Services, ArrayResizeServices,
    CrtThreadDataServices, InvalidParameterServices, family::DispatchServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t AllocateRecord(GuestAddress, GuestMemory&, std::uint64_t,
        std::uint64_t, std::uint64_t, std::uint64_t,
        record::FrameRegisters&) override
    { throw std::runtime_error("unexpected record allocation"); }
    void InitializeManager() override
    { throw std::runtime_error("unexpected array initialization"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected array resize"); }
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
    void CallMethod(GuestAddress target, GuestMemory& caller_memory,
        std::uint64_t receiver, std::uint64_t owner,
        std::uint64_t sp, std::uint64_t lr,
        family::FrameRegisters& frame) override
    {
        events.push_back({{target, receiver, owner, sp, lr,
            frame.r31, frame.r30, frame.ctr}});
        const GuestAddress object = mode == Mode::SavedAlias ?
            static_cast<GuestAddress>(Stack) - 32u : Object;
        const std::uint64_t expected_owner =
            0xabcdef0000000000ull | object;
        if (&caller_memory != &memory || target != (Method & ~3u) ||
            receiver != (mode == Mode::ZeroReceiver ? 0u : Receiver) ||
            owner != expected_owner || sp != Stack - 112u ||
            lr != 0x825e7688u || frame.ctr != Method)
            throw std::runtime_error("incorrect dispatch callback contract");
        if (mode == Mode::SavedAlias)
        {
            frame.r31 = 0x1122334400020000ull;
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 16u,
                0x778899aau);
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 12u,
                0xbbccddee);
        }
    }
};
Services* active = nullptr;

void WriteText(GuestMemory& memory, GuestAddress where, const char* text)
{
    std::size_t index = 0;
    do
    {
        memory.WriteU16(where + static_cast<GuestAddress>(index * 2u),
            static_cast<std::uint8_t>(text[index]));
    } while (text[index++] != '\0');
}

void Initialize(Window& window, Mode mode)
{
    for (const Region region : Regions)
        std::memset(window.bytes + region.start, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Ready, 1u);
    for (std::uint32_t i = 0; i < 256u; ++i)
        memory.WriteU32(0x832ee168u + 4u * i,
            0x87654321u ^ (i * 0x1234567u));
    const GuestAddress object = mode == Mode::SavedAlias ?
        static_cast<GuestAddress>(Stack) - 32u : Object;
    memory.WriteU32(object + 40u,
        mode == Mode::ZeroReceiver ? 0u : Receiver);
    memory.WriteU32(mode == Mode::ZeroReceiver ? 0u : Receiver, Vtable);
    memory.WriteU32(Vtable + 264u, Method);
    WriteText(memory, Source, mode == Mode::Found ? "A" : "");
    if (mode == Mode::Found)
    {
        index::FrameRegisters hash_frame{};
        const auto hash = index::HashName(memory, Source,
            Stack - 0x1000u, hash_frame);
        const GuestAddress bucket = Buckets +
            ((static_cast<std::uint32_t>(hash) << 2u) & 0x3ffcu);
        memory.WriteU32(bucket, Node);
        memory.WriteU32(Node, 0x55aau);
        WriteText(memory, Node + 16u, "a");
    }
}

family::FrameRegisters FrameFrom(const PPCContext& context)
{
    return {context.lr, context.r13.u64, context.r23.u64,
        context.r24.u64, context.r25.u64, context.r26.u64,
        context.r27.u64, context.r28.u64, context.r29.u64,
        context.r30.u64, context.r31.u64, context.r0.u64,
        context.ctr.u64, context.r8.u64, context.r9.u64,
        context.r10.u64};
}

void CopyFrame(PPCContext& context, const family::FrameRegisters& frame)
{
    context.lr = frame.lr;
    context.r13.u64 = frame.r13;
    context.r23.u64 = frame.r23;
    context.r24.u64 = frame.r24;
    context.r25.u64 = frame.r25;
    context.r26.u64 = frame.r26;
    context.r27.u64 = frame.r27;
    context.r28.u64 = frame.r28;
    context.r29.u64 = frame.r29;
    context.r30.u64 = frame.r30;
    context.r31.u64 = frame.r31;
    context.r0.u64 = frame.r0;
    context.ctr.u64 = frame.ctr;
    context.r8.u64 = frame.r8;
    context.r9.u64 = frame.r9;
    context.r10.u64 = frame.r10;
}

bool SameMemory(const Window& a, const Window& b,
    std::uint64_t& first)
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
    Initialize(original, mode);
    Initialize(recovered, mode);
    PPCContext raw{};
    const GuestAddress object = mode == Mode::SavedAlias ?
        static_cast<GuestAddress>(Stack) - 32u : Object;
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000000000ull | object;
    raw.r4.u64 = 0x9988776600000004ull;
    raw.r5.u64 = 0x556677880000a5a5ull;
    raw.r6.u64 = 0x8877665500000000ull | Source;
    raw.r7.u64 = 0x1122334455667788ull;
    raw.lr = 0x9988776655443322ull;
    raw.r13.u64 = 0xabcdef0000013000ull;
    raw.r23.u64 = 0x1234567800000023ull;
    raw.r24.u64 = 0x1234567800000024ull;
    raw.r25.u64 = 0x1234567800000025ull;
    raw.r26.u64 = 0x1234567800000026ull;
    raw.r27.u64 = 0x1234567800000027ull;
    raw.r28.u64 = 0x1234567800000028ull;
    raw.r29.u64 = 0x1234567800000029ull;
    raw.r30.u64 = 0x1234567800000030ull;
    raw.r31.u64 = 0x1234567800000031ull;
    raw.r0.u64 = 0x1234567800000000ull;
    raw.ctr.u64 = 0x12345678000000ccull;
    raw.r8.u64 = 0x1234567800000008ull;
    raw.r9.u64 = 0x1234567800000009ull;
    raw.r10.u64 = 0x1234567800000010ull;
    const PPCContext initial = raw;
    Services expected(original.bytes, mode);
    active = &expected;
    __imp__sub_825E7600(raw, original.bytes);

    Services actual(recovered.bytes, mode);
    auto frame = FrameFrom(initial);
    std::uint64_t result = 0;
    if (!family::Apply(0x825e7600u, actual.memory, actual, actual,
            actual, actual, actual, initial.r3.u64, initial.r4.u64,
            initial.r5.u64, initial.r6.u64, initial.r7.u64,
            initial.r1.u64, frame, result))
        throw std::runtime_error("dispatch entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && expected.events == actual.events &&
        raw.r3.u64 == result && raw.r1.u64 == Stack &&
        raw.lr == frame.lr && raw.r13.u64 == frame.r13 &&
        raw.r23.u64 == frame.r23 && raw.r24.u64 == frame.r24 &&
        raw.r25.u64 == frame.r25 && raw.r26.u64 == frame.r26 &&
        raw.r27.u64 == frame.r27 && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31 && raw.r0.u64 == frame.r0 &&
        raw.ctr.u64 == frame.ctr && raw.r8.u64 == frame.r8 &&
        raw.r9.u64 == frame.r9 && raw.r10.u64 == frame.r10;
    if (!same)
        std::fprintf(stderr,
            "FAIL dispatch mode=%u r3 %llx/%llx lr %llx/%llx "
            "r31 %llx/%llx events %zu/%zu first %llx:%02x/%02x\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(frame.r31),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

void DispatchOriginalLookup(PPCContext& context, std::uint8_t*)
{
    auto frame = FrameFrom(context);
    std::uint64_t result = 0;
    (void)metadata_name_lookup::Apply(0x82296d30u, active->memory,
        *active, *active, *active, *active, context.r3.u64,
        context.r4.u64, context.r5.u64, context.r6.u64,
        context.r7.u64, context.r1.u64, frame, result);
    CopyFrame(context, frame);
    context.r3.u64 = result;
}

void DispatchOriginalIndirect(PPCContext& context, std::uint8_t*,
    std::uint32_t target)
{
    auto frame = FrameFrom(context);
    active->CallMethod(target, active->memory, context.r3.u64,
        context.r4.u64, context.r1.u64, context.lr, frame);
    CopyFrame(context, frame);
}

int main()
{
    try
    {
        for (const Mode mode : Cases) if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Found);
        family::FrameRegisters frame{};
        frame.lr = 9u;
        std::uint64_t result = 7u;
        if (family::Apply(0xffffffffu, service.memory,
                service, service, service, service, service,
                1, 2, 3, 4, 5, Stack, frame, result) ||
            result != 7u || frame.lr != 9u || !service.events.empty())
            throw std::runtime_error("unknown dispatch target changed state");
        std::printf("PASS metadata-name-dispatch %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT lookup uses accepted bounded model; dynamic vtable+264 target and generic lower volatile ABI/MMIO faults external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

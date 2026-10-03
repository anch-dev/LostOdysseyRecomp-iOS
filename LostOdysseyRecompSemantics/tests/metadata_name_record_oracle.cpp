#include "lo_semantics/metadata_name_record.h"
#include "lo_semantics/registered_metadata_composed.h"
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
namespace family = lo::semantic::gpu::metadata_name_record;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800060000ull;
constexpr GuestAddress Source = 0x10000u;
constexpr GuestAddress OtherSource = 0x11000u;
constexpr GuestAddress Record = 0x20000u;
constexpr GuestAddress Manager = 0x30000u;
constexpr GuestAddress ManagerTable = 0x30100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82456780u;
constexpr GuestAddress FirstMethod = 0x82456790u;
constexpr GuestAddress SecondMethod = 0x824567a0u;

enum class Mode { Ready, MutateLive, InitializeManager, LowZero, StackAlias };
constexpr Mode Cases[] = {Mode::Ready, Mode::MutateLive,
    Mode::InitializeManager, Mode::LowZero, Mode::StackAlias};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x80000, MEM_COMMIT,
                PAGE_READWRITE) || !VirtualAlloc(bytes + 0x8330b000u,
                0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve/commit name record guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 8> args;
    bool operator==(const Event&) const = default;
};

struct Services final : family::Services
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}

    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        events.push_back({'R', {bytes}});
        if (mode != Mode::InitializeManager || bytes != 0x48decu)
            throw std::runtime_error("unexpected manager raw allocation");
        return 0xabcdef0000030000ull;
    }
    std::uint64_t ConstructPrimary(std::uint64_t allocation) override
    {
        events.push_back({'P', {allocation}});
        if (allocation != 0xabcdef0000030000ull)
            throw std::runtime_error("wrong manager construction input");
        return allocation;
    }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback manager construction"); }
    std::uint64_t CallMethod(GuestAddress method,
        std::uint64_t receiver) override
    {
        events.push_back({'M', {method, receiver}});
        if (static_cast<GuestAddress>(receiver) != Manager ||
            (method != FirstMethod && method != SecondMethod))
            throw std::runtime_error("unexpected manager init method");
        return method == FirstMethod ? 1u : 0u;
    }
    std::uint64_t AllocateRecord(GuestAddress method, GuestMemory& caller_memory,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment, std::uint64_t sp,
        family::FrameRegisters& frame) override
    {
        events.push_back({'A', {method, manager, bytes, alignment, sp,
            frame.lr, frame.r29, frame.r30}});
        if (&caller_memory != &memory || method != AllocateMethod ||
            manager != Manager || bytes != 22u || alignment != 8u ||
            sp != Stack - 128u || frame.lr != 0x823f7b60u)
            throw std::runtime_error("incorrect record allocator boundary");
        if (mode == Mode::MutateLive)
        {
            frame.r29 = 0xaabbccdd11223344ull;
            frame.r28 = 0x1122334455667788ull;
            frame.r30 = 0xabcdef0000011000ull;
            memory.WriteU16(OtherSource, 'Z');
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 8u,
                0x87654321u);
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 48u,
                0x12345678u);
        }
        if (mode == Mode::LowZero)
            return 0xabcdef0000000000ull;
        if (mode == Mode::StackAlias)
            return 0xabcdef000005ffe0ull; // caller_sp - 32
        return 0xabcdef0000020000ull;
    }
};

Services* active = nullptr;

void Initialize(Window& window, Mode mode)
{
    std::memset(window.bytes, 0xbd, 0x80000);
    std::memset(window.bytes + 0x8330b000u, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal, mode == Mode::InitializeManager ? 0u : Manager);
    memory.WriteU32(Manager, ManagerTable);
    memory.WriteU32(ManagerTable + 4u, AllocateMethod | 3u);
    memory.WriteU32(ManagerTable + 56u, SecondMethod | 3u);
    memory.WriteU32(ManagerTable + 60u, FirstMethod | 3u);
    memory.WriteU16(Source, 'A');
    memory.WriteU16(Source + 2u, 'B');
    memory.WriteU16(Source + 4u, 0);
    memory.WriteU16(OtherSource, 'Q');
    memory.WriteU16(OtherSource + 2u, 'R');
    memory.WriteU16(OtherSource + 4u, 0);
}

bool SameMemory(const Window& a, const Window& b,
    std::uint64_t& first)
{
    for (const std::uint64_t start : {0ull, 0x8330b000ull})
    {
        const std::size_t size = start == 0 ? 0x80000u : 0x1000u;
        for (std::size_t i = 0; i < size; ++i)
            if (a.bytes[start + i] != b.bytes[start + i])
            {
                first = start + i;
                return false;
            }
    }
    return true;
}

bool Compare(Mode mode)
{
    Window original, recovered;
    Initialize(original, mode);
    Initialize(recovered, mode);
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000010000ull;
    raw.r4.u64 = 0x1122334455667788ull;
    raw.r5.u64 = 0x33445566778899aaull;
    raw.r6.u64 = 0xaabbccdd01020304ull;
    raw.lr = 0x1122334455667788ull;
    raw.r27.u64 = 0xabcdef0000000027ull;
    raw.r28.u64 = 0xabcdef0000000028ull;
    raw.r29.u64 = 0xabcdef0000000029ull;
    raw.r30.u64 = 0xabcdef0000000030ull;
    raw.r31.u64 = 0xabcdef0000000031ull;
    PPCContext state = raw;
    Services expected(original.bytes, mode);
    active = &expected;
    __imp__sub_823F7B08(raw, original.bytes);

    Services actual(recovered.bytes, mode);
    family::FrameRegisters frame{state.lr, state.r27.u64, state.r28.u64,
        state.r29.u64, state.r30.u64, state.r31.u64};
    std::uint64_t result = 0;
    if (!family::Apply(0x823f7b08u, actual.memory, actual, state.r3.u64,
            state.r4.u64, state.r5.u64, state.r6.u64, state.r1.u64,
            frame, result))
        throw std::runtime_error("name record entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = raw.r3.u64 == result && raw.r1.u64 == state.r1.u64 &&
        raw.lr == frame.lr && raw.r27.u64 == frame.r27 &&
        raw.r28.u64 == frame.r28 && raw.r29.u64 == frame.r29 &&
        raw.r30.u64 == frame.r30 && raw.r31.u64 == frame.r31 &&
        expected.events == actual.events && same_memory;
    if (!same)
        std::fprintf(stderr,
            "FAIL name-record mode=%u r3 %llx/%llx lr %llx/%llx "
            "r27 %llx/%llx events %zu/%zu first %llx:%02x/%02x\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r27.u64),
            static_cast<unsigned long long>(frame.r27),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

void SaveGprs(PPCContext& context, std::uint8_t* base)
{
    for (unsigned reg = 27; reg <= 31; ++reg)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - reg),
            (&context.r27)[reg - 27u].u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}
void RestoreGprs(PPCContext& context, std::uint8_t* base)
{
    for (unsigned reg = 27; reg <= 31; ++reg)
        (&context.r27)[reg - 27u].u64 =
            PPC_LOAD_U64(context.r1.u32 - 8u * (33u - reg));
    context.lr = PPC_LOAD_U32(context.r1.u32 - 8u);
}

void OriginalDirectCall(PPCContext& context, std::uint8_t* base,
    GuestAddress target)
{
    switch (target)
    {
    case 0x82296830u:
        context.r3.u64 = registered_metadata_string::Utf16Length(
            active->memory, context.r3.u64);
        return;
    case 0x827c5f38u:
    {
        const GuestAddress sp = context.r1.u32;
        PPC_STORE_U32(sp - 8u, context.lr);
        PPC_STORE_U64(sp - 24u, context.r30.u64);
        PPC_STORE_U64(sp - 16u, context.r31.u64);
        context.r31.u64 = context.r1.u64 - 112u;
        PPC_STORE_U32(context.r31.u32, sp);
        context.r1.u64 = context.r31.u64;
        context.r3.u64 = InitializeManager(active->memory, *active,
            context.r31.u32);
        context.r1.u64 = context.r31.u64 + 112u;
        context.lr = PPC_LOAD_U32(sp - 8u);
        context.r30.u64 = PPC_LOAD_U64(sp - 24u);
        context.r31.u64 = PPC_LOAD_U64(sp - 16u);
        return;
    }
    case 0x8230bac0u:
    {
        std::uint64_t source_after = 0;
        context.r3.u64 = registered_metadata_composed::CopyUtf16UntilNull(
            active->memory, context.r3.u64, context.r4.u64, source_after);
        context.r4.u64 = source_after;
        return;
    }
    default: throw std::runtime_error("unexpected name record lower call");
    }
}

void NameRecordIndirect(PPCContext& context, std::uint8_t*,
    GuestAddress target)
{
    family::FrameRegisters frame{context.lr, context.r27.u64,
        context.r28.u64, context.r29.u64, context.r30.u64, context.r31.u64};
    context.r3.u64 = active->AllocateRecord(target, active->memory,
        context.r3.u64, context.r4.u64, context.r5.u64,
        context.r1.u64, frame);
    context.lr = frame.lr;
    context.r27.u64 = frame.r27;
    context.r28.u64 = frame.r28;
    context.r29.u64 = frame.r29;
    context.r30.u64 = frame.r30;
    context.r31.u64 = frame.r31;
}

int main()
{
    try
    {
        for (const Mode mode : Cases)
            if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Ready);
        family::FrameRegisters frame{1, 2, 3, 4, 5, 6};
        std::uint64_t result = 7;
        if (family::Apply(0xffffffffu, service.memory, service, 1, 2, 3, 4,
                Stack, frame, result) || result != 7 || frame.lr != 1 ||
            frame.r27 != 2 || frame.r28 != 3 || frame.r29 != 4 ||
            frame.r30 != 5 || frame.r31 != 6 || !service.events.empty())
            throw std::runtime_error("unknown name record target changed state");
        std::printf("PASS metadata-name-record 1 exact body %zu PPC comparisons + unknown\n",
            std::size(Cases));
        std::puts("LIMIT dynamic manager allocator external; reused UTF16/manager-init lower models and generic lower ABI; bounded RAM");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

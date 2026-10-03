#include "lo_semantics/metadata_utf16_slice.h"
#include "lo_semantics/manager_object_registration.h"
#include "lo_semantics/metadata_utf16_buffer.h"

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
namespace family = lo::semantic::gpu::metadata_utf16_slice;
namespace buffer = lo::semantic::gpu::metadata_utf16_buffer;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Data = 0x10000u;
constexpr GuestAddress Destination = 0x20000u;
constexpr GuestAddress Source = 0x21000u;
constexpr GuestAddress Manager = 0x50000u;
constexpr GuestAddress Table = 0x50100u;
constexpr GuestAddress AlternateManager = 0x51000u;
constexpr GuestAddress AlternateTable = 0x51100u;
constexpr GuestAddress ResizeMethod = 0x7300u;
constexpr GuestAddress FreeMethod = 0x7400u;
constexpr GuestAddress AlternateResizeMethod = 0x7500u;
constexpr GuestAddress AlternateFreeMethod = 0x7600u;
constexpr GuestAddress VirtualBuffer = 0x70000u;
constexpr std::array<GuestAddress, 10> Digits = {
    0x820009f8u, 0x82000b90u, 0x821a6b78u, 0x821a83bcu,
    0x82000cd4u, 0x82000cd0u, 0x821a83c0u, 0x821a83c4u,
    0x82000cc4u, 0x82000cccu};

enum class Mode { ConstructEmpty, ConstructTwo, SliceClamp, SliceMiddle,
    ReverseTwo, ReverseAlias, FormatNegative, FormatZero };
struct Case { GuestAddress address; PPCFunc* original; Mode mode; };
constexpr Case Cases[] = {
    {0x8232d240u, __imp__sub_8232D240, Mode::ConstructEmpty},
    {0x8232d240u, __imp__sub_8232D240, Mode::ConstructTwo},
    {0x8232d190u, __imp__sub_8232D190, Mode::SliceClamp},
    {0x8232d190u, __imp__sub_8232D190, Mode::SliceMiddle},
    {0x8232d040u, __imp__sub_8232D040, Mode::ReverseTwo},
    {0x8232d040u, __imp__sub_8232D040, Mode::ReverseAlias},
    {0x8232ced8u, __imp__sub_8232CED8, Mode::FormatNegative},
    {0x8232ced8u, __imp__sub_8232CED8, Mode::FormatZero},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000u, MEM_COMMIT,
                PAGE_READWRITE) || !VirtualAlloc(bytes + 0x82000000u,
                0x1b0000u, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000u,
                MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve UTF-16 slice RAM");
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
    family::VirtualServices
{
    GuestMemory memory;
    Mode mode;
    GuestAddress next = 0x40000u;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw manager allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary manager construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback manager construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager init method"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected allocator method"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({'Z', {method, manager, old, bytes, argument}});
        if (method != ResizeMethod || manager != Manager ||
            argument != 8u || bytes > 0x1000u)
            throw std::runtime_error("incorrect slice resize boundary");
        if (bytes == 0u) return 0;
        const GuestAddress fresh = next;
        next += 0x1000u;
        if (old != 0u)
            for (std::uint32_t i = 0; i < 64u; ++i)
                memory.WriteU8(fresh + i, memory.ReadU8(old + i));
        return fresh;
    }
    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t buffer) override
    {
        events.push_back({'F', {method, manager, buffer}});
        if (method != FreeMethod || manager != Manager)
            throw std::runtime_error("incorrect slice release boundary");
        return 0xabcdef0000000000ull;
    }
    std::uint64_t CallMethod(GuestAddress method, GuestMemory& caller_memory,
        std::uint64_t r3, std::uint64_t r4, std::uint64_t r5,
        std::uint64_t r6, std::uint64_t sp,
        family::FrameRegisters& frame) override
    {
        events.push_back({'V', {method, r3, r4, r5, r6, sp, frame.lr}});
        const bool alternate = method == AlternateResizeMethod ||
            method == AlternateFreeMethod;
        if (&caller_memory != &memory ||
            r3 != (alternate ? AlternateManager : Manager) ||
            (method != ResizeMethod && method != FreeMethod &&
                !alternate))
            throw std::runtime_error("incorrect slice virtual call");
        if (method == ResizeMethod || method == AlternateResizeMethod)
        {
            if (r5 != 0u || r6 != 8u)
                throw std::runtime_error("incorrect slice virtual resize args");
            if (mode == Mode::ReverseAlias)
            {
                // This is the caller's saved r27, not its current live r27.
                memory.WriteU32(static_cast<GuestAddress>(Stack) - 48u,
                    0x21000u);
                frame.r30 = 0xffffffff83310004ull;
                return 0xabcdef0000070000ull;
            }
            if (mode == Mode::FormatZero)
            {
                // Reverse's saved r31 becomes live in its caller's cleanup.
                memory.WriteU32(static_cast<GuestAddress>(Stack) -
                    176u - 12u, 1u);
            }
            return 0;
        }
        return 0xabcdef00000000ffull;
    }
};
Services* active = nullptr;

void WriteText(GuestMemory& memory, GuestAddress address, const char* text)
{
    std::size_t index = 0;
    do
    {
        memory.WriteU16(address + static_cast<GuestAddress>(index * 2u),
            static_cast<std::uint8_t>(text[index]));
    } while (text[index++] != '\0');
}
void Seed(Window& window)
{
    std::memset(window.bytes, 0, 0x90000u);
    std::memset(window.bytes + 0x82000000u, 0, 0x1b0000u);
    std::memset(window.bytes + 0x8330b000u, 0, 0x1000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(0x8330b60cu, AlternateManager);
    memory.WriteU32(Manager, Table);
    memory.WriteU32(AlternateManager, AlternateTable);
    memory.WriteU32(Table + 8u, ResizeMethod | 3u);
    memory.WriteU32(Table + 12u, FreeMethod | 3u);
    memory.WriteU32(AlternateTable + 8u, AlternateResizeMethod | 3u);
    memory.WriteU32(AlternateTable + 12u, AlternateFreeMethod | 3u);
    WriteText(memory, Data, "AB");
    memory.WriteU32(Source, Data);
    memory.WriteU32(Source + 4u, 3u);
    memory.WriteU32(Source + 8u, 3u);
    WriteText(memory, 0x821a83d0u, "");
    WriteText(memory, 0x821a83c8u, "-");
    for (unsigned i = 0; i < Digits.size(); ++i)
    {
        const char string[] = {static_cast<char>('0' + i), 0};
        WriteText(memory, Digits[i], string);
    }
}

bool SameMemory(const Window& a, const Window& b,
    std::uint64_t& first)
{
    constexpr std::array<std::pair<std::uint64_t, std::size_t>, 3> regions = {{
        {0, 0x90000u}, {0x82000000u, 0x1b0000u},
        {0x8330b000u, 0x1000u}}};
    for (const auto [start, size] : regions)
        for (std::size_t i = 0; i < size; ++i)
            if (a.bytes[start + i] != b.bytes[start + i])
            {
                first = start + i;
                return false;
            }
    return true;
}

PPCContext Initial(const Case& test)
{
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000020000ull;
    raw.r4.u64 = test.address == 0x8232d240u ?
        (test.mode == Mode::ConstructEmpty ? 0u : 2u) :
        test.address == 0x8232ced8u ?
            (test.mode == Mode::FormatNegative ? 0xabcdef00fffffff4ull : 0u) :
            0xabcdef0000021000ull;
    raw.r5.u64 = test.address == 0x8232d240u ?
        0xabcdef0000010000ull :
        test.mode == Mode::SliceClamp ? 99u : 1u;
    raw.r6.u64 = test.mode == Mode::SliceClamp ? 3u : 1u;
    raw.lr = 0x1122334455667788ull;
    const std::array<PPCRegister*, 7> saved = {&raw.r25, &raw.r26,
        &raw.r27, &raw.r28, &raw.r29, &raw.r30, &raw.r31};
    for (unsigned i = 0; i < saved.size(); ++i)
        saved[i]->u64 = 0x1234567800000000ull | (i + 25u);
    return raw;
}

bool Compare(const Case& test)
{
    Window original, recovered;
    Seed(original);
    Seed(recovered);
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
            initial.r6.u64, initial.r1.u64, frame, result))
        throw std::runtime_error("UTF-16 slice entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && expected.events == actual.events &&
        raw.r3.u64 == result && raw.r1.u64 == Stack && raw.lr == frame.lr &&
        raw.r25.u64 == frame.r25 && raw.r26.u64 == frame.r26 &&
        raw.r27.u64 == frame.r27 && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31;
    if (test.mode == Mode::ReverseAlias)
    {
        bool alternate_resize = false;
        bool alternate_free = false;
        for (const Event& event : expected.events)
        {
            alternate_resize |= event.kind == 'V' &&
                event.args[0] == AlternateResizeMethod &&
                event.args[1] == AlternateManager;
            alternate_free |= event.kind == 'V' &&
                event.args[0] == AlternateFreeMethod &&
                event.args[1] == AlternateManager;
        }
        if (!alternate_resize || !alternate_free)
            throw std::runtime_error("live r30 alternate manager path not reached");
    }
    if (test.mode == Mode::FormatZero)
    {
        bool live_cleanup = false;
        for (const Event& event : expected.events)
            live_cleanup |= event.kind == 'Z' && event.args[3] == 2u;
        if (!live_cleanup)
            throw std::runtime_error("live r31 cleanup resize not reached");
    }
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL slice %08x mode=%u r3 %llx/%llx lr %llx/%llx "
            "r27 %llx/%llx events %zu/%zu first %llx:%02x/%02x\n",
            test.address, static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r27.u64),
            static_cast<unsigned long long>(frame.r27),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
        const std::array<const PPCRegister*, 7> raw_regs = {&raw.r25,
            &raw.r26, &raw.r27, &raw.r28, &raw.r29, &raw.r30, &raw.r31};
        const std::array<const std::uint64_t*, 7> semantic_regs = {
            &frame.r25, &frame.r26, &frame.r27, &frame.r28,
            &frame.r29, &frame.r30, &frame.r31};
        for (unsigned i = 0; i < raw_regs.size(); ++i)
            if (raw_regs[i]->u64 != *semantic_regs[i])
                std::fprintf(stderr, "  r%u %llx/%llx\n", i + 25u,
                    static_cast<unsigned long long>(raw_regs[i]->u64),
                    static_cast<unsigned long long>(*semantic_regs[i]));
        for (std::size_t i = 0; i < expected.events.size() &&
            i < actual.events.size(); ++i)
            if (!(expected.events[i] == actual.events[i]))
            {
                std::fprintf(stderr, "  event %zu %c/%c\n", i,
                    expected.events[i].kind, actual.events[i].kind);
                for (unsigned arg = 0; arg < 7; ++arg)
                    if (expected.events[i].args[arg] != actual.events[i].args[arg])
                        std::fprintf(stderr, "    arg%u %llx/%llx\n", arg,
                            static_cast<unsigned long long>(
                                expected.events[i].args[arg]),
                            static_cast<unsigned long long>(
                                actual.events[i].args[arg]));
                break;
            }
    }
    return same;
}
} // namespace

void OriginalVirtualCall(PPCContext& ctx, std::uint8_t*, GuestAddress target)
{
    family::FrameRegisters frame{ctx.lr, ctx.r25.u64, ctx.r26.u64,
        ctx.r27.u64, ctx.r28.u64, ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
    ctx.r3.u64 = active->CallMethod(target, active->memory, ctx.r3.u64,
        ctx.r4.u64, ctx.r5.u64, ctx.r6.u64, ctx.r1.u64, frame);
    ctx.lr = frame.lr;
    const std::array<PPCRegister*, 7> target_regs = {&ctx.r25, &ctx.r26,
        &ctx.r27, &ctx.r28, &ctx.r29, &ctx.r30, &ctx.r31};
    const std::array<const std::uint64_t*, 7> source = {&frame.r25,
        &frame.r26, &frame.r27, &frame.r28, &frame.r29,
        &frame.r30, &frame.r31};
    for (unsigned i = 0; i < target_regs.size(); ++i)
        target_regs[i]->u64 = *source[i];
}

void OriginalDirectCall(PPCContext& ctx, std::uint8_t* base,
    GuestAddress target)
{
    switch (target)
    {
    case 0x8229f678u:
        ResizeArray(active->memory, *active, ctx.r3.u32,
            ctx.r4.u32, ctx.r5.u32);
        return;
    case 0x8232d318u:
    {
        const auto copy = manager_object_registration::CopyUtf16Padded(
            active->memory, ctx.r3.u64, ctx.r4.u64, ctx.r5.u64);
        ctx.r3.u64 = copy.r3;
        ctx.r4.u64 = copy.r4;
        ctx.r5.u64 = copy.r5;
        return;
    }
    case 0x8232d378u:
    {
        buffer::FrameRegisters frame{ctx.lr, ctx.r28.u64,
            ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
        std::uint64_t result = 0;
        (void)buffer::Apply(target, active->memory, *active, *active,
            ctx.r3.u64, ctx.r4.u64, 0, ctx.r1.u64, frame, result);
        ctx.r3.u64 = result;
        ctx.lr = frame.lr;
        ctx.r28.u64 = frame.r28;
        ctx.r29.u64 = frame.r29;
        ctx.r30.u64 = frame.r30;
        ctx.r31.u64 = frame.r31;
        return;
    }
    case 0x827c5f38u:
        ctx.r3.u64 = InitializeManager(active->memory, *active,
            static_cast<GuestAddress>(ctx.r1.u64 - 112u));
        return;
    case 0x82298af8u:
    {
        ctx.r12.u64 = ctx.lr;
        __savegprlr_28(ctx, base);
        PPC_STORE_U32(ctx.r1.u32 - 128u, ctx.r1.u32);
        ctx.r1.u64 -= 128u;
        RemoveArrayRange(active->memory, *active, ctx.r3.u32,
            ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32,
            ctx.r1.u32);
        ctx.r1.u64 += 128u;
        __restgprlr_28(ctx, base);
        return;
    }
    case 0x82298a98u:
    {
        const GuestAddress sp = ctx.r1.u32;
        PPC_STORE_U32(sp - 8u, ctx.lr);
        PPC_STORE_U64(sp - 16u, ctx.r31.u64);
        PPC_STORE_U32(sp - 96u, sp);
        ctx.r3.u64 = ReleaseTwoByteArray(active->memory, *active,
            ctx.r3.u32, sp);
        ctx.lr = PPC_LOAD_U32(sp - 8u);
        ctx.r31.u64 = PPC_LOAD_U64(sp - 16u);
        return;
    }
    default: throw std::runtime_error("unexpected UTF-16 slice lower call");
    }
}

int main()
{
    try
    {
        for (const Case& test : Cases) if (!Compare(test)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::ConstructEmpty);
        family::FrameRegisters frame{1, 2, 3, 4, 5, 6, 7, 8};
        std::uint64_t result = 9;
        if (family::Apply(0xffffffffu, service.memory,
                service, service, service, 1, 2, 3, 4,
                Stack, frame, result) || result != 9 ||
            frame.lr != 1 || frame.r25 != 2 || frame.r31 != 8 ||
            !service.events.empty())
            throw std::runtime_error("unknown UTF-16 slice target changed state");
        std::printf("PASS metadata-utf16-slice %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT real original four-entry composition; existing lower resize/copy/append/remove/release models and dynamic manager methods retain documented ABI boundary");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

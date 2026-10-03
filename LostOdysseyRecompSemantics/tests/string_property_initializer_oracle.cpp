#include "lo_semantics/string_property_initializer.h"

#include "lo_semantics/memory_move.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace property = lo::semantic::gpu::string_property_initializer;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress CopySource = 0x20000u;
constexpr GuestAddress CopyDestination = 0x21000u;
constexpr GuestAddress Data = 0x30000u;
constexpr GuestAddress CustomSource = 0x50000u;
constexpr GuestAddress Stack = 0x70000u;
constexpr GuestAddress Manager = 0x80000u;
constexpr GuestAddress Vtable = 0x80100u;
constexpr GuestAddress GlobalManager = 0x8330b608u;
constexpr GuestAddress GlobalWord = 0x833180acu;
constexpr GuestAddress Headers[] = {
    0x8336a894u, 0x8336a8d0u, 0x8336a8acu, 0x8336a8dcu,
};
constexpr GuestAddress ResizeMethod = 0x82345680u;
constexpr GuestAddress ReleaseMethod = 0x82345690u;

struct Entry { GuestAddress address; PPCFunc* original; };
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

enum class Mode { Copy, LiveReload, CopySaveAlias, DefaultProperty,
                  CustomProperty, ObjectStackAlias };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT,
                                    PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83318000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8336a000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("commit string-property guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    std::array<std::uint64_t, 6> fields;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices, ManagerFacadeServices
{
    GuestMemory memory;
    Mode mode;
    GuestAddress destination;
    GuestAddress source;
    std::uint32_t allocations = 0;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected,
             GuestAddress target, GuestAddress from)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected),
          destination(target), source(from) {}

    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
                                  std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected facade allocation"); }

    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({{1, method, manager, old_storage, bytes, argument}});
        if (method != ResizeMethod || manager != Manager || argument != 8u)
            throw std::runtime_error("unexpected string resize");
        if (mode == Mode::LiveReload && allocations == 0)
        {
            memory.WriteU32(destination + 4u, 1);
            memory.WriteU32(source, Data + 0x800u);
        }
        if (bytes == 0)
            return old_storage;
        return Data + 0x1000u * allocations++;
    }

    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager_register,
        std::uint64_t buffer_register) override
    {
        events.push_back({{2, method, manager_register, buffer_register, 0, 0}});
        if (method != ReleaseMethod || manager_register != Manager)
            throw std::runtime_error("unexpected string release");
        return buffer_register;
    }
};

Services* active = nullptr;

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}

void Initialize(std::uint8_t* bytes, Mode mode)
{
    std::memset(bytes, 0xbd, 0x90000);
    std::memset(bytes + 0x8330b000u, 0, 0x1000);
    std::memset(bytes + 0x83318000u, 0, 0x1000);
    std::memset(bytes + 0x8336a000u, 0, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(GlobalManager, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(Vtable + 12u, ReleaseMethod | 3u);
    memory.WriteU32(GlobalWord, 0x12345678u);
    for (std::size_t index = 0; index < std::size(Headers); ++index)
    {
        memory.WriteU32(Headers[index], Data + 0x400u +
                        static_cast<GuestAddress>(index) * 0x100u);
        memory.WriteU32(Headers[index] + 4u, 2u);
        memory.WriteU32(Headers[index] + 8u, 2u);
        memory.WriteU32(Data + 0x400u + static_cast<GuestAddress>(index) *
                        0x100u, 0x00410000u + static_cast<std::uint32_t>(index));
    }
    memory.WriteU32(CopySource, Data + 0x400u);
    memory.WriteU32(CopySource + 4u, 2u);
    memory.WriteU32(CopySource + 8u, 2u);
    memory.WriteU32(Data + 0x800u, 0x005a0000u);
    memory.WriteU16(CustomSource, 0x0043u);
    memory.WriteU16(CustomSource + 2u, 0x0044u);
    memory.WriteU16(CustomSource + 4u, 0u);
    if (mode == Mode::CopySaveAlias)
        memory.WriteU32(Stack - 24u, 0xfeedbeefu);
}

bool Compare(const Entry& entry, Mode mode, Window& original,
    Window& recovered)
{
    Initialize(original.bytes, mode);
    Initialize(recovered.bytes, mode);
    const bool copy = entry.address == 0x822a06c0u;
    const GuestAddress object = mode == Mode::ObjectStackAlias ?
        Stack - 104u : Object;
    const GuestAddress destination = mode == Mode::CopySaveAlias ?
        Stack - 24u : CopyDestination;
    const GuestAddress source = CopySource;
    Services expected(original.bytes, mode, destination, source);
    Services actual(recovered.bytes, mode, destination, source);
    const std::uint64_t incoming_r3 = 0xabcdef0000000000ull |
        (copy ? destination : object);
    const std::uint64_t incoming_r4 = copy ?
        0x1234567800000000ull | source :
        mode == Mode::CustomProperty || mode == Mode::ObjectStackAlias ?
            0x1234567800000000ull | CustomSource :
            0x1234567800000000ull;
    const std::uint64_t caller_sp = mode == Mode::CustomProperty ?
        0x1234567800070000ull : Stack;
    PPCContext context{};
    context.r1.u64 = caller_sp;
    context.r3.u64 = incoming_r3;
    context.r4.u64 = incoming_r4;
    context.r28.u64 = 0x1111222233334444ull;
    context.r29.u64 = 0x5555666677778888ull;
    context.r30.u64 = 0x9999aaaabbbbccccull;
    context.r31.u64 = 0xddddeeeeffff0000ull;
    context.lr = 0xabcdef0082200000ull;
    active = &expected;
    entry.original(context, original.bytes);

    property::FrameRegisters frame{0xabcdef0082200000ull,
        0x1111222233334444ull, 0x5555666677778888ull,
        0x9999aaaabbbbccccull, 0xddddeeeeffff0000ull};
    std::uint64_t result = 0;
    active = &actual;
    if (!property::Apply(entry.address, actual.memory, actual, actual,
        incoming_r3, incoming_r4, caller_sp, frame, result))
        throw std::runtime_error("string-property entry missing");
    const bool same = result == context.r3.u64 &&
        frame.lr == context.lr && frame.r28 == context.r28.u64 &&
        frame.r29 == context.r29.u64 && frame.r30 == context.r30.u64 &&
        frame.r31 == context.r31.u64 && context.r1.u64 == caller_sp &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x90000) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
                    recovered.bytes + 0x8330b000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83318000u,
                    recovered.bytes + 0x83318000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x8336a000u,
                    recovered.bytes + 0x8336a000u, 0x1000) == 0;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL string-property %08x mode %u r3 %llx/%llx LR %llx/%llx events %zu/%zu\n",
            entry.address, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size());
        for (std::size_t i = 0, shown = 0; i < 0x90000 && shown < 8; ++i)
            if (original.bytes[i] != recovered.bytes[i])
            {
                std::fprintf(stderr, "  memory %08zx %02x/%02x\n", i,
                    original.bytes[i], recovered.bytes[i]);
                ++shown;
            }
    }
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_28)
{
    __imp____savegprlr_28(ctx, base);
}
PPC_FUNC(__restgprlr_28)
{
    __imp____restgprlr_28(ctx, base);
}
PPC_FUNC(sub_8229F678)
{
    (void)base;
    ResizeArray(active->memory, *active, ctx.r3.u32, ctx.r4.u32,
                ctx.r5.u32);
}
PPC_FUNC(sub_82B7A0B0)
{
    (void)base;
    ctx.r3.u64 = CopyGuestMemory(active->memory, ctx.r3.u64, ctx.r4.u32,
                                ctx.r5.u64, ctx.r1.u32);
}
PPC_FUNC(sub_8229C8B0)
{
    (void)base;
    const GuestAddress sp = ctx.r1.u32;
    active->memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    WriteU64(active->memory, sp - 24u, ctx.r30.u64);
    WriteU64(active->memory, sp - 16u, ctx.r31.u64);
    active->memory.WriteU32(sp - 112u, sp);
    ctx.r3.u64 = registered_metadata_string::InitializeString(active->memory,
        *active, ctx.r3.u64, ctx.r4.u64, sp);
    ctx.lr = active->memory.ReadU32(sp - 8u);
    ctx.r30.u64 = ReadU64(active->memory, sp - 24u);
    ctx.r31.u64 = ReadU64(active->memory, sp - 16u);
}
PPC_FUNC(sub_82298938)
{
    (void)base;
    const GuestAddress sp = ctx.r1.u32;
    active->memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    WriteU64(active->memory, sp - 16u, ctx.r31.u64);
    active->memory.WriteU32(sp - 96u, sp);
    ctx.r3.u64 = ResetTwoByteArray(active->memory, *active, ctx.r3.u32, sp);
    ctx.lr = active->memory.ReadU32(sp - 8u);
    ctx.r31.u64 = ReadU64(active->memory, sp - 16u);
}
PPC_FUNC(sub_822A06C0) { __imp__sub_822A06C0(ctx, base); }

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        for (Mode mode : {Mode::Copy, Mode::LiveReload, Mode::CopySaveAlias})
        {
            if (!Compare(Entries[0], mode, original, recovered)) return 1;
            ++cases;
        }
        for (Mode mode : {Mode::DefaultProperty, Mode::CustomProperty,
                          Mode::ObjectStackAlias})
        {
            if (!Compare(Entries[1], mode, original, recovered)) return 1;
            ++cases;
        }
        Services untouched(recovered.bytes, Mode::Copy,
                           CopyDestination, CopySource);
        property::FrameRegisters frame{1, 2, 3, 4, 5};
        std::uint64_t result = 0x1234;
        if (property::Apply(0x822a06c4u, untouched.memory, untouched,
            untouched, CopyDestination, CopySource, Stack, frame, result) ||
            result != 0x1234 || frame.lr != 1 || frame.r28 != 2 ||
            frame.r29 != 3 || frame.r30 != 4 || frame.r31 != 5 ||
            !untouched.events.empty()) return 1;
        std::printf("PASS string-property %u PPC comparisons and 1 unknown\n",
                    cases);
        std::puts("LIMIT original two PPC bodies; resize/copy/UTF-16/reset lower models previously verified; generic lower ABI and unmodeled manager callbacks excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

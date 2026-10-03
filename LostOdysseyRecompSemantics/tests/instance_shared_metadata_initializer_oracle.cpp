#include "lo_semantics/instance_shared_metadata_initializer.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace shared = lo::semantic::gpu::instance_shared_metadata_initializer;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Manager = 0x30000u;
constexpr GuestAddress ManagerTable = 0x30100u;
constexpr GuestAddress Shared = 0x50000u;
constexpr GuestAddress Primary = 0x60000u;
constexpr GuestAddress Alternate = 0x70000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress ManagerGlobal = 0x8330B608u;
constexpr GuestAddress SharedGlobal = 0x83315F60u;
constexpr GuestAddress PrimaryGlobal = 0x83315F9Cu;
constexpr GuestAddress ReadyGate = 0x83315ED8u;
constexpr GuestAddress AllocateMethod = 0x82450000u;
constexpr GuestAddress ReadyMethod = 0x82451000u;

enum class Mode { DirectExisting, TailExisting, TailNull, DirectEmpty,
    CallbackMutation, StackAlias, GlobalAlias };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0xA0000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x82005000u, 0x1000, MEM_COMMIT,
                PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330B000u, 0x1000, MEM_COMMIT,
                PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83315000u, 0x2000, MEM_COMMIT,
                PAGE_READWRITE))
            throw std::runtime_error("commit shared metadata guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    std::array<std::uint64_t, 5> values;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices,
    registered_constructor_family::RegistrationServices,
    registered_callback_family::Services
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, {bytes, Space}), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected resize"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected release"); }
    std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment) override
    {
        events.push_back({{1, method, manager, bytes, alignment}});
        if (method != AllocateMethod || manager != Manager ||
            bytes != 376 || alignment != 8)
            throw std::runtime_error("unexpected constructor allocation");
        return 0x1234567800050000ull;
    }
    std::uint64_t Register(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected lazy registration"); }
    std::uint64_t CallExternalGetter(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected external getter"); }
    std::uint64_t CallExternalRegistration(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected external registration"); }
    std::uint64_t CallReadyMethod(GuestAddress method,
        std::uint64_t receiver, GuestAddress caller_sp) override
    {
        events.push_back({{2, method, receiver, caller_sp, 0}});
        if (mode != Mode::CallbackMutation || method != ReadyMethod ||
            receiver != Shared)
            throw std::runtime_error("unexpected ready callback");
        memory.WriteU32(SharedGlobal, Alternate);
        return 0xABCDEF0000070000ull;
    }
};

Services* active = nullptr;

void Setup(Window& window, Mode mode)
{
    std::memset(window.bytes, 0, 0xA0000);
    std::memset(window.bytes + 0x82005000u, 0, 0x1000);
    std::memset(window.bytes + 0x8330B000u, 0, 0x1000);
    std::memset(window.bytes + 0x83315000u, 0, 0x2000);
    GuestMemory memory(0, {window.bytes, Space});
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerTable);
    memory.WriteU32(ManagerTable + 4u, AllocateMethod | 3u);
    memory.WriteU32(0x82005160u + 124u, ReadyMethod | 3u);
    memory.WriteU32(PrimaryGlobal, Primary);
    memory.WriteU32(ReadyGate, mode == Mode::CallbackMutation ? 1u : 0u);
    memory.WriteU32(SharedGlobal,
        mode == Mode::DirectEmpty || mode == Mode::CallbackMutation ||
        mode == Mode::GlobalAlias ? 0u : Shared);
}

PPCContext Seed(Mode mode)
{
    PPCContext ctx{};
    ctx.r1.u64 = mode == Mode::DirectExisting ?
        0xABCDEF0000080000ull : Stack;
    ctx.lr = 0x1122334455667788ull;
    ctx.r28.u64 = 0x1111222233334444ull;
    ctx.r29.u64 = 0x2222333344445555ull;
    ctx.r30.u64 = 0x3333444455556666ull;
    ctx.r31.u64 = 0x4444555566667777ull;
    const GuestAddress object = mode == Mode::TailNull ? 0u :
        mode == Mode::StackAlias ? Stack - 212u :
        mode == Mode::GlobalAlias ? SharedGlobal - 180u : Object;
    ctx.r3.u64 = 0xDEADBEEF00000000ull | object;
    return ctx;
}

bool SameMemory(const Window& first, const Window& second)
{
    return std::memcmp(first.bytes, second.bytes, 0xA0000) == 0 &&
        std::memcmp(first.bytes + 0x82005000u,
                    second.bytes + 0x82005000u, 0x1000) == 0 &&
        std::memcmp(first.bytes + 0x8330B000u,
                    second.bytes + 0x8330B000u, 0x1000) == 0 &&
        std::memcmp(first.bytes + 0x83315000u,
                    second.bytes + 0x83315000u, 0x2000) == 0;
}

bool Compare(Mode mode, Window& original, Window& recovered)
{
    Setup(original, mode);
    Setup(recovered, mode);
    Services expected(original.bytes, mode), actual(recovered.bytes, mode);
    PPCContext ctx = Seed(mode);
    const PPCContext seeded = ctx;
    active = &expected;
    if (mode == Mode::TailExisting || mode == Mode::TailNull)
        __imp__sub_82412100(ctx, original.bytes);
    else
        __imp__sub_824108C8(ctx, original.bytes);

    shared::FrameRegisters frame{seeded.lr, seeded.r28.u64,
        seeded.r29.u64, seeded.r30.u64, seeded.r31.u64};
    std::uint64_t result = 0xCAFEBABEDEADBEEFull;
    const GuestAddress address = mode == Mode::TailExisting ||
        mode == Mode::TailNull ? 0x82412100u : 0x824108C8u;
    if (!shared::Apply(address, actual.memory, actual, actual, actual,
            seeded.r3.u64, seeded.r1.u64, frame, result))
        throw std::runtime_error("shared metadata mapping missing");
    const bool same = ctx.r3.u64 == result && ctx.r1.u64 == seeded.r1.u64 &&
        ctx.lr == frame.lr && ctx.r28.u64 == frame.r28 &&
        ctx.r29.u64 == frame.r29 && ctx.r30.u64 == frame.r30 &&
        ctx.r31.u64 == frame.r31 && expected.events == actual.events &&
        SameMemory(original, recovered);
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL shared metadata mode=%u r3=%llx/%llx lr=%llx/%llx events=%zu/%zu\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(ctx.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(ctx.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size());
        for (GuestAddress index = 0; index < 0xA0000u; ++index)
            if (original.bytes[index] != recovered.bytes[index])
            {
                std::fprintf(stderr, "  first byte %08x=%02x/%02x\n",
                    index, original.bytes[index], recovered.bytes[index]);
                break;
            }
    }
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
PPC_FUNC(sub_824108C8) { __imp__sub_824108C8(ctx, base); }
PPC_FUNC(sub_82403148)
{
    std::uint64_t result = 0;
    if (!registered_constructor_family::Apply(0x82403148u, active->memory,
            *active, *active, ctx.r3.u64, ctx.r1.u32, result))
        throw std::runtime_error("constructor mapping missing");
    ctx.r3.u64 = result;
}
PPC_FUNC(sub_82403200)
{
    ctx.r3.u64 = registered_callback_family::RegisterSharedMetadataObject(
        active->memory, *active, *active, ctx.r3.u64, ctx.r1.u32);
}

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        for (Mode mode : {Mode::DirectExisting, Mode::TailExisting,
             Mode::TailNull, Mode::DirectEmpty, Mode::CallbackMutation,
             Mode::StackAlias, Mode::GlobalAlias})
        {
            if (!Compare(mode, original, recovered)) return 1;
            ++cases;
        }
        Services services(recovered.bytes, Mode::DirectExisting);
        shared::FrameRegisters frame{1, 2, 3, 4, 5};
        std::uint64_t result = 0x123456789ABCDEFull;
        if (shared::Apply(0xFFFFFFFFu, services.memory, services,
                services, services, Object, Stack, frame, result) ||
            result != 0x123456789ABCDEFull || frame.lr != 1 ||
            !services.events.empty())
            return 1;
        std::printf("PASS instance-shared-metadata %u original-PPC cases; unknown entry\n",
            cases);
        std::puts("LIMIT constructor/registration use proven lower semantic models; generic lower ABI and runtime scene excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

#include "lo_semantics/manager_object_registration.h"
#include "lo_semantics/registered_callback_family.h"
#include "lo_semantics/registered_constructor_family.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::manager_object_registration;
namespace callback = lo::semantic::gpu::registered_callback_family;
namespace constructor = lo::semantic::gpu::registered_constructor_family;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x123456780005f000ull;
constexpr GuestAddress Singleton = 0x83315f7cu;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress Allocated = 0x30000u;
constexpr GuestAddress Existing = 0x20000u;
constexpr GuestAddress Replacement = 0x34000u;
constexpr GuestAddress Manager = 0x38000u;
constexpr GuestAddress ManagerTable = 0x38100u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress ReadyMethod = 0x823456a0u;

enum class Mode { Existing, Fresh, ReadyAlias };
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x70000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x82005000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83315000u, 0x5000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve/commit guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 4> args;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices, family::Services,
    constructor::RegistrationServices, callback::Services
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    explicit Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}

    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected storage release"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected resize"); }
    std::uint64_t AllocateStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t bytes, std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment}});
        if (method != AllocateMethod || manager != Manager || bytes != 376 ||
            alignment != 8)
            throw std::runtime_error("allocation arguments changed");
        return 0x1234567800030000ull;
    }

    static std::uint64_t FullSp(GuestAddress low)
    {
        const auto difference = static_cast<std::int32_t>(
            low - static_cast<GuestAddress>(Stack));
        return Stack + static_cast<std::int64_t>(difference);
    }
    std::uint64_t Register(GuestAddress target, std::uint64_t r3,
        GuestAddress sp) override
    { return Register(target, r3, FullSp(sp)); }
    std::uint64_t Register(GuestAddress target, std::uint64_t r3,
        std::uint64_t sp) override
    {
        events.push_back({'R', {target, r3, sp, 0}});
        throw std::runtime_error("unexpected constructor registration");
    }
    std::uint64_t CallExternalGetter(GuestAddress target, std::uint64_t r3,
        GuestAddress sp) override
    { return CallExternalGetter(target, r3, FullSp(sp)); }
    std::uint64_t CallExternalGetter(GuestAddress target, std::uint64_t r3,
        std::uint64_t sp) override
    {
        events.push_back({'G', {target, r3, sp, 0}});
        throw std::runtime_error("unexpected external getter");
    }
    std::uint64_t CallExternalRegistration(GuestAddress target,
        std::uint64_t r3, GuestAddress sp) override
    { return CallExternalRegistration(target, r3, FullSp(sp)); }
    std::uint64_t CallExternalRegistration(GuestAddress target,
        std::uint64_t r3, std::uint64_t sp) override
    {
        events.push_back({'C', {target, r3, sp, 0}});
        throw std::runtime_error("unexpected external registration");
    }
    std::uint64_t CallReadyMethod(GuestAddress method, std::uint64_t receiver,
        GuestAddress sp) override
    { return CallReadyMethod(method, receiver, FullSp(sp)); }
    std::uint64_t CallReadyMethod(GuestAddress method, std::uint64_t receiver,
        std::uint64_t sp) override
    {
        events.push_back({'V', {method, receiver, sp, 0}});
        if (mode != Mode::ReadyAlias || method != ReadyMethod ||
            receiver != Allocated || sp != Stack - 224u)
            throw std::runtime_error("ready callback arguments changed");
        memory.WriteU32(Singleton, Replacement);
        memory.WriteU32(static_cast<GuestAddress>(Stack) - 8u, 0x87654321u);
        memory.WriteU32(static_cast<GuestAddress>(Stack) - 16u, 0x12345678u);
        memory.WriteU32(static_cast<GuestAddress>(Stack) - 12u, 0x9abcdef0u);
        return 0x777788889999aaaau;
    }
};

Services* active = nullptr;
void Initialize(Window& window, Mode mode)
{
    std::memset(window.bytes, 0xbd, 0x70000);
    std::memset(window.bytes + 0x82005000u, 0, 0x1000);
    std::memset(window.bytes + 0x8330b000u, 0, 0x1000);
    std::memset(window.bytes + 0x83315000u, 0, 0x5000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerTable);
    memory.WriteU32(ManagerTable + 4u, AllocateMethod | 3u);
    memory.WriteU32(0x83315ef0u, 0x40000u);
    memory.WriteU32(Singleton, mode == Mode::Existing ? Existing : 0);
    memory.WriteU32(0x83315f60u, Existing);
    memory.WriteU32(0x83315f9cu, 0x28000u);
    memory.WriteU32(0x83315ed8u, mode == Mode::ReadyAlias ? 1u : 0u);
    memory.WriteU32(0x82005160u + 124u, ReadyMethod | 3u);
}

bool CompareObject(Mode mode, Window& original, Window& recovered)
{
    Initialize(original, mode);
    Initialize(recovered, mode);
    Services expected(original.bytes, mode), actual(recovered.bytes, mode);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.r3.u64 = 0xabcdef0000001234ull;
    context.r31.u64 = 0xaabbccddeeff0011ull;
    context.lr = 0x1122334455667788ull;
    active = &expected;
    __imp__sub_82376EE8(context, original.bytes);
    active = &actual;
    family::FrameRegisters frame{0x1122334455667788ull,
        0xaabbccddeeff0011ull};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(0x82376ee8u, actual.memory, actual, actual,
            0xabcdef0000001234ull, Stack, frame, result))
        throw std::runtime_error("missing 82376EE8 mapping");
    const bool flow = mode == Mode::Existing ?
        (expected.events.empty() && result == Existing) :
        mode == Mode::Fresh ?
        (expected.events.size() == 1 && expected.events[0].kind == 'A' &&
         result == Allocated) :
        (expected.events.size() == 2 && expected.events[0].kind == 'A' &&
         expected.events[1].kind == 'V' && result == Replacement &&
         frame.lr == 0x87654321u && frame.r31 == 0x123456789abcdef0ull);
    const bool same = flow && context.r3.u64 == result && context.r1.u64 == Stack &&
        context.lr == frame.lr && context.r31.u64 == frame.r31 &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x70000) == 0 &&
        std::memcmp(original.bytes + 0x82005000u,
            recovered.bytes + 0x82005000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
            recovered.bytes + 0x8330b000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83315000u,
            recovered.bytes + 0x83315000u, 0x5000) == 0;
    if (!same)
        std::fprintf(stderr, "FAIL object mode %u r3 %llx/%llx lr %llx/%llx events %zu/%zu\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size());
    return same;
}

bool CompareCopy(std::uint64_t destination, std::uint64_t source,
    std::uint64_t count, const std::array<std::uint16_t, 4>& input,
    Window& original, Window& recovered)
{
    std::memset(original.bytes, 0xbd, 0x10000);
    std::memset(recovered.bytes, 0xbd, 0x10000);
    Services expected(original.bytes, Mode::Existing),
        actual(recovered.bytes, Mode::Existing);
    for (unsigned i = 0; i != input.size(); ++i)
    {
        expected.memory.WriteU16(static_cast<GuestAddress>(source) + 2u * i, input[i]);
        actual.memory.WriteU16(static_cast<GuestAddress>(source) + 2u * i, input[i]);
    }
    PPCContext context{};
    context.r3.u64 = destination;
    context.r4.u64 = source;
    context.r5.u64 = count;
    __imp__sub_8232D318(context, original.bytes);
    const auto output = family::CopyUtf16Padded(actual.memory,
        destination, source, count);
    const bool same = context.r3.u64 == output.r3 &&
        context.r4.u64 == output.r4 && context.r5.u64 == output.r5 &&
        std::memcmp(original.bytes, recovered.bytes, 0x10000) == 0;
    if (!same)
        std::fprintf(stderr, "FAIL copy r3 %llx/%llx r4 %llx/%llx r5 %llx/%llx\n",
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(output.r3),
            static_cast<unsigned long long>(context.r4.u64),
            static_cast<unsigned long long>(output.r4),
            static_cast<unsigned long long>(context.r5.u64),
            static_cast<unsigned long long>(output.r5));
    return same;
}
} // namespace

void OriginalDirectCall(PPCContext& context, std::uint8_t*, GuestAddress target)
{
    std::uint64_t result = 0;
    if (target == 0x82408438u)
    {
        if (!constructor::Apply(target, active->memory, *active,
                static_cast<constructor::RegistrationServices&>(*active),
                context.r3.u64, context.r1.u32, result))
            throw std::runtime_error("missing constructor lower mapping");
    }
    else if (target == 0x824084f0u)
        result = callback::RegisterSecondaryObject(active->memory, *active,
            static_cast<callback::Services&>(*active),
            context.r3.u64, context.r1.u32);
    else
        throw std::runtime_error("unexpected original direct call");
    context.r3.u64 = result;
}

int main()
{
    try
    {
        Window original, recovered;
        unsigned comparisons = 0;
        for (Mode mode : {Mode::Existing, Mode::Fresh, Mode::ReadyAlias})
        {
            if (!CompareObject(mode, original, recovered)) return 1;
            ++comparisons;
        }
        if (!CompareCopy(0x1234567800001200ull, 0xabcdef0000001000ull,
                0x1234567800000000ull, {1, 2, 0, 9}, original, recovered) ||
            !CompareCopy(0x1234567800001200ull, 0xabcdef0000001000ull,
                4, {0x41, 0, 0x42, 0}, original, recovered) ||
            !CompareCopy(0x1234567800001002ull, 0xabcdef0000001000ull,
                3, {0x41, 0x42, 0x43, 0}, original, recovered))
            return 1;
        comparisons += 3;
        Services service(recovered.bytes, Mode::Existing);
        family::FrameRegisters frame{9, 8};
        std::uint64_t result = 7;
        if (family::Apply(0x12345678u, service.memory, service, service,
                5, Stack, frame, result) || result != 7 ||
            frame.lr != 9 || frame.r31 != 8 || !service.events.empty())
            return 1;
        std::printf("PASS manager-object-registration 2 entries %u PPC comparisons + unknown\n",
            comparisons);
        std::puts("LIMIT reused lower constructor/secondary registration semantics; external callback bodies and generic volatile ABI excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

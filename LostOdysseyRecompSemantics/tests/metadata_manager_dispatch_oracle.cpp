#include "lo_semantics/metadata_manager_dispatch.h"

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
namespace family = lo::semantic::gpu::metadata_manager_dispatch;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr std::uint64_t Argument = 0xaabbccdd00020000ull;
constexpr std::uint64_t Extra = 0x5566778800000034ull;
constexpr GuestAddress Manager = 0x50000u;
constexpr GuestAddress Manager2 = 0x52000u;
constexpr GuestAddress Vtable = 0x54000u;
constexpr GuestAddress Vtable2 = 0x55000u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress WarmMethod = 0x7015u;
constexpr GuestAddress ColdMethod = 0x7005u;
enum class Mode { Ready, ColdReread, SavedAlias };
constexpr Mode Cases[] = {Mode::Ready, Mode::ColdReread,
    Mode::SavedAlias};
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000}, {0x8330b000u, 0x1000}};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve manager guest RAM");
        for (const Region region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit manager guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 8> args;
    bool operator==(const Event&) const = default;
};

struct Services final : family::ManagerDispatchServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        events.push_back({'A', {bytes}});
        if (mode != Mode::ColdReread || bytes != 0x48decu)
            throw std::runtime_error("unexpected manager allocation");
        return 0xaabbccdd00050000ull;
    }
    std::uint64_t ConstructPrimary(std::uint64_t allocation) override
    {
        events.push_back({'P', {allocation}});
        if (mode != Mode::ColdReread ||
            allocation != 0xaabbccdd00050000ull)
            throw std::runtime_error("incorrect primary construction");
        return 0x1122334400050000ull;
    }
    std::uint64_t ConstructFallback(std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress method,
        std::uint64_t receiver) override
    {
        events.push_back({'M', {method, receiver}});
        if (mode != Mode::ColdReread)
            throw std::runtime_error("unexpected initializer method");
        if (method == 0x8004u && receiver == 0x1122334400050000ull)
            return 1u;
        if (method == 0x8014u && receiver == Manager)
        {
            memory.WriteU32(ManagerGlobal, Manager2);
            return 0xfedcba9876543210ull;
        }
        throw std::runtime_error("incorrect initializer method boundary");
    }
    std::uint64_t CallDescriptorMethod(GuestAddress target,
        GuestMemory& caller_memory, std::uint64_t manager_r3,
        std::uint64_t argument_r4, std::uint64_t argument_r5,
        std::uint64_t sp, std::uint64_t lr,
        family::FrameRegisters& frame) override
    {
        events.push_back({'D', {target, manager_r3, argument_r4,
            argument_r5, sp, lr, frame.r31, frame.ctr}});
        const GuestAddress expected_manager =
            mode == Mode::ColdReread ? Manager2 : Manager;
        const GuestAddress method =
            mode == Mode::ColdReread ? ColdMethod : WarmMethod;
        if (&caller_memory != &memory || target != (method & ~3u) ||
            manager_r3 != expected_manager || argument_r4 != Argument ||
            argument_r5 != Extra || sp != Stack - 112u ||
            lr != 0x823f32dcu || frame.ctr != method ||
            frame.r31 != 0xffffffff83310000ull ||
            frame.r30 != Argument || frame.r29 != Extra)
            throw std::runtime_error("incorrect manager dispatch boundary");
        if (mode == Mode::SavedAlias)
        {
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 24u,
                0x22334455u);
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 20u,
                0x66778899u);
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 16u,
                0xaabbccddu);
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 12u,
                0xeeff0011u);
            frame.r30 = 0;
            frame.r31 = 0;
        }
        return 0x9999888877776666ull;
    }
};
Services* active = nullptr;

void Initialize(Window& window, Mode mode)
{
    for (const Region region : Regions)
        std::memset(window.bytes + region.start, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal,
        mode == Mode::ColdReread ? 0u : Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Manager2, Vtable2);
    memory.WriteU32(Vtable + 4u, WarmMethod);
    memory.WriteU32(Vtable + 56u, 0x8015u);
    memory.WriteU32(Vtable + 60u, 0x8005u);
    memory.WriteU32(Vtable2 + 4u, ColdMethod);
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

family::FrameRegisters FrameFrom(const PPCContext& context)
{
    return {context.lr, context.r29.u64, context.r30.u64,
        context.r31.u64, context.ctr.u64};
}

bool Compare(Mode mode)
{
    Window original, recovered;
    Initialize(original, mode);
    Initialize(recovered, mode);
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = Argument;
    raw.r4.u64 = Extra;
    raw.lr = 0x9988776655443322ull;
    raw.r29.u64 = 0x1234567800000029ull;
    raw.r30.u64 = 0x1234567800000030ull;
    raw.r31.u64 = 0x1234567800000031ull;
    raw.ctr.u64 = 0x12345678000000ccull;
    const PPCContext initial = raw;
    Services expected(original.bytes, mode);
    active = &expected;
    __imp__sub_823F3298(raw, original.bytes);

    Services actual(recovered.bytes, mode);
    auto frame = FrameFrom(initial);
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(0x823f3298u, actual.memory, actual,
            initial.r3.u64, initial.r4.u64, initial.r1.u64,
            frame, result))
        throw std::runtime_error("manager dispatch entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && expected.events == actual.events &&
        raw.r3.u64 == result && raw.r1.u64 == Stack &&
        raw.lr == frame.lr && raw.r29.u64 == frame.r29 &&
        raw.r30.u64 == frame.r30 && raw.r31.u64 == frame.r31 &&
        raw.ctr.u64 == frame.ctr;
    if (!same)
        std::fprintf(stderr,
            "FAIL manager-dispatch mode=%u r3 %llx/%llx "
            "r30 %llx/%llx lr %llx/%llx events %zu/%zu "
            "first %llx:%02x/%02x\n", static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.r30.u64),
            static_cast<unsigned long long>(frame.r30),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

void OriginalInitializeManager(PPCContext& context, std::uint8_t*)
{
    context.r3.u64 = InitializeManager(active->memory, *active,
        context.r1.u32 - 112u);
}

void OriginalDispatchMethod(PPCContext& context, std::uint8_t*,
    std::uint32_t target)
{
    auto frame = FrameFrom(context);
    context.r3.u64 = active->CallDescriptorMethod(target,
        active->memory, context.r3.u64, context.r4.u64,
        context.r5.u64, context.r1.u64, context.lr, frame);
    context.lr = frame.lr;
    context.r29.u64 = frame.r29;
    context.r30.u64 = frame.r30;
    context.r31.u64 = frame.r31;
    context.ctr.u64 = frame.ctr;
}

int main()
{
    try
    {
        for (const Mode mode : Cases) if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Ready);
        family::FrameRegisters frame{9u, 10u, 11u, 12u, 13u};
        std::uint64_t result = 7u;
        spare.bytes[Manager] = 0x5au;
        if (family::Apply(0xffffffffu, service.memory, service,
                1, 2, Stack, frame, result) || result != 7u ||
            frame.lr != 9u || frame.r29 != 10u || frame.r30 != 11u ||
            frame.r31 != 12u || frame.ctr != 13u ||
            spare.bytes[Manager] != 0x5au || !service.events.empty())
            throw std::runtime_error("unknown manager dispatch changed state");
        std::printf("PASS metadata-manager-dispatch %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT accepted InitializeManager model; dynamic vtable+4 target and generic lower ABI/frame/MMIO external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

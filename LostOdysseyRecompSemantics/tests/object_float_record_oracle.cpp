#include "lo_semantics/object_float_record.h"
#include "lo_semantics/object_registration.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_float_record;
using recovery_abi::Address;
using recovery_abi::ReadU64;
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Parent = 0x40000u;
constexpr GuestAddress Node = 0x42000u;
constexpr GuestAddress Existing = 0x20000u;
constexpr GuestAddress Replacement = 0x22000u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Manager = 0x30000u;
constexpr GuestAddress ManagerVtable = 0x30100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress SingletonGlobal = 0x833189ecu;
constexpr GuestAddress RegistrationMode = 0x83315ed8u;
constexpr GuestAddress ListHead = 0x83315ef0u;
constexpr std::uint64_t FullObject = 0x1234567800010000ull;
constexpr test::Region Regions[] = {{0u, 0x70000u},
    {0x8330b000u, 0x1000u}, {0x83315000u, 0x1000u},
    {0x83318000u, 0x1000u}, {0x83319000u, 0x1000u}};

enum class Mode { NullParent, HeadMatch, LinkedMatch, Miss,
    ConstructMatch, AllocationFailure, CallbackReplacement };
constexpr Mode Cases[] = {Mode::NullParent, Mode::HeadMatch,
    Mode::LinkedMatch, Mode::Miss, Mode::ConstructMatch,
    Mode::AllocationFailure, Mode::CallbackReplacement};

struct Event
{
    char kind;
    std::array<std::uint64_t, 5> arguments;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices,
    registered_constructor_family::RegistrationServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(test::GuestWindow& window, Mode selected)
        : memory(window.Memory()), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected manager construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected storage release"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected storage resize"); }
    std::uint64_t AllocateStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t bytes, std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment, 0}});
        if (method != AllocateMethod || manager != Manager ||
            bytes != 376 || alignment != 8)
            throw std::runtime_error("unexpected allocation arguments");
        return mode == Mode::AllocationFailure ?
            0x1234567800000000ull : FullObject;
    }
    std::uint64_t Register(GuestAddress callback, std::uint64_t incoming_r3,
        GuestAddress caller_sp) override
    {
        const auto before = memory.ReadU32(SingletonGlobal);
        events.push_back({'R', {callback, incoming_r3, caller_sp, before, 0}});
        if (callback != 0x82627230u || caller_sp != Stack - 224u)
            throw std::runtime_error("unexpected registration callback");
        if (mode == Mode::CallbackReplacement)
            memory.WriteU32(SingletonGlobal, Replacement);
        return 0xfedcba9876543210ull;
    }
};

Services* active = nullptr;

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    state.sp = context.r1.u64; state.lr = context.lr;
    state.r3 = context.r3.u64; state.r11 = context.r11.u64;
    state.r12 = context.r12.u64; state.r31 = context.r31.u64;
    state.xer_so = context.xer.so;
    state.cr6 = {context.cr6.lt, context.cr6.gt,
        context.cr6.eq, context.cr6.un};
    return state;
}

void Initialize(GuestMemory& memory, Mode mode)
{
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(RegistrationMode, 0);
    memory.WriteU32(ListHead, 0x24000u);
    memory.WriteU32(SingletonGlobal,
        mode == Mode::HeadMatch || mode == Mode::LinkedMatch ||
        mode == Mode::Miss ? Existing : 0);
    memory.WriteU32(Node + 60u,
        mode == Mode::LinkedMatch ? Existing : 0);
    GuestAddress head = Node;
    if (mode == Mode::HeadMatch) head = Existing;
    if (mode == Mode::ConstructMatch) head = Object;
    if (mode == Mode::CallbackReplacement) head = Replacement;
    memory.WriteU32(Parent + 52u, head);
}

bool Check(Mode mode, unsigned ordinal)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    Services expected(original, mode), actual(recovered, mode);
    Initialize(expected.memory, mode);
    Initialize(actual.memory, mode);
    PPCContext context{};
    context.r1.u64 = 0x123456780005f000ull;
    context.r3.u64 = 0xabcdef1200000000ull |
        (mode == Mode::NullParent ? 0 : Parent);
    context.r11.u64 = 0x1122334455667788ull;
    context.r12.u64 = 0x2233445566778899ull;
    context.r31.u64 = 0x33445566778899aaull;
    context.lr = 0x445566778899aabbull;
    context.xer.so = 1;
    context.cr6 = {0, 1, 0, {1}};
    auto state = FromPpc(context);

    active = &expected;
    __imp__sub_82384C08(context, original.Bytes());
    active = nullptr;
    if (!family::Apply(0x82384c08u, actual.memory, actual, actual, state))
        throw std::runtime_error("missing object-float-record entry");

    const auto observed = FromPpc(context);
    bool same = observed.sp == state.sp && observed.lr == state.lr &&
        observed.r3 == state.r3 && observed.r11 == state.r11 &&
        observed.r12 == state.r12 && observed.r31 == state.r31 &&
        observed.cr6 == state.cr6 && observed.xer_so == state.xer_so &&
        expected.events == actual.events &&
        std::memcmp(original.Bytes(), recovered.Bytes(), Stack - 0x1000u) == 0;
    for (const auto region : Regions)
        if (region.base != 0)
            same &= std::memcmp(original.Bytes() + region.base,
                recovered.Bytes() + region.base, region.size) == 0;
    for (GuestAddress offset : {8u, 16u, 12u, 96u})
        same &= expected.memory.ReadU32(Stack - offset) ==
            actual.memory.ReadU32(Stack - offset);
    if (mode == Mode::ConstructMatch || mode == Mode::CallbackReplacement)
    {
        const auto lower_frame = Stack - 96u - 128u;
        for (GuestAddress offset : {80u, 84u, 92u, 100u, 108u})
            same &= expected.memory.ReadU32(lower_frame + offset) ==
                actual.memory.ReadU32(lower_frame + offset);
    }
    const auto expected_events = mode == Mode::ConstructMatch ||
        mode == Mode::AllocationFailure || mode == Mode::CallbackReplacement ?
        2u : 0u;
    same &= expected.events.size() == expected_events;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL object-float-record case %u mode %u r3 %llx/%llx events %zu/%zu\n",
            ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(observed.r3),
            static_cast<unsigned long long>(state.r3),
            expected.events.size(), actual.events.size());
        return false;
    }
    return true;
}
} // namespace

void OriginalAllocate(PPCContext& context, std::uint8_t*)
{
    context.r3.u64 = AllocateManagerBuffer(active->memory, *active,
        context.r3.u64, context.r1.u32);
}

void OriginalInitialize(PPCContext& context, std::uint8_t*)
{
    context.r3.u64 = InitializeRegisteredObject(active->memory,
        RegisteredObjectInput{context.r3.u64, context.r5.u64,
            context.r6.u64, context.r7.u64, context.r8.u64,
            context.r9.u64, context.r10.u64, context.r1.u32});
}

void OriginalRegister(PPCContext& context, std::uint8_t*)
{
    context.r3.u64 = active->Register(0x82627230u,
        context.r3.u64, context.r1.u32);
}

int main()
{
    try
    {
        for (unsigned index = 0; index < std::size(Cases); ++index)
            if (!Check(Cases[index], index)) return 1;
        std::printf("PASS object-float-record 1 new entry %zu cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 8242D038 PPC dependency; accepted allocation/initializer adapters; 82627230 guest callback; nested stack scratch, generic volatile ABI, MMIO, faults and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

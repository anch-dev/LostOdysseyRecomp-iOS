#include "lo_semantics/object_float_record_post_chain.h"
#include "lo_semantics/object_startup.h"
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
namespace chain = object_float_record_post_chain;
namespace post = object_registration_post;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Parent = 0x40000u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Existing = 0x20000u;
constexpr GuestAddress Associated = 0x22000u;
constexpr GuestAddress Shared = 0x24000u;
constexpr GuestAddress SharedNew = 0x26000u;
constexpr GuestAddress Primary = 0x28000u;
constexpr GuestAddress PrimaryNew = 0x2a000u;
constexpr GuestAddress Secondary = 0x2c000u;
constexpr GuestAddress Manager = 0x30000u;
constexpr GuestAddress ManagerVtable = 0x30100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress SingletonGlobal = 0x833189ecu;
constexpr GuestAddress AssociatedGlobal = 0x83318900u;
constexpr GuestAddress SharedGlobal = 0x83315f60u;
constexpr GuestAddress PrimaryGlobal = 0x83315f9cu;
constexpr GuestAddress SecondaryGlobal = 0x83315f7cu;
constexpr GuestAddress ReadyGate = 0x83315ed8u;
constexpr GuestAddress ListHead = 0x83315ef0u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress ReadyMethod = 0x823456a0u;
constexpr GuestAddress RegisteredVtable = 0x82005160u;
constexpr std::uint64_t FullObject = 0x1234567800010000ull;
constexpr test::Region Regions[] = {{0u, 0x70000u},
    {0x82005000u, 0x1000u}, {0x8330b000u, 0x1000u},
    {0x83315000u, 0x5000u}};

enum class Mode { Existing, LazyEqual, LazyDifferent,
    SharedMissing, PrimaryMissing, ReadyVirtual };
constexpr Mode Cases[] = {Mode::Existing, Mode::LazyEqual,
    Mode::LazyDifferent, Mode::SharedMissing,
    Mode::PrimaryMissing, Mode::ReadyVirtual};

struct Event
{
    char kind;
    std::array<std::uint64_t, 5> arguments;
    bool operator==(const Event&) const = default;
};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}

struct Services final : ManagerFacadeServices,
    registered_constructor_family::RegistrationServices,
    registered_callback_family::Services,
    ObjectRegistrationServices, post::VirtualServices, chain::PostObserver
{
    GuestMemory memory;
    Mode mode;
    unsigned allocations = 0;
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
        events.push_back({'A', {method, manager, bytes, alignment,
            allocations}});
        if (method != AllocateMethod || manager != Manager ||
            bytes != 376u || alignment != 8u)
            throw std::runtime_error("unexpected allocation arguments");
        const unsigned index = allocations++;
        if (index == 0u) return FullObject;
        if (index == 1u && mode == Mode::SharedMissing)
            return 0x1234567800026000ull;
        if (index == 1u && mode == Mode::PrimaryMissing)
            return 0x123456780002a000ull;
        throw std::runtime_error("unexpected extra allocation");
    }
    std::uint64_t Register(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected external registration"); }
    std::uint64_t CallExternalGetter(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected external getter"); }
    std::uint64_t CallExternalRegistration(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected external callback"); }
    std::uint64_t CallReadyMethod(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected nested ready method"); }
    std::uint64_t GetPrimaryObject() override
    {
        if (mode != Mode::PrimaryMissing)
            throw std::runtime_error("unexpected graph getter");
        events.push_back({'P', {PrimaryNew, 0, 0, 0, 0}});
        return PrimaryNew;
    }
    std::uint64_t CreateSecondary(std::uint64_t) override
    { throw std::runtime_error("unexpected secondary construction"); }
    std::uint64_t RegisterSecondary(std::uint64_t) override
    { throw std::runtime_error("unexpected secondary registration"); }
    std::uint64_t CallReadyMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected graph ready method"); }
    void Enter(GuestAddress target, std::uint64_t incoming_r3,
        GuestAddress caller_sp, GuestAddress singleton) override
    {
        events.push_back({'Q', {target, incoming_r3, caller_sp,
            singleton, 0}});
        if (target != 0x82627230u || incoming_r3 != FullObject ||
            caller_sp != Stack - 224u || singleton != Object)
            throw std::runtime_error("unexpected post callback entry");
    }
    void Call(GuestAddress target, GuestMemory& guest,
        post::Registers& state) override
    {
        events.push_back({'V', {target, state.r[3], state.r[1],
            state.lr, state.ctr}});
        if (mode != Mode::ReadyVirtual || target != ReadyMethod ||
            Address(state.r[3]) != Object ||
            Address(state.r[1]) != Stack - 336u)
            throw std::runtime_error("unexpected ready virtual call");
        guest.WriteU32(Object + 208u, 0x12345678u);
        state.r[3] = 0x1122334455667788ull;
    }
};

Services* active = nullptr;

object_float_record::Registers FromPpc(const PPCContext& c)
{
    object_float_record::Registers state{};
    state.sp = c.r1.u64; state.lr = c.lr;
    state.r3 = c.r3.u64; state.r11 = c.r11.u64;
    state.r12 = c.r12.u64; state.r31 = c.r31.u64;
    state.xer_so = c.xer.so;
    state.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return state;
}

post::Registers PostFromPpc(PPCContext& c)
{
    post::Registers state{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        state.r[i] = fields[i]->u64;
    state.lr = c.lr; state.ctr = c.ctr.u64;
    state.xer_so = c.xer.so;
    state.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return state;
}

void Initialize(GuestMemory& memory, Mode mode)
{
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(SingletonGlobal,
        mode == Mode::Existing ? Existing : 0u);
    memory.WriteU32(AssociatedGlobal,
        mode == Mode::LazyEqual ? Object : Associated);
    memory.WriteU32(SharedGlobal,
        mode == Mode::SharedMissing ? 0u : Shared);
    memory.WriteU32(PrimaryGlobal,
        mode == Mode::PrimaryMissing ? 0u : Primary);
    memory.WriteU32(SecondaryGlobal, Secondary);
    memory.WriteU32(ReadyGate, mode == Mode::ReadyVirtual ? 1u : 0u);
    memory.WriteU32(ListHead, 0x34000u);
    memory.WriteU32(Parent + 52u,
        mode == Mode::Existing ? Existing : Object);
    memory.WriteU32(RegisteredVtable + 124u, ReadyMethod | 3u);
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
    context.r3.u64 = 0xabcdef1200040000ull;
    context.r11.u64 = 0x1122334455667788ull;
    context.r12.u64 = 0x2233445566778899ull;
    context.r29.u64 = 0x33445566778899aaull;
    context.r30.u64 = 0x445566778899aabbull;
    context.r31.u64 = 0x5566778899aabbccull;
    context.lr = 0x66778899aabbccdduLL;
    context.ctr.u64 = 0x778899aabbccddeeull;
    context.xer.so = 1;
    context.cr6 = {0, 1, 0, {1}};
    auto state = FromPpc(context);
    active = &expected;
    __imp__sub_82384C08(context, original.Bytes());
    active = nullptr;
    chain::Dependencies dependencies{actual, actual, actual,
        actual, actual, actual};
    if (!chain::Apply(actual.memory, dependencies, state))
        throw std::runtime_error("missing complete float-record chain");

    const auto observed = FromPpc(context);
    bool same = observed.sp == state.sp && observed.lr == state.lr &&
        observed.r3 == state.r3 && observed.r11 == state.r11 &&
        observed.r12 == state.r12 && observed.r31 == state.r31 &&
        observed.cr6 == state.cr6 && observed.xer_so == state.xer_so &&
        expected.events == actual.events &&
        expected.allocations == actual.allocations &&
        std::memcmp(original.Bytes(), recovered.Bytes(), Stack - 0x1000u) == 0;
    for (const auto region : Regions)
        if (region.base != 0)
            same &= std::memcmp(original.Bytes() + region.base,
                recovered.Bytes() + region.base, region.size) == 0;
    for (GuestAddress offset : {8u, 16u, 12u, 96u})
        same &= expected.memory.ReadU32(Stack - offset) ==
            actual.memory.ReadU32(Stack - offset);
    if (mode != Mode::Existing)
    {
        const GuestAddress lower_frame = Stack - 224u;
        for (GuestAddress offset : {80u, 84u, 92u, 100u, 108u})
            same &= expected.memory.ReadU32(lower_frame + offset) ==
                actual.memory.ReadU32(lower_frame + offset);
        const GuestAddress object = expected.memory.ReadU32(SingletonGlobal);
        same &= object == Object &&
            expected.memory.ReadU32(object + 52u) ==
                actual.memory.ReadU32(object + 52u) &&
            expected.memory.ReadU32(object + 60u) ==
                actual.memory.ReadU32(object + 60u) &&
            expected.memory.ReadU32(object + 196u) ==
                actual.memory.ReadU32(object + 196u);
    }
    const std::vector<char> wanted = mode == Mode::Existing ?
        std::vector<char>{} :
        mode == Mode::SharedMissing ? std::vector<char>{'A', 'Q', 'A'} :
        mode == Mode::PrimaryMissing ? std::vector<char>{'A', 'Q', 'A', 'P'} :
        mode == Mode::ReadyVirtual ? std::vector<char>{'A', 'Q', 'V'} :
        std::vector<char>{'A', 'Q'};
    same &= expected.events.size() == wanted.size();
    if (expected.events.size() == wanted.size())
        for (std::size_t i = 0; i < wanted.size(); ++i)
            same &= expected.events[i].kind == wanted[i];
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL float-record-post-chain case %u mode %u r3 %llx/%llx events %zu/%zu allocations %u/%u\n",
            ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(observed.r3),
            static_cast<unsigned long long>(state.r3),
            expected.events.size(), actual.events.size(),
            expected.allocations, actual.allocations);
        return false;
    }
    return true;
}
} // namespace

void OriginalSave29(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
    for (unsigned i = 29u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u - (31u - i) * 8u),
            fields[i]->u64);
}
void OriginalRestore29(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
    for (unsigned i = 29u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory,
            Address(c.r1.u64 - 16u - (31u - i) * 8u));
}
void OriginalAllocate(PPCContext& c, std::uint8_t*)
{
    c.r3.u64 = AllocateManagerBuffer(active->memory, *active,
        c.r3.u64, c.r1.u32);
}
void OriginalInitialize(PPCContext& c, std::uint8_t*)
{
    c.r3.u64 = InitializeRegisteredObject(active->memory,
        RegisteredObjectInput{c.r3.u64, c.r5.u64, c.r6.u64,
            c.r7.u64, c.r8.u64, c.r9.u64, c.r10.u64, c.r1.u32});
}
void OriginalPost(PPCContext& c, std::uint8_t* base)
{
    active->Enter(0x82627230u, c.r3.u64, c.r1.u32,
        active->memory.ReadU32(SingletonGlobal));
    __imp__sub_82627230(c, base);
}
void OriginalAssociated(PPCContext& c, std::uint8_t*)
{
    std::uint64_t result = 0;
    if (!registered_constructor_family::Apply(0x8242cf78u,
            active->memory, *active, *active, c.r3.u64, c.r1.u32, result))
        throw std::runtime_error("missing associated getter");
    c.r3.u64 = result;
}
void OriginalSharedConstruct(PPCContext& c, std::uint8_t*)
{
    std::uint64_t result = 0;
    if (!registered_constructor_family::Apply(0x82403148u,
            active->memory, *active, *active, c.r3.u64, c.r1.u32, result))
        throw std::runtime_error("missing shared constructor");
    c.r3.u64 = result;
}
void OriginalSharedRegister(PPCContext& c, std::uint8_t*)
{
    c.r3.u64 = registered_callback_family::RegisterSharedMetadataObject(
        active->memory, *active, *active, c.r3.u64, c.r1.u32);
}
void OriginalPrimaryConstruct(PPCContext& c, std::uint8_t*)
{
    c.r3.u64 = ConstructRegisteredObject(active->memory, *active,
        c.r3.u64, c.r1.u32);
}
void OriginalPrimaryRegister(PPCContext& c, std::uint8_t*)
{
    c.r3.u64 = RegisterObjectGraph(active->memory, *active);
}
void OriginalPrimaryGetter(PPCContext& c, std::uint8_t*)
{
    c.r3.u64 = GetPrimaryRegisteredObject(active->memory, *active,
        *active, c.r1.u32);
}
void OriginalIndirect(std::uint32_t target, PPCContext& c, std::uint8_t*)
{
    auto state = PostFromPpc(c);
    active->Call(target, active->memory, state);
    c.r3.u64 = state.r[3];
}

int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS object-float-record-post-chain 0 new entries %zu cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 82384C08/8242D038/82627230 PPC; accepted other direct lowers; virtual guest callback, generic volatile ABI and nested stack scratch excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

#include "lo_semantics/object_registration_post.h"
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
namespace family = object_registration_post;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Self = 0x10000u;
constexpr GuestAddress SelfVtable = 0x11000u;
constexpr GuestAddress Associated = 0x12000u;
constexpr GuestAddress AssociatedNew = 0x14000u;
constexpr GuestAddress Shared = 0x20000u;
constexpr GuestAddress SharedNew = 0x24000u;
constexpr GuestAddress Primary = 0x28000u;
constexpr GuestAddress PrimaryNew = 0x2c000u;
constexpr GuestAddress Secondary = 0x30000u;
constexpr GuestAddress Manager = 0x38000u;
constexpr GuestAddress ManagerVtable = 0x38100u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress ReadyMethod = 0x823456a0u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AssociatedGlobal = 0x83318900u;
constexpr GuestAddress SingletonGlobal = 0x833189ecu;
constexpr GuestAddress SharedGlobal = 0x83315f60u;
constexpr GuestAddress SecondaryGlobal = 0x83315f7cu;
constexpr GuestAddress PrimaryGlobal = 0x83315f9cu;
constexpr GuestAddress ReadyGate = 0x83315ed8u;
constexpr GuestAddress RegistrationMode = 0x83315ed8u;
constexpr GuestAddress ListHead = 0x83315ef0u;
constexpr test::Region Regions[] = {{0u, 0x70000u},
    {0x8330b000u, 0x1000u}, {0x83315000u, 0x5000u}};

enum class Mode { Equal, Different, GetterMissing, SharedMissing,
    PrimaryMissing, ReadyVirtual };
constexpr Mode Cases[] = {Mode::Equal, Mode::Different,
    Mode::GetterMissing, Mode::SharedMissing,
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

family::Registers FromPpc(PPCContext& c)
{
    family::Registers state{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        state.r[i] = fields[i]->u64;
    state.lr = c.lr; state.ctr = c.ctr.u64;
    state.xer_so = c.xer.so;
    state.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return state;
}

bool SameSelected(PPCContext& c, const family::Registers& state)
{
    const auto left = FromPpc(c);
    for (unsigned index : {1u, 3u, 9u, 10u, 11u, 12u, 29u, 30u, 31u})
        if (left.r[index] != state.r[index]) return false;
    return left.lr == state.lr && left.ctr == state.ctr &&
        left.xer_so == state.xer_so && left.cr6 == state.cr6;
}

struct Services final : ManagerFacadeServices,
    registered_constructor_family::RegistrationServices,
    registered_callback_family::Services,
    ObjectRegistrationServices, family::VirtualServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    explicit Services(test::GuestWindow& window, Mode selected)
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
        if (method != AllocateMethod || manager != Manager || alignment != 8 ||
            (mode != Mode::GetterMissing && mode != Mode::SharedMissing &&
                mode != Mode::PrimaryMissing))
            throw std::runtime_error("unexpected allocation");
        if (mode == Mode::GetterMissing && bytes == 376) return AssociatedNew;
        if (mode == Mode::SharedMissing && bytes == 376) return SharedNew;
        if (mode == Mode::PrimaryMissing && bytes == 376) return PrimaryNew;
        throw std::runtime_error("unexpected allocation size");
    }
    std::uint64_t Register(GuestAddress target, std::uint64_t r3,
        GuestAddress caller_sp) override
    {
        events.push_back({'R', {target, r3, caller_sp, 0, 0}});
        if (mode != Mode::GetterMissing || target != 0x82611008u ||
            caller_sp != Stack - 240u)
            throw std::runtime_error("unexpected constructor registration");
        return 0x1234000000000000ull;
    }
    std::uint64_t CallExternalGetter(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected external getter"); }
    std::uint64_t CallExternalRegistration(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected external registration"); }
    std::uint64_t CallReadyMethod(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected nested ready call"); }
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
    { throw std::runtime_error("unexpected graph ready call"); }
    void Call(GuestAddress target, GuestMemory& guest,
        family::Registers& state) override
    {
        events.push_back({'V', {target, state.r[3], state.r[1], state.lr,
            state.ctr}});
        if (mode != Mode::ReadyVirtual || target != ReadyMethod ||
            Address(state.r[3]) != Self ||
            Address(state.r[1]) != Stack - 112u)
            throw std::runtime_error("unexpected virtual call");
        guest.WriteU32(Self + 208u, 0x12345678u);
        state.r[3] = 0x1122334455667788ull;
    }
};

Services* active = nullptr;

void Initialize(GuestMemory& memory, Mode mode)
{
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(RegistrationMode, mode == Mode::ReadyVirtual ? 1u : 0u);
    memory.WriteU32(ListHead, 0x34000u);
    memory.WriteU32(AssociatedGlobal,
        mode == Mode::GetterMissing ? 0u :
        mode == Mode::Different ? Associated : Self);
    memory.WriteU32(SingletonGlobal, Self);
    memory.WriteU32(SharedGlobal,
        mode == Mode::SharedMissing ? 0u : Shared);
    memory.WriteU32(PrimaryGlobal,
        mode == Mode::PrimaryMissing ? 0u : Primary);
    memory.WriteU32(SecondaryGlobal, Secondary);
    memory.WriteU32(Self, SelfVtable);
    memory.WriteU32(SelfVtable + 124u, ReadyMethod | 3u);
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
    context.r3.u64 = 0xabcdef0000005678ull;
    context.r9.u64 = 0x1111222233334444ull;
    context.r10.u64 = 0x2222333344445555ull;
    context.r11.u64 = 0x3333444455556666ull;
    context.r12.u64 = 0x4444555566667777ull;
    context.r29.u64 = 0x5555666677778888ull;
    context.r30.u64 = 0x6666777788889999ull;
    context.r31.u64 = 0x777788889999aaaauLL;
    context.lr = 0x88889999aaaabbbbull;
    context.ctr.u64 = 0x9999aaaabbbbccccull;
    context.xer.so = 1;
    context.cr6 = {0, 1, 0, {1}};
    auto state = FromPpc(context);
    active = &expected;
    __imp__sub_82627230(context, original.Bytes());
    active = nullptr;
    family::Dependencies dependencies{actual, actual, actual, actual, actual};
    if (!family::Apply(0x82627230u, actual.memory, dependencies, state))
        throw std::runtime_error("missing registration-post entry");

    bool same = SameSelected(context, state) &&
        expected.events == actual.events &&
        std::memcmp(original.Bytes(), recovered.Bytes(), Stack - 0x1000u) == 0;
    for (const auto region : Regions)
        if (region.base != 0)
            same &= std::memcmp(original.Bytes() + region.base,
                recovered.Bytes() + region.base, region.size) == 0;
    for (GuestAddress offset : {8u, 32u, 28u, 24u, 20u, 16u, 12u, 112u})
        same &= expected.memory.ReadU32(Stack - offset) ==
            actual.memory.ReadU32(Stack - offset);
    const unsigned wanted_events = mode == Mode::GetterMissing ? 2u :
        mode == Mode::SharedMissing ? 1u :
        mode == Mode::PrimaryMissing ? 2u :
        mode == Mode::ReadyVirtual ? 1u : 0u;
    same &= expected.events.size() == wanted_events;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL registration-post case %u mode %u r3 %llx/%llx events %zu/%zu\n",
            ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(state.r[3]),
            expected.events.size(), actual.events.size());
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
    auto state = FromPpc(c);
    active->Call(target, active->memory, state);
    c.r3.u64 = state.r[3];
}

int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS object-registration-post 1 new entry %zu cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 82627230 PPC; six accepted direct lower semantics; vtable+124 guest callback; nested ABI scratch, MMIO, faults, concurrency and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

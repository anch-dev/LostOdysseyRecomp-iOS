#include "lo_semantics/object_child_float_record_chain.h"
#include "lo_semantics/object_startup.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace chain = object_child_float_record_chain;
namespace child = object_child_float;
namespace post = object_registration_post;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0xd0000u;
constexpr GuestAddress Parent = 0x20000u, ParentList = 0x30000u;
constexpr GuestAddress Child = 0x40000u, ChildList = 0x50000u;
constexpr GuestAddress Node = 0x60000u, Property = 0x70000u;
constexpr GuestAddress OptionalList = 0x80000u;
constexpr GuestAddress Singleton = 0x90000u, Associated = 0x92000u;
constexpr GuestAddress Shared = 0x94000u, Primary = 0x96000u;
constexpr GuestAddress Secondary = 0x98000u;
constexpr GuestAddress Manager = 0x9a000u, ManagerVtable = 0x9a100u;
constexpr GuestAddress Candidate = 0xa0000u, CandidateVtable = 0xb0000u;
constexpr GuestAddress MissNode = 0xc0000u, Flag = 0xc1000u;
constexpr GuestAddress SingletonGlobal = 0x833189ecu;
constexpr GuestAddress AssociatedGlobal = 0x83318900u;
constexpr GuestAddress SharedGlobal = 0x83315f60u;
constexpr GuestAddress PrimaryGlobal = 0x83315f9cu;
constexpr GuestAddress SecondaryGlobal = 0x83315f7cu;
constexpr GuestAddress ReadyGate = 0x83315ed8u;
constexpr GuestAddress ListHead = 0x83315ef0u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress ChildMethod = 0xc0000u;
constexpr GuestAddress ReadyMethod = 0x823456a0u;
constexpr GuestAddress RegisteredVtable = 0x82005160u;
constexpr GuestAddress BaseSingle = 0x82000e50u;
constexpr GuestAddress NegativeSingle = 0x82000e40u;
constexpr std::uint64_t FullSingleton = 0x1234567800090000ull;
constexpr test::Region Regions[] = {{0u, 0x100000u},
    {0x82000000u, 0x1000u}, {0x82005000u, 0x1000u},
    {0x8330b000u, 0x1000u}, {0x83315000u, 0x5000u}};

enum class Mode { Miss, Existing, LazyChild, ParentReady };
constexpr Mode Cases[] = {Mode::Miss, Mode::Existing,
    Mode::LazyChild, Mode::ParentReady};

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

child::Registers FromPpc(PPCContext& c)
{
    child::Registers state{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        state.r[i] = fields[i]->u64;
    state.lr = c.lr; state.ctr = c.ctr.u64;
    state.f0_bits = c.f0.u64; state.f1_bits = c.f1.u64;
    state.f13_bits = c.f13.u64; state.f30_bits = c.f30.u64;
    state.f31_bits = c.f31.u64; state.cached_fp_control = c.fpscr.csr;
    state.xer_so = c.xer.so; state.xer_ca = c.xer.ca;
    state.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return state;
}
void ToPpc(PPCContext& c, const child::Registers& state)
{
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = state.r[i];
    c.lr = state.lr; c.ctr.u64 = state.ctr;
    c.f0.u64 = state.f0_bits; c.f1.u64 = state.f1_bits;
    c.f13.u64 = state.f13_bits; c.f30.u64 = state.f30_bits;
    c.f31.u64 = state.f31_bits; c.fpscr.csr = state.cached_fp_control;
    c.xer.so = state.xer_so; c.xer.ca = state.xer_ca;
    c.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, {state.cr6.un}};
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

bool SameSelected(PPCContext& c, const child::Registers& state)
{
    const auto actual = FromPpc(c);
    for (unsigned index : {1u, 3u, 11u, 12u, 27u, 28u, 29u, 30u, 31u})
        if (actual.r[index] != state.r[index]) return false;
    return actual.lr == state.lr && actual.ctr == state.ctr &&
        actual.f0_bits == state.f0_bits &&
        actual.f1_bits == state.f1_bits &&
        actual.f13_bits == state.f13_bits &&
        actual.f30_bits == state.f30_bits &&
        actual.f31_bits == state.f31_bits &&
        actual.cached_fp_control == state.cached_fp_control &&
        actual.xer_so == state.xer_so && actual.xer_ca == state.xer_ca &&
        actual.cr6 == state.cr6;
}

struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};

struct Services final : ManagerFacadeServices,
    registered_constructor_family::RegistrationServices,
    registered_callback_family::Services,
    ObjectRegistrationServices, post::VirtualServices,
    object_float_record_post_chain::PostObserver,
    child::NativeServices
{
    GuestMemory memory;
    Mode mode;
    unsigned allocations = 0, fp_calls = 0;
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
        if (mode == Mode::Miss || mode == Mode::Existing ||
            allocations++ != 0 || method != AllocateMethod ||
            manager != Manager || bytes != 376u || alignment != 8u)
            throw std::runtime_error("unexpected singleton allocation");
        return FullSingleton;
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
    { throw std::runtime_error("unexpected primary getter"); }
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
        const GuestAddress expected_sp = Stack -
            (mode == Mode::ParentReady ? 496u : 352u);
        if (target != 0x82627230u || incoming_r3 != FullSingleton ||
            caller_sp != expected_sp || singleton != Singleton)
            throw std::runtime_error("unexpected post entry");
    }
    void Call(GuestAddress target, GuestMemory& guest,
        post::Registers& state) override
    {
        events.push_back({'V', {target, state.r[3], state.r[1],
            state.lr, state.ctr}});
        const GuestAddress expected_sp = Stack - 608u;
        if (mode != Mode::ParentReady || target != ReadyMethod ||
            Address(state.r[3]) != Singleton ||
            Address(state.r[1]) != expected_sp)
            throw std::runtime_error("unexpected post virtual call");
        guest.WriteU32(Singleton + 208u, 0x12345678u);
        state.r[3] = 0x1122334455667788ull;
    }
    void SetHostFpControl(std::uint32_t control) override
    { ++fp_calls; PPCFPSCRRegister{}.setcsr(control); }
    void CallGuest(GuestAddress target, GuestMemory&,
        child::Registers& state) override
    {
        events.push_back({'C', {target, state.r[1], state.lr,
            state.r[3], state.ctr}});
        if (target != ChildMethod || Address(state.r[3]) != Candidate)
            throw std::runtime_error("unexpected child virtual call");
        state.f1_bits = std::bit_cast<std::uint64_t>(0.5);
        state.r[10] = 0x1122334400005678ull;
    }
};

Services* active = nullptr;

void Initialize(GuestMemory& memory, Mode mode)
{
    memory.WriteU32(BaseSingle, 0x3f800000u);
    memory.WriteU32(NegativeSingle, 0xbf800000u);
    memory.WriteU32(Parent + 244u, Flag);
    memory.WriteU32(Flag + 60u, 0u);
    memory.WriteU32(Parent + 116u, ParentList);
    memory.WriteU32(Parent + 120u, 1u);
    memory.WriteU32(ParentList, Child);
    memory.WriteU32(Child + 216u, ChildList);
    memory.WriteU32(Child + 220u, 1u);
    memory.WriteU32(ChildList, Node);
    memory.WriteU32(Node + 72u, Property);
    memory.WriteU32(Node + 76u, OptionalList);
    memory.WriteU32(Node + 80u, 1u);
    memory.WriteU32(Property + 80u, 0u);
    memory.WriteU32(Property + 72u, 0x3fc00000u);
    memory.WriteU32(OptionalList, Candidate);
    memory.WriteU32(Candidate, CandidateVtable);
    memory.WriteU32(CandidateVtable + 332u, ChildMethod | 3u);
    memory.WriteU32(Candidate + 52u,
        mode == Mode::Miss ? MissNode : Singleton);
    memory.WriteU32(MissNode + 60u, 0u);

    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(SingletonGlobal,
        mode == Mode::Miss || mode == Mode::Existing ? Singleton : 0u);
    memory.WriteU32(AssociatedGlobal, Associated);
    memory.WriteU32(SharedGlobal, Shared);
    memory.WriteU32(PrimaryGlobal, Primary);
    memory.WriteU32(SecondaryGlobal, Secondary);
    memory.WriteU32(ReadyGate, mode == Mode::ParentReady ? 1u : 0u);
    memory.WriteU32(ListHead, 0x9c000u);
    memory.WriteU32(RegisteredVtable + 124u, ReadyMethod | 3u);
}

bool Check(Mode mode, unsigned ordinal)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    Services expected(original, mode), actual(recovered, mode);
    Initialize(expected.memory, mode);
    Initialize(actual.memory, mode);
    PPCContext context{};
    context.r1.u64 = 0x12345678000d0000ull;
    context.r3.u64 = 0xabcdef1200000000ull |
        (mode == Mode::ParentReady ? Parent : Child);
    context.r4.u64 = 1u; context.r5.u64 = 0u;
    context.r12.u64 = 0x2233445566778899ull;
    context.r27.u64 = 0x33445566778899aaull;
    context.r28.u64 = 0x445566778899aabbull;
    context.r29.u64 = 0x5566778899aabbccull;
    context.r30.u64 = 0x66778899aabbccdduLL;
    context.r31.u64 = 0x778899aabbccddeeull;
    context.lr = 0x8899aabbccddeeffull;
    context.ctr.u64 = 0x99aabbccddeeff00ull;
    context.f0.u64 = std::bit_cast<std::uint64_t>(-7.0);
    context.f1.u64 = std::bit_cast<std::uint64_t>(6.0);
    context.f13.u64 = std::bit_cast<std::uint64_t>(-8.0);
    context.f30.u64 = std::bit_cast<std::uint64_t>(-9.0);
    context.f31.u64 = std::bit_cast<std::uint64_t>(-10.0);
    context.fpscr.csr = 0x9fc0u;
    context.xer.so = 1; context.xer.ca = 1;
    context.cr6 = {0, 1, 0, {1}};
    auto state = FromPpc(context);
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active = &expected;
    if (mode == Mode::ParentReady)
        __imp__sub_822C5E58(context, original.Bytes());
    else
        __imp__sub_822C5F28(context, original.Bytes());
    active = nullptr;
    const auto original_host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    object_float_record_post_chain::Dependencies record{
        actual, actual, actual, actual, actual, actual};
    chain::Dependencies dependencies{actual, record};
    if (!chain::Apply(mode == Mode::ParentReady ? 0x822c5e58u :
            0x822c5f28u, actual.memory, dependencies, state))
        throw std::runtime_error("missing child-float-record chain");
    const auto recovered_host = PPCFPSCRRegister{}.getcsr();

    bool same = SameSelected(context, state) &&
        expected.events == actual.events &&
        expected.allocations == actual.allocations &&
        actual.fp_calls == 1u && original_host == recovered_host &&
        std::memcmp(original.Bytes(), recovered.Bytes(), Stack - 0x1000u) == 0;
    for (const auto region : Regions)
        if (region.base != 0)
            same &= std::memcmp(original.Bytes() + region.base,
                recovered.Bytes() + region.base, region.size) == 0;
    const std::vector<char> wanted = mode == Mode::Miss ?
        std::vector<char>{} : mode == Mode::Existing ?
        std::vector<char>{'C'} : mode == Mode::LazyChild ?
        std::vector<char>{'A', 'Q', 'C'} :
        std::vector<char>{'A', 'Q', 'V', 'C'};
    same &= expected.events.size() == wanted.size();
    if (expected.events.size() == wanted.size())
        for (std::size_t i = 0; i < wanted.size(); ++i)
            same &= expected.events[i].kind == wanted[i];
    if (mode == Mode::LazyChild || mode == Mode::ParentReady)
        same &= expected.memory.ReadU32(Singleton + 60u) == Associated &&
            expected.memory.ReadU32(Singleton + 196u) == Shared &&
            expected.memory.ReadU32(Singleton + 52u) == Primary;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL child-float-record-chain case %u mode %u r3 %llx/%llx events %zu/%zu f1 %llx/%llx\n",
            ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(state.r[3]),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(context.f1.u64),
            static_cast<unsigned long long>(state.f1_bits));
        return false;
    }
    return true;
}
} // namespace

void OriginalSave27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 27u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u - (31u - i) * 8u),
            fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalSave29(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 29u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u - (31u - i) * 8u),
            fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 27u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory,
            Address(c.r1.u64 - 16u - (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
void OriginalRestore29(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 29u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory,
            Address(c.r1.u64 - 16u - (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
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
    if (target == ReadyMethod)
    {
        auto state = PostFromPpc(c);
        active->Call(target, active->memory, state);
        c.r3.u64 = state.r[3];
    }
    else
    {
        auto state = FromPpc(c);
        active->CallGuest(target, active->memory, state);
        ToPpc(c, state);
    }
}

int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS object-child-float-record-chain 0 new entries %zu cases\n",
            std::size(Cases));
        std::puts("LIMIT five actual PPC bodies; accepted allocation/registration lowers; child and post virtual guests, generic volatile ABI, nested stack scratch, FP edge classes, faults, MMIO and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

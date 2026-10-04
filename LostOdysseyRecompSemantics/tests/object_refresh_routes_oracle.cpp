#include "lo_semantics/object_refresh_routes.h"
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
namespace family = object_refresh_routes;
namespace child = object_child_float;
namespace post = object_registration_post;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u;
constexpr GuestAddress Owner = 0x10000u, Parent = 0x20000u;
constexpr GuestAddress ParentList = 0x30000u, Child = 0x40000u;
constexpr GuestAddress ChildList = 0x50000u, Node = 0x60000u;
constexpr GuestAddress Property = 0x70000u, Array = 0x80000u;
constexpr GuestAddress Singleton = 0x90000u, Associated = 0x92000u;
constexpr GuestAddress Shared = 0x94000u, Primary = 0x96000u;
constexpr GuestAddress Secondary = 0x98000u;
constexpr GuestAddress Manager = 0x9a000u, ManagerVtable = 0x9a100u;
constexpr GuestAddress Flag = 0xc1000u, OwnerVtable = 0xc2000u;
constexpr GuestAddress ChildVtable = 0xc3000u, NodeVtable = 0xc4000u;
constexpr GuestAddress FlagObject = 0xc5000u;
constexpr GuestAddress VirtualBase = 0xd0000u;
constexpr GuestAddress BaseSingle = 0x82000e50u;
constexpr GuestAddress NegativeSingle = 0x82000e40u;
constexpr GuestAddress TickSingle = 0x82189400u;
constexpr GuestAddress RefreshGate = 0x83235098u;
constexpr GuestAddress SingletonGlobal = 0x833189ecu;
constexpr GuestAddress AssociatedGlobal = 0x83318900u;
constexpr GuestAddress SharedGlobal = 0x83315f60u;
constexpr GuestAddress PrimaryGlobal = 0x83315f9cu;
constexpr GuestAddress SecondaryGlobal = 0x83315f7cu;
constexpr GuestAddress RegistrationMode = 0x83315ed8u;
constexpr GuestAddress ListHead = 0x83315ef0u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr test::Region Regions[] = {{0u, 0x130000u},
    {0x82000000u, 0x1000u}, {0x82189000u, 0x1000u},
    {0x83235000u, 0x1000u}, {0x8330b000u, 0x1000u},
    {0x83315000u, 0x5000u}};

enum class Mode { Restricted, FreshEmpty, FreshChild, FreshHole,
    ExistingMatch, UpperGateOff, UpperSkipFlag, UpperChild };
constexpr Mode Cases[] = {Mode::Restricted, Mode::FreshEmpty,
    Mode::FreshChild, Mode::FreshHole, Mode::ExistingMatch,
    Mode::UpperGateOff, Mode::UpperSkipFlag, Mode::UpperChild};

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
    for (unsigned i = 0; i < 32u; ++i) state.r[i] = fields[i]->u64;
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
    for (unsigned i = 0; i < 32u; ++i) fields[i]->u64 = state.r[i];
    c.lr = state.lr; c.ctr.u64 = state.ctr;
    c.f0.u64 = state.f0_bits; c.f1.u64 = state.f1_bits;
    c.f13.u64 = state.f13_bits; c.f30.u64 = state.f30_bits;
    c.f31.u64 = state.f31_bits; c.fpscr.csr = state.cached_fp_control;
    c.xer.so = state.xer_so; c.xer.ca = state.xer_ca;
    c.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, {state.cr6.un}};
}
bool SameSelected(PPCContext& c, const child::Registers& state)
{
    const auto actual = FromPpc(c);
    for (unsigned index : {1u, 3u, 11u, 12u, 22u, 23u, 24u, 25u,
            26u, 27u, 28u, 29u, 30u, 31u})
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
    family::PpcBoundaryServices
{
    GuestMemory memory;
    Mode mode;
    unsigned fp_calls = 0;
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
        throw std::runtime_error("unexpected singleton allocation");
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
        throw std::runtime_error("unexpected post entry");
    }
    void Call(GuestAddress, GuestMemory&, post::Registers&) override
    { throw std::runtime_error("unexpected post virtual call"); }
    void SetHostFpControl(std::uint32_t control) override
    { ++fp_calls; PPCFPSCRRegister{}.setcsr(control); }
    void CallDirect(GuestAddress target, GuestMemory& guest,
        child::Registers& state) override
    {
        events.push_back({'D', {target, state.r[1], state.lr,
            state.r[3], state.ctr}});
        if (target == 0x822c7388u)
        {
            state.f1_bits = std::bit_cast<std::uint64_t>(0.5);
            return;
        }
        if (target == 0x825f41e8u &&
            (mode == Mode::FreshChild || mode == Mode::UpperChild))
        {
            guest.WriteU32(Owner + 628u, Array);
            guest.WriteU32(Owner + 632u, 1u);
            guest.WriteU32(Array, Node);
            return;
        }
        if (target == 0x822c42d8u && mode == Mode::FreshHole)
        {
            guest.WriteU32(Owner + 628u, Array);
            guest.WriteU32(Owner + 632u, 1u);
            state.r[3] = 0;
            return;
        }
        throw std::runtime_error("unexpected direct boundary");
    }
    void CallVirtual(GuestAddress target, GuestMemory&,
        child::Registers& state) override
    {
        events.push_back({'V', {target, state.r[1], state.lr,
            state.r[3], state.ctr}});
        if (target == VirtualBase + 264u)
            state.r[3] = Node;
        else if (target == VirtualBase + 584u)
            state.r[3] = FlagObject;
        else if (target != VirtualBase + 16u &&
            target != VirtualBase + 20u &&
            target != VirtualBase + 60u)
            throw std::runtime_error("unexpected virtual boundary");
    }
    void CallGuest(GuestAddress target, GuestMemory& guest,
        child::Registers& state) override
    { CallVirtual(target, guest, state); }
};
Services* active = nullptr;

void Initialize(GuestMemory& memory, Mode mode)
{
    memory.WriteU32(BaseSingle, 0x3f800000u);
    memory.WriteU32(NegativeSingle, 0xbf800000u);
    memory.WriteU32(TickSingle, 0x3f800000u);
    memory.WriteU32(RefreshGate, mode == Mode::UpperGateOff ? 0u : 1u);
    memory.WriteU32(Owner, OwnerVtable);
    memory.WriteU32(OwnerVtable + 584u, (VirtualBase + 584u) | 3u);
    memory.WriteU32(OwnerVtable + 288u, (VirtualBase + 288u) | 3u);
    memory.WriteU32(Owner + 624u,
        mode == Mode::UpperSkipFlag ? 0u : Parent);
    memory.WriteU32(Owner + 628u, Array);
    memory.WriteU32(Owner + 632u, mode == Mode::ExistingMatch ? 1u : 0u);
    memory.WriteU32(Owner + 668u, 0x00400000u);
    memory.WriteU32(Owner + 708u, 0x40000000u);
    memory.WriteU32(Owner + 712u, 0u);
    memory.WriteU32(Owner + 724u, 0x40400000u);
    memory.WriteU32(Owner + 732u,
        mode == Mode::UpperChild ? 0x20000000u : 0u);
    memory.WriteU32(Owner + 80u,
        mode == Mode::UpperChild ? 0x80000000u : 0u);
    WriteU64(memory, Owner + 8u,
        mode == Mode::Restricted ? 0x600u : 0u);
    memory.WriteU32(Owner + 40u, 0u);
    memory.WriteU32(FlagObject + 60u, 0x80000000u);
    memory.WriteU32(Parent + 244u, Flag);
    memory.WriteU32(Flag + 60u, 0u);
    memory.WriteU32(Parent + 72u, 0x3f800000u);
    memory.WriteU32(Parent + 76u, 0x3f800000u);
    memory.WriteU32(Parent + 24u, 0u);
    memory.WriteU32(Parent + 116u, ParentList);
    memory.WriteU32(Parent + 120u,
        mode == Mode::FreshEmpty ? 0u : 1u);
    memory.WriteU32(ParentList, mode == Mode::FreshHole ? 0u : Child);
    memory.WriteU32(Child, ChildVtable);
    memory.WriteU32(ChildVtable + 264u, (VirtualBase + 264u) | 3u);
    memory.WriteU32(Child + 216u, ChildList);
    memory.WriteU32(Child + 220u, 1u);
    memory.WriteU32(ChildList, Node);
    memory.WriteU32(Node, NodeVtable);
    memory.WriteU32(Node + 4u, Child);
    memory.WriteU32(NodeVtable + 16u, (VirtualBase + 16u) | 3u);
    memory.WriteU32(NodeVtable + 20u, (VirtualBase + 20u) | 3u);
    memory.WriteU32(NodeVtable + 60u, (VirtualBase + 60u) | 3u);
    memory.WriteU32(Array,
        mode == Mode::ExistingMatch ? Node : 0u);
    memory.WriteU32(Node + 72u, Property);
    memory.WriteU32(Property + 72u, 0x3fc00000u);
    memory.WriteU32(Property + 80u, 0u);
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(SingletonGlobal, Singleton);
    memory.WriteU32(AssociatedGlobal, Associated);
    memory.WriteU32(SharedGlobal, Shared);
    memory.WriteU32(PrimaryGlobal, Primary);
    memory.WriteU32(SecondaryGlobal, Secondary);
    memory.WriteU32(RegistrationMode, 0u);
    memory.WriteU32(ListHead, 0x9c000u);
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
    context.r1.u64 = 0x1234567800110000ull;
    context.r3.u64 = 0xabcdef1200000000ull | Owner;
    context.r4.u64 = 0x1111222233334444ull;
    context.r5.u64 = 0x2222333344445555ull;
    context.r12.u64 = 0x3333444455556666ull;
    context.r22.u64 = 0x4444555566667777ull;
    context.r23.u64 = 0x5555666677778888ull;
    context.r24.u64 = 0x6666777788889999ull;
    context.r25.u64 = 0x777788889999aaaauLL;
    context.r26.u64 = 0x88889999aaaabbbbull;
    context.r27.u64 = 0x9999aaaabbbbccccull;
    context.r28.u64 = 0xaaaabbbbccccddddull;
    context.r29.u64 = 0xbbbbccccddddeeeeull;
    context.r30.u64 = 0xccccddddeeeeffffull;
    context.r31.u64 = 0xddddeeeeffff0000ull;
    context.lr = 0xeeeeffff00001111ull;
    context.ctr.u64 = 0xffff000011112222ull;
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
    if (mode >= Mode::UpperGateOff)
        __imp__sub_8260A3F0(context, original.Bytes());
    else
        __imp__sub_826099A8(context, original.Bytes());
    active = nullptr;
    const auto original_host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    object_float_record_post_chain::Dependencies record{
        actual, actual, actual, actual, actual, actual};
    object_child_float_record_chain::Dependencies child_chain{actual, record};
    family::Dependencies dependencies{child_chain, actual};
    if (!family::Apply(mode >= Mode::UpperGateOff ? 0x8260a3f0u :
            0x826099a8u, actual.memory, dependencies, state))
        throw std::runtime_error("missing refresh route entry");
    const auto recovered_host = PPCFPSCRRegister{}.getcsr();

    bool same = SameSelected(context, state) &&
        expected.events == actual.events &&
        original_host == recovered_host &&
        actual.fp_calls == (mode == Mode::Restricted ? 0u : 1u) &&
        std::memcmp(original.Bytes(), recovered.Bytes(), Stack - 0x1000u) == 0 &&
        std::memcmp(original.Bytes() + Stack - 96u,
            recovered.Bytes() + Stack - 96u, 96u) == 0;
    for (const auto region : Regions)
        if (region.base != 0)
            same &= std::memcmp(original.Bytes() + region.base,
                recovered.Bytes() + region.base, region.size) == 0;
    std::vector<char> wanted;
    switch (mode)
    {
    case Mode::Restricted: case Mode::UpperGateOff:
    case Mode::UpperSkipFlag: break;
    case Mode::FreshEmpty: wanted = {'D'}; break;
    case Mode::FreshChild: wanted = {'D', 'V', 'D'}; break;
    case Mode::FreshHole: wanted = {'D', 'D'}; break;
    case Mode::ExistingMatch: wanted = {'D', 'V', 'V'}; break;
    case Mode::UpperChild: wanted = {'D', 'V', 'D', 'V', 'V'}; break;
    }
    same &= expected.events.size() == wanted.size();
    if (expected.events.size() == wanted.size())
        for (std::size_t i = 0; i < wanted.size(); ++i)
            same &= expected.events[i].kind == wanted[i];
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL object-refresh-routes case %u mode %u r3 %llx/%llx events %zu/%zu f1 %llx/%llx\n",
            ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(state.r[3]),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(context.f1.u64),
            static_cast<unsigned long long>(state.f1_bits));
        const auto observed = FromPpc(context);
        for (unsigned index : {1u, 3u, 11u, 12u, 22u, 23u, 24u,
                25u, 26u, 27u, 28u, 29u, 30u, 31u})
            if (observed.r[index] != state.r[index])
                std::fprintf(stderr, " r%u %llx/%llx\n", index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        auto report64 = [](const char* label, std::uint64_t left,
                            std::uint64_t right)
        {
            if (left != right)
                std::fprintf(stderr, " %s %llx/%llx\n", label,
                    static_cast<unsigned long long>(left),
                    static_cast<unsigned long long>(right));
        };
        report64("lr", observed.lr, state.lr);
        report64("ctr", observed.ctr, state.ctr);
        report64("f0", observed.f0_bits, state.f0_bits);
        report64("f1", observed.f1_bits, state.f1_bits);
        report64("f13", observed.f13_bits, state.f13_bits);
        report64("f30", observed.f30_bits, state.f30_bits);
        report64("f31", observed.f31_bits, state.f31_bits);
        report64("cr6.lt", observed.cr6.lt, state.cr6.lt);
        report64("cr6.gt", observed.cr6.gt, state.cr6.gt);
        report64("cr6.eq", observed.cr6.eq, state.cr6.eq);
        report64("cr6.un", observed.cr6.un, state.cr6.un);
        report64("xer.so", observed.xer_so, state.xer_so);
        report64("xer.ca", observed.xer_ca, state.xer_ca);
        report64("cached-fp", observed.cached_fp_control,
            state.cached_fp_control);
        report64("host-fp", original_host, recovered_host);
        report64("fp-calls", mode == Mode::Restricted ? 0u : 1u,
            actual.fp_calls);
        for (std::size_t i = 0; i < expected.events.size() &&
                i < actual.events.size(); ++i)
            if (!(expected.events[i] == actual.events[i]))
            {
                std::fprintf(stderr, " event %zu kind %c/%c\n", i,
                    expected.events[i].kind, actual.events[i].kind);
                for (unsigned j = 0; j < 5u; ++j)
                    report64(" event-arg", expected.events[i].arguments[j],
                        actual.events[i].arguments[j]);
            }
        for (const auto region : Regions)
            for (std::size_t i = 0; i < region.size; ++i)
                if (original.Bytes()[region.base + i] !=
                    recovered.Bytes()[region.base + i])
                {
                    std::fprintf(stderr, " RAM %08llx %02x/%02x\n",
                        static_cast<unsigned long long>(region.base + i),
                        original.Bytes()[region.base + i],
                        recovered.Bytes()[region.base + i]);
                    break;
                }
        return false;
    }
    return true;
}
} // namespace

void OriginalSave25(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 25u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u - (31u - i) * 8u),
            fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalSave28(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 28u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u - (31u - i) * 8u),
            fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
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
void OriginalRestore25(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 25u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory,
            Address(c.r1.u64 - 16u - (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
void OriginalRestore28(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 28u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory,
            Address(c.r1.u64 - 16u - (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
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
void OriginalDirect(std::uint32_t target, PPCContext& c, std::uint8_t*)
{
    auto state = FromPpc(c);
    active->CallDirect(target, active->memory, state);
    ToPpc(c, state);
}
void OriginalIndirect(std::uint32_t target, PPCContext& c, std::uint8_t*)
{
    auto state = FromPpc(c);
    active->CallVirtual(target, active->memory, state);
    ToPpc(c, state);
}

int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS object-refresh-routes 2 new entries %zu cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 826099A8/8260A3F0 PPC bodies; direct 822C7388/825F41E8/822C42D8 and virtual targets remain mutable guest boundaries; generic volatile ABI, nested stack scratch, FP edge classes, faults, MMIO and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

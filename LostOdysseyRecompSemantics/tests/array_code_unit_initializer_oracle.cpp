// Appended after the pinned 822954D8 body by semantic_recovery.py.
#include "lo_semantics/array_code_unit_initializer.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = array_code_unit_initializer;
using recovery_abi::Address;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Stack=0x50000u, Array=0x10000u;
constexpr GuestAddress Manager=0x35000u, Vtable=0x35100u;
constexpr GuestAddress Replacement=0x21000u;
constexpr GuestAddress ManagerGlobal=0x8330b608u;
constexpr GuestAddress Method=0x2401u;
constexpr std::array<Region,2> Regions{{{0,0x60000u},
    {0x8330b000u,0x1000u}}};

enum class Mode {Zero,ExistingManager,InitializeManager,HeaderSaveAlias};
constexpr std::array<Mode,4> Cases{{Mode::Zero,Mode::ExistingManager,
    Mode::InitializeManager,Mode::HeaderSaveAlias}};
using Event=std::array<std::uint64_t,6>;

struct Services final : ArrayResizeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    unsigned initializations=0, resizes=0;
    Services(GuestWindow& window,Mode selected)
        :memory(window.Memory()),mode(selected){}

    void InitializeManager() override
    {
        ++initializations;
        events.push_back({1,memory.ReadU32(ManagerGlobal)});
        if (mode!=Mode::InitializeManager)
            throw std::runtime_error("unexpected manager initialization");
        memory.WriteU32(ManagerGlobal,Manager);
    }

    GuestAddress ResizeStorage(GuestAddress method,GuestAddress manager,
        GuestAddress old_storage,std::uint32_t bytes,
        std::uint32_t argument) override
    {
        ++resizes;
        events.push_back({2,method,manager,old_storage,bytes,argument});
        if (method!=(Method&~3u) || manager!=Manager ||
            old_storage!=0 || bytes!=8 || argument!=8 ||
            memory.ReadU32(ManagerGlobal)!=Manager)
            throw std::runtime_error("accepted array manager/vtable contract");
        return Replacement;
    }
};

Services* active=nullptr;

void Seed(GuestWindow& window,Mode mode)
{
    window.Fill(0);
    auto memory=window.Memory();
    memory.WriteU32(ManagerGlobal,
        mode==Mode::Zero || mode==Mode::InitializeManager ? 0u : Manager);
    memory.WriteU32(Manager,Vtable);
    memory.WriteU32(Vtable+8u,Method);
}

PPCContext Initial(Mode mode)
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0xaabbccdd01020304ull;
    context.r3.u64=0x9876543200000000ull|
        (mode==Mode::HeaderSaveAlias ? Stack-16u : Array);
    context.r4.u64=mode==Mode::Zero ?
        0x778899aa00000000ull : 0x778899aa00000004ull;
    context.r5.u64=0x1111222233334444ull;
    context.r10.u64=0x5555666677778888ull;
    context.r11.u64=0x9999aaaabbbbccccull;
    context.r12.u64=0xddddeeeeffff0000ull;
    context.r31.u64=0x1122334455667788ull;
    return context;
}

family::Registers FromPpc(const PPCContext& context)
{
    return {context.r1.u64,context.lr,context.r3.u64,
        context.r4.u64,context.r5.u64,context.r10.u64,
        context.r11.u64,context.r12.u64,context.r31.u64};
}

bool Same(const family::Registers& left,const family::Registers& right)
{
    return left.sp==right.sp && left.lr==right.lr &&
        left.r3==right.r3 && left.r4==right.r4 &&
        left.r5==right.r5 && left.r10==right.r10 &&
        left.r11==right.r11 && left.r12==right.r12 &&
        left.r31==right.r31;
}

bool ExpectedPath(Mode mode,const Services& services,
    const family::Registers& state)
{
    if (state.sp!=(0x1234567800000000ull|Stack) ||
        state.r3!=(0x9876543200000000ull|
            (mode==Mode::HeaderSaveAlias ? Stack-16u : Array)) ||
        state.r4!=2 || state.r5!=8 || state.r10!=0 ||
        state.r11!=(mode==Mode::Zero ?
            0x778899aa00000000ull : 0x778899aa00000004ull))
        return false;
    if (mode==Mode::Zero)
        return services.initializations==0 && services.resizes==0 &&
            services.memory.ReadU32(ManagerGlobal)==0;
    if (services.resizes!=1 ||
        services.memory.ReadU32(ManagerGlobal)!=Manager)
        return false;
    if (mode==Mode::InitializeManager)
        return services.initializations==1 && services.events.size()==2 &&
            services.events.front()[0]==1 &&
            services.events.back()[0]==2;
    if (mode==Mode::HeaderSaveAlias)
        return services.initializations==0 &&
            state.r31==((std::uint64_t{Replacement}<<32)|4u) &&
            state.lr==4u;
    return services.initializations==0 && services.events.size()==1 &&
        state.r31==0x1122334455667788ull &&
        state.lr==0x01020304u;
}

bool Check(Mode mode)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,mode);Seed(recovered,mode);
    Services expected(original,mode),actual(recovered,mode);
    PPCContext raw=Initial(mode);
    auto state=FromPpc(raw);
    active=&expected;
    __imp__sub_822954D8(raw,original.Bytes());
    active=nullptr;
    const auto original_state=FromPpc(raw);
    if (!ExpectedPath(mode,expected,original_state))
        throw std::runtime_error("fixture missed original array path");
    if (!family::Apply(0x822954d8u,actual.memory,actual,state))
        throw std::runtime_error("recovered array entry missing");
    const bool same=Same(original_state,state) &&
        expected.events==actual.events &&
        original.EqualCommitted(recovered);
    if (!same)
        std::fprintf(stderr,"FAIL array-code-unit mode=%u "
            "r3=%llx/%llx lr=%llx/%llx r31=%llx/%llx "
            "events=%zu/%zu RAM=%u\n",static_cast<unsigned>(mode),
            static_cast<unsigned long long>(original_state.r3),
            static_cast<unsigned long long>(state.r3),
            static_cast<unsigned long long>(original_state.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(original_state.r31),
            static_cast<unsigned long long>(state.r31),
            expected.events.size(),actual.events.size(),
            original.EqualCommitted(recovered));
    return same;
}
} // namespace

void OriginalResize(PPCContext& context,std::uint8_t*)
{
    ResizeArray(active->memory,*active,Address(context.r3.u64),
        Address(context.r4.u64),Address(context.r5.u64));
}

int main()
{
    try
    {
        for (const auto mode:Cases)
            if (!Check(mode)) return 1;
        GuestWindow baseline(Regions),window(Regions);
        Seed(baseline,Mode::Zero);Seed(window,Mode::Zero);
        Services services(window,Mode::Zero);
        const auto initial=FromPpc(Initial(Mode::Zero));
        auto state=initial;
        if (family::Apply(0xffffffffu,services.memory,services,state) ||
            !Same(initial,state) || !services.events.empty() ||
            !baseline.EqualCommitted(window))
            throw std::runtime_error("unknown address changed state or RAM");
        std::printf("PASS array-code-unit-initializer %zu original PPC cases + unknown\n",
            Cases.size());
        std::puts("LIMIT selected wrapper ABI and ordinary RAM; accepted ResizeArray has generic lower ABI and external manager/vtable service; no faults/MMIO/concurrency/runtime acceptance");
        return 0;
    }
    catch (const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

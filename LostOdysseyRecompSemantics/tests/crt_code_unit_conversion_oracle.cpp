// Appended after the two pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_code_unit_conversion.h"
#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_code_unit_conversion;
using recovery_abi::Address;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Stack=0x80000u, Environment=0x60000u;
constexpr GuestAddress Output=0x30000u, Count=0x31000u;
constexpr GuestAddress Record=0x70000u, OtherRecord=0x71000u;
constexpr GuestAddress StaticError=0x83215210u;
constexpr GuestAddress Handler=0x83378e80u;
constexpr std::array<Region,3> Regions{{{0,0x90000u},
    {0x83214000u,0x3000u},{0x83378000u,0x3000u}}};

enum class Mode {Byte,NullCount,CountOnly,NullOutput,ZeroExtent,
    ExcessExtent,Unrepresentable,SecondGetter,Wrapper,StackAlias};
struct Case {GuestAddress address; Mode mode;};
constexpr std::array<Case,10> Cases{{
    {0x82b86ab8u,Mode::Byte},{0x82b86ab8u,Mode::NullCount},
    {0x82b86ab8u,Mode::CountOnly},{0x82b86ab8u,Mode::NullOutput},
    {0x82b86ab8u,Mode::ZeroExtent},{0x82b86ab8u,Mode::ExcessExtent},
    {0x82b86ab8u,Mode::Unrepresentable},
    {0x82b86ab8u,Mode::SecondGetter},
    {0x82b86be0u,Mode::Wrapper},{0x82b86ab8u,Mode::StackAlias}}};
using Event=std::array<std::uint64_t,7>;

struct Services final : CrtThreadDataServices, InvalidParameterServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    unsigned getters=0, handlers=0, traps=0;
    Services(GuestWindow& window,Mode selected)
        :memory(window.Memory()),mode(selected){}
    [[noreturn]] static void Unexpected()
    {throw std::runtime_error("unexpected accepted native boundary");}

    std::uint64_t GetTlsValue(std::uint32_t index) override
    {if (index!=1u) Unexpected(); return 0x2401u;}
    void SetTlsValue(std::uint32_t,std::uint64_t) override {Unexpected();}
    std::uint64_t CallThreadDataGetter(GuestAddress,std::uint64_t) override
    {Unexpected();}
    std::uint64_t CallThreadDataGetterWithState(GuestAddress function,
        std::uint64_t context,CrtThreadDataCall& call) override
    {
        if (function!=0x2400u || context!=0x3456u) Unexpected();
        ++getters;
        events.push_back({1,function,context,call.thread_environment,getters});
        call.thread_environment=0x1234567800000000ull|Environment;
        if (mode==Mode::SecondGetter && getters==2u)
            return 0x8877665500000000ull|OtherRecord;
        if (mode==Mode::ZeroExtent)
            return 0; // Accepted static errno address with sign extension.
        return 0x8877665500000000ull|Record;
    }
    std::uint64_t AllocateThreadData(std::uint32_t,std::uint32_t) override
    {if (mode==Mode::ZeroExtent) return 0; Unexpected();}
    std::uint64_t BindThreadData(GuestAddress,std::uint64_t,
        std::uint64_t) override {Unexpected();}
    void FreeThreadData(std::uint64_t) override {Unexpected();}
    void CallHandler(GuestMemory& m,GuestAddress function,
        InvalidParameterCall& call) override
    {
        ++handlers;
        events.push_back({2,function,call.arguments[0],call.arguments[1],
            call.arguments[4],call.thread_environment,
            m.ReadU32(mode==Mode::ZeroExtent?StaticError:Record+8u)});
        if (function!=0x2600u) Unexpected();
        call.arguments[0]=0xfaceb00c12345678ull;
        call.arguments[1]=0xaabbccdd11223344ull;
        call.thread_environment=0xaabbccdd00000000ull|Environment;
    }
    void Trap(const InvalidParameterCall&) override {++traps;}
};

Services* active=nullptr;

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers s{};
    s.sp=c.r1.u64;s.lr=c.lr;
    s.r={0,0,0,c.r3.u64,c.r4.u64,c.r5.u64,c.r6.u64,c.r7.u64,
        c.r8.u64,c.r9.u64,c.r10.u64,c.r11.u64,c.r12.u64,c.r13.u64};
    s.xer_so=c.xer.so;
    s.cr0={std::uint8_t(c.cr0.lt),std::uint8_t(c.cr0.gt),
        std::uint8_t(c.cr0.eq),std::uint8_t(c.cr0.so)};
    s.cr6={std::uint8_t(c.cr6.lt),std::uint8_t(c.cr6.gt),
        std::uint8_t(c.cr6.eq),std::uint8_t(c.cr6.so)};
    return s;
}

void ToPpc(PPCContext& c,const family::Registers& s)
{
    c.r1.u64=s.sp;c.lr=s.lr;
    c.r3.u64=s.r[3];c.r4.u64=s.r[4];c.r5.u64=s.r[5];
    c.r6.u64=s.r[6];c.r7.u64=s.r[7];c.r8.u64=s.r[8];
    c.r9.u64=s.r[9];c.r10.u64=s.r[10];c.r11.u64=s.r[11];
    c.r12.u64=s.r[12];c.r13.u64=s.r[13];
    c.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,{s.cr0.so}};
    c.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,{s.cr6.so}};
}

bool Same(const family::Registers& a,const family::Registers& b)
{
    return a.sp==b.sp && a.lr==b.lr && a.r==b.r &&
        a.xer_so==b.xer_so && a.cr0.lt==b.cr0.lt &&
        a.cr0.gt==b.cr0.gt && a.cr0.eq==b.cr0.eq &&
        a.cr0.so==b.cr0.so && a.cr6.lt==b.cr6.lt &&
        a.cr6.gt==b.cr6.gt && a.cr6.eq==b.cr6.eq &&
        a.cr6.so==b.cr6.so;
}

void Seed(GuestWindow& window)
{
    window.Fill(0x5au);
    auto m=window.Memory();
    m.WriteU32(Environment+336u,1u);
    m.WriteU32(0x83214d74u,0x3456u);
    m.WriteU32(0x83214d78u,1u);
    m.WriteU32(Handler,0x2601u);
    m.WriteU32(Record+8u,0x789abcdeu);
    m.WriteU32(OtherRecord+8u,0x2468ace0u);
    m.WriteU32(StaticError,0x13579bdfu);
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0xabcdef0101020304ull;
    c.r3.u64=0xabcdef0000000000ull|Count;
    c.r4.u64=0xabcdef0000000000ull|Output;
    c.r5.u64=1;
    c.r6.u64=0x7777000000000041ull;
    c.r7.u64=0x7777000000000007ull;
    c.r8.u64=0x8888000000000008ull;
    c.r9.u64=0x9999000000000009ull;
    c.r10.u64=0xaaaa00000000000aull;
    c.r11.u64=0xbbbb00000000000bull;
    c.r12.u64=0xcccc00000000000cull;
    c.r13.u64=0xdddd000000000000ull|Environment;
    c.xer.so=1;
    c.cr0={1,0,0,{1}};
    c.cr6={0,1,0,{0}};
    switch (item.mode)
    {
    case Mode::NullCount:c.r3.u64=0;break;
    case Mode::CountOnly:c.r4.u64=0;c.r5.u64=0;break;
    case Mode::NullOutput:c.r4.u64=0;c.r5.u64=3;break;
    case Mode::ZeroExtent:c.r5.u64=0;break;
    case Mode::ExcessExtent:c.r5.u64=0x80000000ull;break;
    case Mode::Unrepresentable:c.r6.u64=0x1111000000000100ull;
        c.r5.u64=3;break;
    case Mode::SecondGetter:c.r6.u64=0x1111000000000100ull;break;
    case Mode::StackAlias:c.r3.u64=c.r1.u64-8u;break;
    default:break;
    }
    return c;
}

bool ExpectedPath(const Case& item,const Services& s)
{
    switch (item.mode)
    {
    case Mode::Byte:case Mode::NullCount:case Mode::CountOnly:
    case Mode::NullOutput:case Mode::Wrapper:case Mode::StackAlias:
        return s.getters==0 && s.handlers==0 && s.events.empty();
    case Mode::ZeroExtent:
        return s.getters==1 && s.handlers==1 && s.traps==0 &&
            s.memory.ReadU32(StaticError)==34u;
    case Mode::ExcessExtent:
        return s.getters==1 && s.handlers==1 && s.traps==0 &&
            s.memory.ReadU32(Record+8u)==22u;
    case Mode::Unrepresentable:
        return s.getters==2 && s.handlers==0 && s.traps==0 &&
            s.memory.ReadU32(Record+8u)==42u &&
            s.memory.ReadU8(Output)==0 &&
            s.memory.ReadU8(Output+1u)==0 &&
            s.memory.ReadU8(Output+2u)==0;
    case Mode::SecondGetter:
        return s.getters==2 && s.handlers==0 && s.traps==0 &&
            s.memory.ReadU32(Record+8u)==42u &&
            s.memory.ReadU32(OtherRecord+8u)==0x2468ace0u &&
            s.memory.ReadU8(Output)==0;
    }
    return false;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original);Seed(recovered);
    Services expected(original,item.mode),actual(recovered,item.mode);
    PPCContext raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    if (item.address==0x82b86be0u)
        __imp__sub_82B86BE0(raw,original.Bytes());
    else
        __imp__sub_82B86AB8(raw,original.Bytes());
    active=nullptr;
    if (!ExpectedPath(item,expected))
    {
        std::fprintf(stderr,"fixture path %08X mode=%u getters=%u handlers=%u traps=%u r3=%llX\n",
            item.address,static_cast<unsigned>(item.mode),expected.getters,
            expected.handlers,expected.traps,
            static_cast<unsigned long long>(raw.r3.u64));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if (item.mode==Mode::SecondGetter && raw.r3.u64!=0x2468ace0u)
        throw std::runtime_error("second getter did not supply returned errno");
    if (!family::Apply(item.address,actual.memory,{actual,actual},state))
        throw std::runtime_error("recovered entry missing");
    const auto observed=FromPpc(raw);
    if (!Same(observed,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL code-unit %08X mode=%u r3=%llX/%llX sp=%llX/%llX lr=%llX/%llX events=%zu/%zu RAM=%u\n",
            item.address,static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            expected.events.size(),actual.events.size(),
            original.EqualCommitted(recovered));
        return false;
    }
    return true;
}

class OriginalErrorServices final : public AllocationFailureServices
{
public:
    explicit OriginalErrorServices(PPCContext& context):context_(context){}
    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall call{context_.r13.u64};
        const auto record=GetCrtThreadData(active->memory,*active,call);
        context_.r13.u64=call.thread_environment;
        context_.r3.u64=record;
        return record;
    }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    {Services::Unexpected();}
    std::uint64_t BugCheck(std::uint32_t) override
    {Services::Unexpected();}
    std::uint64_t CallNewHandler(GuestAddress,std::uint64_t) override
    {Services::Unexpected();}
private:
    PPCContext& context_;
};
} // namespace

void OriginalError(PPCContext& context,std::uint8_t*)
{
    auto& m=active->memory;
    context.r12.u64=context.lr;
    m.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
    m.WriteU32(Address(context.r1.u64-96u),context.r1.u32);
    context.r1.u64-=96u;
    OriginalErrorServices services(context);
    context.r3.u64=GetAllocationErrorAddress(services);
    context.r1.u64+=96u;
    context.r12.u64=m.ReadU32(Address(context.r1.u64-8u));
    context.lr=context.r12.u64;
}

void OriginalInvalid(PPCContext& context,std::uint8_t*)
{
    InvalidParameterCall call{{{context.r3.u64,context.r4.u64,
        context.r5.u64,context.r6.u64,context.r7.u64,context.r8.u64,
        context.r9.u64,context.r10.u64}},context.r13.u64};
    context.r3.u64=ReportInvalidParameter(active->memory,*active,call);
    context.r4.u64=call.arguments[1];context.r5.u64=call.arguments[2];
    context.r6.u64=call.arguments[3];context.r7.u64=call.arguments[4];
    context.r8.u64=call.arguments[5];context.r9.u64=call.arguments[6];
    context.r10.u64=call.arguments[7];context.r13.u64=call.thread_environment;
}

void OriginalFill(PPCContext& context,std::uint8_t*)
{
    context.r3.u64=FillGuestMemory(active->memory,context.r3.u32,
        context.r4.u32,context.r5.u32);
}

int main()
{
    try
    {
        for (const auto& item:Cases)
            if (!Check(item)) return 1;
        GuestWindow window(Regions),baseline(Regions);
        Seed(window);Seed(baseline);
        Services services(window,Mode::Byte);
        auto state=FromPpc(Initial(Cases[0]));
        const auto saved=state;
        if (family::Apply(0xffffffffu,services.memory,{services,services},state) ||
            !Same(saved,state) || !services.events.empty() ||
            !window.EqualCommitted(baseline))
            throw std::runtime_error("unknown entry changed state");
        std::printf("PASS crt-code-unit-conversion %zu original PPC cases + unknown\n",
            Cases.size());
        std::puts("LIMIT selected state/RAM; accepted lower ABI and native internals, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

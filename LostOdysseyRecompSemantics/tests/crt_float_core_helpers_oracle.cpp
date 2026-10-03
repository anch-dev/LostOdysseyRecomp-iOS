// Appended after the two pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_float_core_helpers.h"
#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_float_core_helpers;
using recovery_abi::Address;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Input=0x30000u, Output=0x31000u;
constexpr GuestAddress Destination=0x32000u, Source=0x33000u;
constexpr GuestAddress Environment=0x60000u, Record=0x70000u;
constexpr GuestAddress Stack=0x80000u, Handler=0x83378e80u;
constexpr std::array<Region,3> Regions{{{0,0x90000u},
    {0x83214000u,0x3000u},{0x83378000u,0x3000u}}};

enum class Mode {NegativeZero,NormalAlias,Subnormal,Infinity,NanPayload,
    Copy,ExactExtent,ZeroExtent,NullSource,Overlap};
struct Case {GuestAddress address; Mode mode;};
constexpr std::array<Case,10> Cases{{
    {0x8231a390u,Mode::NegativeZero},
    {0x8231a390u,Mode::NormalAlias},
    {0x8231a390u,Mode::Subnormal},
    {0x8231a390u,Mode::Infinity},
    {0x8231a390u,Mode::NanPayload},
    {0x8231b0d0u,Mode::Copy},
    {0x8231b0d0u,Mode::ExactExtent},
    {0x8231b0d0u,Mode::ZeroExtent},
    {0x8231b0d0u,Mode::NullSource},
    {0x8231b0d0u,Mode::Overlap}}};
using Event=std::array<std::uint64_t,8>;

struct Services final : CrtThreadDataServices, InvalidParameterServices
{
    GuestMemory memory;
    std::vector<Event> events;
    PPCContext* original=nullptr;
    family::Registers* recovered=nullptr;
    unsigned getters=0, handlers=0, traps=0;
    explicit Services(GuestWindow& window):memory(window.Memory()){}
    [[noreturn]] static void Unexpected()
    {throw std::runtime_error("unexpected CRT native boundary");}
    std::uint64_t Sp() const
    {return original?original->r1.u64:recovered->sp;}
    std::uint64_t Lr() const
    {return original?original->lr:recovered->lr;}

    std::uint64_t GetTlsValue(std::uint32_t index) override
    {if(index!=1u) Unexpected();return 0x2401u;}
    void SetTlsValue(std::uint32_t,std::uint64_t) override {Unexpected();}
    std::uint64_t CallThreadDataGetter(GuestAddress,std::uint64_t) override
    {Unexpected();}
    std::uint64_t CallThreadDataGetterWithState(GuestAddress function,
        std::uint64_t context,CrtThreadDataCall& call) override
    {
        if(function!=0x2400u || context!=0x3456u) Unexpected();
        ++getters;
        events.push_back({1u,Sp(),Lr(),function,context,
            call.thread_environment,Record,0u});
        call.thread_environment=0x1234567800000000ull|Environment;
        return 0x8877665500000000ull|Record;
    }
    std::uint64_t AllocateThreadData(std::uint32_t,std::uint32_t) override
    {Unexpected();}
    std::uint64_t BindThreadData(GuestAddress,std::uint64_t,
        std::uint64_t) override {Unexpected();}
    void FreeThreadData(std::uint64_t) override {Unexpected();}
    void CallHandler(GuestMemory& m,GuestAddress function,
        InvalidParameterCall& call) override
    {
        if(function!=0x2600u) Unexpected();
        ++handlers;
        events.push_back({2u,Sp(),Lr(),function,call.arguments[0],
            call.thread_environment,m.ReadU32(Record+8u),
            m.ReadU8(Destination)});
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
    s.sp=c.r1.u64;s.lr=c.lr;s.ctr=c.ctr.u64;
    s.r={c.r0.u64,c.r1.u64,c.r2.u64,c.r3.u64,c.r4.u64,c.r5.u64,
        c.r6.u64,c.r7.u64,c.r8.u64,c.r9.u64,c.r10.u64,c.r11.u64,
        c.r12.u64,c.r13.u64,c.r14.u64,c.r15.u64,c.r16.u64,c.r17.u64,
        c.r18.u64,c.r19.u64,c.r20.u64,c.r21.u64,c.r22.u64,c.r23.u64,
        c.r24.u64,c.r25.u64,c.r26.u64,c.r27.u64,c.r28.u64,c.r29.u64,
        c.r30.u64,c.r31.u64};
    s.xer_so=c.xer.so;s.xer_ca=c.xer.ca;
    s.cr0={std::uint8_t(c.cr0.lt),std::uint8_t(c.cr0.gt),
        std::uint8_t(c.cr0.eq),std::uint8_t(c.cr0.so)};
    s.cr6={std::uint8_t(c.cr6.lt),std::uint8_t(c.cr6.gt),
        std::uint8_t(c.cr6.eq),std::uint8_t(c.cr6.so)};
    return s;
}

bool Same(const family::Registers& a,const family::Registers& b)
{
    return a.sp==b.sp && a.lr==b.lr && a.ctr==b.ctr && a.r==b.r &&
        a.xer_so==b.xer_so && a.xer_ca==b.xer_ca &&
        a.cr0.lt==b.cr0.lt && a.cr0.gt==b.cr0.gt &&
        a.cr0.eq==b.cr0.eq && a.cr0.so==b.cr0.so &&
        a.cr6.lt==b.cr6.lt && a.cr6.gt==b.cr6.gt &&
        a.cr6.eq==b.cr6.eq && a.cr6.so==b.cr6.so;
}

std::uint64_t Bits(Mode mode)
{
    switch(mode)
    {
    case Mode::NegativeZero:return 0x8000000000000000ull;
    case Mode::NormalAlias:return 0x3ff0000000000000ull;
    case Mode::Subnormal:return 1ull;
    case Mode::Infinity:return 0x7ff0000000000000ull;
    case Mode::NanPayload:return 0x7ff8000000000001ull;
    default:return 0;
    }
}

void Seed(GuestWindow& window,const Case& item)
{
    window.Fill(0x5au);
    auto m=window.Memory();
    m.WriteU32(Environment+336u,1u);
    m.WriteU32(0x83214d74u,0x3456u);
    m.WriteU32(0x83214d78u,1u);
    m.WriteU32(Handler,0x2601u);
    m.WriteU32(Record+8u,0x12345678u);
    if(item.address==0x8231a390u)
        WriteU64(m,Input,Bits(item.mode));
    else
    {
        m.WriteU8(Source,'A');m.WriteU8(Source+1u,'B');
        m.WriteU8(Source+2u,0);
    }
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0xabcdef0101020304ull;
    c.ctr.u64=0x8877665544332211ull;
    c.r3.u64=0xabcdef0000000000ull|
        (item.address==0x8231a390u?Output:Destination);
    c.r4.u64=item.address==0x8231a390u?
        (0xabcdef0000000000ull|Input):3u;
    c.r5.u64=0xabcdef0000000000ull|Source;
    c.r31.u64=0x3131313100000031ull;
    c.r13.u64=0xdddd000000000000ull|Environment;
    c.xer.so=1;c.xer.ca=1;
    c.cr0={1,0,0,{1}};c.cr6={0,1,0,{1}};
    switch(item.mode)
    {
    case Mode::NormalAlias:c.r3.u64=c.r4.u64;break;
    case Mode::ExactExtent:c.r4.u64=2;break;
    case Mode::ZeroExtent:c.r4.u64=0;break;
    case Mode::NullSource:c.r5.u64=0;break;
    case Mode::Overlap:c.r3.u64=0xabcdef0000000000ull|(Source-1u);break;
    default:break;
    }
    return c;
}

bool ExpectedPath(const Case& item,const Services& services,
    const PPCContext& raw)
{
    const auto m=services.memory;
    if(item.address==0x8231a390u)
    {
        if(services.getters || services.handlers || services.traps)
            return false;
        const auto output=item.mode==Mode::NormalAlias?Input:Output;
        std::uint16_t exponent=0;
        std::uint32_t high=0,low=0;
        switch(item.mode)
        {
        case Mode::NegativeZero:exponent=0x8000u;break;
        case Mode::NormalAlias:exponent=0x3fffu;high=0x80000000u;break;
        case Mode::Subnormal:exponent=0x3bcdu;high=0x80000000u;break;
        case Mode::Infinity:exponent=0x7fffu;high=0x80000000u;break;
        case Mode::NanPayload:exponent=0x7fffu;high=0xc0000000u;
            low=0x800u;break;
        default:return false;
        }
        return m.ReadU16(output)==exponent &&
            m.ReadU32(output+2u)==high && m.ReadU32(output+6u)==low;
    }
    const bool error=item.mode==Mode::ExactExtent ||
        item.mode==Mode::ZeroExtent || item.mode==Mode::NullSource;
    if(error)
    {
        const auto code=item.mode==Mode::ExactExtent?34u:22u;
        if(services.getters!=1 || services.handlers!=1 || services.traps ||
            m.ReadU32(Record+8u)!=code || raw.r3.u64!=code ||
            services.events.size()!=2u || services.events[1][6]!=code)
            return false;
        if(item.mode==Mode::ExactExtent || item.mode==Mode::NullSource)
            return m.ReadU8(Destination)==0;
        return m.ReadU8(Destination)==0x5au;
    }
    if(services.getters || services.handlers || services.traps ||
        raw.r3.u64!=0)
        return false;
    if(item.mode==Mode::Overlap)
        return m.ReadU8(Source-1u)=='A' && m.ReadU8(Source)=='B' &&
            m.ReadU8(Source+1u)==0;
    return m.ReadU8(Destination)=='A' &&
        m.ReadU8(Destination+1u)=='B' && m.ReadU8(Destination+2u)==0;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,item);Seed(recovered,item);
    Services expected(original),actual(recovered);
    auto raw=Initial(item);
    auto state=FromPpc(raw);
    expected.original=&raw;actual.recovered=&state;
    active=&expected;
    if(item.address==0x8231a390u)
        __imp__sub_8231A390(raw,original.Bytes());
    else
        __imp__sub_8231B0D0(raw,original.Bytes());
    active=nullptr;
    if(!ExpectedPath(item,expected,raw))
    {
        std::fprintf(stderr,"fixture path %08X mode=%u getters=%u handlers=%u r3=%llX\n",
            item.address,static_cast<unsigned>(item.mode),expected.getters,
            expected.handlers,static_cast<unsigned long long>(raw.r3.u64));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if(!family::Apply(item.address,actual.memory,{actual,actual},state))
        throw std::runtime_error("recovered entry missing");
    const auto observed=FromPpc(raw);
    if(!Same(observed,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL float-helper %08X mode=%u r3=%llX/%llX sp=%llX/%llX lr=%llX/%llX events=%zu/%zu RAM=%u\n",
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

int main()
{
    try
    {
        for(const auto& item:Cases)
            if(!Check(item)) return 1;
        GuestWindow window(Regions),baseline(Regions);
        Seed(window,Cases[0]);Seed(baseline,Cases[0]);
        Services services(window);
        auto state=FromPpc(Initial(Cases[0]));
        services.recovered=&state;
        const auto saved=state;
        if(family::Apply(0xffffffffu,services.memory,{services,services},state) ||
            !Same(saved,state) || !services.events.empty() ||
            !window.EqualCommitted(baseline))
            throw std::runtime_error("unknown entry changed state");
        std::printf("PASS crt-float-core-helpers %zu original PPC cases + unknown\n",
            Cases.size());
        std::puts("LIMIT selected state/RAM; accepted lower ABI and native internals, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

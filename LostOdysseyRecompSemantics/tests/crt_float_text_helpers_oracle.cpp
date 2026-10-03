// Appended after the three pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_float_text_helpers.h"
#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/memory_move.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=crt_float_text_helpers;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Output=0x30000u,Digits=0x31000u;
constexpr GuestAddress Descriptor=0x32000u,Environment=0x60000u;
constexpr GuestAddress Record=0x70000u,Stack=0x80000u;
constexpr GuestAddress Handler=0x83378e80u;
constexpr GuestAddress LocaleGlobal=0x83215648u;
constexpr GuestAddress LocaleRecord=0x33000u,LocaleText=0x34000u;
constexpr GuestAddress Option=0x832d3cb8u,Template=0x820d3128u;
constexpr std::array<Region,5> Regions{{{0,0x90000u},
    {0x820d3000u,0x1000u},{0x83214000u,0x2000u},
    {0x832d3000u,0x2000u},{0x83378000u,0x3000u}}};

enum class Mode {Pad,Round,Carry,RoundNull,RoundZero,RoundRange,
    SciLower,SciUpperNegative,SciShift,SciNegativeExponent,
    SciTrim,SciHundreds,SciRange,FixedPositive,FixedZeroExponent,
    FixedNegativePad,FixedSpecial,FixedNull};
struct Case {GuestAddress entry;Mode mode;};
constexpr std::array<Case,18> Cases{{
    {0x8231b1b8u,Mode::Pad},{0x8231b1b8u,Mode::Round},
    {0x8231b1b8u,Mode::Carry},{0x8231b1b8u,Mode::RoundNull},
    {0x8231b1b8u,Mode::RoundZero},{0x8231b1b8u,Mode::RoundRange},
    {0x82b7f048u,Mode::SciLower},{0x82b7f048u,Mode::SciUpperNegative},
    {0x82b7f048u,Mode::SciShift},
    {0x82b7f048u,Mode::SciNegativeExponent},
    {0x82b7f048u,Mode::SciTrim},
    {0x82b7f048u,Mode::SciHundreds},
    {0x82b7f048u,Mode::SciRange},
    {0x82b7f828u,Mode::FixedPositive},
    {0x82b7f828u,Mode::FixedZeroExponent},
    {0x82b7f828u,Mode::FixedNegativePad},
    {0x82b7f828u,Mode::FixedSpecial},
    {0x82b7f828u,Mode::FixedNull}}};
using Event=std::array<std::uint64_t,8>;

struct Services final : CrtThreadDataServices,
    InvalidParameterServices,crt_float_environment::NativeServices
{
    GuestMemory memory;
    std::vector<Event> events;
    PPCContext* original=nullptr;
    family::Registers* recovered=nullptr;
    unsigned getters=0,handlers=0,bugs=0;
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
            m.ReadU8(Output)});
        call.arguments[0]=0xfaceb00c12345678ull;
        call.thread_environment=0xaabbccdd00000000ull|Environment;
    }
    void Trap(const InvalidParameterCall&) override {Unexpected();}
    void CallDebugMonitor(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
    void CallExceptionHandler(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
    void BugCheck(GuestMemory&,crt_float_environment::Registers&) override
    {++bugs;Unexpected();}
};
Services* active=nullptr;

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers s{};
    s.sp=c.r1.u64;s.lr=c.lr;s.ctr=c.ctr.u64;
    s.r={c.r0.u64,0,c.r2.u64,c.r3.u64,c.r4.u64,c.r5.u64,
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

void ToPpc(PPCContext& c,const family::Registers& s)
{
    c.r0.u64=s.r[0];c.r1.u64=s.sp;c.r2.u64=s.r[2];
    c.r3.u64=s.r[3];c.r4.u64=s.r[4];c.r5.u64=s.r[5];
    c.r6.u64=s.r[6];c.r7.u64=s.r[7];c.r8.u64=s.r[8];
    c.r9.u64=s.r[9];c.r10.u64=s.r[10];c.r11.u64=s.r[11];
    c.r12.u64=s.r[12];c.r13.u64=s.r[13];
    c.r14.u64=s.r[14];c.r15.u64=s.r[15];c.r16.u64=s.r[16];
    c.r17.u64=s.r[17];c.r18.u64=s.r[18];c.r19.u64=s.r[19];
    c.r20.u64=s.r[20];c.r21.u64=s.r[21];c.r22.u64=s.r[22];
    c.r23.u64=s.r[23];c.r24.u64=s.r[24];
    c.r25.u64=s.r[25];c.r26.u64=s.r[26];c.r27.u64=s.r[27];
    c.r28.u64=s.r[28];c.r29.u64=s.r[29];c.r30.u64=s.r[30];
    c.r31.u64=s.r[31];c.lr=s.lr;c.ctr.u64=s.ctr;
    c.xer.so=s.xer_so;c.xer.ca=s.xer_ca;
    c.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,{s.cr0.so}};
    c.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,{s.cr6.so}};
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

void Seed(GuestWindow& window,const Case& item)
{
    window.Fill(0x5au);
    auto m=window.Memory();
    m.WriteU32(Environment+336u,1u);
    m.WriteU32(0x83214d74u,0x3456u);
    m.WriteU32(0x83214d78u,1u);
    m.WriteU32(Handler,0x2601u);
    m.WriteU32(Record+8u,0x12345678u);
    m.WriteU32(LocaleGlobal,LocaleRecord);
    m.WriteU32(LocaleRecord,LocaleText);
    m.WriteU8(LocaleText,',');
    m.WriteU32(Option,item.mode==Mode::SciTrim?1u:0u);
    for(unsigned i=0;i<6;++i)
        m.WriteU8(Template+i,static_cast<std::uint8_t>("e+000"[i]));
    m.WriteU32(Descriptor,item.mode==Mode::SciUpperNegative ||
        item.mode==Mode::FixedSpecial?45u:43u);
    std::uint32_t exponent=3u;
    if(item.mode==Mode::Carry) exponent=4u;
    if(item.mode==Mode::SciNegativeExponent) exponent=0u;
    if(item.mode==Mode::SciHundreds) exponent=124u;
    if(item.mode==Mode::FixedZeroExponent) exponent=0u;
    if(item.mode==Mode::FixedNegativePad) exponent=0xfffffffeu;
    m.WriteU32(Descriptor+4u,exponent);
    m.WriteU32(Descriptor+12u,Digits);
    const char* digits=item.mode==Mode::Carry?"9995":
        item.mode==Mode::Round?"1256":"1234";
    for(unsigned i=0;i<5;++i)
        m.WriteU8(Digits+i,static_cast<std::uint8_t>(digits[i]));
    const char* output=item.mode==Mode::SciUpperNegative?
        "12345":"1234";
    if(item.mode==Mode::FixedSpecial) output="1200";
    for(unsigned i=0;i<5;++i)
        m.WriteU8(Output+i,static_cast<std::uint8_t>(output[i]));
    m.WriteU8(Output+5u,0);
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0xabcdef0101020304ull;
    c.ctr.u64=0x8877665544332211ull;
    c.r3.u64=0xabcdef0000000000ull|Output;
    c.r4.u64=64u;c.r5.u64=2u;
    c.r6.u64=0xabcdef0000000000ull|Descriptor;
    c.r7.u64=0;
    c.r13.u64=0xdddd000000000000ull|Environment;
    c.r25.u64=0x2525252500000025ull;
    c.r26.u64=0x2626262600000026ull;
    c.r31.u64=0x3131313100000031ull;
    c.xer.so=1;c.xer.ca=1;
    c.cr0={1,0,0,{1}};c.cr6={0,1,0,{1}};
    if(item.entry==0x82b7f048u)
    {
        c.r7.u64=c.r6.u64;
        c.r6.u64=item.mode==Mode::SciUpperNegative?1u:0u;
        c.r8.u64=item.mode==Mode::SciShift?1u:0u;
    }
    if(item.mode==Mode::RoundNull || item.mode==Mode::FixedNull)
        c.r3.u64=0;
    if(item.mode==Mode::RoundZero) c.r4.u64=0;
    if(item.mode==Mode::RoundRange) c.r4.u64=3;
    if(item.mode==Mode::SciRange) c.r4.u64=10;
    if(item.mode==Mode::Pad) c.r5.u64=4;
    if(item.mode==Mode::Round || item.mode==Mode::Carry) c.r5.u64=3;
    if(item.mode==Mode::FixedZeroExponent) c.r5.u64=0;
    if(item.mode==Mode::FixedNegativePad) c.r5.u64=4;
    if(item.mode==Mode::FixedSpecial)
    {c.r7.u64=1;c.r5.u64=2;}
    return c;
}

bool ExpectedPath(const Case& item,const Services& service,
    const PPCContext& raw)
{
    const auto m=service.memory;
    const bool invalid=item.mode==Mode::RoundNull ||
        item.mode==Mode::RoundZero || item.mode==Mode::RoundRange ||
        item.mode==Mode::SciRange || item.mode==Mode::FixedNull;
    if(invalid)
    {
        const auto code=item.mode==Mode::RoundRange ||
            item.mode==Mode::SciRange?34u:22u;
        return service.getters==1 && service.handlers==1 &&
            service.events.size()==2u && service.bugs==0 &&
            m.ReadU32(Record+8u)==code && raw.r3.u64==code;
    }
    if(service.getters || service.handlers || service.bugs ||
        raw.r3.u64!=0)
        return false;
    switch(item.mode)
    {
    case Mode::Pad:return m.ReadU8(Output)=='1' &&
        m.ReadU8(Output+3u)=='4' && m.ReadU8(Output+4u)==0;
    case Mode::Round:return m.ReadU8(Output+2u)=='6' &&
        m.ReadU8(Output+3u)==0;
    case Mode::Carry:return m.ReadU8(Output)=='1' &&
        m.ReadU32(Descriptor+4u)==5u;
    case Mode::SciLower:return m.ReadU8(Output+4u)=='e' &&
        m.ReadU8(Output+5u)=='+';
    case Mode::SciUpperNegative:return m.ReadU8(Output)=='-' &&
        m.ReadU8(Output+5u)=='E';
    case Mode::SciShift:return m.ReadU8(Output+1u)==',' &&
        m.ReadU8(Output+3u)=='e';
    case Mode::SciNegativeExponent:return m.ReadU8(Output+5u)=='-';
    case Mode::SciTrim:return m.ReadU8(Output+5u)=='+' &&
        m.ReadU8(Output+7u)=='2' && m.ReadU8(Output+8u)==0;
    case Mode::SciHundreds:return m.ReadU8(Output+6u)=='1' &&
        m.ReadU8(Output+7u)=='2' && m.ReadU8(Output+8u)=='3';
    case Mode::FixedPositive:return m.ReadU8(Output+3u)==',' &&
        m.ReadU8(Output+4u)=='4';
    case Mode::FixedZeroExponent:return m.ReadU8(Output)=='0' &&
        m.ReadU8(Output+1u)=='1';
    case Mode::FixedNegativePad:return m.ReadU8(Output)=='0' &&
        m.ReadU8(Output+1u)==',' && m.ReadU8(Output+2u)=='0';
    case Mode::FixedSpecial:return m.ReadU8(Output)=='-' &&
        m.ReadU8(Output+3u)=='0';
    default:return false;
    }
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
    if(item.entry==0x8231b1b8u)
        __imp__sub_8231B1B8(raw,original.Bytes());
    else if(item.entry==0x82b7f048u)
        __imp__sub_82B7F048(raw,original.Bytes());
    else
        __imp__sub_82B7F828(raw,original.Bytes());
    active=nullptr;
    if(!ExpectedPath(item,expected,raw))
    {
        std::fprintf(stderr,"fixture path %08X mode=%u getters=%u handlers=%u r3=%llX\n",
            item.entry,static_cast<unsigned>(item.mode),expected.getters,
            expected.handlers,static_cast<unsigned long long>(raw.r3.u64));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if(!family::Apply(item.entry,actual.memory,{ {actual,actual},actual},state))
        throw std::runtime_error("recovered entry missing");
    const auto observed=FromPpc(raw);
    if(!Same(observed,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL float-text %08X mode=%u r3=%llX/%llX sp=%llX/%llX lr=%llX/%llX events=%zu/%zu RAM=%u\n",
            item.entry,static_cast<unsigned>(item.mode),
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
private:PPCContext& context_;
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

void OriginalMove(PPCContext& context,std::uint8_t*)
{
    context.r3.u64=MoveGuestMemory(active->memory,context.r3.u64,
        context.r4.u32,context.r5.u64,context.r1.u32);
}

void OriginalFill(PPCContext& context,std::uint8_t*)
{
    context.r3.u64=FillGuestMemory(active->memory,context.r3.u32,
        context.r4.u32,context.r5.u32);
}

void OriginalBoundedCopy(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    if(!crt_float_core_helpers::Apply(0x8231b0d0u,active->memory,
            {*active,*active},state))
        throw std::runtime_error("missing accepted bounded copy");
    ToPpc(context,state);
}

void OriginalFatal(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    if(!crt_float_environment::Apply(0x82b7ff08u,active->memory,
            *active,state))
        throw std::runtime_error("missing accepted fatal reporter");
    ToPpc(context,state);
}

void OriginalSave25(PPCContext& context,std::uint8_t*)
{
    auto& m=active->memory;
    const auto regs=FromPpc(context);
    for(unsigned i=25;i<=31u;++i)
        WriteU64(m,Address(context.r1.u64-16u-8u*(31u-i)),regs.r[i]);
    m.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}

void OriginalSave26(PPCContext& context,std::uint8_t*)
{
    auto& m=active->memory;
    const auto regs=FromPpc(context);
    for(unsigned i=26;i<=31u;++i)
        WriteU64(m,Address(context.r1.u64-16u-8u*(31u-i)),regs.r[i]);
    m.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}

void OriginalRestore25(PPCContext& context,std::uint8_t*)
{
    auto& m=active->memory;
    auto regs=FromPpc(context);
    for(unsigned i=25;i<=31u;++i)
        regs.r[i]=ReadU64(m,Address(context.r1.u64-16u-8u*(31u-i)));
    ToPpc(context,regs);
    context.r12.u64=m.ReadU32(Address(context.r1.u64-8u));
    context.lr=context.r12.u64;
}

void OriginalRestore26(PPCContext& context,std::uint8_t*)
{
    auto& m=active->memory;
    auto regs=FromPpc(context);
    for(unsigned i=26;i<=31u;++i)
        regs.r[i]=ReadU64(m,Address(context.r1.u64-16u-8u*(31u-i)));
    ToPpc(context,regs);
    context.r12.u64=m.ReadU32(Address(context.r1.u64-8u));
    context.lr=context.r12.u64;
}

int main()
{
    try
    {
        for(const auto& item:Cases)
            if(!Check(item)) return 1;
        GuestWindow window(Regions),baseline(Regions);
        Seed(window,Cases[0]);Seed(baseline,Cases[0]);
        Services service(window);
        auto state=FromPpc(Initial(Cases[0]));
        service.recovered=&state;
        const auto saved=state;
        if(family::Apply(0xffffffffu,service.memory,
                {{service,service},service},state) ||
            !Same(saved,state) || !service.events.empty() ||
            !window.EqualCommitted(baseline))
            throw std::runtime_error("unknown entry changed state");
        std::printf("PASS crt-float-text-helpers %zu original PPC cases + unknown\n",
            Cases.size());
        std::puts("LIMIT selected state/RAM and accepted lower ABI; faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

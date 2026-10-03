// Appended after the three pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_float_environment.h"
#include "lo_semantics/field_arithmetic.h"
#include "lo_semantics/invalid_parameter.h"
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
namespace family = crt_float_environment;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Stack=0x80000u, String=0x30000u;
constexpr GuestAddress Descriptor=0x35000u;
constexpr GuestAddress MonitorStorage=0x40000u, MonitorObject=0x41000u;
constexpr GuestAddress MonitorImport=0x820008f0u;
constexpr GuestAddress HandlerSlot=0x8337486cu;
constexpr GuestAddress RuntimeState=0x83378d64u;
constexpr std::array<Region,4> Regions{{{0,0x90000u},
    {0x82000000u,0x2000u},{0x83374000u,0x2000u},
    {0x83378000u,0x3000u}}};

enum class Mode {Last,NullCharacter,Miss,MonitorAbsent,
    MonitorHandled,HandlerMinusOne,HandlerOther,SaveAlias,
    ReporterClear,ReporterSkipClear};
struct Case {GuestAddress entry;Mode mode;};
constexpr std::array<Case,10> Cases{{
    {0x82b7e580u,Mode::Last},
    {0x82b7e580u,Mode::NullCharacter},
    {0x82b7e580u,Mode::Miss},
    {0x827cb0b0u,Mode::MonitorAbsent},
    {0x827cb0b0u,Mode::MonitorHandled},
    {0x827cb0b0u,Mode::HandlerMinusOne},
    {0x827cb0b0u,Mode::HandlerOther},
    {0x827cb0b0u,Mode::SaveAlias},
    {0x82b7ff08u,Mode::ReporterClear},
    {0x82b7ff08u,Mode::ReporterSkipClear}}};
using Event=std::array<std::uint64_t,9>;

struct Services final : family::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    unsigned monitors=0, filters=0, bugchecks=0;
    Services(GuestWindow& window,Mode selected)
        :memory(window.Memory()),mode(selected){}
    [[noreturn]] static void Unexpected()
    {throw std::runtime_error("unexpected native boundary");}

    void CallDebugMonitor(GuestAddress target,GuestMemory& memory,
        family::Registers& state) override
    {
        if(target!=0x2700u || state.lr!=0x827cb0ecu ||
            state.r[3]!=10u || state.r[4]!=0u) Unexpected();
        ++monitors;
        events.push_back({1u,target,state.sp,state.lr,state.r[3],
            state.r[4],memory.ReadU32(HandlerSlot),0u,0u});
        state.r[21]=0x2121212100000021ull;
        if(mode==Mode::ReporterSkipClear)
            memory.WriteU32(HandlerSlot,0x2801u);
        state.r[3]=mode==Mode::MonitorHandled?1u:0u;
    }
    void CallExceptionHandler(GuestAddress target,GuestMemory& memory,
        family::Registers& state) override
    {
        if(target!=0x2800u || state.lr!=0x827cb11cu) Unexpected();
        ++filters;
        events.push_back({2u,target,state.sp,state.lr,state.r[3],
            state.r[4],memory.ReadU32(HandlerSlot),0u,0u});
        state.r[20]=0x2020202000000020ull;
        if(mode==Mode::SaveAlias)
        {
            memory.WriteU32(Address(state.sp+88u),0xdecafbadu);
            WriteU64(memory,Address(state.sp+80u),
                0x1122334455667788ull);
        }
        state.r[3]=mode==Mode::HandlerOther?5u:~std::uint64_t{0};
    }
    void BugCheck(GuestMemory& memory,family::Registers& state) override
    {
        if(state.r[3]!=30u || state.lr!=0x82b7ff8cu ||
            state.sp!=(0x1234567800000000ull|Stack)-2816u)
            Unexpected();
        const auto sp=Address(state.sp);
        if(memory.ReadU32(sp+96u)!=0xc000000du ||
            memory.ReadU32(sp+108u)!=0x01020304u ||
            memory.ReadU32(sp+80u)!=sp+96u ||
            memory.ReadU32(sp+84u)!=sp+176u ||
            memory.ReadU8(sp+176u)!=0 ||
            memory.ReadU8(sp+2799u)!=0)
            throw std::runtime_error("reporter payload missed original path");
        ++bugchecks;
        events.push_back({3u,0u,state.sp,state.lr,state.r[3],
            state.r[4],memory.ReadU32(HandlerSlot),
            memory.ReadU32(RuntimeState),memory.ReadU32(sp+96u)});
        // The original body has no epilogue after a returning import.
        state.r[3]=0xdeadbeef12345678ull;
        state.r[13]=0xabcddcba00060000ull;
    }
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
    c.r12.u64=s.r[12];c.r13.u64=s.r[13];c.r14.u64=s.r[14];
    c.r15.u64=s.r[15];c.r16.u64=s.r[16];c.r17.u64=s.r[17];
    c.r18.u64=s.r[18];c.r19.u64=s.r[19];c.r20.u64=s.r[20];
    c.r21.u64=s.r[21];c.r22.u64=s.r[22];c.r23.u64=s.r[23];
    c.r24.u64=s.r[24];c.r25.u64=s.r[25];c.r26.u64=s.r[26];
    c.r27.u64=s.r[27];c.r28.u64=s.r[28];c.r29.u64=s.r[29];
    c.r30.u64=s.r[30];c.r31.u64=s.r[31];
    c.lr=s.lr;c.ctr.u64=s.ctr;
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

void Seed(GuestWindow& window,Mode mode)
{
    window.Fill(0x5au);
    auto m=window.Memory();
    for(unsigned index=0;index<6;++index)
        m.WriteU8(String+index,static_cast<std::uint8_t>(
            index==5?0:"abaca"[index]));
    m.WriteU32(MonitorImport,MonitorStorage);
    const bool monitor=mode==Mode::MonitorHandled ||
        mode==Mode::HandlerMinusOne || mode==Mode::HandlerOther ||
        mode==Mode::ReporterSkipClear;
    m.WriteU32(MonitorStorage,monitor?MonitorObject:0u);
    m.WriteU32(MonitorObject+24u,0x2701u);
    const bool handler=mode==Mode::HandlerMinusOne ||
        mode==Mode::HandlerOther || mode==Mode::SaveAlias ||
        mode==Mode::MonitorHandled || mode==Mode::ReporterClear ||
        mode==Mode::ReporterSkipClear;
    m.WriteU32(HandlerSlot,handler?0x2801u:0u);
    m.WriteU32(RuntimeState,0x13579bdfu);
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0xabcdef0101020304ull;
    c.ctr.u64=0x8877665544332211ull;
    c.r3.u64=0xabcdef0000000000ull|Descriptor;
    c.r4.u64=0x4444000000000004ull;
    c.r5.u64=0x5555000000000005ull;
    c.r13.u64=0xdddd000000060000ull;
    c.r31.u64=0x3131313100000031ull;
    c.xer.so=1;c.xer.ca=1;
    c.cr0={1,0,0,{1}};c.cr6={0,1,0,{1}};
    if(item.entry==0x82b7e580u)
    {
        c.r3.u64=0xabcdef0000000000ull|String;
        c.r4.u64=item.mode==Mode::NullCharacter?0u:
            item.mode==Mode::Miss?static_cast<std::uint64_t>('z'):
            static_cast<std::uint64_t>('a');
    }
    return c;
}

bool ExpectedPath(const Case& item,const Services& service,
    const PPCContext& raw,const GuestWindow& window)
{
    const auto m=window.Memory();
    switch(item.mode)
    {
    case Mode::Last:
        return raw.r3.u32==String+4u && raw.r5.u32==0 &&
            service.events.empty();
    case Mode::NullCharacter:
        return raw.r3.u32==String+5u && raw.r5.u32==0 &&
            service.events.empty();
    case Mode::Miss:
        return raw.r3.u64==0 && raw.r5.u32==0 &&
            service.events.empty();
    case Mode::MonitorAbsent:
        return raw.r3.u64==0 && service.events.empty();
    case Mode::MonitorHandled:
        return raw.r3.u64==0 && service.monitors==1 &&
            service.filters==0 && service.events.size()==1u;
    case Mode::HandlerMinusOne:
        return raw.r3.u64==~std::uint64_t{0} &&
            service.monitors==1 && service.filters==1 &&
            service.events.size()==2u;
    case Mode::HandlerOther:
        return raw.r3.u64==0 && service.monitors==1 &&
            service.filters==1 && service.events.size()==2u;
    case Mode::SaveAlias:
        return raw.r3.u64==~std::uint64_t{0} &&
            raw.lr==0xdecafbadu && raw.r31.u64==0x1122334455667788ull &&
            service.monitors==0 && service.filters==1 &&
            service.events.size()==1u;
    case Mode::ReporterClear:
        return service.monitors==0 && service.filters==0 &&
            service.bugchecks==1 && service.events.size()==1u &&
            m.ReadU32(RuntimeState)==0 && m.ReadU32(HandlerSlot)==0 &&
            raw.r1.u64==(0x1234567800000000ull|Stack)-2816u &&
            raw.lr==0x82b7ff8cu &&
            raw.r3.u64==0xdeadbeef12345678ull;
    case Mode::ReporterSkipClear:
        return service.monitors==1 && service.filters==1 &&
            service.bugchecks==1 && service.events.size()==3u &&
            m.ReadU32(RuntimeState)==0x13579bdfu &&
            m.ReadU32(HandlerSlot)==0x2801u &&
            raw.r1.u64==(0x1234567800000000ull|Stack)-2816u &&
            raw.lr==0x82b7ff8cu &&
            raw.r3.u64==0xdeadbeef12345678ull;
    }
    return false;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,item.mode);Seed(recovered,item.mode);
    Services expected(original,item.mode),actual(recovered,item.mode);
    auto raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    if(item.entry==0x82b7e580u)
        __imp__sub_82B7E580(raw,original.Bytes());
    else if(item.entry==0x827cb0b0u)
        __imp__sub_827CB0B0(raw,original.Bytes());
    else
        __imp__sub_82B7FF08(raw,original.Bytes());
    active=nullptr;
    if(!ExpectedPath(item,expected,raw,original))
    {
        std::fprintf(stderr,"fixture path %08X mode=%u monitor=%u filter=%u bug=%u r3=%llX\n",
            item.entry,static_cast<unsigned>(item.mode),expected.monitors,
            expected.filters,expected.bugchecks,
            static_cast<unsigned long long>(raw.r3.u64));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if(!family::Apply(item.entry,actual.memory,actual,state))
        throw std::runtime_error("recovered entry missing");
    const auto observed=FromPpc(raw);
    if(!Same(observed,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL float-environment %08X mode=%u r3=%llX/%llX sp=%llX/%llX lr=%llX/%llX events=%zu/%zu RAM=%u\n",
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
} // namespace

void OriginalFill(PPCContext& context,std::uint8_t* base)
{
    (void)base;
    context.r3.u64=FillGuestMemory(active->memory,context.r3.u32,
        context.r4.u32,context.r5.u32);
}

void OriginalExchange(PPCContext& context,std::uint8_t* base)
{
    (void)base;
    lo::semantic::field_arithmetic::Registers lower{};
    lower.r3=context.r3.u64;lower.r4=context.r4.u64;
    lower.r5=context.r5.u64;lower.r8=context.r8.u64;
    lower.r9=context.r9.u64;lower.r10=context.r10.u64;
    lower.r11=context.r11.u64;
    if(!lo::semantic::field_arithmetic::Apply(0x827cafe0u,lower,active->memory))
        throw std::runtime_error("missing accepted exchange");
    context.r3.u64=lower.r3;context.r4.u64=lower.r4;
    context.r5.u64=lower.r5;context.r8.u64=lower.r8;
    context.r9.u64=lower.r9;context.r10.u64=lower.r10;
    context.r11.u64=lower.r11;
}

void OriginalFilter(PPCContext& context,std::uint8_t* base)
{__imp__sub_827CB0B0(context,base);}

void OriginalClear(PPCContext& context,std::uint8_t* base)
{
    (void)base;
    context.r10.u64=0xffffffff83380000ull;
    context.r11.u64=0;
    ClearInvalidParameterState(active->memory);
}

void OriginalBugCheck(PPCContext& context,std::uint8_t* base)
{
    (void)base;
    auto state=FromPpc(context);
    active->BugCheck(active->memory,state);
    ToPpc(context,state);
}

void OriginalIndirect(GuestAddress target,PPCContext& context,
    std::uint8_t* base)
{
    (void)base;
    auto state=FromPpc(context);
    if(context.lr==0x827cb0ecu)
        active->CallDebugMonitor(target,active->memory,state);
    else if(context.lr==0x827cb11cu)
        active->CallExceptionHandler(target,active->memory,state);
    else
        Services::Unexpected();
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto& item:Cases)
            if(!Check(item)) return 1;
        GuestWindow window(Regions),baseline(Regions);
        Seed(window,Mode::Last);Seed(baseline,Mode::Last);
        Services services(window,Mode::Last);
        auto state=FromPpc(Initial(Cases[0]));
        const auto saved=state;
        if(family::Apply(0xffffffffu,services.memory,services,state) ||
            !Same(saved,state) || !services.events.empty() ||
            !window.EqualCommitted(baseline))
            throw std::runtime_error("unknown entry changed state");
        std::printf("PASS crt-float-environment %zu original PPC cases + unknown\n",
            Cases.size());
        std::puts("LIMIT selected state/RAM and live native boundaries; faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

// Appended after the six pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_float_formatting.h"
#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/crt_float_text_helpers.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=crt_float_formatting;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Input=0x30000u,Output=0x32000u;
constexpr GuestAddress Environment=0x60000u,Record=0x70000u;
constexpr GuestAddress Stack=0x80000u,Handler=0x83378e80u;
constexpr GuestAddress LocaleGlobal=0x83215648u;
constexpr GuestAddress LocaleRecord=0x33000u,LocaleText=0x34000u;
constexpr GuestAddress Option=0x832d3cb8u,Template=0x820d3128u;
constexpr std::array<Region,5> Regions{{{0,0x90000u},
    {0x820d3000u,0x3000u},{0x83214000u,0x3000u},
    {0x832d3000u,0x2000u},{0x83378000u,0x3000u}}};

enum class Mode {SelectorScientific,SelectorFixed,Scientific,Fixed,
    General,HexRound,HexZero,HexSpecialPlain,HexSpecialPayload,
    HexRange};
struct Case {GuestAddress entry;Mode mode;};
constexpr std::array<Case,10> Cases{{
    {0x8231a2a0u,Mode::SelectorScientific},
    {0x82b7fc40u,Mode::SelectorFixed},
    {0x82b7f2c8u,Mode::Scientific},
    {0x82b7fa00u,Mode::Fixed},
    {0x82b7faf0u,Mode::General},
    {0x82b7f3e0u,Mode::HexRound},
    {0x82b7f3e0u,Mode::HexZero},
    {0x82b7f3e0u,Mode::HexSpecialPlain},
    {0x82b7f3e0u,Mode::HexSpecialPayload},
    {0x82b7f3e0u,Mode::HexRange}}};
using Event=std::array<std::uint64_t,8>;

struct Services final:CrtThreadDataServices,InvalidParameterServices,
    crt_float_environment::NativeServices
{
    GuestMemory memory;
    PPCContext* original=nullptr;
    family::Registers* recovered=nullptr;
    std::vector<Event> events;
    unsigned getters=0,handlers=0,converts=0,rounds=0;
    unsigned scientific_calls=0,fixed_calls=0;
    unsigned scientific_texts=0,fixed_texts=0;
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
    void BugCheck(GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
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
    PPCRegister* fields[]={&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,
        &c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,
        &c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,
        &c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,
        &c.r30,&c.r31};
    for(unsigned i=0;i<32u;++i) fields[i]->u64=s.r[i];
    c.r1.u64=s.sp;c.lr=s.lr;c.ctr.u64=s.ctr;
    c.xer.so=s.xer_so;c.xer.ca=s.xer_ca;
    c.cr0={bool(s.cr0.lt),bool(s.cr0.gt),bool(s.cr0.eq),
        {bool(s.cr0.so)}};
    c.cr6={bool(s.cr6.lt),bool(s.cr6.gt),bool(s.cr6.eq),
        {bool(s.cr6.so)}};
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

void ReadImageRange(GuestMemory& memory,GuestAddress address,
    std::size_t length)
{
    std::ifstream image("LostOdysseyRecompLib/private/image_disc1.bin",
        std::ios::binary);
    if(!image) throw std::runtime_error("missing private image fixture");
    image.seekg(static_cast<std::streamoff>(address-0x82000000u));
    std::vector<char> bytes(length);
    image.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    if(image.gcount()!=static_cast<std::streamsize>(bytes.size()))
        throw std::runtime_error("short private image fixture range");
    for(std::size_t i=0;i<bytes.size();++i)
        memory.WriteU8(address+static_cast<GuestAddress>(i),
            static_cast<std::uint8_t>(bytes[i]));
}

std::uint64_t InputBits(Mode mode)
{
    switch(mode)
    {
    case Mode::HexRound:return 0x8f000000000007feull;
    case Mode::HexZero:return 0;
    case Mode::HexSpecialPlain:return 0xffeull;
    case Mode::HexSpecialPayload:return 0x7ff8000000000ffeull;
    default:return 0x3ff0000000000000ull;
    }
}
void Seed(GuestWindow& window,const Case& item)
{
    window.Fill(0xa5u);
    auto memory=window.Memory();
    ReadImageRange(memory,0x820d4c80u,128u);
    ReadImageRange(memory,0x83215c00u,1024u);
    WriteU64(memory,Input,InputBits(item.mode));
    memory.WriteU32(Environment+336u,1u);
    memory.WriteU32(0x83214d74u,0x3456u);
    memory.WriteU32(0x83214d78u,1u);
    memory.WriteU32(Handler,0x2601u);
    memory.WriteU32(Record+8u,0x12345678u);
    memory.WriteU32(LocaleGlobal,LocaleRecord);
    memory.WriteU32(LocaleRecord,LocaleText);
    memory.WriteU8(LocaleText,',');
    memory.WriteU32(Option,0u);
    for(unsigned i=0;i<6u;++i)
        memory.WriteU8(Template+i,
            static_cast<std::uint8_t>("e+000"[i]));
    for(unsigned i=0;i<64u;++i) memory.WriteU8(Output+i,0);
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0xabcdef0101020304ull;
    c.ctr.u64=0x8877665544332211ull;
    c.r3.u64=0xabcdef0000000000ull|Input;
    c.r4.u64=0x2222222200000000ull|Output;
    c.r5.u64=64u;
    c.r6.u64=2u;c.r7.u64=0;c.r8.u64=0;c.r9.u64=0;
    c.r13.u64=0xdddd000000000000ull|Environment;
    c.r25.u64=0x2525252500000025ull;
    c.r26.u64=0x2626262600000026ull;
    c.r27.u64=0x2727272700000027ull;
    c.r28.u64=0x2828282800000028ull;
    c.r29.u64=0x2929292900000029ull;
    c.r30.u64=0x3030303000000030ull;
    c.r31.u64=0x3131313100000031ull;
    c.xer.so=1;c.xer.ca=1;
    c.cr0={1,0,0,{1}};c.cr6={0,1,0,{1}};
    if(item.entry==0x8231a2a0u || item.entry==0x82b7fc40u)
    {
        c.r6.u64=item.mode==Mode::SelectorScientific?101u:102u;
        c.r7.u64=2u;c.r8.u64=0;c.r9.u64=0;
    }
    if(item.mode==Mode::General) c.r6.u64=3u;
    if(item.mode==Mode::HexRound || item.mode==Mode::HexZero ||
        item.mode==Mode::HexSpecialPlain ||
        item.mode==Mode::HexSpecialPayload || item.mode==Mode::HexRange)
    {
        c.r6.u64=item.mode==Mode::HexZero?0u:1u;
        c.r7.u64=1u;
        if(item.mode==Mode::HexRange) c.r5.u64=12u;
    }
    return c;
}

bool Contains(const GuestMemory& memory,char needle)
{
    for(unsigned i=0;i<64u;++i)
        if(memory.ReadU8(Output+i)==static_cast<std::uint8_t>(needle))
            return true;
    return false;
}
bool ExpectedPath(const Case& item,const Services& service,
    const PPCContext& raw)
{
    const auto m=service.memory;
    if(item.mode==Mode::HexRange)
        return service.converts==0 && service.getters==1 &&
            service.handlers==1 && service.events.size()==2u &&
            m.ReadU32(Record+8u)==34u && raw.r3.u64==34u &&
            m.ReadU8(Output)==0;
    if(service.getters || service.handlers || raw.r3.u64!=0)
        return false;
    const bool hex=item.entry==0x82b7f3e0u;
    if(hex)
    {
        if(m.ReadU8(Output)!='0' ||
            (m.ReadU8(Output+1u)!='X' && m.ReadU8(Output+1u)!='x'))
            return false;
        switch(item.mode)
        {
        case Mode::HexRound:return service.converts==0 &&
            service.scientific_calls==0 && Contains(m,'9') &&
            (Contains(m,'P') || Contains(m,'p'));
        case Mode::HexZero:return service.converts==0 &&
            service.scientific_calls==0 && m.ReadU8(Output+2u)=='0' &&
            (Contains(m,'P') || Contains(m,'p'));
        case Mode::HexSpecialPlain:
        case Mode::HexSpecialPayload:return service.scientific_calls==1 &&
            service.converts==1 && service.scientific_texts==1;
        default:return false;
        }
    }
    if(service.converts!=1 || service.rounds!=1 ||
        m.ReadU8(Output)==0)
        return false;
    switch(item.mode)
    {
    case Mode::SelectorScientific:
        return service.scientific_calls==1 &&
            service.scientific_texts==1 && Contains(m,'e');
    case Mode::SelectorFixed:
        return service.fixed_calls==1 && service.fixed_texts==1 &&
            Contains(m,',');
    case Mode::Scientific:return service.scientific_texts==1 &&
        Contains(m,'e');
    case Mode::Fixed:return service.fixed_texts==1 && Contains(m,',');
    case Mode::General:return service.fixed_texts==1 && Contains(m,',');
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
    switch(item.entry)
    {
    case 0x8231a2a0u:__imp__sub_8231A2A0(raw,original.Bytes());break;
    case 0x82b7fc40u:__imp__sub_82B7FC40(raw,original.Bytes());break;
    case 0x82b7f2c8u:__imp__sub_82B7F2C8(raw,original.Bytes());break;
    case 0x82b7f3e0u:__imp__sub_82B7F3E0(raw,original.Bytes());break;
    case 0x82b7fa00u:__imp__sub_82B7FA00(raw,original.Bytes());break;
    default:__imp__sub_82B7FAF0(raw,original.Bytes());break;
    }
    active=nullptr;
    if(!ExpectedPath(item,expected,raw))
    {
        std::fprintf(stderr,"fixture path %08X mode=%u r3=%llX conv=%u round=%u sci=%u fixed=%u err=%u/%u out=",
            item.entry,static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            expected.converts,expected.rounds,expected.scientific_calls,
            expected.fixed_calls,expected.getters,expected.handlers);
        for(unsigned i=0;i<18u;++i)
            std::fprintf(stderr," %02X",expected.memory.ReadU8(Output+i));
        std::fprintf(stderr," raw=%016llX option=%08X\n",
            static_cast<unsigned long long>(ReadU64(expected.memory,Input)),
            expected.memory.ReadU32(Option));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if(!family::Apply(item.entry,actual.memory,
            {{actual,actual},actual},state))
        throw std::runtime_error("recovered entry missing");
    const auto observed=FromPpc(raw);
    if(!Same(observed,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL float-formatting %08X mode=%u r3=%llX/%llX sp=%llX/%llX lr=%llX/%llX events=%zu/%zu RAM=%u\n",
            item.entry,static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            expected.events.size(),actual.events.size(),
            original.EqualCommitted(recovered));
        for(unsigned i=0;i<32u;++i)
            if(observed.r[i]!=state.r[i])
                std::fprintf(stderr,"  r%u %016llX/%016llX\n",i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        return false;
    }
    return true;
}

class OriginalErrorServices final:public AllocationFailureServices
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

void Save(unsigned first,PPCContext& context)
{
    auto m=active->memory;
    auto state=FromPpc(context);
    for(unsigned index=first;index<=31u;++index)
        WriteU64(m,Address(context.r1.u64-16u-8u*(31u-index)),
            state.r[index]);
    m.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}
void Restore(unsigned first,PPCContext& context)
{
    auto m=active->memory;
    auto state=FromPpc(context);
    for(unsigned index=first;index<=31u;++index)
        state.r[index]=ReadU64(m,
            Address(context.r1.u64-16u-8u*(31u-index)));
    ToPpc(context,state);
    context.r12.u64=m.ReadU32(Address(context.r1.u64-8u));
    context.lr=context.r12.u64;
}
} // namespace

void OriginalSave25(PPCContext& c,std::uint8_t*) {Save(25u,c);}
void OriginalSave26(PPCContext& c,std::uint8_t*) {Save(26u,c);}
void OriginalSave27(PPCContext& c,std::uint8_t*) {Save(27u,c);}
void OriginalSave28(PPCContext& c,std::uint8_t*) {Save(28u,c);}
void OriginalRestore25(PPCContext& c,std::uint8_t*) {Restore(25u,c);}
void OriginalRestore26(PPCContext& c,std::uint8_t*) {Restore(26u,c);}
void OriginalRestore27(PPCContext& c,std::uint8_t*) {Restore(27u,c);}
void OriginalRestore28(PPCContext& c,std::uint8_t*) {Restore(28u,c);}

void OriginalError(PPCContext& context,std::uint8_t*)
{
    auto& m=active->memory;
    context.r12.u64=context.lr;
    m.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
    m.WriteU32(Address(context.r1.u64-96u),context.r1.u32);
    context.r1.u64-=96u;
    OriginalErrorServices service(context);
    context.r3.u64=GetAllocationErrorAddress(service);
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
void OriginalConvert(PPCContext& context,std::uint8_t*)
{
    ++active->converts;
    auto state=FromPpc(context);
    if(!crt_float_conversion::Apply(0x8231a2f0u,active->memory,
            {{*active,*active},*active},state))
        throw std::runtime_error("missing accepted conversion");
    ToPpc(context,state);
}
void OriginalRound(PPCContext& context,std::uint8_t*)
{
    ++active->rounds;
    auto state=FromPpc(context);
    if(!crt_float_text_helpers::Apply(0x8231b1b8u,active->memory,
            {{*active,*active},*active},state))
        throw std::runtime_error("missing accepted decimal rounding");
    ToPpc(context,state);
}
void OriginalScientificText(PPCContext& context,std::uint8_t*)
{
    ++active->scientific_texts;
    auto state=FromPpc(context);
    if(!crt_float_text_helpers::Apply(0x82b7f048u,active->memory,
            {{*active,*active},*active},state))
        throw std::runtime_error("missing accepted scientific text");
    ToPpc(context,state);
}
void OriginalFixedText(PPCContext& context,std::uint8_t*)
{
    ++active->fixed_texts;
    auto state=FromPpc(context);
    if(!crt_float_text_helpers::Apply(0x82b7f828u,active->memory,
            {{*active,*active},*active},state))
        throw std::runtime_error("missing accepted fixed text");
    ToPpc(context,state);
}
void OriginalSearch(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    if(!crt_float_environment::Apply(0x82b7e580u,active->memory,
            *active,state))
        throw std::runtime_error("missing accepted last-byte search");
    ToPpc(context,state);
}
void OriginalScientific(PPCContext& context,std::uint8_t* base)
{++active->scientific_calls;__imp__sub_82B7F2C8(context,base);}
void OriginalFixed(PPCContext& context,std::uint8_t* base)
{++active->fixed_calls;__imp__sub_82B7FA00(context,base);}
void OriginalHex(PPCContext& context,std::uint8_t* base)
{__imp__sub_82B7F3E0(context,base);}
void OriginalGeneral(PPCContext& context,std::uint8_t* base)
{__imp__sub_82B7FAF0(context,base);}

int main()
{
    try
    {
        for(const auto& item:Cases)
            if(!Check(item)) return 1;
        std::printf("PASS crt-float-formatting %zu original PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected state/RAM and accepted lower native ABI; faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

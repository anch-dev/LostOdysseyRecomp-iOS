#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_format_stream.h"
#include "lo_semantics/crt_free_context.h"
#include "lo_semantics/crt_status_error.h"
#include "lo_semantics/crt_stream_close_pipeline.h"

namespace
{
namespace close_pipeline = crt_stream_close_pipeline;

enum class Scenario {BufferInactive,BufferUnowned,BufferOwned,
    PipelineNull,PipelineInactive,PipelineActive};
struct PipelineCase {const char* name;GuestAddress entry;Scenario scenario;};
constexpr std::array Cases{
    PipelineCase{"buffer-inactive",0x82b87c90u,Scenario::BufferInactive},
    PipelineCase{"buffer-unowned",0x82b87c90u,Scenario::BufferUnowned},
    PipelineCase{"buffer-owned-zero-pointer",0x82b87c90u,Scenario::BufferOwned},
    PipelineCase{"pipeline-null",0x82b85cc8u,Scenario::PipelineNull},
    PipelineCase{"pipeline-inactive",0x82b85cc8u,Scenario::PipelineInactive},
    PipelineCase{"pipeline-locked-close",0x82b85cc8u,Scenario::PipelineActive}};

struct ExtraServices final : crt_formatting_support::NativeServices,
    crt_float_environment::NativeServices,
    crt_formatter::DynamicServices, crt_free_context::LowerCalls
{
    unsigned free_lower_calls=0;
    [[noreturn]] static void Unselected()
    {throw std::runtime_error("unselected CRT guest/native lower path");}
    void InitAnsiString(GuestMemory&,GuestAddress,GuestAddress) override
    {Unselected();}
    std::uint64_t WriteAnsi(GuestAddress,std::uint16_t) override
    {Unselected();}
    void InitializeUnicodeString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unselected();}
    void UnicodeStringToAnsiString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unselected();}
    void FreeAnsiString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unselected();}
    void CallDebugMonitor(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
    void CallExceptionHandler(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
    void BugCheck(GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
    void CallGuestFormatter(GuestAddress,GuestMemory&,
        crt_formatter::Registers&) override {Unselected();}
    void Call(GuestAddress,GuestMemory&,
        crt_free_context::Registers&) override
    {++free_lower_calls;Unselected();}
};

struct CloseGuest final : crt_stream_close_error::GuestServices
{
    std::vector<std::array<std::uint64_t,5>> events;
    void CallIndirect(GuestMemory&,GuestAddress target,
        close_pipeline::Registers& state) override
    {
        if(target!=0x2600u)
            throw std::runtime_error("unexpected stream close target");
        events.push_back({target,state.sp,state.lr,state.ctr,state.r[3]});
        state.r[11]=0xfedcba9800000011ull;
        state.r[3]=0x1234567800000000ull;
    }
};

ExtraServices* active_extra=nullptr;
CloseGuest* active_guest=nullptr;

crt_format_stream::Dependencies FormatDependencies(Services& stream,
    ExtraServices& extra)
{
    crt_wide_stream_output::Dependencies output{
        Dependencies(stream),extra};
    crt_float_formatting::Dependencies floating{{stream,stream},extra};
    return {{output,floating,extra,stream,extra}};
}

close_pipeline::Dependencies PipelineDependencies(Services& stream,
    ExtraServices& extra,CloseGuest& guest)
{
    return {{Dependencies(stream),guest},
        FormatDependencies(stream,extra),extra};
}

void SeedCase(GuestWindow& window,const PipelineCase& item)
{
    Seed(window,Mode::Binary);
    auto memory=window.Memory();
    memory.WriteU32(0x34004u,0x2601u);
    memory.WriteU32(Stream+28u,0u);
    switch(item.scenario)
    {
    case Scenario::BufferInactive:
    case Scenario::PipelineInactive:
        memory.WriteU32(Stream+12u,0u);break;
    case Scenario::BufferUnowned:
        memory.WriteU32(Stream+12u,2u);break;
    case Scenario::BufferOwned:
        memory.WriteU32(Stream+12u,0x8au);
        memory.WriteU32(Stream+8u,0u);
        memory.WriteU32(Stream,Buffer+3u);
        memory.WriteU32(Stream+4u,17u);
        break;
    case Scenario::PipelineActive:
        memory.WriteU32(Stream+12u,0x82u);
        memory.WriteU32(Stream,Buffer);
        break;
    case Scenario::PipelineNull:break;
    }
}

PPCContext Initial(const PipelineCase& item)
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0xabcdef0123456789ull;
    context.r13.u64=Environment;
    context.r3.u64=item.scenario==Scenario::PipelineNull?0u:
        0x1122334400000000ull|Stream;
    context.r29.u64=0x2911223344556677ull;
    context.r30.u64=0x3011223344556677ull;
    context.r31.u64=0x3111223344556677ull;
    context.xer.so=1;context.xer.ca=1;
    return context;
}

void Check(const PipelineCase& item)
{
    GuestWindow original(Regions),recovered(Regions);
    SeedCase(original,item);SeedCase(recovered,item);
    Services expected(original,Mode::Binary),actual(recovered,Mode::Binary);
    ExtraServices expected_extra,actual_extra;
    CloseGuest expected_guest,actual_guest;
    PPCContext context=Initial(item);
    auto state=FromPpc(context);
    active=&expected;active_extra=&expected_extra;
    active_guest=&expected_guest;
    if(item.entry==0x82b87c90u)
        __imp__sub_82B87C90(context,original.Bytes());
    else
        __imp__sub_82B85CC8(context,original.Bytes());
    active_guest=nullptr;active_extra=nullptr;active=nullptr;
    if(!close_pipeline::Apply(item.entry,actual.memory,
            PipelineDependencies(actual,actual_extra,actual_guest),state))
        throw std::runtime_error("missing recovered close pipeline entry");
    if(!Same(FromPpc(context),state)||
        !original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_guest.events!=actual_guest.events||
        expected.traps!=actual.traps||
        expected.locks!=actual.locks||
        expected.unlocks!=actual.unlocks||
        expected_extra.free_lower_calls!=actual_extra.free_lower_calls)
        throw std::runtime_error("selected context, callbacks or RAM mismatch");
    const auto memory=expected.memory;
    switch(item.scenario)
    {
    case Scenario::BufferInactive:
    case Scenario::BufferUnowned:
        if(expected_guest.events.size()!=0u||
            memory.ReadU32(Stream+8u)!=Buffer)
            throw std::runtime_error("buffer skip branch missed");
        break;
    case Scenario::BufferOwned:
        if(memory.ReadU32(Stream)!=0u||memory.ReadU32(Stream+4u)!=0u||
            memory.ReadU32(Stream+8u)!=0u||
            memory.ReadU32(Stream+12u)!=0x82u||
            expected_extra.free_lower_calls!=0u)
            throw std::runtime_error("owned buffer clear branch missed");
        break;
    case Scenario::PipelineNull:
        if(context.r3.u64!=UINT64_MAX||expected.traps!=1u||
            memory.ReadU32(0x83215210u)!=22u)
            throw std::runtime_error("null stream validation missed");
        break;
    case Scenario::PipelineInactive:
        if(context.r3.u64!=UINT64_MAX||
            memory.ReadU32(Stream+12u)!=0u||
            !expected_guest.events.empty())
            throw std::runtime_error("inactive stream branch missed");
        break;
    case Scenario::PipelineActive:
        if(context.r3.u64!=0u||expected_guest.events.size()!=1u||
            expected.locks!=1u||expected.unlocks!=1u||
            memory.ReadU32(Stream+12u)!=0u||
            memory.ReadU8(Record+4u)!=0u)
            throw std::runtime_error("composed close path missed");
        break;
    }
}
} // namespace

void OriginalFlush(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    if(!crt_format_stream::Apply(0x82b7b838u,active->memory,
            FormatDependencies(*active,*active_extra),state))
        throw std::runtime_error("missing accepted flush bridge");
    ToPpc(context,state);
}

void OriginalFree(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    if(!crt_free_context::Apply(0x823addc0u,active->memory,
            *active_extra,state))
        throw std::runtime_error("missing accepted free bridge");
    ToPpc(context,state);
}

void OriginalPointerLeaf(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp=state.sp;lower.lr=state.lr;
    lower.r3=state.r[3];lower.r9=state.r[9];
    lower.r10=state.r[10];lower.r11=state.r[11];
    lower.r12=state.r[12];lower.r30=state.r[30];
    lower.r31=state.r[31];lower.xer_ca=state.xer_ca;
    if(!crt_stream_pointer_unlock::Apply(0x82b863f0u,active->memory,
            *active,lower))
        throw std::runtime_error("missing pointer unlock bridge");
    state.sp=lower.sp;state.lr=lower.lr;
    state.r[3]=lower.r3;state.r[9]=lower.r9;
    state.r[10]=lower.r10;state.r[11]=lower.r11;
    state.r[12]=lower.r12;state.r[30]=lower.r30;
    state.r[31]=lower.r31;state.xer_ca=lower.xer_ca;
    ToPpc(context,state);
}

void OriginalStatus(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    crt_status_error::Registers lower{};
    lower.sp=state.sp;lower.lr=state.lr;
    lower.r3=state.r[3];lower.r11=state.r[11];
    lower.r12=state.r[12];lower.r13=state.r[13];
    lower.xer_so=state.xer_so;
    lower.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
    if(!crt_status_error::Apply(0x827ca628u,active->memory,*active,lower))
        throw std::runtime_error("missing accepted status bridge");
    state.sp=lower.sp;state.lr=lower.lr;
    state.r[3]=lower.r3;state.r[11]=lower.r11;
    state.r[12]=lower.r12;state.r[13]=lower.r13;
    state.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
    ToPpc(context,state);
}

void OriginalCloseIndirect(std::uint32_t target,PPCContext& context,
    std::uint8_t*)
{
    auto state=FromPpc(context);
    active_guest->CallIndirect(active->memory,target,state);
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto& item:Cases)
        {
            try{Check(item);}
            catch(const std::exception& error)
            {
                std::fprintf(stderr,"%s: %s\n",item.name,error.what());
                return 1;
            }
        }
        std::printf("PASS crt-stream-close-pipeline %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT nonzero heap guest free, native import internals, faults and concurrent mutation remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

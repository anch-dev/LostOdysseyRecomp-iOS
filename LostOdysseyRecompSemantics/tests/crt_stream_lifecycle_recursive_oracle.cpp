#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_stream_lifecycle_recursive.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace lifecycle=crt_stream_lifecycle_recursive;
namespace shutdown=crt_stream_shutdown_sweep;
enum class LifecycleRoute {UpperSkip,UpperShutdown,ModeOneEmpty,
    ModeOneInactive,NullReentry,ModeOneActive};
struct LifecycleCase {GuestAddress entry;LifecycleRoute route;};
constexpr std::array LifecycleCases{
    LifecycleCase{0x82b7b6c8u,LifecycleRoute::UpperSkip},
    LifecycleCase{0x82b7b6c8u,LifecycleRoute::UpperShutdown},
    LifecycleCase{0x82b7b950u,LifecycleRoute::ModeOneEmpty},
    LifecycleCase{0x82b7b950u,LifecycleRoute::ModeOneInactive},
    LifecycleCase{0x82b7b8d0u,LifecycleRoute::NullReentry},
    LifecycleCase{0x82b7b950u,LifecycleRoute::ModeOneActive}};
constexpr std::array<Region,6> LifecycleRegions{{{0,0x120000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x83245000u,0x1000u},{0x832d3000u,0x2000u},
    {0x83378000u,0x3000u}}};
using LifecycleEvent=std::array<std::uint64_t,6>;
struct LifecycleServices final : crt_formatting_support::NativeServices,
    crt_float_environment::NativeServices,
    crt_formatter::DynamicServices,
    crt_stream_close_error::GuestServices,
    crt_free_context::LowerCalls,
    crt_stream_index_unlock::NativeServices,
    crt_stream_bulk_close_routes::NativeServices,
    crt_stream_flush_context::NativeServices
{
    std::vector<LifecycleEvent> events;
    [[noreturn]] static void Unexpected()
    {throw std::runtime_error("unselected lifecycle lower boundary");}
    void InitializeUnicodeString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unexpected();}
    void UnicodeStringToAnsiString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unexpected();}
    void FreeAnsiString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unexpected();}
    void InitAnsiString(GuestMemory&,GuestAddress,GuestAddress) override
    {Unexpected();}
    std::uint64_t WriteAnsi(GuestAddress,std::uint16_t) override
    {Unexpected();}
    void CallDebugMonitor(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
    void CallExceptionHandler(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
    void BugCheck(GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
    void CallGuestFormatter(GuestAddress,GuestMemory&,
        crt_formatter::Registers&) override {Unexpected();}
    void CallIndirect(GuestMemory&,GuestAddress target,
        crt_stream_close_error::Registers& state) override
    {
        if(target!=0x2600u) Unexpected();
        events.push_back({5u,target,state.sp,state.lr,state.r[3],state.r[10]});
        state.r[11]=0xfedcba9800000011ull;
        state.r[3]=0x1234567800000000ull;
    }
    void Call(GuestAddress entry,GuestMemory&,
        crt_free_context::Registers& state) override
    {
        if(entry!=0x823ade28u) Unexpected();
        events.push_back({4u,entry,state.sp,state.lr,state.r[3],state.r[5]});
        state.r[3]=1u;
        state.r[10]=0x1234567800000044ull;
    }
    void EnterCriticalSection(GuestMemory&,
        crt_stream_bulk_close_routes::Registers& state) override
    {
        events.push_back({1u,0u,state.sp,state.lr,state.r[3],state.r[10]});
        state.r[10]=0x12345678000000aau;
    }
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_bulk_close_routes::Registers& state) override
    {
        events.push_back({2u,0u,state.sp,state.lr,state.r[3],state.r[10]});
        state.r[10]=0x12345678000000bbu;
    }
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_index_unlock::Registers& state) override
    {
        events.push_back({3u,0u,state.sp,state.lr,state.r3,state.r10});
        state.r10=0x12345678000000ccu;
    }
    void NtFlushBuffersFile(GuestMemory&,
        crt_stream_flush_context::Registers& state) override
    {
        events.push_back({6u,0u,state.sp,state.lr,state.r[3],state.r[10]});
        state.r[3]=0u;
    }
};
LifecycleServices* active_lifecycle=nullptr;

family::Dependencies LifecycleStreams(Services& stream,
    LifecycleServices& extra)
{return {stream,stream,stream,stream,
    {stream,stream,stream,stream,stream,stream,extra,stream},stream,stream};}
lifecycle::Dependencies LifecycleDependencies(Services& stream,
    LifecycleServices& extra)
{
    crt_wide_stream_output::Dependencies output{
        LifecycleStreams(stream,extra),extra};
    crt_float_formatting::Dependencies floating{{stream,stream},extra};
    crt_formatter::Dependencies formatter{output,floating,extra,stream,extra};
    crt_format_stream::Dependencies format{formatter};
    crt_stream_close_error::Dependencies close{
        LifecycleStreams(stream,extra),extra};
    shutdown::Dependencies shut{{close,format,extra},extra};
    return {shut,extra};
}
void SeedLifecycle(GuestWindow& window,LifecycleRoute route)
{
    Seed(window,Mode::Binary);
    auto memory=window.Memory();
    memory.WriteU32(0x83215360u,0x62000u); // initialized lock 1
    memory.WriteU32(0x83215358u,0x62100u); // initialized lock 0
    memory.WriteU32(0x832153d8u,0x62200u); // stream index 0 uses lock 16
    memory.WriteU32(0x83378e90u,0x39000u);
    memory.WriteU32(0x83378e94u,
        route==LifecycleRoute::ModeOneInactive||
        route==LifecycleRoute::ModeOneActive?1u:0u);
    memory.WriteU8(0x832d3ac0u,
        route==LifecycleRoute::UpperShutdown?1u:0u);
    memory.WriteU32(0x83245708u,0x62000u);
    memory.WriteU32(0x34004u,0x2601u);
    memory.WriteU32(Stream+28u,0u);
    if(route==LifecycleRoute::ModeOneInactive||
        route==LifecycleRoute::ModeOneActive)
        memory.WriteU32(0x39000u,Stream);
    memory.WriteU32(Stream+12u,
        route==LifecycleRoute::ModeOneActive?0x82u:0u);
}
PPCContext InitialLifecycle(const LifecycleCase& item)
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0x1111222233334444ull;
    context.ctr.u64=0x5555666677778888ull;
    context.r13.u64=Environment;
    context.r3.u64=item.route==LifecycleRoute::NullReentry?0u:1u;
    context.r27.u64=0x2711223344556677ull;
    context.r28.u64=0x2811223344556677ull;
    context.r29.u64=0x2911223344556677ull;
    context.r30.u64=0x3011223344556677ull;
    context.r31.u64=0x3111223344556677ull;
    context.xer.ca=1;context.xer.so=1;
    context.cr0={0,1,0,{1}};context.cr6={1,0,0,{1}};
    return context;
}
bool CheckLifecycle(const LifecycleCase& item,unsigned ordinal)
{
    GuestWindow original(LifecycleRegions),recovered(LifecycleRegions);
    SeedLifecycle(original,item.route);SeedLifecycle(recovered,item.route);
    Services expected(original,Mode::Binary),actual(recovered,Mode::Binary);
    LifecycleServices expected_extra,actual_extra;
    auto context=InitialLifecycle(item);auto state=FromPpc(context);
    active=&expected;active_lifecycle=&expected_extra;
    switch(item.entry)
    {
    case 0x82b7b6c8u:__imp__sub_82B7B6C8(context,original.Bytes());break;
    case 0x82b7b950u:__imp__sub_82B7B950(context,original.Bytes());break;
    case 0x82b7b8d0u:__imp__sub_82B7B8D0(context,original.Bytes());break;
    default:throw std::runtime_error("unknown lifecycle entry");
    }
    active_lifecycle=nullptr;active=nullptr;
    if(!lifecycle::Apply(item.entry,actual.memory,
            LifecycleDependencies(actual,actual_extra),state))
        throw std::runtime_error("missing lifecycle entry");
    const auto before=FromPpc(context);
    const bool same=Same(before,state)&&original.EqualCommitted(recovered)&&
        expected.events==actual.events&&
        expected_extra.events==actual_extra.events&&
        expected.locks==actual.locks&&expected.unlocks==actual.unlocks&&
        expected.traps==actual.traps;
    if(!Same(before,state))
        std::fprintf(stderr,"case %u state mismatch r3=%llx/%llx sp=%llx/%llx\n",
            ordinal,static_cast<unsigned long long>(before.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(before.sp),
            static_cast<unsigned long long>(state.sp));
    if(!original.EqualCommitted(recovered))
        for(const auto region:LifecycleRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",ordinal,
                    static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
    const auto& events=expected_extra.events;
    const auto count=[&events](std::uint64_t kind)
    {unsigned n=0;for(const auto& e:events)if(e[0]==kind)++n;return n;};
    const bool covered=expected.locks>=1u&&count(3u)>=1u&&
        (item.route==LifecycleRoute::UpperSkip?
            count(4u)==1u&&expected.locks==1u:
         item.route==LifecycleRoute::UpperShutdown?
            count(4u)==1u&&expected.locks==2u:
         item.route==LifecycleRoute::ModeOneActive?
            expected.locks>=2u&&count(3u)>=2u:
            count(4u)==0u&&count(5u)==0u&&context.r3.u64==0u);
    if(!covered)
        std::fprintf(stderr,"case %u missed lifecycle path r3=%llx events=%zu locks=%u\n",
            ordinal,static_cast<unsigned long long>(context.r3.u64),
            events.size(),expected.locks);
    return same&&covered;
}
} // namespace

void OriginalSave27(PPCContext& context,std::uint8_t*)
{
    const auto state=FromPpc(context);
    for(unsigned index=27u;index<=31u;++index)
        WriteU64(active->memory,Address(context.r1.u64-
            16u-8u*(31u-index)),state.r[index]);
    active->memory.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}
void OriginalRestore27(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    for(unsigned index=27u;index<=31u;++index)
        state.r[index]=ReadU64(active->memory,Address(context.r1.u64-
            16u-8u*(31u-index)));
    state.r[12]=active->memory.ReadU32(Address(context.r1.u64-8u));
    state.lr=state.r[12];ToPpc(context,state);
}
void OriginalLifecycleAccepted(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=FromPpc(context);
    auto dependencies=LifecycleDependencies(*active,*active_lifecycle);
    bool found=false;
    if(entry==0x82b817e8u)
        found=shutdown::Apply(entry,active->memory,
            dependencies.shutdown,state);
    else if(entry==0x823addc0u)
        found=crt_free_context::Apply(entry,active->memory,
            dependencies.shutdown.pipeline.free_lower,state);
    else if(entry==0x82b7b778u||entry==0x82b7b810u||
        entry==0x82b7b838u)
        found=crt_format_stream::Apply(entry,active->memory,
            dependencies.shutdown.pipeline.format,state);
    else if(entry==0x82b81f78u)
        found=crt_stream_flush_context::Apply(entry,active->memory,
            dependencies.shutdown.pipeline.close.accepted,
            dependencies.flush,state);
    else if(entry==0x82b819c8u)
    {
        crt_stream_index_unlock::Registers call{state.sp,state.lr,state.r[3],
            state.r[10],state.r[11],state.r[12],state.r[29],state.r[31]};
        found=crt_stream_index_unlock::Apply(entry,active->memory,
            dependencies.shutdown.pipeline.close.accepted.locks.index_unlock,
            call);
        state.sp=call.sp;state.lr=call.lr;state.r[3]=call.r3;
        state.r[10]=call.r10;state.r[11]=call.r11;state.r[12]=call.r12;
        state.r[29]=call.r29;state.r[31]=call.r31;
    }
    else if(entry==0x82b81b28u)
    {
        crt_stream_locks::Registers call{};
        call.sp=state.sp;call.lr=state.lr;call.ctr=state.ctr;
        call.r3=state.r[3];call.r4=state.r[4];call.r5=state.r[5];
        call.r6=state.r[6];call.r7=state.r[7];call.r8=state.r[8];
        call.r9=state.r[9];call.r10=state.r[10];call.r11=state.r[11];
        call.r12=state.r[12];call.r13=state.r[13];call.r28=state.r[28];
        call.r29=state.r[29];call.r30=state.r[30];call.r31=state.r[31];
        call.cr0={bool(state.cr0.lt),bool(state.cr0.gt),
            bool(state.cr0.eq),bool(state.cr0.so)};
        call.cr6={bool(state.cr6.lt),bool(state.cr6.gt),
            bool(state.cr6.eq),bool(state.cr6.so)};
        call.xer_ca=state.xer_ca;call.xer_so=state.xer_so;
        found=crt_stream_locks::Apply(entry,active->memory,
            dependencies.shutdown.pipeline.close.accepted.locks,call);
        state.sp=call.sp;state.lr=call.lr;state.ctr=call.ctr;
        state.r[3]=call.r3;state.r[4]=call.r4;state.r[5]=call.r5;
        state.r[6]=call.r6;state.r[7]=call.r7;state.r[8]=call.r8;
        state.r[9]=call.r9;state.r[10]=call.r10;state.r[11]=call.r11;
        state.r[12]=call.r12;state.r[13]=call.r13;state.r[28]=call.r28;
        state.r[29]=call.r29;state.r[30]=call.r30;state.r[31]=call.r31;
        state.cr0={std::uint8_t(call.cr0.lt),std::uint8_t(call.cr0.gt),
            std::uint8_t(call.cr0.eq),std::uint8_t(call.cr0.so)};
        state.cr6={std::uint8_t(call.cr6.lt),std::uint8_t(call.cr6.gt),
            std::uint8_t(call.cr6.eq),std::uint8_t(call.cr6.so)};
        state.xer_ca=call.xer_ca;state.xer_so=call.xer_so;
    }
    else found=family::ApplyAcceptedCallee(entry,active->memory,
        dependencies.shutdown.pipeline.close.accepted,state);
    if(!found)throw std::runtime_error("missing accepted lifecycle lower");
    ToPpc(context,state);
}
int main()
{
    try
    {
        for(unsigned index=0;index<LifecycleCases.size();++index)
            if(!CheckLifecycle(LifecycleCases[index],index))return 1;
        std::printf("PASS crt-stream-lifecycle-recursive %zu actual PPC cases\n",
            LifecycleCases.size());
        std::puts("LIMIT selected guest free/close targets, native critical-section/flush imports, faults/MMIO and runtime");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

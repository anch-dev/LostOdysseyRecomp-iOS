#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_stream_shutdown_sweep.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace sweep = crt_stream_shutdown_sweep;
namespace bulk = crt_stream_bulk_close_routes;
enum class SweepRoute {Empty,NullSlot,Inactive,Active,ReleaseAtTwenty,
    DirectUnlock};
struct SweepCase {GuestAddress entry;SweepRoute route;};
constexpr std::array SweepCases{
    SweepCase{0x82b817e8u,SweepRoute::Empty},
    SweepCase{0x82b817e8u,SweepRoute::NullSlot},
    SweepCase{0x82b817e8u,SweepRoute::Inactive},
    SweepCase{0x82b817e8u,SweepRoute::Active},
    SweepCase{0x82b817e8u,SweepRoute::ReleaseAtTwenty},
    SweepCase{0x82b818a8u,SweepRoute::DirectUnlock}};
constexpr std::array<Region,6> SweepRegions{{{0,0x120000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x83245000u,0x1000u},{0x832d3000u,0x2000u},
    {0x83378000u,0x3000u}}};
using BoundaryEvent=std::array<std::uint64_t,6>;
struct SweepServices final : crt_formatting_support::NativeServices,
    crt_float_environment::NativeServices,
    crt_formatter::DynamicServices,
    crt_stream_close_error::GuestServices,
    crt_free_context::LowerCalls,
    crt_stream_index_unlock::NativeServices,
    bulk::NativeServices
{
    std::vector<BoundaryEvent> events;
    [[noreturn]] static void Unexpected()
    {throw std::runtime_error("unselected sweep lower boundary");}
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
        state.r[3]=1u; // successful selected guest release
        state.r[10]=0x1234567800000044ull;
    }
    void EnterCriticalSection(GuestMemory&,bulk::Registers& state) override
    {
        events.push_back({1u,0u,state.sp,state.lr,state.r[3],state.r[10]});
        state.r[10]=0x12345678000000aau;
    }
    void LeaveCriticalSection(GuestMemory&,bulk::Registers& state) override
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
};
SweepServices* active_sweep=nullptr;

family::Dependencies StreamDependencies(Services& stream,SweepServices& extra)
{return {stream,stream,stream,stream,
    {stream,stream,stream,stream,stream,stream,extra,stream},stream,stream};}
sweep::Dependencies SweepDependencies(Services& stream,SweepServices& extra)
{
    crt_wide_stream_output::Dependencies output{
        StreamDependencies(stream,extra),extra};
    crt_float_formatting::Dependencies floating{{stream,stream},extra};
    crt_formatter::Dependencies formatter{output,floating,extra,stream,extra};
    crt_format_stream::Dependencies format{formatter};
    crt_stream_close_error::Dependencies close{
        StreamDependencies(stream,extra),extra};
    return {{close,format,extra},extra};
}
void SeedSweep(GuestWindow& window,SweepRoute route)
{
    Seed(window,Mode::Binary);
    auto memory=window.Memory();
    memory.WriteU32(0x83215360u,0x62000u); // initialized indexed lock 1
    memory.WriteU32(0x83378e90u,0x39000u);
    memory.WriteU32(0x83378e94u,
        route==SweepRoute::Empty?3u:
        route==SweepRoute::ReleaseAtTwenty?21u:4u);
    memory.WriteU32(0x34004u,0x2601u);
    memory.WriteU32(0x83245708u,0x62000u);
    memory.WriteU32(Stream+28u,0u);
    if(route==SweepRoute::Inactive||route==SweepRoute::Active)
        memory.WriteU32(0x39000u+12u,Stream);
    if(route==SweepRoute::ReleaseAtTwenty)
        memory.WriteU32(0x39000u+80u,Stream);
    if(route!=SweepRoute::Active)
        memory.WriteU32(Stream+12u,0u);
    else
    {
        memory.WriteU32(Stream+12u,0x82u);
        memory.WriteU32(Stream,Buffer);
    }
}
PPCContext InitialSweep()
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0x1111222233334444ull;
    context.ctr.u64=0x5555666677778888ull;
    context.r13.u64=Environment;
    context.r26.u64=0x2611223344556677ull;
    context.r27.u64=0x2711223344556677ull;
    context.r28.u64=0x2811223344556677ull;
    context.r29.u64=0x2911223344556677ull;
    context.r30.u64=0x3011223344556677ull;
    context.r31.u64=0x3111223344556677ull;
    context.xer.ca=1;context.xer.so=1;
    context.cr0={0,1,0,{1}};context.cr6={1,0,0,{1}};
    return context;
}
bool CheckSweep(const SweepCase& item,unsigned ordinal)
{
    GuestWindow original(SweepRegions),recovered(SweepRegions);
    SeedSweep(original,item.route);SeedSweep(recovered,item.route);
    Services expected(original,Mode::Binary),actual(recovered,Mode::Binary);
    SweepServices expected_extra,actual_extra;
    auto context=InitialSweep();auto state=FromPpc(context);
    active=&expected;active_sweep=&expected_extra;
    if(item.entry==0x82b817e8u)
        __imp__sub_82B817E8(context,original.Bytes());
    else __imp__sub_82B818A8(context,original.Bytes());
    active_sweep=nullptr;active=nullptr;
    if(!sweep::Apply(item.entry,actual.memory,
            SweepDependencies(actual,actual_extra),state))
        throw std::runtime_error("missing sweep entry");
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
        for(const auto region:SweepRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",ordinal,
                    static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
    const auto& events=expected_extra.events;
    const auto count=[&events](std::uint64_t kind)
    {unsigned n=0;for(const auto& e:events) if(e[0]==kind)++n;return n;};
    const bool covered=item.route==SweepRoute::DirectUnlock?
        count(3u)==1u&&events.size()==1u:
        expected.locks==(item.route==SweepRoute::Active?2u:1u)&&
        count(3u)>=1u&&
        (item.route==SweepRoute::Active?
            count(5u)==1u&&context.r3.u64==1u:
         item.route==SweepRoute::ReleaseAtTwenty?
            count(4u)==1u&&expected.memory.ReadU32(0x39000u+80u)==0u:
            count(4u)==0u&&count(5u)==0u&&context.r3.u64==0u);
    if(!covered)
        std::fprintf(stderr,"case %u missed sweep path r3=%llx events=%zu locks=%u\n",
            ordinal,static_cast<unsigned long long>(context.r3.u64),
            events.size(),expected.locks);
    return same&&covered;
}
} // namespace

void OriginalSave26(PPCContext& context,std::uint8_t*)
{
    const auto state=FromPpc(context);
    for(unsigned index=26u;index<=31u;++index)
        WriteU64(active->memory,Address(context.r1.u64-
            16u-8u*(31u-index)),state.r[index]);
    active->memory.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}
void OriginalRestore26(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    for(unsigned index=26u;index<=31u;++index)
        state.r[index]=ReadU64(active->memory,Address(context.r1.u64-
            16u-8u*(31u-index)));
    state.r[12]=active->memory.ReadU32(Address(context.r1.u64-8u));
    state.lr=state.r[12];ToPpc(context,state);
}
void OriginalAcceptedSweep(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=FromPpc(context);
    auto dependencies=SweepDependencies(*active,*active_sweep);
    bool found=false;
    if(entry==0x82b85d88u)
        found=bulk::Apply(entry,active->memory,dependencies,state);
    else if(entry==0x823addc0u)
        found=crt_free_context::Apply(entry,active->memory,
            dependencies.pipeline.free_lower,state);
    else if(entry==0x82b819c8u)
    {
        crt_stream_index_unlock::Registers call{state.sp,state.lr,state.r[3],
            state.r[10],state.r[11],state.r[12],state.r[29],state.r[31]};
        found=crt_stream_index_unlock::Apply(entry,active->memory,
            dependencies.pipeline.close.accepted.locks.index_unlock,call);
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
            dependencies.pipeline.close.accepted.locks,call);
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
    if(!found) throw std::runtime_error("missing accepted sweep lower");
    ToPpc(context,state);
}
int main()
{
    try
    {
        for(unsigned index=0;index<SweepCases.size();++index)
            if(!CheckSweep(SweepCases[index],index)) return 1;
        std::printf("PASS crt-stream-shutdown-sweep %zu actual PPC cases\n",
            SweepCases.size());
        std::puts("LIMIT selected guest release/dynamic close targets and native critical-section internals; faults/MMIO and runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

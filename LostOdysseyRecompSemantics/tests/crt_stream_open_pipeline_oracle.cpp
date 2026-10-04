#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_stream_open_pipeline.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace pipeline = crt_stream_open_pipeline;
namespace routes = crt_stream_open_routes_context;
using Full = pipeline::Registers;
using Trace = std::array<std::uint64_t,5>;
constexpr GuestAddress OutputFlags=0x53000u,OutputSlot=0x53004u;
constexpr GuestAddress Path=0x40000u,Handle=0x59000u;
constexpr GuestAddress FlagGlobal=0x832ecd20u;
constexpr std::array<test::Region,7> OpenRegions{{{0,0x120000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x83245000u,0x1000u},{0x832d3000u,0x2000u},
    {0x832ec000u,0x2000u},{0x83378000u,0x3000u}}};

enum class Scenario {InvalidFlags,SlotAllocationFailure,NativeOpenFailure,
    BinarySuccess};
struct OpenCase {const char* name;Scenario scenario;};
constexpr std::array Cases{
    OpenCase{"invalid-flags-22",Scenario::InvalidFlags},
    OpenCase{"slot-allocation-24",Scenario::SlotAllocationFailure},
    OpenCase{"native-open-status",Scenario::NativeOpenFailure},
    OpenCase{"binary-open-success",Scenario::BinarySuccess}};

struct IndexService final : crt_stream_index_unlock::NativeServices
{
    std::vector<Trace> events;
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_index_unlock::Registers& state) override
    {events.push_back({1u,state.sp,state.lr,state.r3,state.r29});}
};

struct Host final : routes::GuestServices,
    crt_stream_position_routes::NativeServices,
    crt_async_status_transfer::NativeServices,
    crt_utf8_conversion_routes::NativeServices,
    heap_allocation_context::BoundaryServices,
    heap_free_context::BoundaryServices,
    crt_free_context::LowerCalls,
    crt_stream_close_error::GuestServices,
    crt_float_environment::NativeServices
{
    Scenario scenario;
    std::vector<Trace> events;
    explicit Host(Scenario value):scenario(value){}
    [[noreturn]] static void Unselected()
    {throw std::runtime_error("unselected CRT open guest/native path");}
    void KeTlsGetValue(GuestMemory&,routes::Registers&) override
    {Unselected();}
    void KeTlsSetValue(GuestMemory&,routes::Registers&) override
    {Unselected();}
    void AllocateCrtRecord(GuestMemory&,routes::Registers& state) override
    {
        events.push_back({2u,state.sp,state.lr,state.r[3],state.r[4]});
        state.r[3]=scenario==Scenario::SlotAllocationFailure?0u:0x38000u;
    }
    void EnterCriticalSection(GuestMemory&,routes::Registers& state) override
    {events.push_back({3u,state.sp,state.lr,state.r[3],state.r[30]});}
    void LeaveCriticalSection(GuestMemory&,routes::Registers& state) override
    {events.push_back({4u,state.sp,state.lr,state.r[3],state.r[30]});}
    void InitAnsiString(GuestMemory& memory,routes::Registers& state) override
    {
        events.push_back({5u,state.sp,state.lr,state.r[3],state.r[4]});
        auto length=std::uint16_t{0};
        while(memory.ReadU8(Address(state.r[4])+length)!=0u)++length;
        memory.WriteU16(Address(state.r[3]),length);
        memory.WriteU16(Address(state.r[3]+2u),
            static_cast<std::uint16_t>(length+1u));
        memory.WriteU32(Address(state.r[3]+4u),Address(state.r[4]));
    }
    void CallOpenFile(GuestAddress target,GuestMemory& memory,
        routes::Registers& state) override
    {
        if(target!=0x2600u)
            throw std::runtime_error("unexpected open-file target");
        events.push_back({6u,state.sp,state.lr,target,state.r[4]});
        if(scenario==Scenario::NativeOpenFailure)
            state.r[3]=0xffffffffc0000017ull;
        else
        {
            memory.WriteU32(Address(state.r[3]),Handle);
            memory.WriteU32(Address(state.r[6]+4u),0u);
            state.r[3]=0u;
        }
    }
    void NtQueryInformationFile(GuestMemory&,Full&) override
    {Unselected();}
    void NtSetInformationFile(GuestMemory&,Full&) override
    {Unselected();}
    void CallIndirect(GuestAddress,GuestMemory&,Full&) override
    {Unselected();}
    void NtWaitForSingleObjectEx(GuestMemory&,Full&) override
    {Unselected();}
    void NtStatusToDosError(GuestMemory&,Full&) override
    {Unselected();}
    void RtlMultiByteToUnicodeN(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override {Unselected();}
    void RtlNtStatusToDosError(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override {Unselected();}
    void CallDirect(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override {Unselected();}
    void CallNative(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override {Unselected();}
    void Call(GuestAddress,GuestMemory&,
        crt_free_context::Registers&) override {Unselected();}
    void CallIndirect(GuestMemory&,GuestAddress,
        crt_stream_close_error::Registers&) override {Unselected();}
    void CallDebugMonitor(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
    void CallExceptionHandler(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
    void BugCheck(GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
};

Services* current=nullptr;
IndexService* current_index=nullptr;
Host* current_host=nullptr;

family::Dependencies StreamDeps(Services& stream,IndexService& index)
{return {stream,stream,stream,stream,
    {stream,stream,stream,stream,stream,stream,index,stream},
    stream,stream};}

pipeline::Dependencies OpenDeps(Services& stream,IndexService& index,
    Host& host)
{
    auto accepted=StreamDeps(stream,index);
    return {accepted,host,{accepted,host,host,host},
        {accepted,host,host,host,host},host,host};
}

void SeedOpen(GuestWindow& window,Scenario scenario)
{
    Seed(window,scenario==Scenario::NativeOpenFailure?
        Mode::NativeFailureFive:Mode::LockedWrite);
    auto memory=window.Memory();
    memory.WriteU32(FlagGlobal,0x89abcdefu);
    memory.WriteU32(0x3400cu,0x2601u);
    memory.WriteU8(Path,'C');memory.WriteU8(Path+1u,':');
    memory.WriteU8(Path+2u,'\\');memory.WriteU8(Path+3u,0u);
    memory.WriteU32(0x83245708u,0x10000u);
    memory.WriteU32(0x832153b0u,0x65000u);
    memory.WriteU32(0x832153a8u,0x65100u);
    memory.WriteU32(0x30000u,UINT32_MAX);
    memory.WriteU8(0x30004u,0u);
    memory.WriteU32(0x30008u,1u);
    memory.WriteU32(OutputFlags,0u);
    memory.WriteU32(OutputSlot,UINT32_MAX);
    if(scenario==Scenario::SlotAllocationFailure)
        memory.WriteU32(Blocks,0u);
}

PPCContext InitialOpen(Scenario scenario)
{
    PPCContext context{};
    const auto gprs=crt_full_oracle::Gprs(context);
    for(unsigned i=0;i<32u;++i)
        gprs[i]->u64=0x1122334400000000ull+i;
    context.r1.u64=0x8877665500000000ull|Stack;
    context.r13.u64=0xaabbccdd00000000ull|Environment;
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.r3.u64=OutputFlags;
    context.r4.u64=OutputSlot;
    context.r5.u64=Path;
    context.r6.u64=scenario==Scenario::InvalidFlags?3u:0x8000u;
    context.r7.u64=16u;
    context.r8.u64=0u;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    context.f0.u64=0x3ff0000000000000ull;
    context.f1.u64=0x4008000000000000ull;
    context.f31.u64=0x4010000000000000ull;
    context.fpscr.csr=0x1f80u;
    return context;
}

void CheckOpen(const OpenCase& item)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedOpen(original,item.scenario);SeedOpen(recovered,item.scenario);
    const auto mode=item.scenario==Scenario::NativeOpenFailure?
        Mode::NativeFailureFive:Mode::LockedWrite;
    Services expected(original,mode),actual(recovered,mode);
    IndexService expected_index,actual_index;
    Host expected_host(item.scenario),actual_host(item.scenario);
    PPCContext context=InitialOpen(item.scenario);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;current_index=&expected_index;
    current_host=&expected_host;active=&expected;
    __imp__sub_82DF6240(context,original.Bytes());
    current=nullptr;current_index=nullptr;current_host=nullptr;active=nullptr;
    if(!pipeline::Apply(0x82df6240u,actual.memory,
            OpenDeps(actual,actual_index,actual_host),state))
        throw std::runtime_error("missing recovered open pipeline");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_index.events!=actual_index.events||
        expected_host.events!=actual_host.events||
        expected.traps!=actual.traps||expected.converted_errors!=actual.converted_errors)
        throw std::runtime_error("full context, ordered callbacks or RAM mismatch");
    const auto memory=expected.memory;
    switch(item.scenario)
    {
    case Scenario::InvalidFlags:
        if(context.r3.u64!=22u||memory.ReadU32(OutputSlot)!=UINT32_MAX||
            memory.ReadU32(0x83215210u)!=22u||expected.traps!=1u||
            !expected_host.events.empty())
            throw std::runtime_error("invalid flag path not reached");
        break;
    case Scenario::SlotAllocationFailure:
        if(context.r3.u64!=24u||memory.ReadU32(OutputSlot)!=UINT32_MAX||
            memory.ReadU32(0x83215210u)!=24u||
            expected_host.events.empty()||
            expected_host.events[0][0]!=2u)
            throw std::runtime_error("record slot exhaustion path not reached");
        break;
    case Scenario::NativeOpenFailure:
        if(context.r3.u64==0u||memory.ReadU32(OutputSlot)==UINT32_MAX||
            expected_host.events.empty()||
            expected_host.events.back()[0]!=6u||
            expected.converted_errors==0u)
            throw std::runtime_error("native open failure path not reached");
        break;
    case Scenario::BinarySuccess:
        // The fixture frees slot zero (0x30000); Record is slot five.
        if(context.r3.u64!=0u||memory.ReadU32(OutputSlot)!=0u||
            memory.ReadU32(0x30000u)!=Handle||
            expected_host.events.empty()||
            expected_host.events.back()[0]!=6u)
            throw std::runtime_error("binary open success path not reached");
        break;
    }
}
} // namespace

void OriginalOpenCoreLower(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!pipeline::ApplyLower(entry,current->memory,
            OpenDeps(*current,*current_index,*current_host),state))
        throw std::runtime_error("missing actual accepted open lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalOpenSelectedLower(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=FromPpc(context);
    if(!routes::ApplyAcceptedLower(entry,current->memory,
            StreamDeps(*current,*current_index),*current_host,state))
        throw std::runtime_error("missing accepted open-route lower");
    ToPpc(context,state);
}
void OriginalOpenAllocation(PPCContext& context,std::uint8_t*)
{auto state=FromPpc(context);
 current_host->AllocateCrtRecord(current->memory,state);
 ToPpc(context,state);}
void OriginalOpenNative(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    switch(entry)
    {
    case 0x830d9c6cu:current_host->EnterCriticalSection(current->memory,state);break;
    case 0x830d9c7cu:current_host->LeaveCriticalSection(current->memory,state);break;
    case 0x830d9dfcu:current_host->InitAnsiString(current->memory,state);break;
    default:throw std::runtime_error("unselected open native import");
    }
    ToPpc(context,state);
}
void OriginalOpenIndirect(GuestAddress target,PPCContext& context,std::uint8_t*)
{auto state=FromPpc(context);
 current_host->CallOpenFile(target,current->memory,state);
 ToPpc(context,state);}
void OriginalPositionLower(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!pipeline::ApplyLower(entry,current->memory,
            OpenDeps(*current,*current_index,*current_host),state))
        throw std::runtime_error("missing accepted position lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalPositionHeap(GuestAddress,PPCContext&,std::uint8_t*)
{throw std::runtime_error("unselected position heap path");}
void OriginalPositionQuery(PPCContext&,std::uint8_t*)
{throw std::runtime_error("unselected position query import");}
void OriginalPositionSet(PPCContext&,std::uint8_t*)
{throw std::runtime_error("unselected position set import");}

int main()
{
    try
    {
        for(const auto& item:Cases)
        {
            try{CheckOpen(item);}
            catch(const std::exception& error)
            {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
        }
        std::printf("PASS crt-stream-open-pipeline %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT 885 complete original instructions; selected native/open callbacks, heap guest paths, positive transfer and read branches, faults/MMIO/concurrency remain bounded");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

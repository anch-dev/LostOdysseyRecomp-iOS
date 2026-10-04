// Reuse the accepted 82DF6240 selected-context fixture and its original-body
// adapters. Its main is compiled here only as a helper and is never run.
#pragma push_macro("main")
#undef main
#define main OpenPipelineFixtureMain
#include "crt_stream_open_pipeline_oracle.cpp"
#undef main
#pragma pop_macro("main")

#include "lo_semantics/crt_stream_open_wrapper.h"

namespace wrapper_oracle
{
namespace wrapper = crt_stream_open_wrapper;
using WrapperTrace = std::array<std::uint64_t, 5>;
enum class Route {NullOutput,InvalidFlags,BinarySuccess,ActiveCleanup};
struct WrapperCase {const char* name;Route path;};
constexpr std::array WrapperCases{
    WrapperCase{"null-output-22",Route::NullOutput},
    WrapperCase{"invalid-flags-22",Route::InvalidFlags},
    WrapperCase{"binary-open-success",Route::BinarySuccess},
    WrapperCase{"active-cleanup-6868",Route::ActiveCleanup}};
constexpr GuestAddress ParentFrame=0x81000u;

struct WrapperUnlock final : crt_stream_pointer_unlock::NativeServices
{
    std::vector<WrapperTrace> events;
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_pointer_unlock::Registers& state) override
    {events.push_back({state.sp,state.lr,state.r3,state.r30,state.r31});}
};
WrapperUnlock* original_unlock=nullptr;

wrapper::Dependencies Deps(Services& stream,IndexService& index,
    Host& host,WrapperUnlock& unlock)
{return {OpenDeps(stream,index,host),unlock};}

void SeedWrapper(GuestWindow& window,Route path)
{
    SeedOpen(window,Scenario::BinarySuccess);
    if(path!=Route::ActiveCleanup)return;
    auto memory=window.Memory();
    memory.WriteU32(ParentFrame+80u,1u);
    memory.WriteU32(ParentFrame+84u,1u);
    memory.WriteU32(ParentFrame+164u,OutputSlot);
    memory.WriteU32(OutputSlot,0u);
    memory.WriteU8(0x30004u,0x81u);
}

PPCContext Initial(Route path)
{
    auto context=InitialOpen(Scenario::BinarySuccess);
    if(path==Route::ActiveCleanup)
    {
        context.r12.u64=ParentFrame+112u;
        context.r30.u64=0x1122334400003900ull;
        return context;
    }
    context.r3.u64=Path;
    context.r4.u64=0x8000u;
    context.r5.u64=16u;
    context.r6.u64=path==Route::InvalidFlags?0x200u:0u;
    context.r7.u64=path==Route::NullOutput?0u:OutputSlot;
    context.r8.u64=path==Route::InvalidFlags?1u:0u;
    return context;
}

bool HasOpenCallback(const Host& host)
{
    for(const auto& event:host.events)
        if(event[0]==6u)return true;
    return false;
}

void Check(const WrapperCase& item)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedWrapper(original,item.path);SeedWrapper(recovered,item.path);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    WrapperUnlock expected_unlock,actual_unlock;
    auto context=Initial(item.path);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;current_index=&expected_index;
    current_host=&expected_host;active=&expected;
    original_unlock=&expected_unlock;
    if(item.path==Route::ActiveCleanup)
        __imp__sub_82DF6868(context,original.Bytes());
    else
        __imp__sub_82DF6760(context,original.Bytes());
    current=nullptr;current_index=nullptr;current_host=nullptr;
    active=nullptr;original_unlock=nullptr;
    const auto entry=item.path==Route::ActiveCleanup?0x82df6868u:0x82df6760u;
    if(!wrapper::Apply(entry,actual.memory,
            Deps(actual,actual_index,actual_host,actual_unlock),state))
        throw std::runtime_error("missing recovered CRT open wrapper");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=
            crt_full_oracle::Snapshot(state)||
        !original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_index.events!=actual_index.events||
        expected_host.events!=actual_host.events||
        expected_unlock.events!=actual_unlock.events||
        expected.traps!=actual.traps||
        expected.converted_errors!=actual.converted_errors)
        throw std::runtime_error("open wrapper full state/RAM/callback mismatch");
    const auto memory=expected.memory;
    switch(item.path)
    {
    case Route::NullOutput:case Route::InvalidFlags:
        if(context.r3.u64!=22u||memory.ReadU32(0x83215210u)!=22u||
            expected.traps!=1u||!expected_host.events.empty()||
            !expected_unlock.events.empty())
            throw std::runtime_error("wrapper invalid-argument path not reached");
        break;
    case Route::BinarySuccess:
        if(context.r3.u64!=0u||memory.ReadU32(OutputSlot)!=0u||
            memory.ReadU32(0x30000u)!=Handle||!HasOpenCallback(expected_host)||
            expected_unlock.events.size()!=1u||
            expected_unlock.events[0][2]!=0x3000cu)
            throw std::runtime_error("wrapper binary success path not reached");
        break;
    case Route::ActiveCleanup:
        if(memory.ReadU8(0x30004u)!=0x80u||
            memory.ReadU32(OutputSlot)!=0u||
            expected_unlock.events.size()!=1u||
            expected_unlock.events[0][2]!=0x3000cu)
            throw std::runtime_error("wrapper active cleanup path not reached");
        break;
    }
}
} // namespace wrapper_oracle

void OriginalWrapperPointer(PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp=state.r[1];lower.lr=state.lr;
    lower.r3=state.r[3];lower.r9=state.r[9];
    lower.r10=state.r[10];lower.r11=state.r[11];
    lower.r12=state.r[12];lower.r30=state.r[30];
    lower.r31=state.r[31];lower.xer_ca=state.xer_ca;
    if(!crt_stream_pointer_unlock::Apply(0x82b863f0u,current->memory,
            *wrapper_oracle::original_unlock,lower))
        throw std::runtime_error("missing original accepted pointer unlock");
    state.r[1]=lower.sp;state.lr=lower.lr;
    state.r[3]=lower.r3;state.r[9]=lower.r9;
    state.r[10]=lower.r10;state.r[11]=lower.r11;
    state.r[12]=lower.r12;state.r[30]=lower.r30;
    state.r[31]=lower.r31;state.xer_ca=lower.xer_ca;
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(const auto& item:wrapper_oracle::WrapperCases)
    {
        try{wrapper_oracle::Check(item);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::puts("crt stream open wrapper: 4 focused actual PPC cases passed");
    return 0;
}

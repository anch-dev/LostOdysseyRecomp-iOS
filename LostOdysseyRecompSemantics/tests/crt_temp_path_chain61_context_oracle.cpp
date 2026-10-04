// The accepted close fixture supplies selected CRT errno services and full
// PPC register/RAM comparison without executing its own case matrix.
#pragma push_macro("main")
#undef main
#define main TempPathSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main
#pragma pop_macro("main")

#include "lo_semantics/crt_temp_path_chain61_context.h"
#include "lo_semantics/crt_stream_close_caller.h"
#include "lo_semantics/crt_context_adapter.h"

namespace temp_path_oracle
{
namespace adjacent=crt_temp_path_chain61_context;
constexpr GuestAddress Output=0x55000u,Source=0x55100u;
enum class ScanRoute {DuplicateSuccess,DuplicateNull,ParentSuccess,ParentNullOutput};
struct ScanCase {const char* name;GuestAddress entry;ScanRoute route;};
constexpr std::array ScanCases{
    ScanCase{"duplicate-live-heap-copy",0x82df3770u,ScanRoute::DuplicateSuccess},
    ScanCase{"duplicate-null",0x82df3770u,ScanRoute::DuplicateNull},
    ScanCase{"existing-temp-path-parent-success",0x82df3e80u,ScanRoute::ParentSuccess},
    ScanCase{"temp-parent-null-output-errno22",0x82df3e80u,ScanRoute::ParentNullOutput}};
constexpr GuestAddress TempPath=0x832eccf0u,Table=0x56000u,Allocation=0x58000u;
constexpr GuestAddress MainRecord=0x83214af0u;
using Registers=adjacent::Registers;
struct AllocationBoundary final : raw_allocation_context::PpcBoundaryServices
{
    using Trace=std::array<std::uint64_t,8>;
    std::vector<Trace> events;
    void CallDirect(GuestAddress target,GuestMemory& memory,
        raw_allocation_context::Registers& state) override
    {
        events.push_back({target,state.r[3],state.r[4],state.r[5],state.r[1],
            state.lr,state.r[30],state.r[31]});
        if(target!=0x823accb0u) throw std::runtime_error("unselected raw heap callee");
        state.r[3]=0xaabbccdd00000000ull|Allocation;
        state.r[10]=0x8877665500000033ull;
        state.f1_bits=0x4008000000000000ull;
        memory.WriteU8(Allocation,0u);
    }
};
AllocationBoundary* active_allocation=nullptr;
void Seed(GuestWindow& window,ScanRoute)
{
    close_shared_oracle::SeedShared(window,close_shared_oracle::Route::NullPath);
    auto memory=window.Memory();
    memory.WriteU32(0x83378e90u,Table);memory.WriteU32(0x83378e94u,1u);
    memory.WriteU32(Table,MainRecord);memory.WriteU32(MainRecord+12u,0u);
    memory.WriteU32(0x83215368u,0x62000u);
    memory.WriteU32(0x832153d8u,0x62100u);
    memory.WriteU32(0x832d3aecu,0u);
    memory.WriteU32(0x83215300u,0x57000u);
    memory.WriteU32(0x57000u+172u,1u);memory.WriteU32(0x57000u+200u,0x57400u);
    memory.WriteU32(0x83215304u,0x57200u);memory.WriteU32(0x57208u,1u);
    for(unsigned i=0;i<256u;++i)
    {
        memory.WriteU8(0x5721du+i,0u);
        memory.WriteU16(0x57400u+2u*i,i>='0'&&i<='9'?4u:
            (i>='a'&&i<='z')||(i>='A'&&i<='Z')?1u:0u);
    }
    const char* text="save.1";
    for(unsigned i=0;;++i)
    {
        memory.WriteU8(Source+i,static_cast<std::uint8_t>(text[i]));
        memory.WriteU8(TempPath+i,static_cast<std::uint8_t>(text[i]));
        if(text[i]==0)break;
    }
    memory.WriteU32(Output,0xdeadbeefu);
}
PPCContext Initial(ScanRoute route)
{
    auto context=close_shared_oracle::Initial(close_shared_oracle::Route::NullPath);
    if(route==ScanRoute::DuplicateSuccess||route==ScanRoute::DuplicateNull)
        context.r3.u64=route==ScanRoute::DuplicateNull?0u:0xaabbccdd00000000ull|Source;
    else
    {
        context.r3.u64=route==ScanRoute::ParentNullOutput?0u:0xaabbccdd00000000ull|Output;
        context.r4.u64=16u;
    }
    return context;
}
void Check(const ScanCase& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    Seed(original,item.route);Seed(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    AllocationBoundary expected_allocation,actual_allocation;
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;active=&expected;current_index=&expected_index;
    current_host=&expected_host;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    active_allocation=&expected_allocation;
    if(item.entry==0x82df3770u) __imp__sub_82DF3770(context,original.Bytes());
    else __imp__sub_82DF3E80(context,original.Bytes());
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;
    active_allocation=nullptr;
    const adjacent::Dependencies dependencies{
        close_shared_oracle::Deps(actual,actual_index,actual_host,actual_unlock,actual_extra),
        actual_allocation};
    if(!adjacent::Apply(item.entry,actual.memory,dependencies,state))
        throw std::runtime_error("missing temporary-path body");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_index.events!=actual_index.events||
        expected_host.events!=actual_host.events||
        expected_unlock.events!=actual_unlock.events||
        expected_extra.events!=actual_extra.events||
        expected.traps!=actual.traps||
        expected.converted_errors!=actual.converted_errors||
        expected_allocation.events!=actual_allocation.events)
    {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i])
                std::fprintf(stderr,"case %u state[%u] %llx/%llx\n",ordinal,i,
                    static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        for(const auto region:OpenRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=
                    recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",
                    ordinal,static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
        throw std::runtime_error("adjacent scan full state/RAM/callback mismatch");
    }
    if(context.r1.u64!=(0x8877665500000000ull|Stack)||context.lr!=0x81234567u||
        context.r29.u64!=0x112233440000001dull||context.r31.u64!=0x112233440000001full)
        throw std::runtime_error("missed saved temp-path ABI");
    if(item.route==ScanRoute::DuplicateSuccess)
    {
        if(context.r3.u32!=Allocation||expected_allocation.events.size()!=1u||
            expected.memory.ReadU8(Allocation)!='s'||expected.memory.ReadU8(Allocation+5u)!='1'||
            expected.memory.ReadU8(Allocation+6u)!=0u)
            throw std::runtime_error("missed live heap string duplication");
    }
    else if(item.route==ScanRoute::DuplicateNull)
    {
        if(context.r3.u64!=0u||!expected_allocation.events.empty())
            throw std::runtime_error("missed null duplication");
    }
    else if(item.route==ScanRoute::ParentNullOutput)
    {
        if(context.r3.u64!=22u||expected.memory.ReadU32(0x83215210u)!=22u)
            throw std::runtime_error("missed parent null errno");
    }
    else if(context.r3.u64!=0u||expected.memory.ReadU32(Output)!=MainRecord||
        expected.memory.ReadU8(TempPath+5u)!='2'||
        expected.memory.ReadU32(MainRecord+28u)!=Allocation||
        expected_allocation.events.size()!=1u||expected_host.events.empty()||
        expected_index.events.empty())
        throw std::runtime_error("missed parent parser/open/duplication/unlock success");
}

} // namespace temp_path_oracle

void OriginalTempAllocation(PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    auto raw=crt_context_adapter::ToRaw(state);
    if(!raw_allocation_context::Apply(0x823acbd0u,current->memory,
        *temp_path_oracle::active_allocation,raw))
        throw std::runtime_error("missing accepted raw allocation");
    crt_context_adapter::FromRaw(state,raw);crt_full_oracle::ToPpc(context,state);
}
void OriginalTempClose(PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    auto selected=crt_context_adapter::ToStream(state);
    if(!crt_stream_close_caller::Apply(0x82b87b18u,current->memory,
        {StreamDeps(*current,*current_index),*current_host},selected))
        throw std::runtime_error("missing accepted locked close");
    crt_context_adapter::FromStream(state,selected);crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(unsigned i=0;i<temp_path_oracle::ScanCases.size();++i)
    {
        const auto& item=temp_path_oracle::ScanCases[i];
        try{temp_path_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-temp-path-chain61-context %zu actual PPC cases\n",
        temp_path_oracle::ScanCases.size());
    std::puts("LIMIT selected full context and accepted raw-heap/open/close selected boundary; blank-image initialization untested; no runtime/fault/MMIO/concurrency validation");
    return 0;
}

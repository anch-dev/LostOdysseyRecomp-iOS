#define main NumericFollowTempFixtureMain
#include "crt_temp_path_chain61_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_stream_numeric_follow61_context.h"
namespace numeric_follow_oracle
{
namespace family=crt_stream_numeric_follow61_context;
using namespace temp_path_oracle;
enum class Route {PublicSuccess,PublicLimit,CleanupInactive,CleanupActive};
struct Case {const char* name;GuestAddress entry;Route route;};
constexpr std::array Cases{
    Case{"public-tempstream-success",0x82df41b0u,Route::PublicSuccess},
    Case{"public-tempstream-numeric-limit",0x82df41b0u,Route::PublicLimit},
    Case{"saved-parent-cleanup-inactive",0x82df4144u,Route::CleanupInactive},
    Case{"saved-parent-cleanup-active",0x82df4144u,Route::CleanupActive}};
constexpr GuestAddress Parent=0x81000u;
void SeedCase(GuestWindow& window,Route route)
{
    Seed(window,ScanRoute::ParentSuccess);
    auto memory=window.Memory();
    if(route==Route::PublicLimit)
    {
        const char* text="save.1VVVVVV";
        for(unsigned i=0;;++i)
        {memory.WriteU8(TempPath+i,static_cast<std::uint8_t>(text[i]));if(text[i]==0)break;}
    }
    memory.WriteU32(Parent+88u,MainRecord);
    memory.WriteU32(Parent+92u,route==Route::CleanupActive?1u:0u);
    if(route==Route::CleanupActive) memory.WriteU32(MainRecord+12u,0x8080u);
}
PPCContext InitialCase(Route route)
{
    auto context=Initial(ScanRoute::ParentSuccess);
    if(route==Route::CleanupInactive||route==Route::CleanupActive)
        context.r12.u64=0x8877665500000000ull|(Parent+176u);
    return context;
}
void Check(const Case& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedCase(original,item.route);SeedCase(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    AllocationBoundary expected_allocation,actual_allocation;
    auto context=InitialCase(item.route);auto state=crt_full_oracle::FromPpc(context);
    current=&expected;active=&expected;current_index=&expected_index;current_host=&expected_host;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;active_allocation=&expected_allocation;
    if(item.entry==0x82df41b0u) __imp__sub_82DF41B0(context,original.Bytes());
    else __imp__sub_82DF4144(context,original.Bytes());
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;active_allocation=nullptr;
    const family::Dependencies dependencies{
        close_shared_oracle::Deps(actual,actual_index,actual_host,actual_unlock,actual_extra),actual_allocation};
    if(!family::Apply(item.entry,actual.memory,dependencies,state))
        throw std::runtime_error("missing numeric public wrapper/cleanup body");
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

    if(context.lr!=0x81234567u||context.r29.u64!=0x112233440000001dull||
        context.r31.u64!=0x112233440000001full)
        throw std::runtime_error("missed public/cleanup saved registers");
    if(item.route==Route::PublicSuccess)
    {
        if(context.r3.u64!=MainRecord||context.r1.u64!=(0x8877665500000000ull|Stack)||
            expected.memory.ReadU8(TempPath+5u)!='2'||expected_allocation.events.size()!=1u)
            throw std::runtime_error("missed complete public temp-stream return");
    }
    else if(item.route==Route::PublicLimit)
    {
        if(context.r3.u64!=0u||context.r1.u64!=(0x8877665500000000ull|Stack)||
            !expected_allocation.events.empty()||expected.memory.ReadU8(TempPath+5u)!='1')
            throw std::runtime_error("missed public parser limit/no stream");
    }
    else
    {
        const auto count=item.route==Route::CleanupActive?2u:1u;
        if(context.r1.u64!=Stack||expected_index.events.size()!=count||
            (item.route==Route::CleanupActive&&expected.memory.ReadU32(MainRecord+12u)!=0x80u))
            throw std::runtime_error("missed saved parent/index cleanup ABI");
    }
}
} // namespace numeric_follow_oracle
int main()
{
    for(unsigned i=0;i<numeric_follow_oracle::Cases.size();++i)
    {
        const auto& item=numeric_follow_oracle::Cases[i];
        try { numeric_follow_oracle::Check(item,i); }
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-stream-numeric-follow61-context %zu actual PPC cases\n",numeric_follow_oracle::Cases.size());
    std::puts("LIMIT selected heap/open/close native ABI; no blank-image/fault/MMIO/runtime coverage");
    return 0;
}

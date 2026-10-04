// The accepted close fixture supplies selected CRT errno services and full
// PPC register/RAM comparison without executing its own case matrix.
#define main ScanAdjacentSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main

#include "lo_semantics/crt_stream_scan_adjacent_context.h"

namespace scan_adjacent_oracle
{
namespace adjacent=crt_stream_scan_adjacent_context;
constexpr GuestAddress Output=0x55000u,Source=0x55100u;
enum class ScanRoute {AppendSuccess,AppendOverflow,FormatSuccess,
    FormatInvalidBase};
struct ScanCase {const char* name;GuestAddress entry;ScanRoute route;};
constexpr std::array ScanCases{
    ScanCase{"append-with-terminator",0x82df5d18u,
        ScanRoute::AppendSuccess},
    ScanCase{"append-capacity-errno34",0x82df5d18u,
        ScanRoute::AppendOverflow},
    ScanCase{"decimal-format",0x82df5e30u,
        ScanRoute::FormatSuccess},
    ScanCase{"invalid-base-errno22",0x82df5e30u,
        ScanRoute::FormatInvalidBase}};

void Seed(GuestWindow& window)
{
    close_shared_oracle::SeedShared(window,
        close_shared_oracle::Route::NullPath);
    auto memory=window.Memory();
    memory.WriteU8(Output,'x');memory.WriteU8(Output+1u,'y');
    memory.WriteU8(Output+2u,0u);
    memory.WriteU8(Source,'A');memory.WriteU8(Source+1u,'B');
    memory.WriteU8(Source+2u,0u);
}
PPCContext Initial(ScanRoute route)
{
    auto context=close_shared_oracle::Initial(
        close_shared_oracle::Route::NullPath);
    if(route==ScanRoute::AppendSuccess||
        route==ScanRoute::AppendOverflow)
    {
        context.r3.u64=Output;
        context.r4.u64=route==ScanRoute::AppendSuccess?12u:4u;
        context.r5.u64=Source;
    }
    else
    {
        context.r3.u64=123u;
        context.r4.u64=Output;
        context.r5.u64=16u;
        context.r6.u64=route==ScanRoute::FormatSuccess?10u:1u;
        context.r7.u64=0u;
    }
    return context;
}
void Check(const ScanCase& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    Seed(original);Seed(recovered);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;active=&expected;current_index=&expected_index;
    current_host=&expected_host;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    if(item.entry==0x82df5d18u)
        __imp__sub_82DF5D18(context,original.Bytes());
    else
        __imp__sub_82DF5E30(context,original.Bytes());
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;
    if(!adjacent::Apply(item.entry,actual.memory,
            close_shared_oracle::Deps(actual,actual_index,actual_host,
                actual_unlock,actual_extra),state))
        throw std::runtime_error("missing adjacent scan body");
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
        expected.converted_errors!=actual.converted_errors)
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
    switch(item.route)
    {
    case ScanRoute::AppendSuccess:
        if(context.r3.u32!=0u||expected.memory.ReadU8(Output)!='x'||
            expected.memory.ReadU8(Output+2u)!='A'||
            expected.memory.ReadU8(Output+3u)!='B'||
            expected.memory.ReadU8(Output+4u)!=0u)
            throw std::runtime_error("missed bounded append success");
        break;
    case ScanRoute::AppendOverflow:
        if(context.r3.u32!=34u||expected.memory.ReadU8(Output)!=0u||
            expected.memory.ReadU32(0x83215210u)!=34u)
            throw std::runtime_error("missed bounded append overflow");
        break;
    case ScanRoute::FormatSuccess:
        if(context.r3.u32!=0u||expected.memory.ReadU8(Output)!='1'||
            expected.memory.ReadU8(Output+1u)!='2'||
            expected.memory.ReadU8(Output+2u)!='3'||
            expected.memory.ReadU8(Output+3u)!=0u)
            throw std::runtime_error("missed decimal format success");
        break;
    case ScanRoute::FormatInvalidBase:
        if(context.r3.u32!=22u||expected.memory.ReadU8(Output)!=0u||
            expected.memory.ReadU32(0x83215210u)!=22u)
            throw std::runtime_error("missed invalid base error");
        break;
    }
}
} // namespace scan_adjacent_oracle

int main()
{
    for(unsigned i=0;i<scan_adjacent_oracle::ScanCases.size();++i)
    {
        const auto& item=scan_adjacent_oracle::ScanCases[i];
        try{scan_adjacent_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-stream-scan-adjacent-context %zu actual PPC cases\n",
        scan_adjacent_oracle::ScanCases.size());
    std::puts("LIMIT selected full context and accepted CRT errno boundary; no runtime/fault/MMIO/concurrency validation");
    return 0;
}

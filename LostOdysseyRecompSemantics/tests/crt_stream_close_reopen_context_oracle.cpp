// Reuse the accepted shared-close full-context service and original PPC pin.
#define main ReopenSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main

#include "lo_semantics/crt_stream_close_reopen_context.h"

namespace close_reopen_oracle
{
namespace reopen = crt_stream_close_reopen_context;
enum class ReopenRoute {NullUpper,StaticModeZero,StaticModeOne,
    InnerInactive,DirectCleanup};
struct ReopenCase {const char* name;GuestAddress entry;ReopenRoute route;};
constexpr std::array ReopenCases{
    ReopenCase{"upper-null-record",0x82df1ea8u,ReopenRoute::NullUpper},
    ReopenCase{"upper-static-mode-zero",0x82df1ea8u,
        ReopenRoute::StaticModeZero},
    ReopenCase{"upper-static-mode-one",0x82df1ea8u,
        ReopenRoute::StaticModeOne},
    ReopenCase{"inner-inactive-record",0x82df1dd0u,
        ReopenRoute::InnerInactive},
    ReopenCase{"direct-static-cleanup",0x82df1f7cu,
        ReopenRoute::DirectCleanup}};
constexpr GuestAddress StaticRecord=0x83214af0u;

void SeedReopen(GuestWindow& window,ReopenRoute route)
{
    close_shared_oracle::SeedShared(window,
        close_shared_oracle::Route::NullPath);
    auto memory=window.Memory();
    memory.WriteU32(StaticRecord,0u);
    memory.WriteU32(StaticRecord+4u,0u);
    memory.WriteU32(StaticRecord+8u,0u);
    memory.WriteU32(StaticRecord+12u,
        route==ReopenRoute::InnerInactive?0u:
        route==ReopenRoute::DirectCleanup?0x8000u:1u);
    memory.WriteU32(StaticRecord+16u,UINT32_MAX);
    memory.WriteU32(StaticRecord+24u,0u);
    memory.WriteU32(0x832153d8u,0x62100u); // indexed lock 16
}
PPCContext Initial(ReopenRoute route)
{
    auto context=close_shared_oracle::Initial(
        close_shared_oracle::Route::NullPath);
    context.r3.u64=route==ReopenRoute::NullUpper?0u:StaticRecord;
    context.r4.u64=0u;
    context.r5.u64=route==ReopenRoute::StaticModeOne?1u:0u;
    if(route==ReopenRoute::DirectCleanup)
    {
        context.r30.u64=StaticRecord;
        context.r12.u64=context.r1.u64;
    }
    return context;
}
void RunOriginal(GuestAddress entry,PPCContext& context,
    std::uint8_t* bytes)
{
    switch(entry)
    {
    case 0x82df1ea8u:__imp__sub_82DF1EA8(context,bytes);return;
    case 0x82df1dd0u:__imp__sub_82DF1DD0(context,bytes);return;
    case 0x82df1f7cu:__imp__sub_82DF1F7C(context,bytes);return;
    default:throw std::runtime_error("unknown reopen oracle entry");
    }
}
void Check(const ReopenCase& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedReopen(original,item.route);SeedReopen(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;current_index=&expected_index;
    current_host=&expected_host;active=&expected;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    RunOriginal(item.entry,context,original.Bytes());
    current=nullptr;current_index=nullptr;current_host=nullptr;active=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;
    if(!reopen::Apply(item.entry,actual.memory,
            close_shared_oracle::Deps(actual,actual_index,actual_host,
                actual_unlock,actual_extra),state))
        throw std::runtime_error("missing recovered reopen body");
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
                std::fprintf(stderr,"case %u state[%u] %llx/%llx\n",
                    ordinal,i,static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        for(const auto region:OpenRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=
                    recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",
                    ordinal,static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
        throw std::runtime_error("full state/RAM/ordered callbacks mismatch");
    }
    switch(item.route)
    {
    case ReopenRoute::NullUpper:case ReopenRoute::InnerInactive:
        if(context.r3.u32!=UINT32_MAX||
            expected.memory.ReadU32(0x83215210u)!=22u)
            throw std::runtime_error("missed invalid reopen argument path");
        break;
    case ReopenRoute::StaticModeZero:case ReopenRoute::StaticModeOne:
        if(context.r3.u32!=UINT32_MAX||
            expected.memory.ReadU32(0x83215210u)!=9u||
            expected_index.events.empty())
            throw std::runtime_error("missed static record reopen/cleanup");
        break;
    case ReopenRoute::DirectCleanup:
        if(expected_index.events.size()!=1u)
            throw std::runtime_error("missed direct cleanup unlock");
        break;
    }
}
} // namespace close_reopen_oracle

void OriginalFileAccepted(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_close_file_lower::ApplyAcceptedLower(entry,current->memory,
            close_shared_oracle::Deps(*current,*current_index,*current_host,
                *wrapper_oracle::original_unlock,
                *close_shared_oracle::active_extra),state))
        throw std::runtime_error("missing accepted close file lower");
    crt_full_oracle::ToPpc(context,state);
}

void OriginalReopenAccepted(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_close_reopen_context::ApplyAcceptedLower(entry,
            current->memory,
            close_shared_oracle::Deps(*current,*current_index,*current_host,
                *wrapper_oracle::original_unlock,
                *close_shared_oracle::active_extra),state))
        throw std::runtime_error("missing accepted reopen lower");
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(unsigned i=0;i<close_reopen_oracle::ReopenCases.size();++i)
    {
        const auto& item=close_reopen_oracle::ReopenCases[i];
        try{close_reopen_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-stream-close-reopen-context %zu actual PPC cases\n",
        close_reopen_oracle::ReopenCases.size());
    std::puts("LIMIT selected full state; successful descriptor I/O and deeper accepted native/guest boundaries remain unverified");
    return 0;
}

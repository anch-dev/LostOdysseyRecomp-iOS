// The accepted close fixture supplies selected CRT errno services and full
// PPC register/RAM comparison without executing its own case matrix.
#define main Numeric61SharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main

#include "lo_semantics/crt_numeric61_context.h"

namespace numeric61_oracle
{
namespace adjacent=crt_numeric61_context;
constexpr GuestAddress Output=0x55000u,Source=0x55100u;
enum class ScanRoute {FormatSuccess,FormatInvalidBase,SearchSuccess,SearchNull};
struct ScanCase {const char* name;GuestAddress entry;ScanRoute route;};
constexpr std::array ScanCases{
    ScanCase{"numeric-tail-decimal",0x82df5fb0u,ScanRoute::FormatSuccess},
    ScanCase{"numeric-tail-invalid-base",0x82df5fb0u,ScanRoute::FormatInvalidBase},
    ScanCase{"search-tail-last-byte",0x82df6098u,ScanRoute::SearchSuccess},
    ScanCase{"search-null-errno22",0x82df5fb8u,ScanRoute::SearchNull}};
void Seed(GuestWindow& window)
{
    close_shared_oracle::SeedShared(window,close_shared_oracle::Route::NullPath);
    auto memory=window.Memory();
    memory.WriteU32(0x83215304u,0x55200u);
    memory.WriteU32(0x55208u,1u);
    for(unsigned i=0;i<256;++i) memory.WriteU8(0x5521du+i,0u);
    memory.WriteU8(Source,'A'); memory.WriteU8(Source+1,'B');
    memory.WriteU8(Source+2,'A'); memory.WriteU8(Source+3,0u);
}
PPCContext Initial(ScanRoute route)
{
    auto context=close_shared_oracle::Initial(close_shared_oracle::Route::NullPath);
    if(route==ScanRoute::SearchSuccess||route==ScanRoute::SearchNull)
    {
        context.r3.u64=route==ScanRoute::SearchNull?0u:Source;
        context.r4.u64='A';context.r5.u64=0xabcdef0123456789ull;
    }
    else
    {
        context.r3.u64=123u;context.r4.u64=Output;context.r5.u64=16u;
        context.r6.u64=route==ScanRoute::FormatSuccess?10u:1u;
        context.r7.u64=0xfedcba9876543210ull;
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
    if(item.entry==0x82df5fb0u) __imp__sub_82DF5FB0(context,original.Bytes());
    else if(item.entry==0x82df6098u) __imp__sub_82DF6098(context,original.Bytes());
    else __imp__sub_82DF5FB8(context,original.Bytes());
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
    case ScanRoute::SearchSuccess:
        if(context.r3.u32!=Source+2u) throw std::runtime_error("missed last occurrence");
        break;
    case ScanRoute::SearchNull:
        if(context.r3.u64!=0u||expected.memory.ReadU32(0x83215210u)!=22u)
            throw std::runtime_error("missed search null error");
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
} // namespace numeric61_oracle

int main()
{
    for(unsigned i=0;i<numeric61_oracle::ScanCases.size();++i)
    {
        const auto& item=numeric61_oracle::ScanCases[i];
        try{numeric61_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-numeric61-context %zu actual PPC cases\n",
        numeric61_oracle::ScanCases.size());
    std::puts("LIMIT selected full context and accepted CRT errno boundary; no runtime/fault/MMIO/concurrency validation");
    return 0;
}

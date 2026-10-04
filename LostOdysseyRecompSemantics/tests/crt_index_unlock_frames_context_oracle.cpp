// The accepted close fixture supplies selected CRT errno services and full
// PPC register/RAM comparison without executing its own case matrix.
#define main IndexFramesSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main

#include "lo_semantics/crt_index_unlock_frames_context.h"

namespace index_frames_oracle
{
namespace adjacent=crt_index_unlock_frames_context;
enum class ScanRoute {ParentIndex0,ParentIndex3,LocalIndex19};
struct ScanCase {const char* name;GuestAddress entry;ScanRoute route;};
constexpr std::array ScanCases{
    ScanCase{"parent-index0-high-frame",0x82df61acu,ScanRoute::ParentIndex0},
    ScanCase{"parent-index3-high-nonvolatile",0x82df61acu,ScanRoute::ParentIndex3},
    ScanCase{"local-index19-saved-parent-slot",0x82df61d8u,ScanRoute::LocalIndex19}};
constexpr GuestAddress Frame=0x81000u,Table=0x56000u;
constexpr GuestAddress StreamTable=0x83378e90u,StreamCount=0x83378e94u;
unsigned Index(ScanRoute route)
{ return route==ScanRoute::ParentIndex0?0u:route==ScanRoute::ParentIndex3?3u:19u; }
void Seed(GuestWindow& window,ScanRoute route)
{
    close_shared_oracle::SeedShared(window,close_shared_oracle::Route::NullPath);
    auto memory=window.Memory(); const unsigned index=Index(route);
    memory.WriteU32(StreamTable,Table);memory.WriteU32(StreamCount,20u);
    memory.WriteU32(Table+4u*index,Record);
    memory.WriteU32(Frame+80u,index);
    memory.WriteU32(Record+12u,0x8003u);
    memory.WriteU32(0x83215358u+8u*(index+16u),0x62000u);
}
PPCContext Initial(ScanRoute route)
{
    auto context=close_shared_oracle::Initial(close_shared_oracle::Route::NullPath);
    context.r12.u64=0x8877665500000000ull|(Frame+128u);
    context.r29.u64=0xaabbccdd00000000ull|Index(route);
    context.r30.u64=0xffffffff83378e90ull;
    context.r31.u64=0x1122334400059000ull;
    context.lr=0xfedcba9882df617cull;
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
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;active=&expected;current_index=&expected_index;
    current_host=&expected_host;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    if(item.entry==0x82df61acu) __imp__sub_82DF61AC(context,original.Bytes());
    else __imp__sub_82DF61D8(context,original.Bytes());
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
    if(expected_index.events.size()!=1u||context.r1.u64!=Stack||
        context.r29.u64!=(0xaabbccdd00000000ull|Index(item.route))||
        context.r30.u64!=0xffffffff83378e90ull||
        context.r31.u64!=0x1122334400059000ull||context.lr!=0x82df617cu||
        expected.memory.ReadU32(Record+12u)!=3u)
    {
        std::fprintf(stderr,"guard index-events=%zu sp=%llx r29=%llx r30=%llx r31=%llx lr=%llx flags=%x\n",
            expected_index.events.size(),static_cast<unsigned long long>(context.r1.u64),
            static_cast<unsigned long long>(context.r29.u64),
            static_cast<unsigned long long>(context.r30.u64),
            static_cast<unsigned long long>(context.r31.u64),
            static_cast<unsigned long long>(context.lr),expected.memory.ReadU32(Record+12u));
        throw std::runtime_error("missed indexed unlock or local-frame ABI restoration");
    }
}

} // namespace index_frames_oracle

int main()
{
    for(unsigned i=0;i<index_frames_oracle::ScanCases.size();++i)
    {
        const auto& item=index_frames_oracle::ScanCases[i];
        try{index_frames_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-index-unlock-frames-context %zu actual PPC cases\n",
        index_frames_oracle::ScanCases.size());
    std::puts("LIMIT selected full context and accepted indexed-unlock/native ABI boundary; no runtime/fault/MMIO/concurrency validation");
    return 0;
}

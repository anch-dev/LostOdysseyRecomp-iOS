#pragma push_macro("main")
#undef main
#define main BlockOutputSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main
#pragma pop_macro("main")

#include "lo_semantics/crt_close_block_output_context.h"

namespace block_output_oracle
{
namespace block=crt_close_block_output_context;
using Full=block::Registers;
enum class BlockRoute {Zero,Overflow,NullStream,Buffered,DebugOutput,Unlock};
struct BlockCase {const char* name;GuestAddress entry;BlockRoute route;};
constexpr std::array BlockCases{
    BlockCase{"zero-items",0x82df2538u,BlockRoute::Zero},
    BlockCase{"multiply-overflow-invalid",0x82df2538u,BlockRoute::Overflow},
    BlockCase{"wrapper-null-stream",0x82df27b0u,BlockRoute::NullStream},
    BlockCase{"wrapper-buffered-copy-unlock",0x82df27b0u,BlockRoute::Buffered},
    BlockCase{"debug-output-live-two-chunks",0x82df2538u,BlockRoute::DebugOutput},
    BlockCase{"caller-frame-unlock",0x82df2884u,BlockRoute::Unlock}};
constexpr GuestAddress Input=0x55000u;

struct BlockExtra final : close_shared_oracle::SharedExtra,
    block::ErrorOutputServices
{
    std::vector<std::array<std::uint64_t,8>> output_events;
    void EnterCriticalSection(GuestMemory&,
        crt_stream_bulk_close_routes::Registers& state) override
    {
        events.push_back({3u,state.sp,state.lr,state.r[3],state.r[10],state.r[31]});
        state.r[10]=0xabcddcba00000031ull;
    }
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_bulk_close_routes::Registers& state) override
    {
        events.push_back({4u,state.sp,state.lr,state.r[3],state.r[10],state.r[31]});
        state.r[10]=0xabcddcba00000041ull;
    }
    void CallOutput(GuestMemory& memory,Full& state) override
    {
        const auto message=Address(state.r[3]);
        unsigned size=0;
        while(memory.ReadU8(message+size)!=0u&&size<256u)++size;
        output_events.push_back({state.r[1],state.lr,state.r[3],state.r[30],
            state.r[31],size,memory.ReadU8(message),state.r[11]});
        if(size==0u||size>255u)throw std::runtime_error("missed debug chunk");
        state.r[3]=0x1122334400000001ull;
        state.r[10]=0xaabbccdd00000021ull;
        state.r[11]=0xaabbccdd00000022ull;
        state.fpr_bits[7]^=0x100u;
        state.cached_fp_control^=0x40u;
        state.cr1.gt^=1u;state.cr7.eq^=1u;state.xer_ca^=1u;
    }
};
BlockExtra* original_extra=nullptr;

block::Dependencies Deps(Services& stream,IndexService& index,Host& host,
    wrapper_oracle::WrapperUnlock& unlock,BlockExtra& extra)
{
    return {close_shared_oracle::Deps(stream,index,host,unlock,extra),extra};
}

void SeedBlock(GuestWindow& window)
{
    SeedOpen(window,Scenario::BinarySuccess);
    auto memory=window.Memory();
    memory.WriteU32(Stream,Buffer);
    memory.WriteU32(Stream+4u,12u);
    memory.WriteU32(Stream+12u,8u);
    memory.WriteU32(Stream+24u,12u);
    for(unsigned i=0;i<260u;++i)
        memory.WriteU8(Input+i,static_cast<std::uint8_t>(1u+(i%251u)));
}

PPCContext InitialBlock(BlockRoute route)
{
    auto context=InitialOpen(Scenario::BinarySuccess);
    context.cr1.gt=1u;context.cr7.lt=1u;
    context.r3.u64=Input;
    context.r4.u64=2u;context.r5.u64=3u;context.r6.u64=Stream;
    if(route==BlockRoute::Zero)context.r4.u64=0u;
    if(route==BlockRoute::Overflow)
    {context.r4.u64=0x80000000u;context.r5.u64=3u;}
    if(route==BlockRoute::NullStream)context.r6.u64=0u;
    if(route==BlockRoute::DebugOutput)
    {context.r4.u64=1u;context.r5.u64=260u;
        context.r6.u64=0xffffffff83214b10ull;}
    if(route==BlockRoute::Unlock)
    {context.r30.u64=Stream;context.r12.u64=context.r1.u64+144u;}
    return context;
}

void CheckBlock(const BlockCase& item)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedBlock(original);SeedBlock(recovered);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    BlockExtra expected_extra,actual_extra;
    auto context=InitialBlock(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;active=&expected;current_index=&expected_index;
    current_host=&expected_host;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    original_extra=&expected_extra;
    switch(item.entry)
    {
    case 0x82df2538u:__imp__sub_82DF2538(context,original.Bytes());break;
    case 0x82df27b0u:__imp__sub_82DF27B0(context,original.Bytes());break;
    case 0x82df2884u:__imp__sub_82DF2884(context,original.Bytes());break;
    default:throw std::runtime_error("unknown selected block entry");
    }
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;original_extra=nullptr;
    if(!block::Apply(item.entry,actual.memory,
        Deps(actual,actual_index,actual_host,actual_unlock,actual_extra),state))
        throw std::runtime_error("missing selected block body");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||expected.traps!=actual.traps||
        expected_extra.events!=actual_extra.events||
        expected_extra.output_events!=actual_extra.output_events)
    {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i])std::fprintf(stderr,"state[%u] %llx/%llx\n",i,
                static_cast<unsigned long long>(before[i]),
                static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("block output full state/RAM/callback mismatch");
    }
    if(item.route==BlockRoute::Buffered&&
        (context.r3.u32!=3u||expected.memory.ReadU32(Stream+4u)!=6u||
            expected_extra.events.size()!=2u))
        throw std::runtime_error("missed buffered parent copy/lock/unlock");
    if(item.route==BlockRoute::DebugOutput&&
        (context.r3.u32!=260u||expected_extra.output_events.size()!=2u||
            expected_extra.output_events[0][5]!=255u||
            expected_extra.output_events[1][5]!=5u))
        throw std::runtime_error("missed debug chunk loop/live output");
    if((item.route==BlockRoute::Overflow||item.route==BlockRoute::NullStream)&&
        expected.memory.ReadU32(0x83215210u)!=22u)
        throw std::runtime_error("missed invalid arguments");
    if(item.route==BlockRoute::Zero&&context.r3.u32!=0u)
        throw std::runtime_error("missed zero-size return");
    if(item.route==BlockRoute::Unlock&&expected_extra.events.size()!=1u)
        throw std::runtime_error("missed caller-frame unlock");
}
}

void OriginalBlockLower(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_close_block_output_context::ApplyAcceptedLower(entry,current->memory,
        block_output_oracle::Deps(*current,*current_index,*current_host,
            *wrapper_oracle::original_unlock,*block_output_oracle::original_extra),state))
        throw std::runtime_error("missing selected actual block lower");
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(const auto& item:block_output_oracle::BlockCases)
    {
        try{block_output_oracle::CheckBlock(item);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::puts("PASS crt-close-block-output-context 6 focused actual PPC cases");
    std::puts("LIMIT accepted lower/native ABI and mutable guest output; no runtime/fault/MMIO/concurrency validation");
    return 0;
}

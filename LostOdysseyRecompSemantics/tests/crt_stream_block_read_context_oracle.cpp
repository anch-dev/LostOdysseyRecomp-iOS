// Reuse the accepted block-output fixture for its actual PPC pin and the
// selected full-register, stream, lock and native boundary services.
#define main BlockReadOutputFixtureMain
#include "crt_close_block_output_context_oracle.cpp"
#undef main

#include "lo_semantics/crt_stream_block_read_context.h"

namespace block_read_oracle
{
namespace read=crt_stream_block_read_context;
using block_output_oracle::Input;
enum class ReadRoute {ZeroItems,BufferedTail,CopyCapacity,ReadCapacity,
    NullStream};
struct ReadCase {const char* name;GuestAddress entry;ReadRoute route;};
constexpr std::array ReadCases{
    ReadCase{"zero-items",0x82df2158u,ReadRoute::ZeroItems},
    ReadCase{"tail-buffered-copy-unlock",0x82df2520u,
        ReadRoute::BufferedTail},
    ReadCase{"copy-capacity-fill-errno34",0x82df4318u,
        ReadRoute::CopyCapacity},
    ReadCase{"read-capacity-fill-errno34",0x82df2158u,
        ReadRoute::ReadCapacity},
    ReadCase{"wrapper-null-stream-errno22",0x82df23f8u,
        ReadRoute::NullStream}};

void SeedRead(GuestWindow& window,ReadRoute route)
{
    block_output_oracle::SeedBlock(window);
    auto memory=window.Memory();
    if(route==ReadRoute::BufferedTail)
    {
        memory.WriteU32(Stream,Buffer);
        memory.WriteU32(Stream+4u,12u);
        memory.WriteU32(Stream+12u,0x108u);
        memory.WriteU32(Stream+24u,12u);
        for(unsigned i=0;i<12u;++i)
            memory.WriteU8(Buffer+i,static_cast<std::uint8_t>('A'+i));
    }
}

PPCContext InitialRead(ReadRoute route)
{
    auto context=block_output_oracle::InitialBlock(
        block_output_oracle::BlockRoute::Buffered);
    context.r3.u64=Input;
    context.r4.u64=16u;
    context.r5.u64=1u;
    context.r6.u64=0u;
    context.r7.u64=Stream;
    switch(route)
    {
    case ReadRoute::ZeroItems:
        break;
    case ReadRoute::BufferedTail:
        // DF2520 shifts (size,count,stream) into the DF23F8 locked wrapper.
        context.r4.u64=1u;
        context.r5.u64=6u;
        context.r6.u64=Stream;
        break;
    case ReadRoute::CopyCapacity:
        context.r4.u64=4u;
        context.r5.u64=Buffer;
        context.r6.u64=8u;
        break;
    case ReadRoute::ReadCapacity:
        context.r4.u64=3u;
        context.r5.u64=2u;
        context.r6.u64=4u;
        break;
    case ReadRoute::NullStream:
        context.r4.u64=6u;
        context.r5.u64=1u;
        context.r6.u64=6u;
        context.r7.u64=0u;
        break;
    }
    return context;
}

void RunOriginal(GuestAddress entry,PPCContext& context,std::uint8_t* bytes)
{
    switch(entry)
    {
    case 0x82df2158u:__imp__sub_82DF2158(context,bytes);return;
    case 0x82df23f8u:__imp__sub_82DF23F8(context,bytes);return;
    case 0x82df2520u:__imp__sub_82DF2520(context,bytes);return;
    case 0x82df4318u:__imp__sub_82DF4318(context,bytes);return;
    case 0x82df24e4u:__imp__sub_82DF24E4(context,bytes);return;
    default:throw std::runtime_error("unknown block-read entry");
    }
}

void CheckRead(const ReadCase& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedRead(original,item.route);SeedRead(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    block_output_oracle::BlockExtra expected_extra,actual_extra;
    auto context=InitialRead(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;active=&expected;current_index=&expected_index;
    current_host=&expected_host;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    block_output_oracle::original_extra=&expected_extra;
    RunOriginal(item.entry,context,original.Bytes());
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;
    block_output_oracle::original_extra=nullptr;
    if(!read::Apply(item.entry,actual.memory,
            close_shared_oracle::Deps(actual,actual_index,actual_host,
                actual_unlock,actual_extra),state))
        throw std::runtime_error("missing selected block-read body");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_index.events!=actual_index.events||
        expected_host.events!=actual_host.events||
        expected_unlock.events!=actual_unlock.events||
        expected_extra.events!=actual_extra.events||
        expected_extra.output_events!=actual_extra.output_events||
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
        throw std::runtime_error("block read full state/RAM/callback mismatch");
    }
    switch(item.route)
    {
    case ReadRoute::ZeroItems:
        if(context.r3.u32!=0u||!expected_extra.events.empty())
            throw std::runtime_error("missed zero-items return");
        break;
    case ReadRoute::BufferedTail:
        if(context.r3.u32!=6u||expected.memory.ReadU32(Stream+4u)!=6u||
            expected_extra.events.size()!=2u||
            expected.memory.ReadU8(Input)!='A'||
            expected.memory.ReadU8(Input+5u)!='F')
            throw std::runtime_error("missed buffered read/copy/unlock path");
        break;
    case ReadRoute::CopyCapacity:
        if(context.r3.u32!=34u||
            expected.memory.ReadU32(0x83215210u)!=34u||
            expected.memory.ReadU8(Input)!=0u||
            expected.memory.ReadU8(Input+3u)!=0u)
            throw std::runtime_error("missed copy capacity fill/error path");
        break;
    case ReadRoute::ReadCapacity:
        if(expected.memory.ReadU32(0x83215210u)!=34u||
            expected.memory.ReadU8(Input)!=0u||
            expected.memory.ReadU8(Input+2u)!=0u)
            throw std::runtime_error("missed read capacity fill/error path");
        break;
    case ReadRoute::NullStream:
        if(context.r3.u32!=0u||
            expected.memory.ReadU32(0x83215210u)!=22u||
            expected.memory.ReadU8(Input)!=0u||
            expected.memory.ReadU8(Input+5u)!=0u)
            throw std::runtime_error("missed null-stream wrapper error path");
        break;
    }
}
} // namespace block_read_oracle

void OriginalBlockReadLower(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_block_read_context::ApplyAcceptedLower(entry,
            current->memory,
            close_shared_oracle::Deps(*current,*current_index,*current_host,
                *wrapper_oracle::original_unlock,
                *close_shared_oracle::active_extra),state))
        throw std::runtime_error("missing selected block-read lower");
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(unsigned i=0;i<block_read_oracle::ReadCases.size();++i)
    {
        const auto& item=block_read_oracle::ReadCases[i];
        try{block_read_oracle::CheckRead(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-stream-block-read-context %zu actual PPC cases\n",
        block_read_oracle::ReadCases.size());
    std::puts("LIMIT selected full state; accepted lower/native ABI, faults, MMIO and concurrency remain bounded");
    return 0;
}

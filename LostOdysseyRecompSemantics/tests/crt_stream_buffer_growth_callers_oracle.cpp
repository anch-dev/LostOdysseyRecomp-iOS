#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_stream_buffer_growth_callers.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace growth_oracle
{
namespace growth = crt_stream_buffer_growth_callers;
namespace record = crt_record_allocation_context;
namespace resize = crt_stream_resize_context;
using Full = growth::Registers;
using HeapRegisters = heap_allocation_context::Registers;
using GrowthEvent = std::array<std::uint64_t, 7>;
constexpr GuestAddress Heap=0x10000u,Block=0x100000u;
constexpr GuestAddress Descriptor=0x360000u,Sentinel=0x370000u;
constexpr GuestAddress Source=0x390000u,Other=0x391000u;
constexpr GuestAddress CountField=0x53000u,PointerField=0x53004u;
constexpr GuestAddress FlagField=0x53008u;
constexpr std::array<test::Region,6> GrowthRegions{{{0,0x400000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x83245000u,0x1000u},{0x832d3000u,0x2000u},
    {0x83378000u,0x3000u}}};

enum class Route {Fast,CopyTwo,CopyFour,ResizeOverflow};
struct GrowthCase {const char* name;GuestAddress entry;Route route;};
constexpr std::array GrowthCases{
    GrowthCase{"char-fast-count-mismatch",0x82df4a58u,Route::Fast},
    GrowthCase{"char-new-record-copy",0x82df4a58u,Route::CopyTwo},
    GrowthCase{"wide-new-record-copy",0x82b824b0u,Route::CopyFour},
    GrowthCase{"wide-resize-overflow",0x82b824b0u,Route::ResizeOverflow}};

struct GrowthHeap final : heap_allocation_context::BoundaryServices
{
    std::vector<GrowthEvent> events;
    static GrowthEvent Trace(GuestAddress entry,const HeapRegisters& state)
    {return {entry,state.r[1],state.lr,state.r[3],state.r[4],
        state.r[5],state.r[30]};}
    void CallDirect(GuestAddress entry,GuestMemory& memory,
        HeapRegisters& state) override
    {
        events.push_back(Trace(entry,state));
        switch(entry)
        {
        case 0x827cc428u:state.r[3]=0u;return;
        case 0x82b7bc40u:
            state.r[3]=FillGuestMemory(memory,Address(state.r[3]),
                Address(state.r[4]),Address(state.r[5]));return;
        default:throw std::runtime_error("unselected growth heap guest call");
        }
    }
    void CallNative(GuestAddress entry,GuestMemory&,
        HeapRegisters& state) override
    {
        events.push_back(Trace(entry,state));
        switch(entry)
        {
        case 0x830da07cu:state.r[3]=1u;return;
        case 0x830d9c6cu:case 0x830d9c7cu:return;
        default:throw std::runtime_error("unselected growth heap import");
        }
    }
};
struct GrowthHandler final : record::HandlerServices
{
    std::vector<GrowthEvent> events;
    void CallNewHandler(GuestAddress target,GuestMemory&,Full& state) override
    {
        events.push_back({target,state.r[1],state.lr,state.r[3],
            state.r[4],state.r[5],state.ctr});
        throw std::runtime_error("unselected growth new-handler target");
    }
};
struct GrowthGuest final : crt_reallocation_context::GuestServices
{
    std::vector<GrowthEvent> events;
    void CallLower(GuestAddress entry,GuestMemory&,Full& state) override
    {
        events.push_back({entry,state.r[1],state.lr,state.r[3],
            state.r[4],state.r[5],state.ctr});
        throw std::runtime_error("unselected deeper guest reallocation");
    }
};
struct GrowthQuery final : heap_block_query_context::NativeServices
{
    std::vector<GrowthEvent> events;
    void CallNative(GuestAddress entry,GuestMemory&,
        heap_block_query_context::Registers& state) override
    {
        events.push_back({entry,state.r[1],state.lr,state.r[3],
            state.r[4],state.r[5],state.ctr});
        throw std::runtime_error("unselected growth query import");
    }
};

void SeedFreeList(GuestMemory& memory,std::uint32_t units)
{
    const auto head=Heap+(units+48u)*8u,node=Block+8u;
    memory.WriteU16(Block,static_cast<std::uint16_t>(units));
    memory.WriteU16(Block+2u,0u);
    memory.WriteU8(Block+4u,0u);memory.WriteU8(Block+5u,0u);
    memory.WriteU32(head,node);memory.WriteU32(head+4u,node);
    memory.WriteU32(node,head);memory.WriteU32(node+4u,head);
    const auto word=Heap+((units>>5u)+88u)*4u;
    memory.WriteU32(word,memory.ReadU32(word)|(1u<<(units&31u)));
    memory.WriteU32(Heap+48u,memory.ReadU32(Heap+48u)+units);
    const auto next=Block+units*16u;
    memory.WriteU16(next,1u);
    memory.WriteU16(next+2u,static_cast<std::uint16_t>(units));
    memory.WriteU8(next+4u,0u);memory.WriteU8(next+5u,1u);
}

void SeedGrowth(test::GuestWindow& window,Route route)
{
    Seed(window,Mode::LockedWrite);
    auto memory=window.Memory();
    memory.WriteU32(0x83245708u,Heap);
    memory.WriteU32(0x832d3aecu,0u);
    memory.WriteU32(0x832d3ae8u,0u);
    memory.WriteU32(0x83378e80u,0u);
    memory.WriteU32(0x83215210u,0u);
    memory.WriteU32(Heap+20u,0u);memory.WriteU32(Heap+24u,1u);
    memory.WriteU32(Heap+28u,0xffffu);memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+96u,Descriptor);
    memory.WriteU32(Descriptor+44u,0x340000u);
    memory.WriteU32(Descriptor+64u,Block);
    memory.WriteU32(Heap+1408u,Sentinel);
    memory.WriteU8(Heap+379u,1u);
    for(unsigned units=0;units<128u;++units)
    {
        const auto head=Heap+(units+48u)*8u;
        memory.WriteU32(head,head);memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0;word<4u;++word)
        memory.WriteU32(Heap+(88u+word)*4u,0u);
    memory.WriteU32(Heap+88u,Heap+88u);
    memory.WriteU32(Heap+92u,Heap+88u);
    if(route==Route::CopyTwo||route==Route::CopyFour)
        SeedFreeList(memory,14u);
    const auto count=route==Route::ResizeOverflow?0x40000000u:3u;
    memory.WriteU32(CountField,count);
    memory.WriteU32(PointerField,Source);
    memory.WriteU32(FlagField,0u);
    for(unsigned i=0;i<8u;++i)
        memory.WriteU8(Source+i,static_cast<std::uint8_t>(0x51u+i));
}

PPCContext InitialGrowth(Route route)
{
    PPCContext context{};
    const auto gprs=crt_full_oracle::Gprs(context);
    const auto fprs=crt_full_oracle::Fprs(context);
    for(unsigned index=0;index<32u;++index)
    {
        gprs[index]->u64=0x1122334400000000ull+index;
        fprs[index]->u64=0x3ff0000000000000ull+index;
    }
    context.r1.u64=0x8877665500000000ull|Stack;
    context.r13.u64=0xaabbccdd00000000ull|Environment;
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.fpscr.csr=0x9fc0u;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr1.gt=1;
    context.cr6.gt=1;context.cr7.lt=1;
    context.r3.u64=route==Route::Fast?4u:
        route==Route::ResizeOverflow?0x40000000u:3u;
    context.r4.u64=CountField;
    context.r5.u64=PointerField;
    context.r6.u64=route==Route::ResizeOverflow?Other:Source;
    context.r7.u64=FlagField;
    return context;
}

growth::Dependencies Deps(Services& stream,GrowthHeap& heap,
    GrowthHandler& handler,GrowthGuest& guest,GrowthQuery& query)
{
    record::Dependencies allocation{Dependencies(stream),heap,handler};
    crt_reallocation_context::Dependencies reallocation{allocation,guest};
    resize::Dependencies resized{reallocation,query};
    return {allocation,resized};
}

Services* original_stream=nullptr;
GrowthHeap* original_heap=nullptr;
GrowthHandler* original_handler=nullptr;
GrowthGuest* original_guest=nullptr;
GrowthQuery* original_query=nullptr;

void Check(const GrowthCase& item)
{
    test::GuestWindow original(GrowthRegions),recovered(GrowthRegions);
    SeedGrowth(original,item.route);SeedGrowth(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    GrowthHeap expected_heap,actual_heap;
    GrowthHandler expected_handler,actual_handler;
    GrowthGuest expected_guest,actual_guest;
    GrowthQuery expected_query,actual_query;
    auto context=InitialGrowth(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_stream=&expected;original_heap=&expected_heap;
    original_handler=&expected_handler;original_guest=&expected_guest;
    original_query=&expected_query;
    if(item.entry==0x82df4a58u)
        __imp__sub_82DF4A58(context,original.Bytes());
    else
        __imp__sub_82B824B0(context,original.Bytes());
    active=nullptr;original_stream=nullptr;original_heap=nullptr;
    original_handler=nullptr;original_guest=nullptr;original_query=nullptr;
    if(!growth::Apply(item.entry,actual.memory,
            Deps(actual,actual_heap,actual_handler,actual_guest,actual_query),
            state))
        throw std::runtime_error("missing recovered growth caller");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_heap.events!=actual_heap.events||
        expected_handler.events!=actual_handler.events||
        expected_guest.events!=actual_guest.events||
        expected_query.events!=actual_query.events||
        expected.traps!=actual.traps)
        throw std::runtime_error("growth full state/RAM/ordered event mismatch");
    const auto memory=expected.memory;
    switch(item.route)
    {
    case Route::Fast:
        if(context.r3.u64!=1u||memory.ReadU32(CountField)!=3u||
            memory.ReadU32(PointerField)!=Source||
            !expected_heap.events.empty())
            throw std::runtime_error("growth fast path not reached");
        break;
    case Route::CopyTwo:case Route::CopyFour:
    {
        const auto copied=item.route==Route::CopyTwo?3u:6u;
        const auto allocation=memory.ReadU32(PointerField);
        if(context.r3.u64!=1u||allocation!=Block+16u||
            memory.ReadU32(CountField)!=6u||memory.ReadU32(FlagField)!=1u||
            expected_heap.events.empty())
            throw std::runtime_error("growth allocation path not reached");
        for(unsigned i=0;i<copied;++i)
            if(memory.ReadU8(allocation+i)!=memory.ReadU8(Source+i))
                throw std::runtime_error("growth byte copy not reached");
        break;
    }
    case Route::ResizeOverflow:
        if(context.r3.u64!=0u||memory.ReadU32(PointerField)!=Source||
            memory.ReadU32(CountField)!=0x40000000u||
            memory.ReadU32(0x83215210u)!=12u||
            !expected_heap.events.empty()||expected.traps!=1u)
            throw std::runtime_error("growth resize overflow not reached");
        break;
    }
}
} // namespace growth_oracle

void OriginalGrowthSave28(PPCContext& context)
{
    auto memory=active->memory;
    const auto state=crt_full_oracle::FromPpc(context);
    for(unsigned index=28u;index<=31u;++index)
        WriteU64(memory,Address(context.r1.u64-8u*(33u-index)),
            state.r[index]);
    memory.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}
void OriginalGrowthRestore28(PPCContext& context)
{
    auto memory=active->memory;
    auto state=crt_full_oracle::FromPpc(context);
    for(unsigned index=28u;index<=31u;++index)
        state.r[index]=ReadU64(memory,
            Address(context.r1.u64-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(context.r1.u64-8u));
    state.lr=state.r[12];crt_full_oracle::ToPpc(context,state);
}
void OriginalGrowthLower(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    auto dependencies=growth_oracle::Deps(*growth_oracle::original_stream,
        *growth_oracle::original_heap,*growth_oracle::original_handler,
        *growth_oracle::original_guest,*growth_oracle::original_query);
    bool applied=false;
    if(entry==0x82b81778u)
        applied=crt_record_allocation_context::Apply(entry,active->memory,
            dependencies.allocation,state);
    else if(entry==0x82b7d330u)
        applied=crt_stream_resize_context::Apply(entry,active->memory,
            dependencies.resize,state);
    if(!applied)throw std::runtime_error("missing selected growth lower");
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(const auto& item:growth_oracle::GrowthCases)
    {
        try{growth_oracle::Check(item);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::puts("CRT stream buffer growth: 4 focused actual PPC cases passed");
    return 0;
}

// Appended after the seven exact original PPC bodies in the pin.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_stream_table_initialize_context.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/raw_allocation_context.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace table_family = crt_stream_table_initialize_context;
using Full = table_family::Registers;
using HeapRegisters = raw_allocation_context::Registers;
constexpr GuestAddress AllocHeap = 0x10000u;
constexpr GuestAddress AllocBlock = 0x100000u;
constexpr GuestAddress AllocSentinel = 0x370000u;
constexpr std::array<test::Region,6> InitRegions{{{0,0x400000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x83245000u,0x1000u},{0x832d3000u,0x2000u},
    {0x83378000u,0x3000u}}};
enum class Scenario {LargeExact,NoFreeBlock};
constexpr std::array Scenarios{Scenario::LargeExact,Scenario::NoFreeBlock};

HeapRegisters ToHeap(const Full& state)
{
    HeapRegisters lower{};
    lower.r=state.r;lower.lr=state.lr;lower.ctr=state.ctr;
    lower.f0_bits=state.fpr_bits[0];lower.f1_bits=state.fpr_bits[1];
    lower.f13_bits=state.fpr_bits[13];lower.f30_bits=state.fpr_bits[30];
    lower.f31_bits=state.fpr_bits[31];
    lower.cached_fp_control=state.cached_fp_control;
    lower.xer_so=state.xer_so;lower.xer_ca=state.xer_ca;
    lower.cr0={state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.so};
    lower.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
    return lower;
}
void FromHeap(Full& state,const HeapRegisters& lower)
{
    state.r=lower.r;state.lr=lower.lr;state.ctr=lower.ctr;
    state.fpr_bits[0]=lower.f0_bits;state.fpr_bits[1]=lower.f1_bits;
    state.fpr_bits[13]=lower.f13_bits;state.fpr_bits[30]=lower.f30_bits;
    state.fpr_bits[31]=lower.f31_bits;
    state.cached_fp_control=lower.cached_fp_control;
    state.xer_so=lower.xer_so;state.xer_ca=lower.xer_ca;
    state.cr0={lower.cr0.lt,lower.cr0.gt,lower.cr0.eq,lower.cr0.un};
    state.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.un};
}

using InitEvent=std::array<std::uint64_t,7>;
struct HeapBoundary final : heap_allocation_context::BoundaryServices
{
    std::vector<InitEvent> events;
    static InitEvent Record(GuestAddress entry,const HeapRegisters& state)
    {return {entry,state.r[1],state.lr,state.r[3],state.r[4],
        state.r[5],state.r[30]};}
    void CallDirect(GuestAddress entry,GuestMemory& memory,
        HeapRegisters& state) override
    {
        events.push_back(Record(entry,state));
        switch(entry)
        {
        case 0x827cc428u:state.r[3]=0u;return;
        case 0x82b7bc40u:
            state.r[3]=FillGuestMemory(memory,Address(state.r[3]),
                Address(state.r[4]),Address(state.r[5]));return;
        default:throw std::runtime_error("unexpected table heap direct");
        }
    }
    void CallNative(GuestAddress entry,GuestMemory&,
        HeapRegisters& state) override
    {
        events.push_back(Record(entry,state));
        switch(entry)
        {
        case 0x830da07cu:state.r[3]=1u;return;
        case 0x830d9c6cu:case 0x830d9c7cu:return;
        default:throw std::runtime_error("unexpected table heap native");
        }
    }
};
struct Handler final : crt_record_allocation_context::HandlerServices
{
    void CallNewHandler(GuestAddress,GuestMemory&,Full&) override
    {throw std::runtime_error("unexpected table new handler");}
};

void SeedCase(test::GuestWindow& window,Scenario scenario)
{
    Seed(window,Mode::LockedWrite);
    auto memory=window.Memory();
    memory.WriteU32(0x83245708u,AllocHeap);
    memory.WriteU32(0x832d3aecu,0u);
    memory.WriteU32(0x832d3ae8u,0u);
    memory.WriteU32(0x83378e80u,0u);
    memory.WriteU32(0x83215210u,0u);
    memory.WriteU32(AllocHeap+20u,0u);
    memory.WriteU32(AllocHeap+24u,1u);
    memory.WriteU32(AllocHeap+28u,0xffffu);
    memory.WriteU32(AllocHeap+48u,0u);
    memory.WriteU32(AllocHeap+96u,0x360000u);
    memory.WriteU32(0x360000u+44u,0x340000u);
    memory.WriteU32(0x360000u+64u,AllocBlock);
    memory.WriteU32(AllocHeap+1408u,AllocSentinel);
    memory.WriteU8(AllocHeap+379u,1u);
    for(unsigned units=0;units<128u;++units)
    {
        const auto head=AllocHeap+(units+48u)*8u;
        memory.WriteU32(head,head);memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0;word<4u;++word)
        memory.WriteU32(AllocHeap+(88u+word)*4u,0u);
    memory.WriteU32(AllocHeap+88u,AllocHeap+88u);
    memory.WriteU32(AllocHeap+92u,AllocHeap+88u);
    if(scenario!=Scenario::LargeExact)return;
    constexpr auto units=129u,head=AllocHeap+384u,node=AllocBlock+8u;
    memory.WriteU16(AllocBlock,static_cast<std::uint16_t>(units));
    memory.WriteU16(AllocBlock+2u,0u);
    memory.WriteU8(AllocBlock+4u,0u);
    memory.WriteU8(AllocBlock+5u,0u);
    memory.WriteU32(head,node);memory.WriteU32(head+4u,node);
    memory.WriteU32(node,head);memory.WriteU32(node+4u,head);
    memory.WriteU32(AllocHeap+48u,units);
    const auto next=AllocBlock+units*16u;
    memory.WriteU16(next,1u);
    memory.WriteU16(next+2u,static_cast<std::uint16_t>(units));
    memory.WriteU8(next+4u,0u);
    memory.WriteU8(next+5u,1u);
}

PPCContext Initial()
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
    context.cr0.lt=1;context.cr6.gt=1;
    return context;
}

table_family::Dependencies Deps(Services& stream,HeapBoundary& heap,
    Handler& handler)
{return {Dependencies(stream),heap,handler};}

bool Independent(Scenario scenario,const PPCContext& context,
    const Services& stream,const HeapBoundary& heap)
{
    if(scenario==Scenario::NoFreeBlock)
        return context.r3.u64==UINT64_MAX &&
            stream.memory.ReadU32(Count)==16u &&
            stream.memory.ReadU32(Blocks)==0x30000u &&
            stream.memory.ReadU32(0x83215210u)==12u &&
            !heap.events.empty();
    if(context.r3.u32!=0u || stream.memory.ReadU32(Count)!=32u ||
        stream.memory.ReadU32(Blocks)!=AllocBlock+16u ||
        heap.events.empty())return false;
    for(unsigned index=0;index<32u;++index)
    {
        const auto record=AllocBlock+16u+index*64u;
        if(stream.memory.ReadU32(record)!=(index<3u?0xfffffffeu:UINT32_MAX) ||
            stream.memory.ReadU8(record+4u)!=(index<3u?0xc1u:0u) ||
            stream.memory.ReadU8(record+5u)!=10u ||
            stream.memory.ReadU32(record+8u)!=0u ||
            stream.memory.ReadU8(record+40u)!=0u ||
            stream.memory.ReadU8(record+41u)!=10u ||
            stream.memory.ReadU8(record+42u)!=10u)
            return false;
    }
    return true;
}

Services* stream_current=nullptr;
HeapBoundary* heap_current=nullptr;
Handler* handler_current=nullptr;

void Check(Scenario scenario)
{
    test::GuestWindow original(InitRegions),recovered(InitRegions);
    SeedCase(original,scenario);SeedCase(recovered,scenario);
    Services expected(original,Mode::LockedWrite);
    Services actual(recovered,Mode::LockedWrite);
    HeapBoundary expected_heap,actual_heap;
    Handler expected_handler,actual_handler;
    auto context=Initial();auto state=crt_full_oracle::FromPpc(context);
    active=&expected;stream_current=&expected;
    heap_current=&expected_heap;handler_current=&expected_handler;
    __imp__sub_82B81520(context,original.Bytes());
    active=nullptr;stream_current=nullptr;
    heap_current=nullptr;handler_current=nullptr;
    if(!Independent(scenario,context,expected,expected_heap))
    {
        std::fprintf(stderr,"EXPECT table scenario=%u result=%llX count=%u "
            "block=%08X errno=%u heap=%zu\n",static_cast<unsigned>(scenario),
            static_cast<unsigned long long>(context.r3.u64),
            expected.memory.ReadU32(Count),expected.memory.ReadU32(Blocks),
            expected.memory.ReadU32(0x83215210u),expected_heap.events.size());
        throw std::runtime_error("independent stream table outcome");
    }
    if(!table_family::Apply(0x82b81520u,actual.memory,
        Deps(actual,actual_heap,actual_handler),state))
        throw std::runtime_error("missing selected table initializer");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    const bool same_state=before==after;
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events ||
        expected.traps!=actual.traps ||
        expected_heap.events!=actual_heap.events)
    {
        std::fprintf(stderr,"FAIL table scenario=%u state=%u RAM=%u "
            "stream=%zu/%zu heap=%zu/%zu traps=%u/%u\n",
            static_cast<unsigned>(scenario),same_state,same_ram,
            expected.events.size(),actual.events.size(),
            expected_heap.events.size(),actual_heap.events.size(),
            expected.traps,actual.traps);
        for(unsigned index=0;index<before.size();++index)
            if(before[index]!=after[index])
                std::fprintf(stderr," state[%u]=%llX/%llX\n",index,
                    static_cast<unsigned long long>(before[index]),
                    static_cast<unsigned long long>(after[index]));
        throw std::runtime_error("table initializer selected context differs");
    }
}
} // namespace

void OriginalTableLower(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_record_allocation_context::ApplyAcceptedLower(entry,
        stream_current->memory,
        Deps(*stream_current,*heap_current,*handler_current),state))
        throw std::runtime_error("missing original table lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalTableHandler(GuestAddress target,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    handler_current->CallNewHandler(target,stream_current->memory,state);
    crt_full_oracle::ToPpc(context,state);
}
void OriginalTableHeapBoundary(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    auto lower=ToHeap(state);
    if(entry>=0x83000000u)
        heap_current->CallNative(entry,stream_current->memory,lower);
    else heap_current->CallDirect(entry,stream_current->memory,lower);
    FromHeap(state,lower);
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto scenario:Scenarios)Check(scenario);
        std::printf("PASS crt-stream-table-initialize-context %zu original PPC cases\n",
            Scenarios.size());
        std::puts("LIMIT actual seven-body selected composition; FD78/FEC0 accepted selected adapters, heap direct/native and handler guest boundaries; unselected CR, faults, MMIO, concurrency and runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}

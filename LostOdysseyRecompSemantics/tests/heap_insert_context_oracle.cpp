// Appended after the actual 827CBA60 PPC body.
#include "lo_semantics/heap_insert_context.h"
#include "lo_semantics/recovery_abi.h"
#include "heap_insert_context_fixture.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family=heap_insert_context;
namespace fixture=heap_insert_context_fixture;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr std::array<Region,1> Regions{{{0u,0x400000u}}};
constexpr std::array Modes{fixture::Mode::SmallEmpty,
    fixture::Mode::SmallOccupied,fixture::Mode::LargeOrdered,
    fixture::Mode::HugeSplit,fixture::Mode::SegmentEnd};

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    const PPCRegister* fields[]={&context.r0,&context.r1,&context.r2,
        &context.r3,&context.r4,&context.r5,&context.r6,&context.r7,
        &context.r8,&context.r9,&context.r10,&context.r11,&context.r12,
        &context.r13,&context.r14,&context.r15,&context.r16,&context.r17,
        &context.r18,&context.r19,&context.r20,&context.r21,&context.r22,
        &context.r23,&context.r24,&context.r25,&context.r26,&context.r27,
        &context.r28,&context.r29,&context.r30,&context.r31};
    for(unsigned index=0u;index<32u;++index)
        state.r[index]=fields[index]->u64;
    state.lr=context.lr;state.ctr=context.ctr.u64;
    state.xer_so=context.xer.so;state.xer_ca=context.xer.ca;
    state.cr0={context.cr0.lt,context.cr0.gt,context.cr0.eq,
        context.cr0.so};
    state.cr6={context.cr6.lt,context.cr6.gt,context.cr6.eq,
        context.cr6.so};
    return state;
}

void ToPpc(PPCContext& context,const family::Registers& state)
{
    PPCRegister* fields[]={&context.r0,&context.r1,&context.r2,
        &context.r3,&context.r4,&context.r5,&context.r6,&context.r7,
        &context.r8,&context.r9,&context.r10,&context.r11,&context.r12,
        &context.r13,&context.r14,&context.r15,&context.r16,&context.r17,
        &context.r18,&context.r19,&context.r20,&context.r21,&context.r22,
        &context.r23,&context.r24,&context.r25,&context.r26,&context.r27,
        &context.r28,&context.r29,&context.r30,&context.r31};
    for(unsigned index=0u;index<32u;++index)
        fields[index]->u64=state.r[index];
    context.lr=state.lr;context.ctr.u64=state.ctr;
    context.xer.so=state.xer_so;context.xer.ca=state.xer_ca;
    context.cr0={state.cr0.lt,state.cr0.gt,state.cr0.eq,
        {state.cr0.un}};
    context.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,
        {state.cr6.un}};
}

bool Same(const family::Registers& a,const family::Registers& b)
{
    return a.r==b.r && a.lr==b.lr && a.ctr==b.ctr &&
        a.cr0==b.cr0 && a.cr6==b.cr6 &&
        a.xer_so==b.xer_so && a.xer_ca==b.xer_ca;
}

PPCContext Initial(fixture::Mode mode)
{
    PPCContext context{};
    PPCRegister* fields[]={&context.r0,&context.r1,&context.r2,
        &context.r3,&context.r4,&context.r5,&context.r6,&context.r7,
        &context.r8,&context.r9,&context.r10,&context.r11,&context.r12,
        &context.r13,&context.r14,&context.r15,&context.r16,&context.r17,
        &context.r18,&context.r19,&context.r20,&context.r21,&context.r22,
        &context.r23,&context.r24,&context.r25,&context.r26,&context.r27,
        &context.r28,&context.r29,&context.r30,&context.r31};
    for(unsigned index=0u;index<32u;++index)
        fields[index]->u64=0x1122334400000000ull+index;
    context.r1.u64=0x8877665500000000ull|fixture::Stack;
    context.r3.u64=0xaabbccdd00000000ull|fixture::Heap;
    context.r4.u64=0x9988776600000000ull|fixture::Block;
    context.r5.u64=0x5566778800000000ull|fixture::Units(mode);
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    return context;
}

bool Independent(const GuestMemory& memory,fixture::Mode mode)
{
    const auto block=fixture::Block,units=fixture::Units(mode);
    const auto first=fixture::Head(units),node=block+8u;
    if(memory.ReadU32(fixture::Heap+48u)!=units) return false;
    if(mode==fixture::Mode::SmallEmpty)
        return memory.ReadU16(block)==2u &&
            memory.ReadU32(first)==node &&
            memory.ReadU32(first+4u)==node &&
            memory.ReadU32(node)==first &&
            memory.ReadU32(node+4u)==first &&
            (memory.ReadU32(fixture::Heap+88u*4u)&4u)!=0u;
    if(mode==fixture::Mode::SmallOccupied)
        return memory.ReadU16(block)==2u &&
            memory.ReadU32(first)==fixture::Existing+8u &&
            memory.ReadU32(first+4u)==node &&
            memory.ReadU32(fixture::Existing+8u)==node &&
            memory.ReadU32(node)==first &&
            memory.ReadU32(node+4u)==fixture::Existing+8u;
    if(mode==fixture::Mode::LargeOrdered)
        return memory.ReadU16(block)==150u &&
            memory.ReadU32(first)==fixture::Existing+8u &&
            memory.ReadU32(fixture::Existing+8u)==node &&
            memory.ReadU32(node)==fixture::Existing2+8u &&
            memory.ReadU32(node+4u)==fixture::Existing+8u &&
            memory.ReadU32(fixture::Existing2+12u)==node;
    const auto second=block+0xeff0u*16u;
    const auto small_head=fixture::Head(17u);
    return memory.ReadU16(block)==0xeff0u &&
        memory.ReadU32(first)==node &&
        (mode==fixture::Mode::SegmentEnd?
            memory.ReadU32(small_head)==small_head:
            memory.ReadU16(second)==17u &&
            memory.ReadU32(small_head)==second+8u);
}

void Check(fixture::Mode mode)
{
    GuestWindow original(Regions),recovered(Regions);
    original.Fill(0xa5u);recovered.Fill(0xa5u);
    auto original_memory=original.Memory();
    auto recovered_memory=recovered.Memory();
    fixture::Seed(original_memory,mode);
    fixture::Seed(recovered_memory,mode);
    auto context=Initial(mode);
    auto state=FromPpc(context);
    __imp__sub_827CBA60(context,original.Bytes());
    if(!Independent(original_memory,mode))
    {
        std::fprintf(stderr,"EXPECT insert mode=%u count=%u first=%08X size=%u\n",
            static_cast<unsigned>(mode),original_memory.ReadU32(fixture::Heap+48u),
            original_memory.ReadU32(fixture::Head(fixture::Units(mode))),
            original_memory.ReadU16(fixture::Block));
        throw std::runtime_error("independent free-list insertion");
    }
    if(!family::Apply(0x827cba60u,recovered_memory,state))
        throw std::runtime_error("selected insert entry missing");
    const auto observed=FromPpc(context);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram)
    {
        std::fprintf(stderr,"FAIL insert mode=%u state=%u RAM=%u\n",
            static_cast<unsigned>(mode),same_state,same_ram);
        for(unsigned index=0u;index<32u;++index)
            if(observed.r[index]!=state.r[index])
                std::fprintf(stderr," r%u=%llX/%llX\n",index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        std::fprintf(stderr," LR=%llX/%llX CTR=%llX/%llX "
            "CR0=%u%u%u%u/%u%u%u%u CR6=%u%u%u%u/%u%u%u%u "
            "SO=%u/%u CA=%u/%u\n",
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(observed.ctr),
            static_cast<unsigned long long>(state.ctr),
            observed.cr0.lt,observed.cr0.gt,observed.cr0.eq,observed.cr0.un,
            state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.un,
            observed.cr6.lt,observed.cr6.gt,observed.cr6.eq,observed.cr6.un,
            state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.un,
            observed.xer_so,state.xer_so,observed.xer_ca,state.xer_ca);
        unsigned shown=0u;
        for(const auto region:Regions)
        {
            for(std::size_t offset=0;offset<region.size;++offset)
            {
                const auto address=region.base+static_cast<GuestAddress>(offset);
                if(original.Bytes()[address]==recovered.Bytes()[address])
                    continue;
                std::fprintf(stderr," RAM %08X %02X/%02X\n",address,
                    original.Bytes()[address],recovered.Bytes()[address]);
                if(++shown==5u) break;
            }
            if(shown==5u) break;
        }
        throw std::runtime_error("insert selected context differs");
    }
}
} // namespace

void OriginalSave28(PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    const auto state=FromPpc(context);
    for(unsigned index=28u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Address(state.r[12]));
}

void OriginalRestore28(PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    auto state=FromPpc(context);
    for(unsigned index=28u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto mode:Modes) Check(mode);
        std::printf("PASS heap-insert-context %zu original PPC cases\n",
            Modes.size());
        std::puts("LIMIT selected integer ABI/RAM; other CR fields, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

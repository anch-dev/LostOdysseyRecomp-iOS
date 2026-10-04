// Appended after the complete 823ACCB0 and 823AD544 PPC bodies.
#include "lo_semantics/heap_allocation_context.h"
#include "lo_semantics/heap.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=heap_allocation_context;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Heap=0x10000u,Block=0x100000u;
constexpr GuestAddress GrowthBlock=0x180000u,VirtualBase=0x300000u;
constexpr GuestAddress Descriptor=0x360000u,Sentinel=0x370000u;
constexpr GuestAddress Stack=0x3f0000u,Frame=Stack-320u;
constexpr std::array<Region,1> Regions{{{0u,0x400000u}}};

enum class Mode {SmallExact,SmallSplit,LargeExact,LargeSplit,
    Grow,Locked,Virtual,ProcessGuard,CleanupLocked};
constexpr std::array Modes{Mode::SmallExact,Mode::SmallSplit,
    Mode::LargeExact,Mode::LargeSplit,Mode::Grow,Mode::Locked,
    Mode::Virtual,Mode::ProcessGuard,Mode::CleanupLocked};
struct Inputs
{
    std::uint32_t bytes=33u,block_units=4u,heap_flags=1u;
    std::uint32_t cutoff=0xffffu,status=0u;
    bool block=true,grow=false,cleanup=false,guard=false;
};
Inputs Select(Mode mode)
{
    Inputs in{};
    switch(mode)
    {
    case Mode::SmallSplit:in.block_units=6u;break;
    case Mode::LargeExact:in.bytes=2048u;in.block_units=129u;break;
    case Mode::LargeSplit:in.bytes=2048u;in.block_units=160u;break;
    case Mode::Grow:in.block=false;in.grow=true;break;
    case Mode::Locked:in.heap_flags=0u;break;
    case Mode::Virtual:in.bytes=2033u;in.block=false;
        in.cutoff=128u;in.status=2u;break;
    case Mode::ProcessGuard:in.guard=true;break;
    case Mode::CleanupLocked:in.cleanup=true;in.block=false;break;
    default:break;
    }
    return in;
}

struct Event
{
    GuestAddress entry;
    std::array<std::uint64_t,7> args;
    bool operator==(const Event&) const=default;
};

struct Services final : family::BoundaryServices
{
    GuestMemory memory;
    Inputs inputs;
    std::vector<Event> events;
    const char* unexpected=nullptr;
    explicit Services(GuestWindow& window,Inputs selected)
        : memory(window.Memory()),inputs(selected) {}

    void Record(GuestAddress entry,const family::Registers& state)
    {
        events.push_back({entry,{state.r[3],state.r[4],state.r[5],
            state.r[6],state.r[7],state.r[1],state.lr}});
    }
    void CallDirect(GuestAddress entry,GuestMemory& guest,
        family::Registers& state) override
    {
        std::fprintf(stderr,"heap direct=%08X r3=%llX r4=%llX r5=%llX "
            "SP=%llX LR=%llX\n",entry,
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(state.r[4]),
            static_cast<unsigned long long>(state.r[5]),
            static_cast<unsigned long long>(state.r[1]),
            static_cast<unsigned long long>(state.lr));
        std::fflush(stderr);
        Record(entry,state);
        if(entry==0x827cc428u)
        {
            if(!inputs.grow || guest.ReadU32(Heap+384u)!=Heap+384u)
            {
                unexpected="unexpected grow lower";
                state.r[3]=0u;
                return;
            }
            const auto node=GrowthBlock+8u,head=Heap+384u;
            guest.WriteU16(GrowthBlock,256u);
            guest.WriteU16(GrowthBlock+2u,0u);
            guest.WriteU8(GrowthBlock+4u,0u);
            guest.WriteU8(GrowthBlock+5u,0x10u);
            guest.WriteU32(node,head);guest.WriteU32(node+4u,head);
            guest.WriteU32(head,node);guest.WriteU32(head+4u,node);
            guest.WriteU32(Heap+48u,guest.ReadU32(Heap+48u)+256u);
            state.r[3]=GrowthBlock;
            return;
        }
        if(entry==0x827cba60u)
        {
            InsertFreeBlocks(guest,Address(state.r[3]),
                Address(state.r[4]),Address(state.r[5]));
            return;
        }
        if(entry==0x82b7bc40u)
        {
            state.r[3]=FillGuestMemory(guest,Address(state.r[3]),
                Address(state.r[4]),Address(state.r[5]));
            return;
        }
        unexpected="unknown direct heap lower";
    }
    void CallNative(GuestAddress entry,GuestMemory& guest,
        family::Registers& state) override
    {
        std::fprintf(stderr,"heap native=%08X r3=%llX r4=%llX r5=%llX "
            "SP=%llX LR=%llX\n",entry,
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(state.r[4]),
            static_cast<unsigned long long>(state.r[5]),
            static_cast<unsigned long long>(state.r[1]),
            static_cast<unsigned long long>(state.lr));
        std::fflush(stderr);
        Record(entry,state);
        switch(entry)
        {
        case 0x830da07cu:state.r[3]=1u;return;
        case 0x830d9c6cu:
        case 0x830d9c7cu:return;
        case 0x830d9d1cu:
            if(!inputs.status || inputs.cutoff==0xffffu)
            {
                unexpected="unexpected VM native";
                state.r[3]=0u;
                return;
            }
            guest.WriteU32(Address(state.r[3]),VirtualBase);
            guest.WriteU32(Address(state.r[4]),0x2000u);
            state.r[3]=0u;
            return;
        default:unexpected="unselected heap native branch";return;
        }
    }
};
Services* active=nullptr;

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
    for(unsigned i=0;i<32u;++i) state.r[i]=fields[i]->u64;
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
    for(unsigned i=0;i<32u;++i) fields[i]->u64=state.r[i];
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

GuestAddress Head(std::uint32_t units)
{return Heap+384u+(units<128u?units*8u:0u);}
void Link(GuestMemory& memory,GuestAddress block,std::uint32_t units)
{
    const auto head=Head(units),node=block+8u;
    memory.WriteU32(head,node);memory.WriteU32(head+4u,node);
    memory.WriteU32(node,head);memory.WriteU32(node+4u,head);
    if(units<128u)
    {
        const auto word=Heap+((units>>5u)+88u)*4u;
        memory.WriteU32(word,memory.ReadU32(word)|(1u<<(units&31u)));
    }
    memory.WriteU32(Heap+48u,memory.ReadU32(Heap+48u)+units);
}

void Seed(GuestWindow& window,const Inputs& in)
{
    window.Fill(0xa5u);
    auto memory=window.Memory();
    for(unsigned units=0;units<128u;++units)
    {
        const auto head=Head(units);
        memory.WriteU32(head,head);memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0;word<4u;++word)
        memory.WriteU32(Heap+(88u+word)*4u,0u);
    memory.WriteU32(Heap+88u,Heap+88u);
    memory.WriteU32(Heap+92u,Heap+88u);
    memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+20u,in.status|(in.guard?0x40000u:0u));
    memory.WriteU32(Heap+24u,in.heap_flags);
    memory.WriteU32(Heap+28u,in.cutoff);
    memory.WriteU32(Heap+96u,Descriptor);
    memory.WriteU32(Descriptor+44u,0x340000u);
    memory.WriteU32(Descriptor+64u,Block);
    memory.WriteU32(Heap+1408u,Sentinel);
    memory.WriteU8(Heap+379u,1u);
    if(!in.block) return;
    memory.WriteU16(Block,static_cast<std::uint16_t>(in.block_units));
    memory.WriteU16(Block+2u,0u);
    memory.WriteU8(Block+4u,0u);
    memory.WriteU8(Block+5u,0u);
    Link(memory,Block,in.block_units);
    const auto next=Block+in.block_units*16u;
    memory.WriteU16(next,1u);
    memory.WriteU16(next+2u,
        static_cast<std::uint16_t>(in.block_units));
    memory.WriteU8(next+4u,0u);
    memory.WriteU8(next+5u,1u);
}

PPCContext Initial(Mode mode,const Inputs& in)
{
    PPCContext context{};
    PPCRegister* fields[]={&context.r0,&context.r1,&context.r2,
        &context.r3,&context.r4,&context.r5,&context.r6,&context.r7,
        &context.r8,&context.r9,&context.r10,&context.r11,&context.r12,
        &context.r13,&context.r14,&context.r15,&context.r16,&context.r17,
        &context.r18,&context.r19,&context.r20,&context.r21,&context.r22,
        &context.r23,&context.r24,&context.r25,&context.r26,&context.r27,
        &context.r28,&context.r29,&context.r30,&context.r31};
    for(unsigned i=0;i<32u;++i)
        fields[i]->u64=0x1122334400000000ull+i;
    context.r1.u64=0x8877665500000000ull|Stack;
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    if(in.cleanup)
    {
        context.r12.u64=context.r1.u64;
        context.r27.u64=Heap;
        context.r22.u64=mode==Mode::CleanupLocked?1u:0u;
    }
    else
    {
        context.r3.u64=0xaabbccdd00000000ull|Heap;
        context.r4.u64=0u;
        context.r5.u64=0x5566778800000000ull|in.bytes;
    }
    return context;
}

void Check(Mode mode)
{
    std::fprintf(stderr,"heap mode=%u seed\n",
        static_cast<unsigned>(mode));
    std::fflush(stderr);
    const auto in=Select(mode);
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,in);Seed(recovered,in);
    Services expected(original,in),actual(recovered,in);
    auto context=Initial(mode,in);
    auto state=FromPpc(context);
    if(in.block)
    {
        const auto memory=original.Memory();
        const auto head=Head(in.block_units),node=Block+8u;
        if(memory.ReadU32(head)!=node ||
            memory.ReadU32(head+4u)!=node ||
            memory.ReadU32(node)!=head ||
            memory.ReadU32(node+4u)!=head)
            throw std::runtime_error("invalid seeded free-list links");
    }
    active=&expected;
    std::fprintf(stderr,"heap mode=%u original entry=%08X\n",
        static_cast<unsigned>(mode),
        in.cleanup?0x823ad544u:0x823accb0u);
    std::fflush(stderr);
    if(in.cleanup)
        __imp__sub_823AD544(context,original.Bytes());
    else
        __imp__sub_823ACCB0(context,original.Bytes());
    active=nullptr;
    if(expected.unexpected)
        throw std::runtime_error(expected.unexpected);
    std::fprintf(stderr,"heap mode=%u original result=%08X events=%zu\n",
        static_cast<unsigned>(mode),context.r3.u32,expected.events.size());
    std::fflush(stderr);
    const auto expected_result=in.cleanup?0u:
        mode==Mode::Grow?GrowthBlock+16u:
        mode==Mode::Virtual?VirtualBase+48u:Block+16u;
    if(!in.cleanup && context.r3.u32!=expected_result)
    {
        std::fprintf(stderr,"EXPECT heap mode=%u r3=%08X/%08X\n",
            static_cast<unsigned>(mode),context.r3.u32,expected_result);
        throw std::runtime_error("independent heap allocation result");
    }
    const auto entry=in.cleanup?0x823ad544u:0x823accb0u;
    std::fprintf(stderr,"heap mode=%u recovered entry=%08X\n",
        static_cast<unsigned>(mode),entry);
    std::fflush(stderr);
    if(!family::Apply(entry,actual.memory,actual,state))
        throw std::runtime_error("heap context entry missing");
    const auto observed=FromPpc(context);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events)
    {
        std::fprintf(stderr,"FAIL heap mode=%u state=%u RAM=%u "
            "events=%zu/%zu r3=%llX/%llX SP=%llX/%llX LR=%llX/%llX "
            "CR0=%u%u%u%u/%u%u%u%u CR6=%u%u%u%u/%u%u%u%u "
            "SO=%u/%u CA=%u/%u\n",static_cast<unsigned>(mode),
            same_state,same_ram,expected.events.size(),actual.events.size(),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.r[1]),
            static_cast<unsigned long long>(state.r[1]),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            observed.cr0.lt,observed.cr0.gt,observed.cr0.eq,observed.cr0.un,
            state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.un,
            observed.cr6.lt,observed.cr6.gt,observed.cr6.eq,observed.cr6.un,
            state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.un,
            observed.xer_so,state.xer_so,observed.xer_ca,state.xer_ca);
        for(unsigned i=0;i<32u;++i)
            if(observed.r[i]!=state.r[i])
                std::fprintf(stderr," r%u=%llX/%llX",i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        std::fputc('\n',stderr);
        unsigned shown=0;
        for(const auto region:Regions)
        {
            for(std::size_t offset=0;offset<region.size;++offset)
            {
                const auto address=region.base+
                    static_cast<GuestAddress>(offset);
                if(original.Bytes()[address]==recovered.Bytes()[address])
                    continue;
                std::fprintf(stderr," RAM %08X %02X/%02X\n",address,
                    original.Bytes()[address],recovered.Bytes()[address]);
                if(++shown==5u) break;
            }
            if(shown==5u) break;
        }
        throw std::runtime_error("heap selected context differs");
    }
}
} // namespace

void OriginalSave22(PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    const auto state=FromPpc(context);
    for(unsigned i=22u;i<=31u;++i)
        WriteU64(memory,Address(state.r[1]-8u*(33u-i)),state.r[i]);
    memory.WriteU32(Address(state.r[1]-8u),Address(state.r[12]));
}
void OriginalRestore22(PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    auto state=FromPpc(context);
    for(unsigned i=22u;i<=31u;++i)
        state.r[i]=ReadU64(memory,Address(state.r[1]-8u*(33u-i)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
    ToPpc(context,state);
}

void OriginalHeapBoundary(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=FromPpc(context);
    if(entry>=0x83000000u)
        active->CallNative(entry,active->memory,state);
    else
        active->CallDirect(entry,active->memory,state);
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto mode:Modes) Check(mode);
        std::printf("PASS heap-allocation-context %zu original PPC cases\n",
            Modes.size());
        std::puts("LIMIT selected CR0/6, 32 GPR, SO/CA and RAM; accepted deep guest algorithms and explicit native boundaries, no faults/MMIO/concurrency/runtime");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

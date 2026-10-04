// Appended after actual 823ADE28, 823AE0BC and 823AE108 PPC bodies.
#include "lo_semantics/heap_coalesce_context.h"
#include "lo_semantics/recovery_abi.h"
#include "heap_coalesce_context_fixture.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=heap_coalesce_context;
namespace fixture=heap_coalesce_context_fixture;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress UnitsSlot=fixture::Stack-256u;
constexpr std::array<Region,1> Regions{{{0u,0x400000u}}};
constexpr std::array Modes{fixture::Mode::Previous,
    fixture::Mode::Following,fixture::Mode::Both,
    fixture::Mode::DirectBoth};

struct Event
{
    GuestAddress entry;
    std::array<std::uint64_t,7> args;
    bool operator==(const Event&) const=default;
};

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

struct Services final : family::BoundaryServices
{
    GuestMemory memory;
    std::vector<Event> events;
    const char* unexpected=nullptr;
    explicit Services(GuestWindow& guest):memory(guest.Memory()){}
    void CallDirect(GuestAddress entry,GuestMemory&,
        family::Registers&) override
    {
        (void)entry;
        unexpected="unselected coalesce guest lower";
    }
    void CallNative(GuestAddress entry,GuestMemory&,
        family::Registers& state) override
    {
        events.push_back({entry,{state.r[3],state.r[4],state.r[5],
            state.r[6],state.r[7],state.r[1],state.lr}});
        switch(entry)
        {
        case 0x830da08cu:state.r[3]=0x12345678u;return;
        default:unexpected="unselected coalesce native";return;
        }
    }
};
Services* active=nullptr;

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
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    context.r3.u64=0xaabbccdd00000000ull|fixture::Heap;
    context.r4.u64=mode==fixture::Mode::DirectBoth?
        0x9988776600000000ull|fixture::Block:0u;
    context.r5.u64=mode==fixture::Mode::DirectBoth?
        0x5566778800000000ull|UnitsSlot:
        0x5566778800000000ull|fixture::Payload;
    context.r6.u64=0u;
    return context;
}

void Check(fixture::Mode mode)
{
    GuestWindow original(Regions),recovered(Regions);
    original.Fill(0xa5u);recovered.Fill(0xa5u);
    auto original_memory=original.Memory();
    auto recovered_memory=recovered.Memory();
    fixture::Seed(original_memory,mode);
    fixture::Seed(recovered_memory,mode);
    original_memory.WriteU32(UnitsSlot,4u);
    recovered_memory.WriteU32(UnitsSlot,4u);
    Services expected(original),actual(recovered);
    auto context=Initial(mode);
    auto state=FromPpc(context);
    const bool direct=mode==fixture::Mode::DirectBoth;
    active=&expected;
    if(direct)
        __imp__sub_823AE108(context,original.Bytes());
    else
        __imp__sub_823ADE28(context,original.Bytes());
    active=nullptr;
    if(expected.unexpected)
        throw std::runtime_error(expected.unexpected);
    const auto expected_block=fixture::ExpectedBlock(mode);
    const auto expected_units=fixture::ExpectedUnits(mode);
    if((direct && (context.r3.u32!=expected_block ||
            original_memory.ReadU32(UnitsSlot)!=expected_units)) ||
        (!direct && context.r3.u32!=1u))
    {
        std::fprintf(stderr,"EXPECT coalesce mode=%u r3=%08X units=%u\n",
            static_cast<unsigned>(mode),context.r3.u32,
            original_memory.ReadU32(UnitsSlot));
        throw std::runtime_error("independent coalesce result");
    }
    if(!direct)
    {
        const auto head=fixture::Heap+(expected_units+48u)*8u;
        if(original_memory.ReadU32(head)!=expected_block+8u ||
            original_memory.ReadU32(head+4u)!=expected_block+8u ||
            original_memory.ReadU16(expected_block)!=expected_units)
            throw std::runtime_error("independent merged free-list state");
    }
    active=&actual;
    const auto applied=direct?
        family::Apply(0x823ae108u,actual.memory,actual,state):
        family::ApplyFree(0x823ade28u,actual.memory,actual,state);
    active=nullptr;
    if(!applied || actual.unexpected)
        throw std::runtime_error(actual.unexpected?
            actual.unexpected:"selected coalesce entry missing");
    const auto observed=FromPpc(context);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events)
    {
        std::fprintf(stderr,"FAIL coalesce mode=%u state=%u RAM=%u "
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
        for(unsigned index=0u;index<32u;++index)
            if(observed.r[index]!=state.r[index])
                std::fprintf(stderr," r%u=%llX/%llX",index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        std::fputc('\n',stderr);
        unsigned shown=0u;
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
        throw std::runtime_error("coalesce selected context differs");
    }
}
} // namespace

void OriginalSave25(PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    const auto state=FromPpc(context);
    for(unsigned index=25u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Address(state.r[12]));
}

void OriginalRestore25(PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    auto state=FromPpc(context);
    for(unsigned index=25u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
    ToPpc(context,state);
}

void OriginalDirect(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    active->CallDirect(entry,active->memory,state);
    ToPpc(context,state);
}

void OriginalNative(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    active->CallNative(entry,active->memory,state);
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto mode:Modes) Check(mode);
        std::printf("PASS heap-coalesce-context %zu original PPC cases\n",
            Modes.size());
        std::puts("LIMIT selected coalesce+free ABI and RAM; 827CBA60/827CC668 remain guest boundaries; native/fault/unwind/MMIO/concurrency/runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

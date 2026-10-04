// Appended after actual 827CC668, 827CBA60, 827CB498 and 827CB658 bodies; all four selected implementations compose on the recovered side.
#include "lo_semantics/heap_decommit_range_chain.h"
#include "lo_semantics/recovery_abi.h"
#include "heap_decommit_context_fixture.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=heap_decommit_range_chain;
namespace fixture=heap_decommit_context_fixture;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr std::array<Region,1> Regions{{{0u,0x400000u}}};
constexpr std::array Modes{fixture::Mode::Decommit,
    fixture::Mode::NativeFailure,fixture::Mode::LeadingRemainder};

struct Event
{
    GuestAddress entry;
    std::array<std::uint64_t,9> args;
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

struct Services final : family::NativeServices
{
    GuestMemory memory;
    std::vector<Event> events;
    fixture::Mode mode;
    const char* unexpected=nullptr;
    Services(GuestWindow& guest,fixture::Mode scenario):
        memory(guest.Memory()),mode(scenario){}
    void CallNative(GuestAddress entry,GuestMemory&,
        family::Registers& state) override
    {
        if(entry!=0x830d9d3cu)
        {
            unexpected="descriptor allocation escaped fixture";
            state.r[3]=~std::uint64_t{0};
            return;
        }
        const auto base=Address(state.r[3]);
        const auto size=Address(state.r[4]);
        events.push_back({entry,{state.r[3],state.r[4],state.r[5],
            state.r[6],state.r[7],state.r[1],state.lr,
            memory.ReadU32(base),memory.ReadU32(size)}});
        state.r[3]=mode==fixture::Mode::NativeFailure?
            ~std::uint64_t{0}:0u;
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
    context.r3.u64=0xaabbccdd00000000ull|fixture::Heap;
    context.r4.u64=0x9988776600000000ull|
        fixture::BlockAddress(mode);
    context.r5.u64=0x5566778800000000ull|fixture::Units(mode);
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    return context;
}

bool Independent(const GuestMemory& memory,fixture::Mode mode,
    std::size_t native_count)
{
    const auto block=fixture::BlockAddress(mode);
    if(mode==fixture::Mode::GateFallback ||
        mode==fixture::Mode::NoWholePage ||
        mode==fixture::Mode::NativeFailure)
    {
        const auto units=fixture::Units(mode);
        const auto head=units<128u?
            fixture::Heap+(units+48u)*8u:fixture::LargeHead;
        return native_count==(mode==fixture::Mode::NativeFailure?1u:0u) &&
            memory.ReadU32(fixture::Heap+48u)==units &&
            memory.ReadU16(block)==units &&
            memory.ReadU32(head)==block+8u &&
            memory.ReadU32(fixture::Segment+56u)==0u;
    }
    const bool leading=mode==fixture::Mode::LeadingRemainder;
    const auto start=leading?0x110000u:0x100000u;
    const auto bytes=leading?0x10000u:0x20000u;
    const auto trailing=leading?809u:808u;
    const auto remainder=0x120000u;
    return native_count==1u &&
        memory.ReadU32(fixture::Segment+48u)==bytes/0x10000u &&
        memory.ReadU32(fixture::Segment+56u)==fixture::Descriptor &&
        memory.ReadU32(fixture::Descriptor+4u)==start &&
        memory.ReadU32(fixture::Descriptor+8u)==bytes &&
        memory.ReadU32(fixture::Segment+52u)==1u &&
        memory.ReadU16(remainder)==trailing &&
        memory.ReadU32(fixture::Heap+48u)==
            trailing+(leading?4095u:0u) &&
        memory.ReadU32(fixture::LargeHead)==remainder+8u &&
        (!leading || (memory.ReadU16(block)==4095u &&
            memory.ReadU32(remainder+8u)==block+8u));
}

void Check(fixture::Mode mode)
{
    GuestWindow original(Regions),recovered(Regions);
    original.Fill(0xa5u);recovered.Fill(0xa5u);
    auto original_memory=original.Memory();
    auto recovered_memory=recovered.Memory();
    fixture::Seed(original_memory,mode);
    fixture::Seed(recovered_memory,mode);
    Services expected(original,mode),actual(recovered,mode);
    auto context=Initial(mode);
    auto state=FromPpc(context);
    active=&expected;
    __imp__sub_827CC668(context,original.Bytes());
    active=nullptr;
    if(expected.unexpected) throw std::runtime_error(expected.unexpected);
    if(!Independent(original_memory,mode,expected.events.size()))
    {
        std::fprintf(stderr,"EXPECT decommit mode=%u count=%u segcount=%u "
            "head=%08X blocksize=%u events=%zu\n",static_cast<unsigned>(mode),
            original_memory.ReadU32(fixture::Heap+48u),
            original_memory.ReadU32(fixture::Segment+52u),
            original_memory.ReadU32(fixture::LargeHead),
            original_memory.ReadU16(fixture::BlockAddress(mode)),
            expected.events.size());
        throw std::runtime_error("independent decommit outcome");
    }
    active=&actual;
    const bool applied=family::Apply(0x827cc668u,
        recovered_memory,actual,state);
    active=nullptr;
    if(!applied || actual.unexpected)
        throw std::runtime_error(actual.unexpected?
            actual.unexpected:"selected decommit entry missing");
    const auto observed=FromPpc(context);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events)
    {
        std::fprintf(stderr,"FAIL decommit mode=%u state=%u RAM=%u "
            "events=%zu/%zu\n",static_cast<unsigned>(mode),same_state,
            same_ram,expected.events.size(),actual.events.size());
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
        throw std::runtime_error("decommit-range selected context differs");
    }
}
} // namespace

void OriginalSave19(PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    const auto state=FromPpc(context);
    for(unsigned index=19u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Address(state.r[12]));
}
void OriginalRestore19(PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    auto state=FromPpc(context);
    for(unsigned index=19u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];ToPpc(context,state);
}
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
    state.lr=state.r[12];ToPpc(context,state);
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
        std::printf("PASS heap-decommit-range-chain %zu original PPC cases\n",
            Modes.size());
        std::puts("LIMIT four selected guest bodies compose; kernel import modeled, other CR/fault/MMIO/concurrency/runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

// Appended after actual 823ADE28, 823AE0BC, and 823AE108 PPC bodies.
#include "lo_semantics/heap_free_context.h"
#include "lo_semantics/recovery_abi.h"
#include "heap_free_context_fixture.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=heap_free_context;
namespace fixture=heap_free_context_fixture;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr std::array<Region,1> Regions{{{0u,0x400000u}}};
constexpr std::array Modes{fixture::Mode::Null,fixture::Mode::GuardNull,
    fixture::Mode::Small,fixture::Mode::Large,
    fixture::Mode::LargeLocked,fixture::Mode::VirtualLocked,
    fixture::Mode::VirtualFailure,fixture::Mode::CleanupLocked};

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
    GuestWindow& window;
    GuestMemory memory;
    fixture::Mode mode;
    std::vector<Event> events;
    const char* unexpected=nullptr;
    Services(GuestWindow& guest,fixture::Mode selected)
        :window(guest),memory(guest.Memory()),mode(selected) {}

    void Record(GuestAddress entry,const family::Registers& state)
    {
        events.push_back({entry,{state.r[3],state.r[4],state.r[5],
            state.r[6],state.r[7],state.r[1],state.lr}});
    }

    void CallDirect(GuestAddress entry,GuestMemory&,
        family::Registers& state) override
    {
        Record(entry,state);
        if(entry!=0x823ae108u)
        {
            unexpected="unselected direct heap-free lower";
            return;
        }
        // Validation-only actual PPC lower. The new production source retains
        // this guest call as an explicit mutable boundary.
        PPCContext context{};
        ToPpc(context,state);
        __imp__sub_823AE108(context,window.Bytes());
        state=FromPpc(context);
    }

    void CallNative(GuestAddress entry,GuestMemory&,
        family::Registers& state) override
    {
        Record(entry,state);
        switch(entry)
        {
        case 0x830da07cu:state.r[3]=1u;return;
        case 0x830d9c6cu:
        case 0x830d9c7cu:return;
        case 0x830d9d3cu:
            state.r[3]=static_cast<std::uint64_t>(
                static_cast<std::int64_t>(fixture::Select(mode).vm_status));
            return;
        case 0x830da08cu:state.r[3]=0x12345678u;return;
        default:unexpected="unselected heap-free native";return;
        }
    }
};
Services* active=nullptr;

void Seed(GuestWindow& window,fixture::Mode mode)
{
    window.Fill(0xa5u);
    auto memory=window.Memory();
    fixture::Seed(memory,mode);
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
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    if(mode==fixture::Mode::CleanupLocked)
    {
        context.r12.u64=context.r1.u64;
        context.r30.u64=fixture::Heap;
        context.r25.u64=1u;
    }
    else
    {
        context.r3.u64=0xaabbccdd00000000ull|fixture::Heap;
        context.r4.u64=0u;
        context.r5.u64=(mode==fixture::Mode::Null ||
            mode==fixture::Mode::GuardNull)?0u:
            0x5566778800000000ull|fixture::Payload;
    }
    return context;
}

void Check(fixture::Mode mode)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,mode);Seed(recovered,mode);
    Services expected(original,mode),actual(recovered,mode);
    auto context=Initial(mode);
    auto state=FromPpc(context);
    const auto entry=mode==fixture::Mode::CleanupLocked?
        0x823ae0bcu:0x823ade28u;
    active=&expected;
    if(entry==0x823ae0bcu)
        __imp__sub_823AE0BC(context,original.Bytes());
    else
        __imp__sub_823ADE28(context,original.Bytes());
    active=nullptr;
    if(expected.unexpected)
        throw std::runtime_error(expected.unexpected);
    const auto event_count=[&](GuestAddress address)
    {
        unsigned count=0u;
        for(const auto& event:expected.events)
            if(event.entry==address) ++count;
        return count;
    };
    const auto ordinary=mode==fixture::Mode::Small ||
        mode==fixture::Mode::Large ||
        mode==fixture::Mode::LargeLocked;
    const auto locked=mode==fixture::Mode::LargeLocked ||
        mode==fixture::Mode::VirtualLocked ||
        mode==fixture::Mode::CleanupLocked;
    const auto virtual_block=mode==fixture::Mode::VirtualLocked ||
        mode==fixture::Mode::VirtualFailure;
    if(event_count(0x823ae108u)!=(ordinary?1u:0u) ||
        event_count(0x830d9c6cu)!=
            (locked && mode!=fixture::Mode::CleanupLocked?1u:0u) ||
        event_count(0x830d9c7cu)!=(locked?1u:0u) ||
        event_count(0x830d9d3cu)!=(virtual_block?1u:0u) ||
        event_count(0x830da07cu)!=
            (mode==fixture::Mode::GuardNull?1u:0u))
        throw std::runtime_error("independent heap-free path events");
    const auto result=mode==fixture::Mode::VirtualFailure?0u:1u;
    if(entry==0x823ade28u && context.r3.u32!=result)
    {
        std::fprintf(stderr,"EXPECT free mode=%u result=%08X/%08X\n",
            static_cast<unsigned>(mode),context.r3.u32,result);
        throw std::runtime_error("independent heap-free result");
    }
    if(ordinary)
    {
        const auto units=fixture::Select(mode).units;
        const auto head=fixture::Heap+
            (units<128u?(units+48u)*8u:384u);
        if(expected.memory.ReadU32(head)!=fixture::Block+8u ||
            expected.memory.ReadU32(head+4u)!=fixture::Block+8u)
            throw std::runtime_error("independent heap-free list insertion");
    }
    if(virtual_block &&
        (expected.memory.ReadU32(fixture::Sentinel)!=fixture::Sentinel ||
            expected.memory.ReadU32(fixture::Sentinel+4u)!=
                fixture::Sentinel))
        throw std::runtime_error("independent virtual list unlink");
    active=&actual;
    const bool recovered_applied=family::Apply(entry,actual.memory,
        actual,state);
    active=nullptr;
    if(!recovered_applied)
        throw std::runtime_error("heap-free selected entry missing");
    if(actual.unexpected)
        throw std::runtime_error(actual.unexpected);
    const auto observed=FromPpc(context);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events)
    {
        std::fprintf(stderr,"FAIL heap-free mode=%u state=%u RAM=%u "
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
        throw std::runtime_error("heap-free selected context differs");
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
        std::printf("PASS heap-free-context %zu original PPC cases\n",
            Modes.size());
        std::puts("LIMIT selected 32 GPR/CR0/CR6/SO/CA/LR/CTR/RAM; 823AE108 actual PPC validation-only boundary, 827CBA60/827CC668 unselected; native/fault/unwind/MMIO/concurrency/runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

// Appended after the actual 827CBA08 and 827CBB98 PPC bodies.
#include "lo_semantics/heap_destroy_context.h"
#include "lo_semantics/recovery_abi.h"
#include "heap_destroy_context_fixture.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=heap_destroy_context;
namespace fixture=heap_destroy_context_fixture;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr std::array<Region,1> Regions{{{0u,0x200000u}}};
struct Scenario {fixture::Mode mode;GuestAddress entry;};
constexpr std::array Cases{
    Scenario{fixture::Mode::DirectFlagged,0x827cba08u},
    Scenario{fixture::Mode::DirectVirtual,0x827cba08u},
    Scenario{fixture::Mode::Null,0x827cbb98u},
    Scenario{fixture::Mode::Empty,0x827cbb98u},
    Scenario{fixture::Mode::Mixed,0x827cbb98u}};

struct Event
{
    GuestAddress entry;
    std::array<std::uint64_t,8> args;
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
    const char* unexpected=nullptr;
    explicit Services(GuestWindow& guest):memory(guest.Memory()){}
    void CallNative(GuestAddress entry,GuestMemory& guest,
        family::Registers& state) override
    {
        const bool freeing=entry==0x830d9d3cu;
        events.push_back({entry,{state.r[3],state.r[4],state.r[5],
            state.r[6],state.r[1],state.lr,
            freeing?guest.ReadU32(Address(state.r[3])):0u,
            freeing?guest.ReadU32(Address(state.r[4])):0u}});
        if(freeing) state.r[3]=0u;
        else if(entry==0x830da07cu) state.r[3]=3u;
        else {unexpected="unexpected destroy native";state.r[3]=0u;}
    }
};
Services* active=nullptr;

PPCContext Initial(const Scenario& scenario)
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
    context.r3.u64=scenario.mode==fixture::Mode::Null?0u:
        (0xaabbccdd00000000ull|(scenario.entry==0x827cba08u?
            fixture::Page:fixture::Heap));
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    return context;
}

bool Independent(const GuestMemory& memory,const PPCContext& context,
    fixture::Mode mode,const std::vector<Event>& events)
{
    if(context.r3.u32!=0u) return false;
    if(mode==fixture::Mode::DirectFlagged)
        return events.empty() &&
            memory.ReadU32(fixture::Page+32u)==fixture::Reservation;
    if(mode==fixture::Mode::DirectVirtual)
        return events.size()==1u && events[0].entry==0x830d9d3cu &&
            events[0].args[6]==fixture::Reservation &&
            events[0].args[7]==0u && events[0].args[2]==32768u;
    if(mode==fixture::Mode::Null)
        return events.empty() &&
            memory.ReadU32(fixture::Heap+1408u)==0xdeadbeefu;
    if(mode==fixture::Mode::Empty)
        return events.empty() &&
            memory.ReadU32(fixture::Heap+1408u)==0u &&
            memory.ReadU32(fixture::Heap+72u)==0u;
    return events.size()==3u &&
        events[0].entry==0x830d9d3cu &&
        events[1].entry==0x830d9d3cu &&
        events[2].entry==0x830d9d3cu &&
        events[0].args[6]==fixture::Segment &&
        events[1].args[6]==fixture::Descriptor &&
        events[2].args[6]==fixture::Reservation &&
        memory.ReadU32(fixture::Heap+1408u)==0u &&
        memory.ReadU32(fixture::Heap+72u)==0u;
}

void Check(const Scenario& scenario)
{
    GuestWindow original(Regions),recovered(Regions);
    original.Fill(0xa5u);recovered.Fill(0xa5u);
    auto original_memory=original.Memory();
    auto recovered_memory=recovered.Memory();
    fixture::Seed(original_memory,scenario.mode);
    fixture::Seed(recovered_memory,scenario.mode);
    Services expected(original),actual(recovered);
    auto context=Initial(scenario);
    auto state=FromPpc(context);
    active=&expected;
    if(scenario.entry==0x827cba08u)
        __imp__sub_827CBA08(context,original.Bytes());
    else __imp__sub_827CBB98(context,original.Bytes());
    active=nullptr;
    if(expected.unexpected) throw std::runtime_error(expected.unexpected);
    if(!Independent(original_memory,context,scenario.mode,expected.events))
    {
        std::fprintf(stderr,"EXPECT destroy mode=%u r3=%08X events=%zu "
            "first=%08X/%08llX\n",static_cast<unsigned>(scenario.mode),
            context.r3.u32,expected.events.size(),
            expected.events.empty()?0u:expected.events[0].entry,
            static_cast<unsigned long long>(expected.events.empty()?0u:
                expected.events[0].args[6]));
        throw std::runtime_error("independent destroy outcome");
    }
    active=&actual;
    const bool applied=family::Apply(scenario.entry,
        recovered_memory,actual,state);
    active=nullptr;
    if(!applied || actual.unexpected)
        throw std::runtime_error(actual.unexpected?
            actual.unexpected:"selected destroy entry missing");
    const auto observed=FromPpc(context);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events)
    {
        std::fprintf(stderr,"FAIL destroy mode=%u state=%u RAM=%u "
            "events=%zu/%zu\n",static_cast<unsigned>(scenario.mode),
            same_state,same_ram,expected.events.size(),actual.events.size());
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
        throw std::runtime_error("destroy selected context differs");
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
        for(const auto& scenario:Cases) Check(scenario);
        std::printf("PASS heap-destroy-context %zu original PPC cases\n",
            Cases.size());
        std::puts("LIMIT two complete selected guest bodies; kernel native services modeled, other CR/fault/MMIO/concurrency/runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

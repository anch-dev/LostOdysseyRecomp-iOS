// Appended after the actual 827CC218 PPC body.
#include "lo_semantics/heap_block_query_context.h"
#include "heap_block_query_context_fixture.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=heap_block_query_context;
namespace fixture=heap_block_query_context_fixture;
using test::GuestWindow;
using test::Region;

constexpr std::array<Region,1> Regions{{{0u,0x200000u}}};
constexpr std::array Modes{fixture::Mode::Inactive,fixture::Mode::Small,
    fixture::Mode::Large,fixture::Mode::GuardSmall};

struct Event
{
    GuestAddress entry;
    std::array<std::uint64_t,2> args;
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
    void CallNative(GuestAddress entry,GuestMemory&,
        family::Registers& state) override
    {
        events.push_back({entry,{state.r[3],state.lr}});
        if(entry==0x830da07cu) state.r[3]=3u;
        else {unexpected="unexpected block-query native";state.r[3]=0u;}
    }
};
Services* active=nullptr;

PPCContext Initial()
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
    context.r5.u64=0x9988776600000000ull|fixture::Payload;
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    return context;
}

bool Independent(const PPCContext& context,fixture::Mode mode,
    const std::vector<Event>& events)
{
    const std::uint64_t result=mode==fixture::Mode::Inactive?
        ~std::uint64_t{0}:mode==fixture::Mode::Large?4852u:104u;
    if(context.r3.u64!=result) return false;
    if(mode!=fixture::Mode::GuardSmall) return events.empty();
    return events.size()==1u && events[0].entry==0x830da07cu &&
        events[0].args[1]==0x827cc244u;
}

void Check(fixture::Mode mode)
{
    GuestWindow original(Regions),recovered(Regions);
    original.Fill(0xa5u);recovered.Fill(0xa5u);
    auto original_memory=original.Memory();
    auto recovered_memory=recovered.Memory();
    fixture::Seed(original_memory,mode);
    fixture::Seed(recovered_memory,mode);
    Services expected(original),actual(recovered);
    auto context=Initial();
    auto state=FromPpc(context);
    active=&expected;
    __imp__sub_827CC218(context,original.Bytes());
    active=nullptr;
    if(expected.unexpected) throw std::runtime_error(expected.unexpected);
    if(!Independent(context,mode,expected.events))
    {
        std::fprintf(stderr,"EXPECT block-query mode=%u r3=%llX "
            "events=%zu\n",static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            expected.events.size());
        throw std::runtime_error("independent block-query result");
    }
    active=&actual;
    const bool applied=family::Apply(0x827cc218u,
        recovered_memory,actual,state);
    active=nullptr;
    if(!applied || actual.unexpected)
        throw std::runtime_error(actual.unexpected?
            actual.unexpected:"selected block-query entry missing");
    const auto observed=FromPpc(context);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events)
    {
        std::fprintf(stderr,"FAIL block-query mode=%u state=%u RAM=%u "
            "events=%zu/%zu\n",static_cast<unsigned>(mode),
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
        throw std::runtime_error("block-query selected context differs");
    }
}
} // namespace

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
        std::printf("PASS heap-block-query-context %zu original PPC cases\n",
            Modes.size());
        std::puts("LIMIT complete selected guest body; process native imports modeled, other CR/fault/MMIO/concurrency/runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

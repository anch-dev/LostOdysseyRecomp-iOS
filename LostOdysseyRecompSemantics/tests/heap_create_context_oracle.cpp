// Appended after actual heap-create, fill, segment, range and insert PPC bodies.
#include "lo_semantics/heap_create_context.h"
#include "lo_semantics/recovery_abi.h"
#include "heap_create_context_fixture.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=heap_create_context;
namespace fixture=heap_create_context_fixture;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr std::array<Region,4> Regions{{
    {0u,0x400000u},{fixture::DescriptorPool,
        fixture::DescriptorCommitBytes},{0x831e7000u,0x1000u},
    {0x83374000u,0x1000u}}};
constexpr std::array Modes{fixture::Mode::ReserveFailure,
    fixture::Mode::CommitFailure,fixture::Mode::FreshSuccess,
    fixture::Mode::ProvidedSuccess};

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

struct Services final : family::BoundaryServices
{
    GuestMemory memory;
    std::uint8_t* bytes;
    fixture::Mode mode;
    std::vector<Event> events;
    const char* unexpected=nullptr;
    Services(GuestWindow& guest,fixture::Mode scenario):
        memory(guest.Memory()),bytes(guest.Bytes()),mode(scenario){}
    void CallNative(GuestAddress entry,GuestMemory& guest,
        family::Registers& state) override
    {
        const bool alloc=entry==0x830d9d1cu;
        const bool free=entry==0x830d9d3cu;
        events.push_back({entry,{state.r[3],state.r[4],state.r[5],
            state.r[6],state.r[7],state.lr,
            (alloc||free)?guest.ReadU32(Address(state.r[3])):0u,
            (alloc||free)?guest.ReadU32(Address(state.r[4])):0u}});
        if(alloc)
        {
            if(state.lr==0x827ccd04u)
            {
                if(mode==fixture::Mode::ReserveFailure)
                {state.r[3]=~std::uint64_t{0};return;}
                guest.WriteU32(Address(state.r[3]),fixture::Heap);
                guest.WriteU32(Address(state.r[4]),fixture::ReserveBytes);
            }
            else if(state.lr==0x827ccd54u)
            {
                if(mode==fixture::Mode::CommitFailure)
                {state.r[3]=~std::uint64_t{0};return;}
                guest.WriteU32(Address(state.r[3]),fixture::Heap);
                guest.WriteU32(Address(state.r[4]),fixture::CommitBytes);
                // Newly committed virtual pages are zero-initialized.
                std::memset(bytes+fixture::Heap,0,fixture::CommitBytes);
            }
            else if(state.lr==0x827cb568u)
            {
                guest.WriteU32(Address(state.r[3]),fixture::DescriptorPool);
                guest.WriteU32(Address(state.r[4]),
                    fixture::DescriptorReserveBytes);
            }
            else if(state.lr==0x827cb59cu)
            {
                guest.WriteU32(Address(state.r[3]),fixture::DescriptorPool);
                guest.WriteU32(Address(state.r[4]),
                    fixture::DescriptorCommitBytes);
                std::memset(bytes+fixture::DescriptorPool,0,
                    fixture::DescriptorCommitBytes);
            }
            else unexpected="unexpected segment allocation site";
            state.r[3]=0u;
        }
        else if(free) state.r[3]=0u;
        else if(entry==0x830da09cu)
        {
            if(mode!=fixture::Mode::ProvidedSuccess)
                unexpected="unexpected virtual query";
            guest.WriteU32(Address(state.r[4]),Address(state.r[3]));
            guest.WriteU32(Address(state.r[4]+12u),
                state.lr==0x827ccc44u?0x10000u:0xf0000u);
            guest.WriteU32(Address(state.r[4]+16u),
                state.lr==0x827ccc44u?4096u:8192u);
            state.r[3]=0u;
        }
        else if(entry==0x830da07cu) state.r[3]=3u;
        else if(entry==0x830d9e9cu) state.r[3]=0u;
        else {unexpected="unexpected heap-create native";state.r[3]=0u;}
    }
    void CallIndirect(GuestAddress,GuestMemory&,
        family::Registers& state) override
    {unexpected="unexpected heap-create indirect";state.r[3]=0u;}
    void CallGuestMove(GuestMemory&,family::Registers& state) override
    {unexpected="unexpected accepted move boundary";state.r[3]=0u;}
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
    for(unsigned index=3u;index<=8u;++index) fields[index]->u64=0u;
    if(mode==fixture::Mode::ProvidedSuccess)
        context.r4.u64=0xaabbccdd00000000ull|fixture::Heap;
    context.lr=0xabcdef0181234567ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.xer.so=1;context.xer.ca=1;
    context.cr0.lt=1;context.cr6.gt=1;
    return context;
}

bool Independent(const GuestMemory& memory,const PPCContext& context,
    fixture::Mode mode,const std::vector<Event>& events)
{
    if(mode==fixture::Mode::ReserveFailure)
        return context.r3.u32==0u && events.size()==1u &&
            events[0].entry==0x830d9d1cu;
    if(mode==fixture::Mode::CommitFailure)
        return context.r3.u32==0u && events.size()==3u &&
            events[0].entry==0x830d9d1cu &&
            events[1].entry==0x830d9d1cu &&
            events[2].entry==0x830d9d3cu;
    if(context.r3.u32!=fixture::Heap ||
        memory.ReadU32(fixture::Heap+96u)!=fixture::FirstSegment)
        return false;
    if(mode==fixture::Mode::FreshSuccess)
        return events.size()>=2u &&
            events[0].entry==0x830d9d1cu &&
            events[1].entry==0x830d9d1cu;
    return events.size()>=2u &&
        events[0].entry==0x830da09cu &&
        events[1].entry==0x830da09cu;
}

void Check(fixture::Mode mode)
{
    std::fprintf(stderr,"heap-create mode=%u begin\n",
        static_cast<unsigned>(mode));
    std::fflush(stderr);
    GuestWindow original(Regions),recovered(Regions);
    original.Fill(0xa5u);recovered.Fill(0xa5u);
    auto original_memory=original.Memory();
    auto recovered_memory=recovered.Memory();
    fixture::Seed(original_memory);
    fixture::Seed(recovered_memory);
    Services expected(original,mode),actual(recovered,mode);
    auto context=Initial(mode);
    auto state=FromPpc(context);
    std::fprintf(stderr,"heap-create mode=%u original\n",
        static_cast<unsigned>(mode));
    std::fflush(stderr);
    active=&expected;
    __imp__sub_827CC9D0(context,original.Bytes());
    active=nullptr;
    std::fprintf(stderr,"heap-create mode=%u original-done\n",
        static_cast<unsigned>(mode));
    std::fflush(stderr);
    if(expected.unexpected) throw std::runtime_error(expected.unexpected);
    if(!Independent(original_memory,context,mode,expected.events))
    {
        std::fprintf(stderr,"EXPECT heap-create mode=%u r3=%08X "
            "slot=%08X events=%zu\n",static_cast<unsigned>(mode),
            context.r3.u32,original_memory.ReadU32(fixture::Heap+96u),
            expected.events.size());
        throw std::runtime_error("independent heap-create outcome");
    }
    std::fprintf(stderr,"heap-create mode=%u recovered\n",
        static_cast<unsigned>(mode));
    std::fflush(stderr);
    active=&actual;
    const bool applied=family::Apply(0x827cc9d0u,
        recovered_memory,actual,state);
    active=nullptr;
    std::fprintf(stderr,"heap-create mode=%u recovered-done\n",
        static_cast<unsigned>(mode));
    std::fflush(stderr);
    if(!applied || actual.unexpected)
        throw std::runtime_error(actual.unexpected?
            actual.unexpected:"selected heap-create entry missing");
    const auto observed=FromPpc(context);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events)
    {
        std::fprintf(stderr,"FAIL heap-create mode=%u state=%u RAM=%u "
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
        throw std::runtime_error("heap-create selected context differs");
    }
}
} // namespace

void OriginalSave(unsigned first,PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    const auto state=FromPpc(context);
    for(unsigned index=first;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Address(state.r[12]));
}
void OriginalRestore(unsigned first,PPCContext& context,std::uint8_t* base)
{
    auto memory=GuestMemory(0u,
        std::span<std::uint8_t>(base,GuestWindow::Space));
    auto state=FromPpc(context);
    for(unsigned index=first;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];ToPpc(context,state);
}
void OriginalSave21(PPCContext& c,std::uint8_t* b)
{OriginalSave(21u,c,b);}
void OriginalRestore21(PPCContext& c,std::uint8_t* b)
{OriginalRestore(21u,c,b);}
void OriginalSave22(PPCContext& c,std::uint8_t* b)
{OriginalSave(22u,c,b);}
void OriginalRestore22(PPCContext& c,std::uint8_t* b)
{OriginalRestore(22u,c,b);}
void OriginalSave28(PPCContext& c,std::uint8_t* b)
{OriginalSave(28u,c,b);}
void OriginalRestore28(PPCContext& c,std::uint8_t* b)
{OriginalRestore(28u,c,b);}
void OriginalNative(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    std::fprintf(stderr,"heap-create native entry=%08X lr=%08X "
        "r3=%08X r4=%08X r5=%08X\n",entry,
        static_cast<std::uint32_t>(context.lr),
        context.r3.u32,context.r4.u32,context.r5.u32);
    std::fflush(stderr);
    auto state=FromPpc(context);
    active->CallNative(entry,active->memory,state);
    ToPpc(context,state);
}
void OriginalGuestMove(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    active->CallGuestMove(active->memory,state);
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto mode:Modes) Check(mode);
        std::printf("PASS heap-create-context %zu original PPC cases\n",
            Modes.size());
        std::puts("LIMIT complete selected heap-create body with selected segment/range/insert and accepted fill; move guest ABI and kernel imports remain mutable boundaries; other CR/fault/MMIO/concurrency/runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

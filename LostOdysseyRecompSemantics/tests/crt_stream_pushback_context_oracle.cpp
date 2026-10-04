#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_stream_pushback_context.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace pushback_oracle
{
namespace pushback = crt_stream_pushback_context;
namespace record = crt_record_allocation_context;
using Full = pushback::Registers;
using PushbackEvent = std::array<std::uint64_t, 7>;

enum class Route {Eof, Write, Match, Mismatch, Allocate, InlineBuffer};
struct PushbackCase {const char* name;Route route;};
constexpr std::array PushbackCases{
    PushbackCase{"eof-early-return",Route::Eof},
    PushbackCase{"text-byte-store",Route::Write},
    PushbackCase{"binary-prior-byte-match",Route::Match},
    PushbackCase{"binary-prior-byte-mismatch",Route::Mismatch},
    PushbackCase{"lazy-heap-buffer",Route::Allocate},
    PushbackCase{"lazy-inline-buffer",Route::InlineBuffer}};

struct PushbackHeap final : heap_allocation_context::BoundaryServices
{
    void CallDirect(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unselected pushback heap guest call");}
    void CallNative(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unselected pushback heap import");}
};
struct PushbackHandler final : record::HandlerServices
{
    void CallNewHandler(GuestAddress,GuestMemory&,Full&) override
    {throw std::runtime_error("unselected pushback new-handler target");}
};
struct PushbackGuest final : crt_reallocation_context::GuestServices
{
    explicit PushbackGuest(Route route):route(route){}
    Route route;
    std::vector<PushbackEvent> events;
    void CallLower(GuestAddress entry,GuestMemory&,Full& state) override
    {
        events.push_back({entry,state.r[1],state.lr,state.r[3],
            state.r[4],state.r[5],state.ctr});
        if(entry!=0x823acbd0u||state.r[3]!=4096u)
            throw std::runtime_error("unselected pushback guest allocator");
        state.r[3]=route==Route::InlineBuffer?0u:Buffer;
    }
};

pushback::Dependencies Deps(Services& stream,PushbackHeap& heap,
    PushbackHandler& handler,PushbackGuest& guest)
{
    record::Dependencies allocation{Dependencies(stream),heap,handler};
    return {{allocation,guest}};
}

void SeedPushback(test::GuestWindow& window,Route route)
{
    Seed(window,Mode::BufferedChar);
    auto memory=window.Memory();
    const bool lazy=route==Route::Allocate||route==Route::InlineBuffer;
    const bool text=route==Route::Write||lazy;
    memory.WriteU32(Stream,lazy?0u:Buffer+1u);
    memory.WriteU32(Stream+4u,0u);
    memory.WriteU32(Stream+8u,lazy?0u:Buffer);
    memory.WriteU32(Stream+12u,text?0x80u:0x41u);
    memory.WriteU32(Stream+16u,0xffffffffu);
    memory.WriteU8(Buffer,'A');
    memory.WriteU8(0x83215340u,0u);
    memory.WriteU32(0x832d3ab8u,0u);
}

PPCContext Initial(Route route)
{
    PPCContext context{};
    const auto gprs=crt_full_oracle::Gprs(context);
    const auto fprs=crt_full_oracle::Fprs(context);
    for(unsigned index=0;index<32u;++index)
    {
        gprs[index]->u64=0x2233445500000000ull+index;
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
    context.r3.u64=route==Route::Eof?UINT64_MAX:
        route==Route::Mismatch?static_cast<std::uint64_t>('B'):
        route==Route::Write?0xc1u:static_cast<std::uint64_t>('A');
    context.r4.u64=0x1234567800000000ull|Stream;
    return context;
}

Services* original_stream=nullptr;
PushbackHeap* original_heap=nullptr;
PushbackHandler* original_handler=nullptr;
PushbackGuest* original_guest=nullptr;

void Check(const PushbackCase& item)
{
    test::GuestWindow original(Regions),recovered(Regions);
    SeedPushback(original,item.route);SeedPushback(recovered,item.route);
    Services expected(original,Mode::BufferedChar);
    Services actual(recovered,Mode::BufferedChar);
    PushbackHeap expected_heap,actual_heap;
    PushbackHandler expected_handler,actual_handler;
    PushbackGuest expected_guest(item.route),actual_guest(item.route);
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_stream=&expected;original_heap=&expected_heap;
    original_handler=&expected_handler;original_guest=&expected_guest;
    __imp__sub_82B87CF8(context,original.Bytes());
    active=nullptr;original_stream=nullptr;original_heap=nullptr;
    original_handler=nullptr;original_guest=nullptr;
    if(!pushback::Apply(0x82b87cf8u,actual.memory,
            Deps(actual,actual_heap,actual_handler,actual_guest),state))
        throw std::runtime_error("missing recovered pushback");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||expected_guest.events!=actual_guest.events||
        expected.traps!=actual.traps)
        throw std::runtime_error("pushback full state/RAM/ordered event mismatch");
    const auto memory=expected.memory;
    const auto result=context.r3.u64;
    const bool allocation=item.route==Route::Allocate||item.route==Route::InlineBuffer;
    if(expected_guest.events.size()!=(allocation?1u:0u))
        throw std::runtime_error("pushback guest allocation route missed");
    switch(item.route)
    {
    case Route::Eof:
        if(result!=UINT64_MAX||memory.ReadU32(Stream)!=Buffer+1u||
            memory.ReadU32(Stream+4u)!=0u)
            throw std::runtime_error("pushback EOF route missed");
        break;
    case Route::Write:
        if(result!=0xc1u||memory.ReadU8(Buffer)!=0xc1u||
            memory.ReadU32(Stream)!=Buffer||memory.ReadU32(Stream+4u)!=1u)
            throw std::runtime_error("pushback text byte route missed");
        break;
    case Route::Match:
        if(result!=static_cast<std::uint64_t>('A')||
            memory.ReadU32(Stream)!=Buffer||memory.ReadU32(Stream+4u)!=1u)
            throw std::runtime_error("pushback binary match route missed");
        break;
    case Route::Mismatch:
        if(result!=UINT64_MAX||memory.ReadU32(Stream)!=Buffer+1u||
            memory.ReadU32(Stream+4u)!=0u)
            throw std::runtime_error("pushback binary mismatch route missed");
        break;
    case Route::Allocate:
        if(result!=static_cast<std::uint64_t>('A')||
            memory.ReadU32(Stream+8u)!=Buffer||
            memory.ReadU32(Stream+24u)!=4096u||
            memory.ReadU8(Buffer)!='A'||memory.ReadU32(Stream+4u)!=1u)
            throw std::runtime_error("pushback allocated buffer route missed");
        break;
    case Route::InlineBuffer:
        if(result!=static_cast<std::uint64_t>('A')||
            memory.ReadU32(Stream+8u)!=Stream+20u||
            memory.ReadU32(Stream+24u)!=2u||
            memory.ReadU8(Stream+20u)!='A'||memory.ReadU32(Stream+4u)!=1u)
            throw std::runtime_error("pushback inline buffer route missed");
        break;
    }
}
} // namespace pushback_oracle

void OriginalPushbackSave27(PPCContext& context)
{
    auto memory=active->memory;
    const auto state=crt_full_oracle::FromPpc(context);
    for(unsigned index=27u;index<=31u;++index)
        WriteU64(memory,Address(context.r1.u64-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}
void OriginalPushbackRestore27(PPCContext& context)
{
    auto memory=active->memory;
    auto state=crt_full_oracle::FromPpc(context);
    for(unsigned index=27u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(context.r1.u64-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(context.r1.u64-8u));
    state.lr=state.r[12];crt_full_oracle::ToPpc(context,state);
}
void OriginalPushbackLower(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    auto dependencies=pushback_oracle::Deps(*pushback_oracle::original_stream,
        *pushback_oracle::original_heap,*pushback_oracle::original_handler,
        *pushback_oracle::original_guest);
    if(!crt_reallocation_context::ApplyLower(entry,active->memory,
            dependencies.reallocation,state))
        throw std::runtime_error("missing selected pushback lower");
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(const auto& item:pushback_oracle::PushbackCases)
    {
        try{pushback_oracle::Check(item);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::puts("CRT stream pushback: 6 focused actual PPC cases passed");
    return 0;
}

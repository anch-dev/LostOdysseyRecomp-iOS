#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_stream_position_routes.h"
#include "lo_semantics/heap_allocation_context.h"
#include "lo_semantics/heap_free_context.h"
#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/crt_status_error.h"

namespace
{
namespace position = crt_stream_position_routes;
using Full = position::Registers;
constexpr GuestAddress Result = 0x53000u;
constexpr GuestAddress FlagGlobal = 0x832ecd20u;
constexpr std::array<Region,6> PositionRegions{{{0,0x120000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x832d3000u,0x2000u},{0x832ec000u,0x2000u},
    {0x83378000u,0x3000u}}};

enum class Scenario {SeekAndClear,SetFlag,ReadGlobal,NativePosition,
    TransferInvalid,TransferNoBytes};
struct PositionCase {const char* name;GuestAddress entry;Scenario scenario;};
constexpr std::array Cases{
    PositionCase{"seek-and-clear",0x82df43f0u,Scenario::SeekAndClear},
    PositionCase{"set-stream-flag",0x82df6ae0u,Scenario::SetFlag},
    PositionCase{"read-global",0x82df6b68u,Scenario::ReadGlobal},
    PositionCase{"native-position",0x82be44a8u,Scenario::NativePosition},
    PositionCase{"transfer-invalid",0x82df6948u,Scenario::TransferInvalid},
    PositionCase{"transfer-no-bytes",0x82df6948u,Scenario::TransferNoBytes}};

std::array<PPCRegister*,32> Fields(PPCContext& context)
{
    return {&context.r0,&context.r1,&context.r2,&context.r3,&context.r4,
        &context.r5,&context.r6,&context.r7,&context.r8,&context.r9,
        &context.r10,&context.r11,&context.r12,&context.r13,&context.r14,
        &context.r15,&context.r16,&context.r17,&context.r18,&context.r19,
        &context.r20,&context.r21,&context.r22,&context.r23,&context.r24,
        &context.r25,&context.r26,&context.r27,&context.r28,&context.r29,
        &context.r30,&context.r31};
}

std::array<PPCRegister*,32> FprFields(PPCContext& context)
{
    return {&context.f0,&context.f1,&context.f2,&context.f3,&context.f4,
        &context.f5,&context.f6,&context.f7,&context.f8,&context.f9,
        &context.f10,&context.f11,&context.f12,&context.f13,&context.f14,
        &context.f15,&context.f16,&context.f17,&context.f18,&context.f19,
        &context.f20,&context.f21,&context.f22,&context.f23,&context.f24,
        &context.f25,&context.f26,&context.f27,&context.f28,&context.f29,
        &context.f30,&context.f31};
}

Full FullFromPpc(PPCContext& context)
{
    Full state{};
    const auto gpr=Fields(context),fpr=FprFields(context);
    for(unsigned i=0;i<32u;++i)
    {state.r[i]=gpr[i]->u64;state.fpr_bits[i]=fpr[i]->u64;}
    state.lr=context.lr;state.ctr=context.ctr.u64;
    state.cached_fp_control=context.fpscr.csr;
    state.xer_so=context.xer.so;state.xer_ca=context.xer.ca;
    state.cr0={context.cr0.lt,context.cr0.gt,context.cr0.eq,context.cr0.so};
    state.cr6={context.cr6.lt,context.cr6.gt,context.cr6.eq,context.cr6.so};
    return state;
}
void FullToPpc(PPCContext& context,const Full& state)
{
    const auto gpr=Fields(context),fpr=FprFields(context);
    for(unsigned i=0;i<32u;++i)
    {gpr[i]->u64=state.r[i];fpr[i]->u64=state.fpr_bits[i];}
    context.lr=state.lr;context.ctr.u64=state.ctr;
    context.fpscr.csr=state.cached_fp_control;
    context.xer.so=state.xer_so;context.xer.ca=state.xer_ca;
    context.cr0={state.cr0.lt,state.cr0.gt,state.cr0.eq,{state.cr0.so}};
    context.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,{state.cr6.so}};
}
bool FullSame(const Full& a,const Full& b)
{
    const auto equal=[](crt_async_status_transfer::Condition x,
        crt_async_status_transfer::Condition y)
    {return x.lt==y.lt&&x.gt==y.gt&&x.eq==y.eq&&x.so==y.so;};
    return a.r==b.r&&a.fpr_bits==b.fpr_bits&&a.lr==b.lr&&a.ctr==b.ctr&&
        a.cached_fp_control==b.cached_fp_control&&
        a.xer_so==b.xer_so&&a.xer_ca==b.xer_ca&&
        equal(a.cr0,b.cr0)&&equal(a.cr6,b.cr6);
}

struct PositionNative final : position::NativeServices,
    heap_allocation_context::BoundaryServices,
    heap_free_context::BoundaryServices
{
    std::vector<std::array<std::uint64_t,5>> events;
    void NtQueryInformationFile(GuestMemory& memory,Full& state) override
    {
        if(state.r[6]!=8u||state.r[7]!=14u)
            throw std::runtime_error("unexpected query native ABI");
        events.push_back({14,state.r[1],state.lr,state.r[3],state.r[5]});
        WriteU64(memory,Address(state.r[5]),0x123456789abcdef0ull);
        state.r[3]=0;
    }
    void NtSetInformationFile(GuestMemory& memory,Full& state) override
    {
        if(state.r[6]!=8u||(state.r[7]!=19u&&state.r[7]!=20u)||
            ReadU64(memory,Address(state.r[5]))!=0x123456789abcdef0ull)
            throw std::runtime_error("unexpected set native ABI");
        events.push_back({state.r[7],state.r[1],state.lr,state.r[3],
            state.r[5]});
        state.r[3]=0;
    }
    void CallDirect(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unselected accepted heap guest path");}
    void CallNative(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unselected accepted heap import");}
};

PositionNative* active_position=nullptr;

position::Dependencies PositionDependencies(Services& stream,
    PositionNative& native)
{return {Dependencies(stream),native,native,native};}

void SeedPosition(GuestWindow& window,const PositionCase& item)
{
    Seed(window,Mode::Binary);
    auto memory=window.Memory();
    memory.WriteU32(FlagGlobal,0x89abcdefu);
    memory.WriteU32(0x34004u,0x2601u);
    if(item.scenario==Scenario::SeekAndClear)
        memory.WriteU8(Record+4u,3u);
}

PPCContext InitialPosition(const PositionCase& item)
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0xabcdef0123456789ull;
    context.r13.u64=Environment;
    context.r3.u64=item.scenario==Scenario::ReadGlobal?Result:
        item.scenario==Scenario::NativePosition?0x3500u:
        item.scenario==Scenario::TransferInvalid?17u:
        0x1122334400000005ull;
    context.r4.u64=item.scenario==Scenario::SetFlag?16384u:
        item.scenario==Scenario::TransferNoBytes?0x24u:
        item.scenario==Scenario::SeekAndClear?0x20u:0u;
    context.r5.u64=0u;
    context.r26.u64=0x2611223344556677ull;
    context.r27.u64=0x2711223344556677ull;
    context.r28.u64=0x2811223344556677ull;
    context.r29.u64=0x2911223344556677ull;
    context.r30.u64=0x3011223344556677ull;
    context.r31.u64=0x3111223344556677ull;
    context.f0.u64=0x3ff0000000000000ull;
    context.f1.u64=0x4008000000000000ull;
    context.f31.u64=0x4010000000000000ull;
    context.fpscr.csr=0x1f80u;
    context.xer.so=1;context.xer.ca=1;
    return context;
}

void CheckPosition(const PositionCase& item)
{
    GuestWindow original(PositionRegions),recovered(PositionRegions);
    SeedPosition(original,item);SeedPosition(recovered,item);
    Services expected(original,Mode::Binary),actual(recovered,Mode::Binary);
    PositionNative expected_native,actual_native;
    PPCContext context=InitialPosition(item);
    auto state=FullFromPpc(context);
    active=&expected;active_position=&expected_native;
    switch(item.entry)
    {
    case 0x82df43f0u:__imp__sub_82DF43F0(context,original.Bytes());break;
    case 0x82df6948u:__imp__sub_82DF6948(context,original.Bytes());break;
    case 0x82df6ae0u:__imp__sub_82DF6AE0(context,original.Bytes());break;
    case 0x82df6b68u:__imp__sub_82DF6B68(context,original.Bytes());break;
    case 0x82be44a8u:__imp__sub_82BE44A8(context,original.Bytes());break;
    default:throw std::runtime_error("missing original position entry");
    }
    active_position=nullptr;active=nullptr;
    if(!position::Apply(item.entry,actual.memory,
            PositionDependencies(actual,actual_native),state))
        throw std::runtime_error("missing recovered position entry");
    if(!FullSame(FullFromPpc(context),state)||
        !original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_native.events!=actual_native.events||
        expected.traps!=actual.traps||
        expected.seeks!=actual.seeks||expected.writes!=actual.writes)
        throw std::runtime_error("selected context, callback or RAM mismatch");
    const auto memory=expected.memory;
    switch(item.scenario)
    {
    case Scenario::SeekAndClear:
        if(context.r3.u32==UINT32_MAX||expected.seeks==0u||
            memory.ReadU8(Record+4u)!=1u)
            throw std::runtime_error("seek flag-clear path missed");
        break;
    case Scenario::SetFlag:
        if(context.r3.u64!=32768u||
            (memory.ReadU8(Record+4u)&0x80u)==0u)
            throw std::runtime_error("set flag branch missed");
        break;
    case Scenario::ReadGlobal:
        if(context.r3.u64!=0u||memory.ReadU32(Result)!=0x89abcdefu)
            throw std::runtime_error("global stream value missed");
        break;
    case Scenario::NativePosition:
        if(context.r3.u64!=1u||expected_native.events.size()!=3u)
            throw std::runtime_error("query/two-set native path missed");
        break;
    case Scenario::TransferInvalid:
        if(context.r3.u64!=9u||expected.seeks!=0u||
            memory.ReadU32(0x83215210u)!=9u)
            throw std::runtime_error("invalid transfer seek path missed");
        break;
    case Scenario::TransferNoBytes:
    {
        // Three seeks perform two query/set pairs, then the final restore
        // issues one set call. Event[2] is the live PPC return address.
        constexpr std::array<std::uint64_t,5> expected_lr{
            0x82be29e8u,0x82be2a54u,0x82be29a8u,
            0x82be2a54u,0x82be2a54u};
        bool sequence=expected.events.size()==expected_lr.size();
        if(sequence)
        {
            for(std::size_t i=0;i<expected_lr.size();++i)
                sequence=sequence&&expected.events[i][0]==5u&&
                    expected.events[i][2]==expected_lr[i];
        }
        if(context.r3.u64!=0u||expected.seeks!=5u||
            !expected_native.events.empty()||!sequence)
            throw std::runtime_error("zero-length transfer restore missed");
        break;
    }
    }
}
} // namespace

void OriginalPositionStream(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=FromPpc(context);
    const bool found=entry==0x82b85ea8u||entry==0x82b81b88u?
        crt_stream_operations::Apply(entry,active->memory,
            Dependencies(*active),state):
        crt_stream_operations::ApplyAcceptedCallee(entry,active->memory,
            Dependencies(*active),state);
    if(!found)throw std::runtime_error("missing accepted stream lower");
    ToPpc(context,state);
}

void OriginalPositionStatus(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    crt_status_error::Registers lower{};
    lower.sp=state.sp;lower.lr=state.lr;
    lower.r3=state.r[3];lower.r11=state.r[11];
    lower.r12=state.r[12];lower.r13=state.r[13];
    lower.xer_so=state.xer_so;
    lower.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
    if(!crt_status_error::Apply(0x827ca628u,active->memory,*active,lower))
        throw std::runtime_error("missing accepted status lower");
    state.sp=lower.sp;state.lr=lower.lr;
    state.r[3]=lower.r3;state.r[11]=lower.r11;
    state.r[12]=lower.r12;state.r[13]=lower.r13;
    state.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
    ToPpc(context,state);
}

void OriginalPositionHeap(GuestAddress,PPCContext&,std::uint8_t*)
{throw std::runtime_error("unselected accepted heap allocation/free path");}

void OriginalPositionQuery(PPCContext& context,std::uint8_t*)
{auto state=FullFromPpc(context);
 active_position->NtQueryInformationFile(active->memory,state);
 FullToPpc(context,state);}
void OriginalPositionSet(PPCContext& context,std::uint8_t*)
{auto state=FullFromPpc(context);
 active_position->NtSetInformationFile(active->memory,state);
 FullToPpc(context,state);}

int main()
{
    try
    {
        for(const auto& item:Cases)
        {
            try{CheckPosition(item);}
            catch(const std::exception& error)
            {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
        }
        std::printf("PASS crt-stream-position-routes %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT positive transfer heap/read-write loop and deeper accepted guest heap paths remain unselected; native query/set imports use mutable selected context");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

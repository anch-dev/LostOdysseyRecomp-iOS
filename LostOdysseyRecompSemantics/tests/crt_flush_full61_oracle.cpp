#define main FullFlushRefillFixtureMain
#include "crt_stream_refill_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_flush_full61.h"

namespace full_flush_oracle
{
namespace family=crt_flush_full61;
using Full=family::Registers;
constexpr GuestAddress Environment2=0x65000u,Environment3=0x66000u;
constexpr GuestAddress Error2=0x67000u,Error3=0x68000u;
struct Native final : family::NativeServices
{
    bool failure;
    std::vector<std::array<std::uint64_t,72>> events;
    explicit Native(bool fail):failure(fail){}
    void Trace(Full& state){events.push_back(crt_full_oracle::Snapshot(state));}
    static void Mutate(Full& state,unsigned phase)
    {
        state.r[10]=0x1234567800000000ull|phase;
        state.fpr_bits[3]=0x4008000000000000ull|phase;
        state.fpr_bits[13]=0xc009000000000000ull|phase;
        state.cr1={1,0,1,1};state.cr7={0,1,0,1};
        state.cached_fp_control=0x100u|phase;
        state.xer_so=1;state.xer_ca=phase&1u;
    }
    void EnterCriticalSection(GuestMemory&,Full& state) override
    {
        Trace(state);
        if(state.lr!=0x82b863acu||Address(state.r[3])!=Record+12u)
            throw std::runtime_error("Full flush enter/table route missed");
        Mutate(state,1);state.r[3]=0x1357246800000042ull;
    }
    void NtFlushBuffersFile(GuestMemory& memory,Full& state) override
    {
        Trace(state);
        if(state.lr!=0x82be48acu||state.r[3]!=0x3500u||
            Address(state.r[4])!=Address(state.r[1]+80u))
            throw std::runtime_error("Full flush handle/IOSB route missed");
        memory.WriteU32(Address(state.r[4]),0x10203040u);
        memory.WriteU32(Address(state.r[4]+4u),0x50607080u);
        Mutate(state,2);
        state.r[3]=failure?0xffffffffc0000017ull:0u;
        if(failure)state.r[13]=0x9876543200000000ull|Environment2;
    }
    void RtlNtStatusToDosError(GuestMemory&,Full& state) override
    {
        Trace(state);
        if(!failure||state.lr!=0x827ca638u||
            state.r[3]!=0xffffffffc0000017ull||Address(state.r[13])!=Environment2)
            throw std::runtime_error("Full flush live status boundary missed");
        Mutate(state,3);state.r[3]=5u;
        state.r[13]=0xabcdef0100000000ull|Environment3;
    }
    void LeaveCriticalSection(GuestMemory&,Full& state) override
    {
        Trace(state);
        if(state.lr!=0x82b820e4u||Address(state.r[3])!=Record+12u)
            throw std::runtime_error("Full flush cleanup/tail route missed");
        Mutate(state,4);state.r[3]=0x2468135700000066ull;
    }
};
family::Dependencies* original_dependencies=nullptr;
void Seed(GuestWindow& window)
{
    refill_oracle::SeedRefill(window);
    auto memory=window.Memory();
    memory.WriteU32(Record,0x3500u);memory.WriteU32(Record+8u,0x70000u);
    // D80 descriptors and E90 streams are different actual tables.
    memory.WriteU32(Blocks,0x30000u);
    memory.WriteU32(0x83378e90u,0x56000u);
    memory.WriteU32(0x56140u,0xdeadc0deu);
    memory.WriteU8(0x56144u,0u);
    memory.WriteU32(Environment2+336u,0u);
    memory.WriteU32(Environment2+256u,Error2);
    memory.WriteU32(Environment3+336u,0u);
    memory.WriteU32(Environment3+256u,Error3);
    memory.WriteU32(ErrorState+352u,0x11111111u);
    memory.WriteU32(Error2+352u,0x22222222u);
    memory.WriteU32(Error3+352u,0x33333333u);
}
void Check(bool failure)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    Seed(original);Seed(recovered);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    refill_oracle::RefillHost expected_host,actual_host;
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    Native expected_native(failure),actual_native(failure);
    family::Dependencies expected_deps{refill_oracle::Deps(expected,expected_index,
        expected_host,expected_unlock,expected_extra),expected_native};
    family::Dependencies actual_deps{refill_oracle::Deps(actual,actual_index,
        actual_host,actual_unlock,actual_extra),actual_native};
    auto context=refill_oracle::InitialRefill(refill_oracle::RefillRoute::Binary);
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0x8765432112345678ull;
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_dependencies=&expected_deps;
    __imp__sub_82B81F78(context,original.Bytes());
    active=nullptr;original_dependencies=nullptr;
    if(!family::Apply(0x82b81f78u,actual.memory,actual_deps,state))
        throw std::runtime_error("missing Full flush owner");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=
        crt_full_oracle::Snapshot(state)||!original.EqualCommitted(recovered)||
        expected_native.events!=actual_native.events||expected.events!=actual.events||
        expected_index.events!=actual_index.events||expected_host.events!=actual_host.events||
        expected_unlock.events!=actual_unlock.events||expected_extra.events!=actual_extra.events||
        expected.traps!=actual.traps)
        throw std::runtime_error("Full flush Full72/RAM/native snapshot mismatch");
    if(expected_native.events.size()!=(failure?4u:3u)||
        context.r3.u32!=(failure?UINT32_MAX:0u)||
        expected.memory.ReadU32(Blocks)!=0x30000u||
        expected.memory.ReadU32(0x83378e90u)!=0x56000u||
        expected.memory.ReadU32(0x56140u)!=0xdeadc0deu)
        throw std::runtime_error("Full flush success/failure/table guard missed");
    if(failure&&(context.r13.u64!=(0xabcdef0100000000ull|Environment3)||
        expected.memory.ReadU32(Error3+352u)!=5u||
        expected.memory.ReadU32(ErrorState+352u)!=0x11111111u||
        expected.memory.ReadU32(Error2+352u)!=0x22222222u))
        throw std::runtime_error("Full flush post-status live thread slot missed");
}
}
void OriginalFullFlushLower(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_flush_full61::ApplyAcceptedLower(entry,active->memory,
        *full_flush_oracle::original_dependencies,state))
        throw std::runtime_error("missing accepted Full flush lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalFullFlushNative(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    auto& native=full_flush_oracle::original_dependencies->native;
    switch(entry)
    {
    case 0x830d9c6cu:native.EnterCriticalSection(active->memory,state);break;
    case 0x830d9c7cu:native.LeaveCriticalSection(active->memory,state);break;
    case 0x830da39cu:native.NtFlushBuffersFile(active->memory,state);break;
    case 0x830d9efcu:native.RtlNtStatusToDosError(active->memory,state);break;
    default:throw std::runtime_error("unselected Full flush import");
    }
    crt_full_oracle::ToPpc(context,state);
}
int main()
{
    for(bool failure:{false,true})
    {
        try{full_flush_oracle::Check(failure);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",failure?"failure-live-status":"success",error.what());return 1;}
    }
    std::puts("CRT Full flush: 2 actual PPC success/live-status cases passed");
    return 0;
}

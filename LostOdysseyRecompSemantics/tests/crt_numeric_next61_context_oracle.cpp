// The accepted close fixture supplies selected CRT errno services and full
// PPC register/RAM comparison without executing its own case matrix.
#define main NumericNextSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main

#include "lo_semantics/crt_numeric_next61_context.h"

namespace numeric_next_oracle
{
namespace adjacent=crt_numeric_next61_context;
enum class ScanRoute {ParentIndex1,LocalIndex33,LiveSavedSlot};
struct ScanCase {const char* name;GuestAddress entry;ScanRoute route;};
constexpr std::array ScanCases{
    ScanCase{"mode-parent-index1",0x82df45e0u,ScanRoute::ParentIndex1},
    ScanCase{"local-index33-high64",0x82df4600u,ScanRoute::LocalIndex33},
    ScanCase{"live-callback-saved-r30",0x82df4600u,ScanRoute::LiveSavedSlot}};
constexpr GuestAddress Frame=0x81000u;
struct LiveUnlock final : crt_stream_pointer_unlock::NativeServices
{
    std::vector<wrapper_oracle::WrapperTrace> events;
    bool mutate=false;
    void LeaveCriticalSection(GuestMemory& memory,
        crt_stream_pointer_unlock::Registers& state) override
    {
        events.push_back({state.sp,state.lr,state.r3,state.r30,state.r31});
        if(mutate) memory.WriteU32(static_cast<GuestAddress>(state.sp+112u-12u),0x55667788u);
    }
};
LiveUnlock* active_unlock=nullptr;
void Seed(GuestWindow& window,ScanRoute)
{
    close_shared_oracle::SeedShared(window,close_shared_oracle::Route::NullPath);
    auto memory=window.Memory();
    memory.WriteU32(0x83378d80u,0x30000u);
    memory.WriteU32(0x83378d84u,0x32000u);
    memory.WriteU32(Frame+180u,1u);
}
PPCContext Initial(ScanRoute)
{
    auto context=close_shared_oracle::Initial(close_shared_oracle::Route::NullPath);
    context.r12.u64=0x8877665500000000ull|(Frame+160u);
    context.r30.u64=0xaabbccdd00000021ull;
    context.r31.u64=0x1122334400059000ull;
    context.lr=0xfedcba9882df477cull;
    return context;
}
void Check(const ScanCase& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    Seed(original,item.route);Seed(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock unused_unlock;
    LiveUnlock expected_unlock,actual_unlock;
    expected_unlock.mutate=actual_unlock.mutate=item.route==ScanRoute::LiveSavedSlot;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;active=&expected;current_index=&expected_index;
    current_host=&expected_host;
    active_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    if(item.entry==0x82df45e0u) __imp__sub_82DF45E0(context,original.Bytes());
    else __imp__sub_82DF4600(context,original.Bytes());
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;
    active_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;
    const auto accepted=close_shared_oracle::Deps(actual,actual_index,actual_host,
        unused_unlock,actual_extra);
    const adjacent::Dependencies dependencies{accepted.close,
        {accepted.open.open,actual_unlock},accepted.native};
    if(!adjacent::Apply(item.entry,actual.memory,dependencies,state))
        throw std::runtime_error("missing mode pointer frame body");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_index.events!=actual_index.events||
        expected_host.events!=actual_host.events||
        expected_unlock.events!=actual_unlock.events||
        expected_extra.events!=actual_extra.events||
        expected.traps!=actual.traps||
        expected.converted_errors!=actual.converted_errors)
    {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i])
                std::fprintf(stderr,"case %u state[%u] %llx/%llx\n",ordinal,i,
                    static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        for(const auto region:OpenRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=
                    recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",
                    ordinal,static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
        throw std::runtime_error("adjacent scan full state/RAM/callback mismatch");
    }
    const auto expected_r30=item.route==ScanRoute::LiveSavedSlot?
        0xaabbccdd55667788ull:0xaabbccdd00000021ull;
    const auto expected_pointer=item.route==ScanRoute::ParentIndex1?0x3004cu:0x3204cu;
    if(expected_unlock.events.size()!=1u||expected_unlock.events[0][2]!=expected_pointer||
        expected_unlock.events[0][1]!=0x82df4620u||context.r1.u64!=Stack||
        context.r30.u64!=expected_r30||context.r31.u64!=0x1122334400059000ull||
        context.lr!=0x82df477cu)
        throw std::runtime_error("missed live pointer callback/frame restoration");
}

} // namespace numeric_next_oracle

void OriginalNumericNextPointer(PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp=state.r[1];lower.lr=state.lr;
    lower.r3=state.r[3];lower.r9=state.r[9];
    lower.r10=state.r[10];lower.r11=state.r[11];
    lower.r12=state.r[12];lower.r30=state.r[30];
    lower.r31=state.r[31];lower.xer_ca=state.xer_ca;
    if(!crt_stream_pointer_unlock::Apply(0x82b863f0u,current->memory,
            *numeric_next_oracle::active_unlock,lower))
        throw std::runtime_error("missing original accepted pointer unlock");
    state.r[1]=lower.sp;state.lr=lower.lr;
    state.r[3]=lower.r3;state.r[9]=lower.r9;
    state.r[10]=lower.r10;state.r[11]=lower.r11;
    state.r[12]=lower.r12;state.r[30]=lower.r30;
    state.r[31]=lower.r31;state.xer_ca=lower.xer_ca;
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(unsigned i=0;i<numeric_next_oracle::ScanCases.size();++i)
    {
        const auto& item=numeric_next_oracle::ScanCases[i];
        try{numeric_next_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-numeric-next61-context %zu actual PPC cases\n",
        numeric_next_oracle::ScanCases.size());
    std::puts("LIMIT selected full context and accepted pointer-unlock/native ABI boundary; no runtime/fault/MMIO/concurrency validation");
    return 0;
}

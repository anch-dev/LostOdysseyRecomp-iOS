#pragma push_macro("main")
#undef main
#define main FormatFrameCloseFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_format_frame61.h"
#include "crt_full_context_oracle_fixture.h"
namespace frame61_oracle {
constexpr GuestAddress CallerFrame=0x60000u;
struct Extra final:close_shared_oracle::SharedExtra {
    void LeaveCriticalSection(GuestMemory& memory,crt_stream_bulk_close_routes::Registers& state) override {
        events.push_back({88u,state.sp,state.lr,state.r[3],state.r[10],state.r[31]});
        memory.WriteU32(0x53000u,static_cast<std::uint32_t>(state.r[3]));
        state.r[3]=0x9988776655443322ull;state.r[10]=0x123456789abcdef0ull;
        state.ctr=0xaabbccdd87654321ull;state.cr0.lt^=1u;state.cr6.eq^=1u;state.xer_ca^=1u;
    }
};
struct Environment {
    Services services;IndexService index;Host host;wrapper_oracle::WrapperUnlock unlock;Extra extra;
    explicit Environment(GuestWindow& window):services(window,Mode::LockedWrite),host(Scenario::BinarySuccess) {}
    crt_stream_close_shared_lower::Dependencies Deps(){return close_shared_oracle::Deps(services,index,host,unlock,extra);}
};
Environment* active_environment=nullptr;
void Check(GuestAddress entry) {
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    auto seed=[](GuestWindow& window) {
        close_shared_oracle::SeedShared(window,close_shared_oracle::Route::NullPath);
        auto memory=window.Memory();memory.WriteU32(CallerFrame+188u,Stream);memory.WriteU32(CallerFrame+88u,Stream);
    };
    seed(original);seed(recovered);Environment expected(original),actual(recovered);
    auto context=close_shared_oracle::Initial(close_shared_oracle::Route::NullPath);
    context.r1.u64=0x8877665500000000ull|Stack;context.lr=0x1234567887654321ull;
    context.r12.u64=0xaabbccdd00000000ull|(CallerFrame+(entry==0x82df2ac8u ? 160u:144u));
    context.r30.u64=0x9988776600000000ull|Stream;
    const auto initial=crt_full_oracle::FromPpc(context);auto state=initial;
    current=&expected.services;active=&expected.services;current_index=&expected.index;current_host=&expected.host;
    wrapper_oracle::original_unlock=&expected.unlock;close_shared_oracle::active_extra=&expected.extra;active_environment=&expected;
    switch(entry){case 0x82df2864u:__imp__sub_82DF2864(context,original.Bytes());break;case 0x82df2884u:__imp__sub_82DF2884(context,original.Bytes());break;default:__imp__sub_82DF2AC8(context,original.Bytes());}
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;wrapper_oracle::original_unlock=nullptr;close_shared_oracle::active_extra=nullptr;active_environment=nullptr;
    if(!crt_format_frame61::Apply(entry,actual.services.memory,actual.Deps(),state)) throw std::runtime_error("missing format frame entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) || !original.EqualCommitted(recovered) ||
        expected.extra.events!=actual.extra.events || expected.services.events!=actual.services.events || expected.services.traps!=actual.services.traps ||
        expected.index.events!=actual.index.events || expected.host.events!=actual.host.events || expected.unlock.events!=actual.unlock.events)
        throw std::runtime_error("format frame Full72/RAM/live callback mismatch");
    const auto frame=entry==0x82df2ac8u ? 96u:112u;const auto lrslot=entry==0x82df2ac8u ? 16u:24u;
    const auto memory=expected.services.memory;
    if(context.r1.u64!=Stack || context.lr!=0x87654321u || context.r31.u64!=initial.r[31] ||
        context.r3.u64!=0x9988776655443322ull || context.r10.u64!=0x123456789abcdef0ull || context.ctr.u64!=0xaabbccdd87654321ull ||
        memory.ReadU32(Stack-frame)!=static_cast<std::uint32_t>(initial.r[1]) || memory.ReadU32(Stack-lrslot)!=0x87654321u ||
        recovery_abi::ReadU64(memory,Stack-8u)!=initial.r[31] || expected.extra.events.size()!=1u ||
        expected.extra.events[0][1]!=initial.r[1]-frame || expected.extra.events[0][2]!=(entry==0x82df2ac8u ? 0x82df2ae4u:0x82df28a4u) ||
        static_cast<std::uint32_t>(expected.extra.events[0][3])!=Stream+32u)
        throw std::runtime_error("format frame independent LR slot/SP truncation/live high64 absent");
    if(entry!=0x82df2ac8u && (context.r30.u64!=initial.r[30] || recovery_abi::ReadU64(memory,Stack-16u)!=initial.r[30]))
        throw std::runtime_error("format frame r30 save absent");
}
}
void OriginalFrameUnlock(PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);auto& env=*frame61_oracle::active_environment;
    if(!crt_stream_close_shared_lower::ApplyAcceptedLower(0x82b7b7c8u,env.services.memory,env.Deps(),state)) throw std::runtime_error("missing accepted original frame unlock");
    crt_full_oracle::ToPpc(context,state);
}
int main(){try {
    for(auto entry:{0x82df2864u,0x82df2884u,0x82df2ac8u}) frame61_oracle::Check(entry);
    std::puts("PASS crt-format-frame61 3 actual PPC cases");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}

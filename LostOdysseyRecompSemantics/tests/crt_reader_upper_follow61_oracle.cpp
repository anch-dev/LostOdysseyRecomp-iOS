#pragma push_macro("main")
#undef main
#define main UpperFollow61RecursiveFixtureMain
#include "crt_close_recursive_buffer_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_reader_upper_follow61.h"
namespace upper_follow61_oracle {
constexpr GuestAddress Target=0x2a18u,Object=0x56000u,Vtable=0x57000u;
using Event=std::array<std::uint64_t,8>;
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    std::vector<Event> events;
    void CallIndirect(GuestAddress target,GuestMemory& memory,RecursiveFull& state) override {
        events.push_back({target,state.r[1],state.lr,state.r[3],state.r[4],state.ctr,state.fpr_bits[7],state.r[10]});
        if(target!=Target || events.size()!=1u) throw std::runtime_error("unexpected reader size target");
        memory.WriteU32(0x55000u,static_cast<std::uint32_t>(state.r[3]));
        state.r[3]=0xaabbccdd00000010ull;state.r[10]^=0x123456789abcdef0ull;
        state.fpr_bits[7]^=0x180u;state.cr1.gt^=1u;state.cr7.eq^=1u;state.xer_ca^=1u;
    }
};
Guest* active_guest=nullptr;
void Check(GuestAddress entry,bool live) {
    test::GuestWindow original(RecursiveRegions),recovered(RecursiveRegions);
    auto seed=[&](test::GuestWindow& window) {
        RecursiveSeed(window,RecursiveScenario::Empty);auto memory=window.Memory();
        memory.WriteU32(RecursiveReader+16u,live ? Object:0u);
        memory.WriteU32(RecursiveReader+20u,2u);memory.WriteU32(RecursiveReader+24u,live ? 1u:0u);
        memory.WriteU32(RecursiveReader+28u,3u);memory.WriteU32(RecursiveReader+32u,live ? 1u:0u);
        memory.WriteU32(Object,Vtable);memory.WriteU32(Vtable+28u,Target|3u);
    };
    seed(original);seed(recovered);
    auto original_memory=original.Memory();auto recovered_memory=recovered.Memory();
    auto context=RecursiveInitial(RecursiveScenario::Empty);const auto initial=crt_full_oracle::FromPpc(context);auto state=initial;
    Guest expected,actual;recursive_original_memory=&original_memory;active_guest=&expected;
    if(entry==0x82bd1aa8u) __imp__sub_82BD1AA8(context,original.Bytes());else __imp__sub_82BD2030(context,original.Bytes());
    recursive_original_memory=nullptr;active_guest=nullptr;
    if(!crt_reader_upper_follow61::Apply(entry,recovered_memory,actual,state)) throw std::runtime_error("missing upper reader entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events) throw std::runtime_error("upper reader Full72/RAM/callback mismatch");
    if(context.r1.u64!=initial.r[1] || context.lr!=0x81234567u || context.r31.u64!=initial.r[31] ||
        original_memory.ReadU32(Stack-96u)!=static_cast<std::uint32_t>(initial.r[1]) ||
        original_memory.ReadU32(Stack-8u)!=0x81234567u || recovery_abi::ReadU64(original_memory,Stack-16u)!=initial.r[31])
        throw std::runtime_error("upper reader high-SP/backchain/save/LR absent");
    if(entry==0x82bd1aa8u) {
        constexpr auto table=static_cast<std::uint32_t>(std::int64_t(-2113077248)+27700);
        if(context.r3.u64!=initial.r[3] || original_memory.ReadU32(RecursiveReader)!=table || !expected.events.empty())
            throw std::runtime_error("reader constructor high64/vtable absent");
        for(unsigned offset=4u;offset<=32u;offset+=4u)
            if(original_memory.ReadU32(RecursiveReader+offset)!=0u) throw std::runtime_error("reader constructor cleared fields absent");
    } else if(live) {
        if(context.r3.u64!=0xaabbccdd00000024ull || expected.events.size()!=1u || expected.events[0][1]!=initial.r[1]-96u ||
            expected.events[0][2]!=0x82bd2068u || expected.events[0][3]!=Object)
            throw std::runtime_error("reader size high64 callback and count additions absent");
    } else if(context.r3.u64!=0u || !expected.events.empty()) throw std::runtime_error("empty reader size absent");
}
}
void OriginalUpperFollowIndirect(GuestAddress target,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);upper_follow61_oracle::active_guest->CallIndirect(target,*recursive_original_memory,state);crt_full_oracle::ToPpc(context,state);
}
int main() {try {
    upper_follow61_oracle::Check(0x82bd1aa8u,false);upper_follow61_oracle::Check(0x82bd2030u,true);upper_follow61_oracle::Check(0x82bd2030u,false);
    std::puts("PASS crt-reader-upper-follow61 3 actual PPC cases");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}}

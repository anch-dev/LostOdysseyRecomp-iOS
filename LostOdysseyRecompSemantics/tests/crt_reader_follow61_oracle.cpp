#pragma push_macro("main")
#undef main
#define main Follow61RecursiveFixtureMain
#include "crt_close_recursive_buffer_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_reader_follow61.h"
namespace follow61_oracle {
constexpr GuestAddress FreeTarget=0x2a04u,First=0x43000u,Second=0x44000u;
using FollowEvent=std::array<std::uint64_t,9>;
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    bool fail_second;unsigned allocations=0;std::vector<FollowEvent> events;
    explicit Guest(bool failure):fail_second(failure) {}
    void CallIndirect(GuestAddress target,GuestMemory& memory,RecursiveFull& state) override {
        events.push_back({target,state.r[1],state.lr,state.r[3],state.r[4],state.r[5],state.ctr,state.fpr_bits[7],state.r[10]});
        memory.WriteU32(0x55000u+static_cast<GuestAddress>(4u*(events.size()-1u)),static_cast<std::uint32_t>(state.r[4]));
        state.r[10]^=0x123456789abcdef0ull;state.fpr_bits[7]^=0x180u;
        state.cached_fp_control^=0x40u;state.cr1.gt^=1u;state.cr7.eq^=1u;state.xer_ca^=1u;
        if(target==FreeTarget) state.r[3]=0x1234567800000000ull;
        else if(target==RecursiveTarget) {
            ++allocations;
            state.r[3]=allocations==1u ? (0x9988776600000000ull|First) :
                (fail_second ? 0u:(0x7766554400000000ull|Second));
        } else throw std::runtime_error("unexpected reader follow target");
    }
};
Guest* active_guest=nullptr;
void Check(GuestAddress entry,bool fail_second) {
    test::GuestWindow original(RecursiveRegions),recovered(RecursiveRegions);
    auto seed=[](test::GuestWindow& window) {
        RecursiveSeed(window,RecursiveScenario::Empty);auto memory=window.Memory();
        memory.WriteU32(RecursiveVtable+12u,FreeTarget|3u);
        memory.WriteU32(RecursiveReader+4u,RecursiveOldBuffer);
        memory.WriteU32(RecursiveReader+8u,RecursiveNewBuffer);
        memory.WriteU8(RecursiveReader+20u,1u);
    };
    seed(original);seed(recovered);
    auto original_memory=original.Memory();auto recovered_memory=recovered.Memory();
    auto context=RecursiveInitial(RecursiveScenario::Empty);context.r4.u64=0xaabbccdd00000002ull;
    const auto initial=crt_full_oracle::FromPpc(context);auto state=initial;
    Guest expected(fail_second),actual(fail_second);
    recursive_original_memory=&original_memory;active_guest=&expected;
    switch(entry) {case 0x82bd1af8u:__imp__sub_82BD1AF8(context,original.Bytes());break;
        case 0x82bd2c78u:__imp__sub_82BD2C78(context,original.Bytes());break;
        default:__imp__sub_82BD2D08(context,original.Bytes());}
    recursive_original_memory=nullptr;active_guest=nullptr;
    if(!crt_reader_follow61::Apply(entry,recovered_memory,actual,state)) throw std::runtime_error("missing reader follow entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events)
        throw std::runtime_error("reader follow Full72/RAM/callback mismatch");
    const auto frame=entry==0x82bd1af8u ? 96u:112u;
    const auto first_saved=entry==0x82bd1af8u ? 31u:entry==0x82bd2c78u ? 30u:29u;
    if(context.r1.u64!=initial.r[1] || context.lr!=0x81234567u ||
        original_memory.ReadU32(Stack-frame)!=static_cast<std::uint32_t>(initial.r[1]) ||
        original_memory.ReadU32(Stack-8u)!=0x81234567u)
        throw std::runtime_error("reader follow high-SP/backchain/LR absent");
    for(unsigned i=first_saved;i<=31u;++i)
        if(state.r[i]!=initial.r[i] || recovery_abi::ReadU64(original_memory,Stack-8u*(33u-i))!=initial.r[i])
            throw std::runtime_error("reader follow save slots absent");
    const auto count=entry==0x82bd1af8u ? 1u:entry==0x82bd2c78u ? 2u:4u;
    if(expected.events.size()!=count) throw std::runtime_error("reader follow actual route absent");
    for(const auto& event:expected.events)
        if(event[1]!=initial.r[1]-frame) throw std::runtime_error("reader follow callback high-SP absent");
    if(entry==0x82bd1af8u) {
        if(original_memory.ReadU32(RecursiveReader+4u)!=0u || context.r3.u64!=0x1234567800000000ull ||
            expected.events[0][2]!=0x82bd1b30u || expected.events[0][4]!=RecursiveOldBuffer)
            throw std::runtime_error("single reader cleanup high-return absent");
    } else {
        const auto lr1=entry==0x82bd2c78u ? 0x82bd2cc4u:0x82bd2d50u;
        const auto lr2=entry==0x82bd2c78u ? 0x82bd2cecu:0x82bd2d78u;
        if(expected.events[0][2]!=lr1 || expected.events[0][4]!=RecursiveNewBuffer ||
            expected.events[1][2]!=lr2 || expected.events[1][4]!=RecursiveOldBuffer)
            throw std::runtime_error("owned reader free order/LR absent");
        if(entry==0x82bd2c78u) {
            if(original_memory.ReadU32(RecursiveReader+4u)!=0u || original_memory.ReadU32(RecursiveReader+8u)!=0u || context.r3.u64!=0x1234567800000000ull)
                throw std::runtime_error("owned reader cleanup absent");
        } else if(expected.allocations!=2u || expected.events[2][2]!=0x82bd2d9cu || expected.events[3][2]!=0x82bd2dd0u ||
            expected.events[2][4]!=8u || expected.events[3][4]!=8u || expected.events[2][5]!=72u || expected.events[3][5]!=73u ||
            original_memory.ReadU32(RecursiveReader+4u)!=First || original_memory.ReadU32(RecursiveReader+8u)!=(fail_second ? 0u:Second) ||
            context.r3.u64!=(fail_second ? 0u:1u)) throw std::runtime_error("reader reallocation type/count/failure absent");
    }
}
}
void OriginalFollowIndirect(GuestAddress target,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);follow61_oracle::active_guest->CallIndirect(target,*recursive_original_memory,state);crt_full_oracle::ToPpc(context,state);
}
int main() {try {
    follow61_oracle::Check(0x82bd1af8u,false);follow61_oracle::Check(0x82bd2c78u,false);
    follow61_oracle::Check(0x82bd2d08u,false);follow61_oracle::Check(0x82bd2d08u,true);
    std::puts("PASS crt-reader-follow61 4 actual PPC cases");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}}

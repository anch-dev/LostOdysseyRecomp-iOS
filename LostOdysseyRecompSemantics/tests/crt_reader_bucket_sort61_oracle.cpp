#pragma push_macro("main")
#undef main
#define main BucketSortReaderFixtureMain
#include "crt_close_reader_callers_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_reader_bucket_sort61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include <algorithm>
#include <bit>
namespace bucket61_oracle {
using namespace reader_callers_oracle;
constexpr GuestAddress FreeTarget=0x2a04u;
enum class Route {UnsignedAllocation,SignedAllocation,SortedReuse,ZeroCount};
using SortEvent=std::array<std::uint64_t,9>;
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    unsigned allocations=0;std::vector<SortEvent> events;
    void CallIndirect(GuestAddress target,GuestMemory& memory,Full& state) override {
        events.push_back({target,state.r[1],state.lr,state.r[3],state.r[4],state.r[5],state.ctr,state.fpr_bits[7],state.r[10]});
        memory.WriteU32(0x53000u+static_cast<GuestAddress>(4u*(events.size()-1u)),static_cast<std::uint32_t>(state.r[4]));
        state.fpr_bits[7]^=0x180u;state.cached_fp_control^=0x40u;
        state.cr1.gt^=1u;state.cr7.eq^=1u;state.xer_ca^=1u;state.r[10]^=0x123456789abcdef0ull;
        if(target==FreeTarget) state.r[3]=0x1234567800000000ull;
        else if(target==Target && allocations<2u) {++allocations;state.r[3]=allocations==1u ? (0x9988776600000000ull|NewNode):(0x7766554400000000ull|NewData);}
        else throw std::runtime_error("unexpected sort allocator target");
    }
};
struct Environment {
    Services services;IndexService index;Host host;wrapper_oracle::WrapperUnlock unlock;
    close_shared_oracle::SharedExtra extra;Guest guest;
    explicit Environment(GuestWindow& window):services(window,Mode::LockedWrite),host(Scenario::BinarySuccess) {}
    crt_close_reader_callers_context::Dependencies Deps() {return {guest,close_shared_oracle::Deps(services,index,host,unlock,extra)};}
};
Environment* active_environment=nullptr;
std::array<std::uint32_t,6> Keys(Route route) {
    if(route==Route::SortedReuse) return {0u,1u,1u,0x1234u,0x80000000u,0xffffffffu};
    return {0x80000002u,0xffffffffu,0u,0x7fffffffu,1u,0x80000002u};
}
void Seed(GuestWindow& window,Route route) {
    reader_callers_oracle::Seed(window);auto memory=window.Memory();
    memory.WriteU32(Vtable+12u,FreeTarget|3u);
    memory.WriteU32(Reader,route==Route::SortedReuse ? 0x80000006u:2u);
    memory.WriteU32(Reader+4u,OldNode);memory.WriteU32(Reader+8u,OldData);
    memory.WriteU32(Reader+12u,0u);memory.WriteU32(Reader+16u,0u);memory.WriteU8(Reader+20u,1u);
    const auto keys=Keys(route);
    for(unsigned i=0;i!=keys.size();++i) {memory.WriteU32(Source+4u*i,keys[i]);memory.WriteU32(OldNode+4u*i,i);}
    recovery_abi::WriteU64(memory,Stack-4096u,0x1020304050607080ull);
}
void Check(Route route) {
    GuestWindow original(ReaderRegions),recovered(ReaderRegions);Seed(original,route);Seed(recovered,route);
    Environment expected(original),actual(recovered);
    auto context=reader_callers_oracle::Initial(reader_callers_oracle::Route::CopySuccess);
    context.r4.u64=0x9988776600000000ull|Source;context.r5.u64=route==Route::ZeroCount ? 0u:0xaabbccdd00000006ull;
    context.r6.u64=route==Route::SignedAllocation ? 0u:1u;
    const auto initial=crt_full_oracle::FromPpc(context);auto state=initial;
    current=&expected.services;active=&expected.services;current_index=&expected.index;current_host=&expected.host;
    wrapper_oracle::original_unlock=&expected.unlock;close_shared_oracle::active_extra=&expected.extra;active_environment=&expected;
    __imp__sub_82BD2DF0(context,original.Bytes());
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;wrapper_oracle::original_unlock=nullptr;close_shared_oracle::active_extra=nullptr;active_environment=nullptr;
    if(!crt_reader_bucket_sort61::Apply(0x82bd2df0u,actual.services.memory,actual.Deps(),state)) throw std::runtime_error("missing bucket sort entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) || !original.EqualCommitted(recovered) ||
        expected.guest.events!=actual.guest.events || expected.services.events!=actual.services.events || expected.services.traps!=actual.services.traps ||
        expected.index.events!=actual.index.events || expected.host.events!=actual.host.events || expected.unlock.events!=actual.unlock.events || expected.extra.events!=actual.extra.events)
        throw std::runtime_error("bucket sort Full72/RAM/callback mismatch");
    const auto memory=expected.services.memory;
    if(context.r3.u64!=initial.r[3] || context.r1.u64!=initial.r[1] || context.lr!=0x87654321u ||
        memory.ReadU32(Stack-5280u)!=static_cast<std::uint32_t>(initial.r[1]) || memory.ReadU32(Stack-8u)!=0x87654321u)
        throw std::runtime_error("bucket sort large frame/stack probe/high-SP/LR absent");
    for(unsigned i=23u;i<=31u;++i)
        if(state.r[i]!=initial.r[i] || recovery_abi::ReadU64(memory,Stack-8u*(33u-i))!=initial.r[i]) throw std::runtime_error("bucket sort save slots absent");
    if(route==Route::ZeroCount) {
        if(!expected.guest.events.empty() || memory.ReadU32(Reader+12u)!=0u) throw std::runtime_error("bucket sort zero guard absent");return;
    }
    const auto keys=Keys(route);std::array<std::uint32_t,6> permutation{0u,1u,2u,3u,4u,5u};
    std::stable_sort(permutation.begin(),permutation.end(),[&](auto left,auto right) {
        if(route==Route::SignedAllocation) return std::bit_cast<std::int32_t>(keys[left])<std::bit_cast<std::int32_t>(keys[right]);
        return keys[left]<keys[right];
    });
    const auto ranks=memory.ReadU32(Reader+4u);
    for(unsigned i=0;i!=permutation.size();++i)
        if(memory.ReadU32(ranks+4u*i)!=permutation[i] || memory.ReadU32(Source+4u*i)!=keys[i])
            throw std::runtime_error("independent stable permutation/keys mismatch");
    if(memory.ReadU32(Reader+12u)!=1u) throw std::runtime_error("bucket sort invocation count absent");
    const bool allocation=route==Route::UnsignedAllocation || route==Route::SignedAllocation;
    if(expected.guest.events.size()!=(allocation ? 4u:0u)) throw std::runtime_error("bucket sort allocation path absent");
    if(allocation) {
        constexpr std::array<GuestAddress,4> returns{0x82bd2d50u,0x82bd2d78u,0x82bd2d9cu,0x82bd2dd0u};
        for(unsigned i=0;i!=4u;++i) if(expected.guest.events[i][1]!=initial.r[1]-5392u || expected.guest.events[i][2]!=returns[i])
            throw std::runtime_error("bucket sort nested allocation frame/LR absent");
        if(expected.guest.events[2][4]!=24u || expected.guest.events[3][4]!=24u || expected.guest.events[2][5]!=72u || expected.guest.events[3][5]!=73u)
            throw std::runtime_error("bucket sort allocation size/type absent");
    } else if(memory.ReadU32(Reader+16u)!=1u) throw std::runtime_error("bucket sort already-sorted route absent");
}
}
void OriginalSortAllocation(PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);auto& env=*bucket61_oracle::active_environment;
    if(!crt_reader_follow61::Apply(0x82bd2d08u,env.services.memory,env.guest,state)) throw std::runtime_error("missing accepted original sort allocation");
    crt_full_oracle::ToPpc(context,state);
}
int main(){try {
    for(auto route:{bucket61_oracle::Route::UnsignedAllocation,bucket61_oracle::Route::SignedAllocation,bucket61_oracle::Route::SortedReuse,bucket61_oracle::Route::ZeroCount}) bucket61_oracle::Check(route);
    std::puts("PASS crt-reader-bucket-sort61 4 actual PPC cases");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}

#pragma push_macro("main")
#undef main
#define main Units61RecursiveFixtureMain
#include "crt_close_recursive_buffer_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_reader_units61.h"
namespace units61_oracle {
constexpr GuestAddress Source=0x52000u;
void Check(GuestAddress entry) {
    const bool growth=entry==0x82bd1050u;
    const bool consume=entry==0x82bd09c0u || entry==0x82bd09f8u;
    const auto scenario=growth ? RecursiveScenario::Growth : RecursiveScenario::Empty;
    test::GuestWindow original(RecursiveRegions),recovered(RecursiveRegions);
    auto seed=[&](test::GuestWindow& window) {
        RecursiveSeed(window,scenario);auto memory=window.Memory();
        memory.WriteU8(RecursiveReader+24u,consume ? 7u : 0u);
        for(unsigned i=0;i!=4u;++i) memory.WriteU8(RecursiveOldBuffer+i,static_cast<std::uint8_t>(0x78u-0x22u*i));
        for(unsigned i=0;i!=3u;++i) memory.WriteU8(Source+i,static_cast<std::uint8_t>("XYZ"[i]));
    };
    seed(original);seed(recovered);
    auto original_memory=original.Memory();auto recovered_memory=recovered.Memory();
    auto context=RecursiveInitial(RecursiveScenario::Empty);
    context.r4.u64=0xaabbccdd12345678ull;
    if(entry==0x82bd1160u) {context.r4.u64=0xaabbccdd00000000ull|Source;context.r5.u64=0x8877665500000003ull;}
    const auto initial=crt_full_oracle::FromPpc(context);auto state=initial;
    RecursiveGuest expected(scenario),actual(scenario);
    recursive_original_memory=&original_memory;recursive_guest_current=&expected;
    switch(entry) {
    case 0x82bd1050u:__imp__sub_82BD1050(context,original.Bytes());break;
    case 0x82bd1160u:__imp__sub_82BD1160(context,original.Bytes());break;
    case 0x82bd09c0u:__imp__sub_82BD09C0(context,original.Bytes());break;
    default:__imp__sub_82BD09F8(context,original.Bytes());
    }
    recursive_original_memory=nullptr;recursive_guest_current=nullptr;
    if(!crt_reader_units61::Apply(entry,recovered_memory,actual,state)) throw std::runtime_error("missing reader unit entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events)
        throw std::runtime_error("reader units Full72/RAM/callback mismatch");
    if(context.r1.u64!=initial.r[1]) throw std::runtime_error("reader units high-SP absent");
    if(consume) {
        const auto width=entry==0x82bd09c0u ? 1u:4u;
        if(context.r3.u64!=(width==1u ? 0x78ull:0x78563412ull) || context.lr!=initial.lr ||
            original_memory.ReadU32(RecursiveOldNode+4u)!=width ||
            original_memory.ReadU32(RecursiveReader+16u)!=RecursiveOldBuffer ||
            original_memory.ReadU8(RecursiveReader+24u)!=0u || !expected.events.empty())
            throw std::runtime_error("reader unit consumption result absent");
        return;
    }
    const auto frame=growth ? 112u:128u;
    const auto first=growth ? 30u:28u;
    if(context.r3.u64!=initial.r[3] || context.lr!=0x81234567u ||
        original_memory.ReadU32(Stack-frame)!=static_cast<std::uint32_t>(initial.r[1]) ||
        original_memory.ReadU32(Stack-8u)!=0x81234567u)
        throw std::runtime_error("reader writer high64/SP/backchain/LR absent");
    for(unsigned i=first;i<=31u;++i)
        if(state.r[i]!=initial.r[i] || recovery_abi::ReadU64(original_memory,Stack-8u*(33u-i))!=initial.r[i])
            throw std::runtime_error("reader unit nonvolatile save slots absent");
    if(growth) {
        if(expected.events.size()!=2u || expected.events[0][1]!=initial.r[1]-240u || expected.events[0][2]!=0x82bd080cu || expected.events[1][2]!=0x82bd0868u ||
            original_memory.ReadU32(RecursiveReader)!=RecursiveNewNode || original_memory.ReadU32(RecursiveNewNode+4u)!=4u ||
            original_memory.ReadU32(RecursiveReader+16u)!=RecursiveNewBuffer || original_memory.ReadU32(RecursiveNewBuffer)!=0x12345678u)
            throw std::runtime_error("U32 writer live allocation/write absent");
    } else {
        if(!expected.events.empty() || original_memory.ReadU32(RecursiveOldNode+4u)!=3u ||
            original_memory.ReadU32(RecursiveReader+16u)!=RecursiveOldBuffer+2u ||
            original_memory.ReadU8(RecursiveOldBuffer)!='X' || original_memory.ReadU8(RecursiveOldBuffer+1u)!='Y' || original_memory.ReadU8(RecursiveOldBuffer+2u)!='Z')
            throw std::runtime_error("reader byte loop absent");
    }
}
}
int main() {try {
    for(auto entry:{0x82bd1050u,0x82bd1160u,0x82bd09c0u,0x82bd09f8u}) units61_oracle::Check(entry);
    std::puts("PASS crt-reader-units61 4 actual PPC cases");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}}

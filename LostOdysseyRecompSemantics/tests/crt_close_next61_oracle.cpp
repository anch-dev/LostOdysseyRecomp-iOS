#pragma push_macro("main")
#undef main
#define main Next61RecursiveFixtureMain
#include "crt_close_recursive_buffer_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_close_next61.h"
namespace next61_oracle {
void Check(GuestAddress entry,bool growth) {
    test::GuestWindow original(RecursiveRegions),recovered(RecursiveRegions);
    const auto scenario=growth ? RecursiveScenario::Growth : RecursiveScenario::Empty;
    auto seed=[&](test::GuestWindow& window) {
        RecursiveSeed(window,scenario); auto memory=window.Memory();
        memory.WriteU8(RecursiveReader+24u,0u);
        memory.WriteU32(RecursiveReader+4u,RecursiveOldNode);
        if(entry==0x82bd0900u) {
            memory.WriteU32(RecursiveOldNode+4u,0xfffffffeu);
            memory.WriteU32(RecursiveOldNode+12u,RecursiveNewNode);
            memory.WriteU32(RecursiveNewNode+4u,5u);
            memory.WriteU32(RecursiveNewNode+12u,0u);
        }
    };
    seed(original);seed(recovered);
    auto original_memory=original.Memory(); auto recovered_memory=recovered.Memory();
    auto context=RecursiveInitial(RecursiveScenario::Empty);
    context.r4.u64=0xdeadbeef12345678ull;
    const auto initial=crt_full_oracle::FromPpc(context); auto state=initial;
    RecursiveGuest expected(scenario),actual(scenario);
    recursive_original_memory=&original_memory; recursive_guest_current=&expected;
    switch(entry) {
    case 0x82bd0900u: __imp__sub_82BD0900(context,original.Bytes());break;
    case 0x82bd0938u: __imp__sub_82BD0938(context,original.Bytes());break;
    default: __imp__sub_82BD0FC8(context,original.Bytes());
    }
    recursive_original_memory=nullptr;recursive_guest_current=nullptr;
    if(!crt_close_next61::Apply(entry,recovered_memory,actual,state)) throw std::runtime_error("missing next reader entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
       !original.EqualCommitted(recovered) || expected.events!=actual.events)
        throw std::runtime_error("next reader Full72/RAM/callback mismatch");
    if(context.r1.u64!=initial.r[1]) throw std::runtime_error("next reader high SP absent");
    if(entry==0x82bd0900u) {
        if(context.r3.u64!=0x100000003ull || context.lr!=initial.lr || !expected.events.empty())
            throw std::runtime_error("reader node sum high64 absent");
        return;
    }
    const auto width=entry==0x82bd0938u ? 1u : 2u;
    const auto node=growth ? RecursiveNewNode : RecursiveOldNode;
    const auto buffer=growth ? RecursiveNewBuffer : RecursiveOldBuffer;
    const auto offset=0u;
    if(context.r3.u64!=initial.r[3] || context.lr!=0x81234567u ||
       context.r30.u64!=initial.r[30] || context.r31.u64!=initial.r[31] ||
       original_memory.ReadU32(Stack-112u)!=static_cast<std::uint32_t>(initial.r[1]) ||
       original_memory.ReadU32(Stack-8u)!=0x81234567u ||
       recovery_abi::ReadU64(original_memory,Stack-24u)!=initial.r[30] ||
       recovery_abi::ReadU64(original_memory,Stack-16u)!=initial.r[31] ||
       original_memory.ReadU32(RecursiveReader)!=node || original_memory.ReadU32(node+4u)!=offset+width ||
       original_memory.ReadU32(RecursiveReader+16u)!=buffer+offset)
        throw std::runtime_error("reader append save/LR/pointer outcome absent");
    if((width==1u && original_memory.ReadU8(buffer+offset)!=0x78u) ||
       (width==2u && original_memory.ReadU16(buffer+offset)!=0x5678u) ||
       expected.events.size()!=(growth ? 2u : 0u)) throw std::runtime_error("reader append write/growth absent");
    if(growth && (expected.events[0][1]!=initial.r[1]-240u ||
        expected.events[0][2]!=0x82bd080cu || expected.events[1][2]!=0x82bd0868u))
        throw std::runtime_error("reader allocation callback SP/LR absent");
}
}
int main() {
    try {
        next61_oracle::Check(0x82bd0900u,false);
        next61_oracle::Check(0x82bd0938u,false);
        next61_oracle::Check(0x82bd0938u,true);
        next61_oracle::Check(0x82bd0fc8u,true);
        std::puts("PASS crt-close-next61 4 actual PPC cases");return 0;
    } catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

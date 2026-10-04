#pragma push_macro("main")
#undef main
#define main ObjectChainReaderFixtureMain
#include "crt_close_reader_callers_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_reader_object_chain61.h"
#include "lo_semantics/crt_close_upper61.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_close_next61.h"
namespace chain61_oracle {
using namespace reader_callers_oracle;
constexpr GuestAddress FreeTarget=0x2a04u, ReadTarget=0x2a08u, CountTarget=0x2a0cu, WriteTarget=0x2a10u;
constexpr GuestAddress Object=0x5c000u,ObjectTable=0x5d000u,Sink=0x5e000u,SinkTable=0x5f000u,Flat=0x60000u;
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    bool chain; unsigned allocations=0; std::vector<reader_callers_oracle::Event> events;
    explicit Guest(bool selected):chain(selected) {}
    void CallIndirect(GuestAddress target,GuestMemory& memory,Full& state) override {
        events.push_back({target,state.r[1],state.lr,state.r[3],state.r[4],state.r[5],state.ctr,state.fpr_bits[7],state.r[10]});
        state.r[10]^=0x123456789abcdef0ull;state.fpr_bits[7]^=0x180u;
        state.cr1.gt^=1u;state.cr7.eq^=1u;state.xer_ca^=1u;
        if(target==Target) {
            ++allocations; const auto result=chain ? (allocations==1u ? NewNode : allocations==2u ? NewData : Flat) : Flat;
            state.r[3]=0xaabbccdd00000000ull|result;
        } else if(target==ReadTarget) {
            const auto reader=static_cast<GuestAddress>(state.r[5]);
            const auto node=memory.ReadU32(reader);const auto data=memory.ReadU32(node);
            memory.WriteU32(node+4u,4u);
            for(unsigned i=0;i!=4u;++i) memory.WriteU8(data+i,static_cast<std::uint8_t>("DATA"[i]));
            state.r[3]=0x1122334455667788ull;
        } else if(target==CountTarget) {
            memory.WriteU32(0x53000u,static_cast<std::uint32_t>(state.r[4]));state.r[3]=0x8877665544332211ull;
        } else if(target==WriteTarget) {
            if(state.r[5]!=4u) throw std::runtime_error("unexpected serialized count");
            for(unsigned i=0;i!=4u;++i) memory.WriteU8(0x53100u+i,memory.ReadU8(static_cast<GuestAddress>(state.r[4])+i));
            state.r[3]=0x6655443322110099ull;
        } else if(target==FreeTarget) {state.r[3]=0x1234567800000000ull;}
        else throw std::runtime_error("unexpected object-chain target");
    }
};
struct Environment {
    Services services;IndexService index;Host host;wrapper_oracle::WrapperUnlock unlock;
    close_shared_oracle::SharedExtra extra;Guest guest;
    Environment(GuestWindow& window,bool chain):services(window,Mode::LockedWrite),host(Scenario::BinarySuccess),guest(chain) {}
    crt_close_reader_callers_context::Dependencies Deps() {return {guest,close_shared_oracle::Deps(services,index,host,unlock,extra)};}
};
Environment* active_environment=nullptr;
void Seed(GuestWindow& window) {
    reader_callers_oracle::Seed(window);auto memory=window.Memory();
    memory.WriteU32(Vtable+12u,FreeTarget|1u);
    memory.WriteU32(Object,ObjectTable);memory.WriteU32(ObjectTable+24u,ReadTarget|2u);
    memory.WriteU32(Sink,SinkTable);memory.WriteU32(SinkTable+36u,CountTarget|3u);memory.WriteU32(SinkTable+48u,WriteTarget|1u);
    memory.WriteU8(Reader+24u,0u);memory.WriteU32(Reader+4u,OldNode);
    memory.WriteU32(Reader+8u,0u);memory.WriteU32(OldNode+12u,0u);memory.WriteU32(OldNode+4u,4u);
}
bool Lower(GuestAddress entry,GuestMemory& memory,crt_close_reader_callers_context::Dependencies deps,Full& state) {
    switch(entry) {
    case 0x82bd0cd0u:return crt_close_upper61::Apply(entry,memory,deps,state);
    case 0x82bd0df0u:return crt_reader_cleanup_callers_context::Apply(entry,memory,deps.guest,state);
    case 0x82bd0900u:return crt_close_next61::Apply(entry,memory,deps.guest,state);
    case 0x82b7a0b0u:return crt_copy_full_context::Apply(entry,memory,state);
    default:return crt_close_recursive_buffer_context::Apply(entry,memory,deps.guest,state);
    }
}
void Check(GuestAddress entry,bool endian) {
    GuestWindow original(ReaderRegions),recovered(ReaderRegions);Seed(original);Seed(recovered);
    Environment expected(original,entry==0x82bb44a8u),actual(recovered,entry==0x82bb44a8u);
    auto context=reader_callers_oracle::Initial(Route::CopySuccess);
    context.r4.u64=0u;
    if(entry==0x82bada00u) {context.r3.u64=0xaabbccdd12345678ull;context.r4.u64=endian ? 1u:0u;context.r5.u64=0x9988776600000000ull|Sink;}
    if(entry==0x82bb44a8u) {context.r3.u64=0xaabbccdd00000000ull|Object;context.r4.u64=endian ? 1u:0u;context.r5.u64=0x9988776600000000ull|Sink;}
    const auto initial=crt_full_oracle::FromPpc(context);auto state=initial;
    current=&expected.services;active=&expected.services;current_index=&expected.index;current_host=&expected.host;
    wrapper_oracle::original_unlock=&expected.unlock;close_shared_oracle::active_extra=&expected.extra;active_environment=&expected;
    switch(entry) {case 0x82bd0ea8u:__imp__sub_82BD0EA8(context,original.Bytes());break;case 0x82bada00u:__imp__sub_82BADA00(context,original.Bytes());break;default:__imp__sub_82BB44A8(context,original.Bytes());}
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;wrapper_oracle::original_unlock=nullptr;close_shared_oracle::active_extra=nullptr;active_environment=nullptr;
    if(!crt_reader_object_chain61::Apply(entry,actual.services.memory,actual.Deps(),state)) throw std::runtime_error("missing object-chain entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) || !original.EqualCommitted(recovered) ||
       expected.guest.events!=actual.guest.events || expected.services.events!=actual.services.events || expected.services.traps!=actual.services.traps ||
       expected.index.events!=actual.index.events || expected.host.events!=actual.host.events || expected.unlock.events!=actual.unlock.events || expected.extra.events!=actual.extra.events)
        throw std::runtime_error("object chain Full72/RAM/callback mismatch");
    const auto memory=expected.services.memory;
    if(context.r1.u64!=initial.r[1] || context.lr!=0x87654321u || memory.ReadU32(Stack-8u)!=0x87654321u)
        throw std::runtime_error("object chain high-SP/LR return absent");
    const auto frame=entry==0x82bada00u ? 96u:144u;
    if(memory.ReadU32(Stack-frame)!=static_cast<std::uint32_t>(initial.r[1])) throw std::runtime_error("object-chain backchain absent");
    if(entry==0x82bada00u) {
        if(expected.guest.events.size()!=1u || expected.guest.events[0][1]!=initial.r[1]-96u || expected.guest.events[0][2]!=0x82bada5cu ||
           expected.guest.events[0][4]!=(endian ? 0x78563412ull:initial.r[3]) || context.r3.u64!=0x8877665544332211ull)
            throw std::runtime_error("endian live callback absent");
    } else if(entry==0x82bd0ea8u) {
        for(unsigned i=26u;i<=31u;++i) if(state.r[i]!=initial.r[i] || recovery_abi::ReadU64(memory,Stack-8u*(33u-i))!=initial.r[i]) throw std::runtime_error("flatten save slots absent");
        if(expected.guest.events.size()!=1u || expected.guest.events[0][1]!=initial.r[1]-144u || expected.guest.events[0][2]!=0x82bd0f50u || context.r3.u64!=(0xaabbccdd00000000ull|Flat) ||
           memory.ReadU32(Reader+8u)!=Flat || memory.ReadU16(Reader+20u)!=0u || memory.ReadU32(Flat)!=0x4f4c4421u)
            throw std::runtime_error("flatten allocation/copy high64 absent");
    } else {
        if(expected.guest.allocations!=3u || expected.guest.events.size()!=9u || memory.ReadU32(0x53000u)!=4u || memory.ReadU32(0x53100u)!=0x44415441u)
            throw std::runtime_error("complete reader serialization/copy/cleanup chain absent");
        for(unsigned i=29u;i<=31u;++i) if(state.r[i]!=initial.r[i] || recovery_abi::ReadU64(memory,Stack-8u*(33u-i))!=initial.r[i])
            throw std::runtime_error("object-chain saved nonvolatile absent");
    }
}
}
void OriginalChainDirect(GuestAddress entry,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);auto& env=*chain61_oracle::active_environment;
    if(!chain61_oracle::Lower(entry,env.services.memory,env.Deps(),state)) throw std::runtime_error("missing original accepted chain lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalChainIndirect(GuestAddress target,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);auto& env=*chain61_oracle::active_environment;
    env.guest.CallIndirect(target,env.services.memory,state);crt_full_oracle::ToPpc(context,state);
}
int main() {try {
    chain61_oracle::Check(0x82bada00u,false);chain61_oracle::Check(0x82bada00u,true);
    chain61_oracle::Check(0x82bd0ea8u,false);chain61_oracle::Check(0x82bb44a8u,false);
    std::puts("PASS crt-reader-object-chain61 4 actual PPC cases");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}}

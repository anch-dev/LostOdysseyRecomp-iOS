#define main ReaderChainRefillFixtureMain
#include "crt_stream_refill_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_reader_chain61.h"
namespace reader_chain_oracle {
namespace family=crt_reader_chain61;
enum class Route {WideMatch,ConvertSuccess,ConvertError};
struct Case {const char* name;Route route;};
constexpr std::array Cases{Case{"wide-prior-code-unit-match",Route::WideMatch},
    Case{"actual-tail-conversion-byte-success",Route::ConvertSuccess},
    Case{"actual-tail-conversion-fill-errno42",Route::ConvertError}};
family::Dependencies* original_dependencies=nullptr;
void Seed(GuestWindow& window,Route route) {
    refill_oracle::SeedRefill(window);auto memory=window.Memory();
    memory.WriteU32(Stream+12u,1u);memory.WriteU32(Stream+16u,5u);
    memory.WriteU32(Stream+8u,Buffer);memory.WriteU32(Stream+24u,4u);
    memory.WriteU32(Stream,route==Route::WideMatch?Buffer+2u:Buffer+1u);
    memory.WriteU32(Stream+4u,0u);memory.WriteU8(Buffer,'A');memory.WriteU8(Buffer+1u,'B');
    memory.WriteU8(Record+4u,route==Route::WideMatch?1u:0x81u);
    memory.WriteU8(Record+40u,0u);
}
void Check(const Case& item) {
    GuestWindow original(OpenRegions),recovered(OpenRegions);Seed(original,item.route);Seed(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;refill_oracle::RefillHost expected_host,actual_host;
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    auto expected_deps=refill_oracle::Deps(expected,expected_index,expected_host,expected_unlock,expected_extra);
    auto actual_deps=refill_oracle::Deps(actual,actual_index,actual_host,actual_unlock,actual_extra);
    auto context=refill_oracle::InitialRefill(refill_oracle::RefillRoute::Binary);
    context.r1.u64=0x8877665500000000ull|Stack;context.lr=0x1234567887654321ull;
    context.r3.u64=0x1122334400000000ull|(item.route==Route::WideMatch?0x4142u:
        item.route==Route::ConvertSuccess?0x41u:0x100u);
    context.r4.u64=0xaabbccdd00000000ull|Stream;
    const auto character=context.r3.u64;
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_dependencies=&expected_deps;
    __imp__sub_82358B30(context,original.Bytes());active=nullptr;original_dependencies=nullptr;
    if(!family::Apply(0x82358b30u,actual.memory,actual_deps,state))throw std::runtime_error("missing wide pushback");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||expected.events!=actual.events||
       expected_index.events!=actual_index.events||expected_host.events!=actual_host.events||
       expected_unlock.events!=actual_unlock.events||expected_extra.events!=actual_extra.events||
       expected.traps!=actual.traps||expected.converted_errors!=actual.converted_errors) {
        for(unsigned i=0;i<before.size();++i)if(before[i]!=after[i])std::fprintf(stderr,"chain state[%u] %llx/%llx\n",i,
            static_cast<unsigned long long>(before[i]),static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("reader chain Full72/RAM/callback mismatch");
    }
    if(item.route==Route::ConvertError) {
        if(context.r3.u32!=UINT32_MAX||expected.memory.ReadU32(Stream)!=Buffer+1u||
           expected.memory.ReadU32(Stream+4u)!=0u||
           expected.memory.ReadU32(0x83215210u)!=42u||
           expected.memory.ReadU32(Stack-80u)!=UINT32_MAX)
            throw std::runtime_error("conversion rejection and output sentinel missed");
        for(unsigned i=0;i<5u;++i)if(expected.memory.ReadU8(Stack-76u+i)!=0u)
            throw std::runtime_error("actual fill before errno missed");
    } else if(context.r3.u64!=character||expected.memory.ReadU32(Stream)!=Buffer||
        expected.memory.ReadU32(Stream+4u)!=(item.route==Route::WideMatch?2u:1u)||
        expected.memory.ReadU8(Buffer)!='A')
        throw std::runtime_error("wide pushback success or full return value missed");
    if(!expected_host.events.empty()||!expected_extra.events.empty())
        throw std::runtime_error("unexpected wide pushback native boundary");
}
}
void OriginalReaderChainLower(GuestAddress entry,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,active->memory,
        *reader_chain_oracle::original_dependencies,state))
        throw std::runtime_error("missing original wide pushback lower");
    crt_full_oracle::ToPpc(context,state);
}
int main() {
    for(const auto& item:reader_chain_oracle::Cases)try{reader_chain_oracle::Check(item);}
        catch(const std::exception& e){std::fprintf(stderr,"%s: %s\n",item.name,e.what());return 1;}
    std::puts("CRT reader chain: 3 focused actual PPC cases passed");return 0;
}

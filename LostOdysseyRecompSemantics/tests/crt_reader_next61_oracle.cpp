#define main NextReaderRefillFixtureMain
#include "crt_stream_refill_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_reader_next61.h"
namespace next_reader_oracle {
namespace family=crt_reader_next61;
constexpr GuestAddress Output=0x55000u;
enum class Route {NullOutput,Line,Eof};
struct Case {const char* name;Route route;};
constexpr std::array Cases{Case{"null-output-errno22",Route::NullOutput},
    Case{"locked-buffered-newline-success",Route::Line},
    Case{"locked-actual-byte-reader-eof",Route::Eof}};
struct Extra final : close_shared_oracle::SharedExtra {
    void LeaveCriticalSection(GuestMemory&,crt_stream_bulk_close_routes::Registers& state) override {
        if(state.lr!=0x82b7b584u||static_cast<GuestAddress>(state.r[3])!=Stream+32u||
            static_cast<GuestAddress>(state.sp)!=Stack-288u)
            throw std::runtime_error("next reader tail unlock ABI");
        events.push_back({3u,state.sp,state.lr,state.r[3],state.r[10],state.r[31]});
        state.r[10]=0x1122334400000055ull;
    }
};
family::Dependencies* original_dependencies=nullptr;
void Seed(GuestWindow& window,Route route) {
    refill_oracle::SeedRefill(window);auto memory=window.Memory();
    memory.WriteU32(Stream+12u,route==Route::Line?0x40u:9u);
    memory.WriteU32(Stream+16u,0xfffffffeu);
    memory.WriteU32(Stream+8u,Buffer);memory.WriteU32(Stream+24u,4u);
    memory.WriteU32(Stream,Buffer);memory.WriteU32(Stream+4u,route==Route::Line?3u:0u);
    memory.WriteU8(Buffer,'A');memory.WriteU8(Buffer+1u,10u);memory.WriteU8(Buffer+2u,'C');
    memory.WriteU8(0x8321531cu,0u);memory.WriteU8(0x83215340u,0u);
    for(unsigned i=0;i<8u;++i)memory.WriteU8(Output+i,0x55u);
}
void Check(const Case& item) {
    GuestWindow original(OpenRegions),recovered(OpenRegions);Seed(original,item.route);Seed(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;refill_oracle::RefillHost expected_host,actual_host;
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;Extra expected_extra,actual_extra;
    auto expected_deps=refill_oracle::Deps(expected,expected_index,expected_host,expected_unlock,expected_extra);
    auto actual_deps=refill_oracle::Deps(actual,actual_index,actual_host,actual_unlock,actual_extra);
    auto context=refill_oracle::InitialRefill(refill_oracle::RefillRoute::Binary);
    context.r1.u64=0x8877665500000000ull|Stack;context.lr=0x1234567887654321ull;
    context.r3.u64=item.route==Route::NullOutput?0u:0x1122334400000000ull|Output;
    context.r4.u64=4u;context.r5.u64=0xaabbccdd00000000ull|Stream;
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_dependencies=&expected_deps;
    __imp__sub_82B7B2F8(context,original.Bytes());active=nullptr;original_dependencies=nullptr;
    if(!family::Apply(0x82b7b2f8u,actual.memory,actual_deps,state))throw std::runtime_error("missing next reader");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||expected.events!=actual.events||
       expected_index.events!=actual_index.events||expected_host.events!=actual_host.events||
       expected_unlock.events!=actual_unlock.events||expected_extra.events!=actual_extra.events||
       expected.traps!=actual.traps||expected.converted_errors!=actual.converted_errors) {
        for(unsigned i=0;i<before.size();++i)if(before[i]!=after[i])std::fprintf(stderr,"next state[%u] %llx/%llx\n",i,
            static_cast<unsigned long long>(before[i]),static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("next reader Full72/RAM/callback mismatch");
    }
    if(item.route==Route::Line) {
        if(context.r3.u32!=Output||expected.memory.ReadU8(Output)!='A'||
           expected.memory.ReadU8(Output+1u)!=10u||expected.memory.ReadU8(Output+2u)!=0u||
           expected.memory.ReadU32(Stream)!=Buffer+2u||expected.memory.ReadU32(Stream+4u)!=1u)
            throw std::runtime_error("line success and terminator missed");
    } else if(context.r3.u32!=0u)throw std::runtime_error("line error return missed");
    if(item.route!=Route::NullOutput) {
        if(expected_extra.events.size()!=2u||expected_extra.events[0][2]!=0x82b7b760u||
           expected_extra.events[1][2]!=0x82b7b584u||!expected_host.events.empty())
            throw std::runtime_error("line lock and tail cleanup missed");
    } else if(!expected_extra.events.empty())throw std::runtime_error("invalid input locked");
}
}
void OriginalNextReaderLower(GuestAddress entry,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);auto& deps=*next_reader_oracle::original_dependencies;
    const bool found=entry==0x82b81360u?crt_stream_byte_read_context::Apply(entry,active->memory,deps,state)
        :crt_stream_close_shared_lower::ApplyAcceptedLower(entry,active->memory,deps,state);
    if(!found)throw std::runtime_error("missing original next reader lower");crt_full_oracle::ToPpc(context,state);
}
void OriginalNextReaderEnter(PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    next_reader_oracle::original_dependencies->native.EnterCriticalSection(active->memory,state);
    crt_full_oracle::ToPpc(context,state);
}
int main() {
    for(const auto& item:next_reader_oracle::Cases)try{next_reader_oracle::Check(item);}
        catch(const std::exception& e){std::fprintf(stderr,"%s: %s\n",item.name,e.what());return 1;}
    std::puts("CRT next reader: 3 focused actual PPC cases passed");return 0;
}

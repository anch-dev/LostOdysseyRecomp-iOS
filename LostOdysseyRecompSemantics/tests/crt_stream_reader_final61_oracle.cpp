#define main FinalReaderRefillFixtureMain
#include "crt_stream_refill_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_stream_reader_final61.h"
namespace final_reader_oracle {
namespace family=crt_stream_reader_final61;
enum class Route {Stdout,Reset,NativeFlush};
struct Case {const char* name;Route route;};
constexpr std::array Cases{Case{"stdout-bypass",Route::Stdout},
    Case{"locked-read-buffer-reset",Route::Reset},
    Case{"locked-actual-native-flush",Route::NativeFlush}};
struct Extra final : close_shared_oracle::SharedExtra {
    void LeaveCriticalSection(GuestMemory&,crt_stream_bulk_close_routes::Registers& state) override {
        if(state.lr!=0x82b7bc14u||static_cast<GuestAddress>(state.r[3])!=Stream+32u||
           static_cast<GuestAddress>(state.sp)!=Stack-224u)
            throw std::runtime_error("flush wrapper tail unlock ABI");
        events.push_back({3u,state.sp,state.lr,state.r[3],state.r[10],state.r[31]});
        state.r[10]=0x1122334400000055ull;
    }
};
using FlushEvent=std::array<std::uint64_t,5>;
struct FlushNative final : crt_stream_flush_context::NativeServices {
    std::vector<FlushEvent> events;
    void NtFlushBuffersFile(GuestMemory&,crt_stream_flush_context::Registers& state) override {
        if(state.lr!=0x82be48acu||static_cast<GuestAddress>(state.r[4])!=
            static_cast<GuestAddress>(state.sp+80u))
            throw std::runtime_error("actual flush import ABI");
        events.push_back({state.sp,state.lr,state.r[3],state.r[4],state.r[10]});
        state.r[3]=0u;state.r[10]=0x8877665500000077ull;
    }
};
family::Dependencies* original_dependencies=nullptr;
void Seed(GuestWindow& window,Route route) {
    refill_oracle::SeedRefill(window);auto memory=window.Memory();
    memory.WriteU32(Stream+12u,route==Route::NativeFlush?0x4001u:1u);
    memory.WriteU32(Stream+16u,5u);memory.WriteU32(Stream+8u,Buffer);
    memory.WriteU32(Stream,Buffer+2u);memory.WriteU32(Stream+4u,2u);
    // Existing selected flush implementation's descriptor-table global;
    // its ABI is the accepted lower boundary, separate from this wrapper.
    memory.WriteU32(0x83378d80u,0x30000u);
}
void Check(const Case& item) {
    GuestWindow original(OpenRegions),recovered(OpenRegions);Seed(original,item.route);Seed(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;refill_oracle::RefillHost expected_host,actual_host;
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;Extra expected_extra,actual_extra;
    FlushNative expected_flush,actual_flush;
    family::Dependencies expected_deps{refill_oracle::Deps(expected,expected_index,expected_host,expected_unlock,expected_extra),expected_flush};
    family::Dependencies actual_deps{refill_oracle::Deps(actual,actual_index,actual_host,actual_unlock,actual_extra),actual_flush};
    auto context=refill_oracle::InitialRefill(refill_oracle::RefillRoute::Binary);
    context.r1.u64=0x8877665500000000ull|Stack;context.lr=0x1234567887654321ull;
    context.r3.u64=0xaabbccdd00000000ull|(item.route==Route::Stdout?0x83214b10u:Stream);
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_dependencies=&expected_deps;
    __imp__sub_82B7BB38(context,original.Bytes());active=nullptr;original_dependencies=nullptr;
    if(!family::Apply(0x82b7bb38u,actual.memory,actual_deps,state))throw std::runtime_error("missing flush wrapper");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||expected.events!=actual.events||
       expected_index.events!=actual_index.events||expected_host.events!=actual_host.events||
       expected_unlock.events!=actual_unlock.events||expected_extra.events!=actual_extra.events||
       expected_flush.events!=actual_flush.events||expected.traps!=actual.traps||
       expected.converted_errors!=actual.converted_errors) {
        for(unsigned i=0;i<before.size();++i)if(before[i]!=after[i])std::fprintf(stderr,"final state[%u] %llx/%llx\n",i,
            static_cast<unsigned long long>(before[i]),static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("flush wrapper Full72/RAM/callback mismatch");
    }
    if(context.r3.u32!=0u)throw std::runtime_error("flush wrapper successful return missed");
    if(item.route==Route::Stdout) {
        if(!expected_extra.events.empty()||!expected_flush.events.empty()||
           expected.memory.ReadU32(Stream)!=Buffer+2u)
            throw std::runtime_error("stdout bypass touched stream");
    } else if(expected_extra.events.size()!=2u||expected_extra.events[0][2]!=0x82b7b760u||
        expected_extra.events[1][2]!=0x82b7bc14u||expected.memory.ReadU32(Stream)!=Buffer||
        expected.memory.ReadU32(Stream+4u)!=0u||
        expected_flush.events.size()!=(item.route==Route::NativeFlush?1u:0u))
        throw std::runtime_error("real lock/reset/flush/tail unlock route missed");
}
}
void OriginalFinalReaderLower(GuestAddress entry,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_reader_final61::ApplyAcceptedLower(entry,active->memory,
        *final_reader_oracle::original_dependencies,state))
        throw std::runtime_error("missing original flush wrapper lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalFinalReaderEnter(PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    final_reader_oracle::original_dependencies->accepted.native.EnterCriticalSection(active->memory,state);
    crt_full_oracle::ToPpc(context,state);
}
int main() {
    for(const auto& item:final_reader_oracle::Cases)try{final_reader_oracle::Check(item);}
        catch(const std::exception& e){std::fprintf(stderr,"%s: %s\n",item.name,e.what());return 1;}
    std::puts("CRT final stream reader: 3 focused actual PPC cases passed");return 0;
}

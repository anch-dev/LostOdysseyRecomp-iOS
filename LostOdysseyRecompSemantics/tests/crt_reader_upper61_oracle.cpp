#define main ReaderUpperRefillFixtureMain
#include "crt_stream_refill_context_oracle.cpp"
#undef main

#include "lo_semantics/crt_reader_upper61.h"

namespace reader_upper_oracle {
namespace family = crt_reader_upper61;
enum class Route { Null, WriteOnly, ReadFailure, Success };
struct Case { const char* name; Route route; };
constexpr std::array Cases{
    Case{"null-stream-errno22",Route::Null},
    Case{"write-only-error",Route::WriteOnly},
    Case{"refill-invalid-handle",Route::ReadFailure},
    Case{"refill-wide-success-live-native",Route::Success}};

void Seed(GuestWindow& window, Route route) {
    refill_oracle::SeedRefill(window);
    auto memory = window.Memory();
    memory.WriteU32(Stream+12u,route==Route::WriteOnly ? 2u : 9u);
    memory.WriteU32(Stream+8u,Buffer);
    memory.WriteU32(Stream+24u,4u);
    memory.WriteU32(Stream+16u,route==Route::ReadFailure ? 0xfffffffeu : 5u);
    memory.WriteU32(Stream,Buffer);
    memory.WriteU32(Stream+4u,0u);
}

void Check(const Case& item) {
    GuestWindow original(OpenRegions), recovered(OpenRegions);
    Seed(original,item.route); Seed(recovered,item.route);
    Services expected(original,Mode::LockedWrite), actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    refill_oracle::RefillHost expected_host,actual_host;
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    auto context=refill_oracle::InitialRefill(refill_oracle::RefillRoute::Binary);
    context.r3.u64=item.route==Route::Null ? 0u : 0x8877665500000000ull|Stream;
    context.r1.u64=0x8877665500000000ull|Stack;
    context.lr=0x1234567887654321ull;
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;
    refill_oracle::original_stream=&expected;
    refill_oracle::original_index=&expected_index;
    refill_oracle::original_host=&expected_host;
    refill_oracle::original_unlock=&expected_unlock;
    refill_oracle::original_extra=&expected_extra;
    __imp__sub_82358DB0(context,original.Bytes());
    active=nullptr;
    refill_oracle::original_stream=nullptr;
    refill_oracle::original_index=nullptr;
    refill_oracle::original_host=nullptr;
    refill_oracle::original_unlock=nullptr;
    refill_oracle::original_extra=nullptr;
    if(!family::Apply(0x82358db0u,actual.memory,
        refill_oracle::Deps(actual,actual_index,actual_host,actual_unlock,actual_extra),state))
        throw std::runtime_error("missing wide-reader upper");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after || !original.EqualCommitted(recovered) ||
       expected.events!=actual.events || expected_index.events!=actual_index.events ||
       expected_host.events!=actual_host.events || expected_unlock.events!=actual_unlock.events ||
       expected_extra.events!=actual_extra.events || expected.traps!=actual.traps ||
       expected.converted_errors!=actual.converted_errors) {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i]) std::fprintf(stderr,"reader state[%u] %llx/%llx\n",i,
                static_cast<unsigned long long>(before[i]),static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("wide-reader selected Full72/RAM/callback mismatch");
    }
    if(item.route==Route::Success) {
        if(context.r3.u32!=0x4142u || expected.memory.ReadU32(Stream)!=Buffer+2u ||
           expected.memory.ReadU32(Stream+4u)!=2u || expected_host.events.size()!=1u ||
           expected_unlock.events.size()!=1u || state.fpr_bits[3]!=0x4008000000000000ull)
            throw std::runtime_error("wide-reader successful native refill missed");
    } else if(context.r3.u32!=65535u || !expected_host.events.empty())
        throw std::runtime_error("wide-reader failure route missed");
}
}

void OriginalReaderUpperLower(GuestAddress entry,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,active->memory,
        refill_oracle::Deps(*refill_oracle::original_stream,*refill_oracle::original_index,
            *refill_oracle::original_host,*refill_oracle::original_unlock,
            *refill_oracle::original_extra),state))
        throw std::runtime_error("missing selected reader lower ABI");
    crt_full_oracle::ToPpc(context,state);
}

int main() {
    for(const auto& item:reader_upper_oracle::Cases) {
        try { reader_upper_oracle::Check(item); }
        catch(const std::exception& e) { std::fprintf(stderr,"%s: %s\n",item.name,e.what());return 1; }
    }
    std::puts("CRT reader upper: 4 focused actual PPC cases passed");
    return 0;
}

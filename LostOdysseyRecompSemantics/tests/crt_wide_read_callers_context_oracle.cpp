#define main WideReadRefillFixtureMain
#include "crt_stream_refill_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_wide_read_callers_context.h"

namespace wide_read_oracle {
namespace family=crt_wide_read_callers_context;
using Full=family::Registers;
enum class Route {WideFast,WideRefill,ByteRefill,Pushback,NullPushback};
struct Case {const char* name;Route route;};
constexpr std::array Cases{
    Case{"wide-buffer-success",Route::WideFast},
    Case{"wide-native-refill-success",Route::WideRefill},
    Case{"two-byte-native-refill-success",Route::ByteRefill},
    Case{"locked-pushback-unlock-success",Route::Pushback},
    Case{"null-pushback-errno22",Route::NullPushback}};

struct Handler final : crt_record_allocation_context::HandlerServices {
    void CallNewHandler(GuestAddress,GuestMemory&,Full&) override
    {throw std::runtime_error("unselected wide read new-handler");}
};
struct WideExtra final : close_shared_oracle::SharedExtra {
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_bulk_close_routes::Registers& state) override {
        if(state.lr!=0x82b87facu||static_cast<GuestAddress>(state.r[3])!=Stream+32u||
           static_cast<GuestAddress>(state.sp)!=Stack-240u)
            throw std::runtime_error("unexpected wide caller tail unlock ABI");
        events.push_back({3u,state.sp,state.lr,state.r[3],state.r[10],state.r[31]});
        state.r[10]=0x1122334400000055ull;
    }
};
struct Allocation final : crt_reallocation_context::GuestServices {
    explicit Allocation(crt_stream_close_shared_lower::Dependencies deps):accepted(deps){}
    crt_stream_close_shared_lower::Dependencies accepted;
    void CallLower(GuestAddress entry,GuestMemory& memory,Full& state) override {
        if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,memory,accepted,state))
            throw std::runtime_error("unselected wide read allocator boundary");
    }
};
family::Dependencies Deps(Services& stream,IndexService& index,
    refill_oracle::RefillHost& host,wrapper_oracle::WrapperUnlock& unlock,
    close_shared_oracle::SharedExtra& extra,Handler& handler,Allocation& allocation) {
    const auto accepted=refill_oracle::Deps(stream,index,host,unlock,extra);
    crt_record_allocation_context::Dependencies record{StreamDeps(stream,index),host,handler};
    return {accepted,{{record,allocation}}};
}
family::Dependencies* original_dependencies=nullptr;

void Seed(GuestWindow& window,Route route) {
    refill_oracle::SeedRefill(window);
    auto memory=window.Memory();
    const bool pushing=route==Route::Pushback||route==Route::NullPushback;
    memory.WriteU32(Stream+12u,pushing?0x80u:route==Route::WideFast?0x40u:9u);
    memory.WriteU32(Stream+8u,Buffer);
    memory.WriteU32(Stream+24u,4u);
    memory.WriteU32(Stream+16u,pushing?0xffffffffu:5u);
    memory.WriteU32(Stream,pushing?Buffer+1u:Buffer);
    memory.WriteU32(Stream+4u,route==Route::WideFast?4u:0u);
    memory.WriteU8(Buffer,'A');memory.WriteU8(Buffer+1u,'B');
    // Descriptor encoding 2 selects the real two-byte read path while keeping
    // the four-byte native fixture payload in its original buffer. Encoding 1
    // (the former value 2) instead enters UTF-8 scratch allocation/conversion,
    // which the shared binary native fixture deliberately does not implement.
    memory.WriteU8(Record+40u,route==Route::ByteRefill?4u:0u);
    memory.WriteU8(0x83215340u,0u);
}

void Check(const Case& item) {
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    Seed(original,item.route);Seed(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    refill_oracle::RefillHost expected_host,actual_host;
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    WideExtra expected_extra,actual_extra;
    Handler expected_handler,actual_handler;
    Allocation expected_allocation(refill_oracle::Deps(expected,expected_index,expected_host,expected_unlock,expected_extra));
    Allocation actual_allocation(refill_oracle::Deps(actual,actual_index,actual_host,actual_unlock,actual_extra));
    auto expected_deps=Deps(expected,expected_index,expected_host,expected_unlock,expected_extra,expected_handler,expected_allocation);
    auto actual_deps=Deps(actual,actual_index,actual_host,actual_unlock,actual_extra,actual_handler,actual_allocation);
    const bool pushing=item.route==Route::Pushback||item.route==Route::NullPushback;
    auto context=refill_oracle::InitialRefill(refill_oracle::RefillRoute::Binary);
    context.r1.u64=0x8877665500000000ull|Stack;
    context.lr=0x1234567887654321ull;
    context.r3.u64=pushing?'A':0x8877665500000000ull|Stream;
    context.r4.u64=item.route==Route::NullPushback?0u:0x1234567800000000ull|Stream;
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_dependencies=&expected_deps;
    if(pushing)__imp__sub_82B87ED8(context,original.Bytes());
    else __imp__sub_823588C0(context,original.Bytes());
    active=nullptr;original_dependencies=nullptr;
    if(!family::Apply(pushing?0x82b87ed8u:0x823588c0u,actual.memory,actual_deps,state))
        throw std::runtime_error("missing wide read caller");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||expected.events!=actual.events||
       expected_index.events!=actual_index.events||expected_host.events!=actual_host.events||
       expected_unlock.events!=actual_unlock.events||expected_extra.events!=actual_extra.events||
       expected.traps!=actual.traps||expected.converted_errors!=actual.converted_errors) {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i])std::fprintf(stderr,"wide state[%u] %llx/%llx\n",i,
                static_cast<unsigned long long>(before[i]),static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("wide read caller Full72/RAM/callback mismatch");
    }
    if(item.route==Route::Pushback) {
        if(context.r3.u32!='A'||expected.memory.ReadU32(Stream)!=Buffer||
           expected.memory.ReadU32(Stream+4u)!=1u||expected_extra.events.size()!=2u||
           expected_extra.events[0][2]!=0x82b7b760u||
           expected_extra.events[1][0]!=3u||expected_extra.events[1][2]!=0x82b87facu||
           !expected_unlock.events.empty())
            throw std::runtime_error("actual locked pushback/unlock route missed");
    } else if(item.route==Route::NullPushback) {
        if(context.r3.u32!=UINT32_MAX||!expected_extra.events.empty())
            throw std::runtime_error("null pushback route missed");
    } else {
        const bool native=item.route!=Route::WideFast;
        if(context.r3.u32!=0x4142u||expected.memory.ReadU32(Stream)!=Buffer+2u||
           expected_host.events.size()!=(native?1u:0u))
            throw std::runtime_error("wide read success route missed");
    }
}
}

void OriginalWideReadLower(GuestAddress entry,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);auto& deps=*wide_read_oracle::original_dependencies;
    bool found=false;
    if(entry==0x82358db0u)found=crt_reader_upper61::Apply(entry,active->memory,deps.accepted,state);
    else if(entry==0x82b81360u)found=crt_stream_byte_read_context::Apply(entry,active->memory,deps.accepted,state);
    else if(entry==0x82b87cf8u)found=crt_stream_pushback_context::Apply(entry,active->memory,deps.pushback,state);
    else found=crt_stream_close_shared_lower::ApplyAcceptedLower(entry,active->memory,deps.accepted,state);
    if(!found)throw std::runtime_error("unselected original wide read accepted lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalWideReadEnter(PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    wide_read_oracle::original_dependencies->accepted.native.EnterCriticalSection(active->memory,state);
    crt_full_oracle::ToPpc(context,state);
}
int main() {
    for(const auto& item:wide_read_oracle::Cases) {
        try{wide_read_oracle::Check(item);}
        catch(const std::exception& e){std::fprintf(stderr,"%s: %s\n",item.name,e.what());return 1;}
    }
    std::puts("CRT wide read callers: 5 focused actual PPC cases passed");return 0;
}

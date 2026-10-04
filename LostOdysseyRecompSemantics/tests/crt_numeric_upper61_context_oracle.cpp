#define main Upper61ScanFixtureMain
#include "crt_stream_scan_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_numeric_upper61_context.h"
namespace upper61_oracle
{
namespace family=crt_numeric_upper61_context;
using namespace scan_oracle;
enum class Route {PublicInteger,PublicEmpty,DynamicCallback,NullFormat};
struct Case {const char* name;GuestAddress entry;Route route;};
constexpr std::array Cases{
    Case{"public-string-scan-integer",0x82df3440u,Route::PublicInteger},
    Case{"public-string-scan-empty-format",0x82df3440u,Route::PublicEmpty},
    Case{"upper-full-callback-live-lr-slot",0x82df3378u,Route::DynamicCallback},
    Case{"upper-null-format-errno22",0x82df3378u,Route::NullFormat}};
struct Callback final : crt_stream_scan_context::GuestServices
{
    std::vector<scan_oracle::Event> events;
    void CallIndirect(GuestAddress target,GuestMemory& memory,Full& state) override
    {
        if(target!=IndirectTarget||state.lr!=0x82df3430u||
            memory.ReadU32(Address(state.r[3]))!=Buffer||
            memory.ReadU32(Address(state.r[3]+4u))!=2u||
            memory.ReadU32(Address(state.r[3]+8u))!=Buffer||
            memory.ReadU32(Address(state.r[3]+12u))!=73u)
            throw std::runtime_error("unexpected upper callback object/target/LR");
        events.push_back({target,state.r[1],state.lr,state.r[3],state.r[4],
            state.r[5],state.r[6],state.ctr,state.fpr_bits[3]});
        memory.WriteU32(Address(state.r[1]+120u),0x81dead01u);
        state.r[3]=0x8877665500000033ull;state.r[10]=0xaabbccdd12345678ull;
        state.fpr_bits[3]=0x4008000000000000ull;state.cr1.gt^=1u;state.cr7.eq^=1u;
    }
};
Callback* original_callback=nullptr;
void SeedCase(GuestWindow& window,Route route)
{
    scan_oracle::Seed(window,scan_oracle::Route::FloatIndirect);
    auto memory=window.Memory();
    if(route==Route::PublicInteger)
    {
        memory.WriteU8(Format,'%');memory.WriteU8(Format+1u,'d');memory.WriteU8(Format+2u,0u);
        memory.WriteU8(Buffer,'7');memory.WriteU8(Buffer+1u,0u);
    }
    else
    {
        memory.WriteU8(Format,route==Route::PublicEmpty?0u:'A');memory.WriteU8(Format+1u,0u);
        memory.WriteU8(Buffer,'A');memory.WriteU8(Buffer+1u,'B');memory.WriteU8(Buffer+2u,0u);
    }
    memory.WriteU32(Destination,0xdeadbeefu);
}
PPCContext InitialCase(Route route)
{
    auto context=scan_oracle::Initial(scan_oracle::Route::Literal);
    if(route==Route::PublicInteger||route==Route::PublicEmpty)
    {
        context.r3.u64=0xaabbccdd00000000ull|Buffer;
        context.r4.u64=0x1122334400000000ull|Format;
        context.r5.u64=0x9988776600000000ull|Destination;
    }
    else
    {
        context.r3.u64=IndirectTarget|3u;
        context.r4.u64=0xaabbccdd00000000ull|Buffer;
        context.r5.u64=route==Route::NullFormat?0u:0x1122334400000000ull|Format;
        context.r6.u64=0xfedcba9812345678ull;
        context.r7.u64=0x8877665500000000ull|Arguments;
    }
    return context;
}
void Check(const Case& item)
{
    GuestWindow original(ScanRegions),recovered(ScanRegions);
    SeedCase(original,item.route);SeedCase(recovered,item.route);
    ServicesBundle expected(original),actual(recovered);
    Callback expected_callback,actual_callback;
    auto context=InitialCase(item.route);auto state=crt_full_oracle::FromPpc(context);
    current=&expected.stream;active=&expected.stream;
    current_index=&expected.index;current_host=&expected.host;
    wrapper_oracle::original_unlock=&expected.unlock;close_shared_oracle::active_extra=&expected.extra;
    original_services=&expected;original_callback=&expected_callback;
    if(item.entry==0x82df3440u) __imp__sub_82DF3440(context,original.Bytes());
    else __imp__sub_82DF3378(context,original.Bytes());
    original_services=nullptr;original_callback=nullptr;
    close_shared_oracle::active_extra=nullptr;wrapper_oracle::original_unlock=nullptr;
    current_host=nullptr;current_index=nullptr;active=nullptr;current=nullptr;
    if(!family::Apply(item.entry,actual.stream.memory,{scan_oracle::Deps(actual),actual_callback},state))
        throw std::runtime_error("missing recovered upper scanner body");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.stream.events!=actual.stream.events||
        expected.stream.traps!=actual.stream.traps||
        expected.index.events!=actual.index.events||
        expected.host.events!=actual.host.events||
        expected.unlock.events!=actual.unlock.events||
        expected.extra.events!=actual.extra.events||
        expected.native.events!=actual.native.events||
        expected.indirect.events!=actual.indirect.events||
        expected_callback.events!=actual_callback.events)
    {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i])
                std::fprintf(stderr,"%s state[%u] %llx/%llx\n",item.name,
                    i,static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        for(const auto region:ScanRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=
                    recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"%s RAM %08llx %02x/%02x\n",
                    item.name,
                    static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
        throw std::runtime_error("scan Full72/RAM/callback mismatch");
    }

    const auto memory=expected.stream.memory;
    if(context.r1.u64!=(0x8877665500000000ull|Stack))
        throw std::runtime_error("missed scanner upper stack high64");
    if(item.route!=Route::DynamicCallback&&context.lr!=0x87654321u)
        throw std::runtime_error("missed upper saved LR truncation");
    if(item.route==Route::PublicInteger)
    {
        if(context.r3.u32!=1u||memory.ReadU32(Destination)!=7u||
            memory.ReadU32(Stack+32u)!=0x99887766u||memory.ReadU32(Stack+36u)!=Destination)
            throw std::runtime_error("missed public integer scan/full variadic spill");
    }
    else if(item.route==Route::PublicEmpty)
    {
        if(context.r3.u64!=0u||memory.ReadU32(Destination)!=0xdeadbeefu)
            throw std::runtime_error("missed empty scanner format");
    }
    else if(item.route==Route::DynamicCallback)
    {
        if(context.r3.u64!=0x8877665500000033ull||context.lr!=0x81dead01u||
            expected_callback.events.size()!=1u||context.f3.u64!=0x4008000000000000ull)
            throw std::runtime_error("missed live full callback/LR saved slot");
    }
    else if(context.r3.s64!=-1||memory.ReadU32(0x83215210u)!=22u||
        !expected_callback.events.empty())
        throw std::runtime_error("missed upper null format rejection");
}
} // namespace upper61_oracle
void OriginalUpper61Indirect(GuestAddress target,PPCContext& context,std::uint8_t* bytes)
{
    if(context.lr!=0x82df3430u)
    {OriginalScanIndirect(target,context,bytes);return;}
    if(target==0x82df4af8u)
    {__imp__sub_82DF4AF8(context,bytes);return;}
    auto state=crt_full_oracle::FromPpc(context);
    upper61_oracle::original_callback->CallIndirect(target,current->memory,state);
    crt_full_oracle::ToPpc(context,state);
}
int main()
{
    for(const auto& item:upper61_oracle::Cases)
    {
        try {upper61_oracle::Check(item);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-numeric-upper61-context %zu actual PPC cases\n",upper61_oracle::Cases.size());
    std::puts("LIMIT selected accepted scanner/native ABI and full upper guest callback; fault/MMIO/runtime unvalidated");
    return 0;
}

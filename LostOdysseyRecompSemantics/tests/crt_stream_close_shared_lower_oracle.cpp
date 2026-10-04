// Reuse the accepted actual open-wrapper pin and its selected lower services.
#pragma push_macro("main")
#undef main
#define main CloseSharedOpenFixtureMain
#include "crt_stream_open_wrapper_oracle.cpp"
#undef main
#pragma pop_macro("main")

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace close_shared_oracle
{
namespace shared = crt_stream_close_shared_lower;
using SharedTrace = std::array<std::uint64_t, 6>;
enum class Route { NullPath, EmptyTable, ScanEmpty, InvalidMode,
    OpenNullOutput, ReturningUnwind };
struct SharedCase { const char* name; GuestAddress entry; Route route; };
constexpr std::array SharedCases{
    SharedCase{"tail-null-path",0x82df2150u,Route::NullPath},
    SharedCase{"parent-empty-table",0x82df1fc0u,Route::EmptyTable},
    SharedCase{"scan-empty-table",0x82df4898u,Route::ScanEmpty},
    SharedCase{"mode-invalid-character",0x82df4638u,Route::InvalidMode},
    SharedCase{"tail-actual-open-null-output",0x82df6908u,
        Route::OpenNullOutput},
    SharedCase{"returning-native-unwind",0x82df3030u,
        Route::ReturningUnwind}};
constexpr GuestAddress ModeString=0x41000u;

struct SharedExtra : crt_formatting_support::NativeServices,
    crt_formatter::DynamicServices,
    crt_stream_close_error::GuestServices,
    crt_stream_bulk_close_routes::NativeServices,
    shared::NativeServices
{
    std::vector<SharedTrace> events;
    [[noreturn]] static void Unselected()
    {throw std::runtime_error("unselected close shared guest/native path");}
    void InitializeUnicodeString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unselected();}
    void UnicodeStringToAnsiString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unselected();}
    void FreeAnsiString(GuestMemory&,
        crt_formatting_support::Registers&) override {Unselected();}
    void InitAnsiString(GuestMemory&,GuestAddress,GuestAddress) override
    {Unselected();}
    std::uint64_t WriteAnsi(GuestAddress,std::uint16_t) override
    {Unselected();}
    void CallGuestFormatter(GuestAddress,GuestMemory&,
        crt_formatter::Registers&) override {Unselected();}
    void CallIndirect(GuestMemory&,GuestAddress,
        crt_stream_close_error::Registers&) override {Unselected();}
    void EnterCriticalSection(GuestMemory&,
        crt_stream_bulk_close_routes::Registers&) override {Unselected();}
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_bulk_close_routes::Registers&) override {Unselected();}
    void EnterCriticalSection(GuestMemory&,shared::Registers& state) override
    {
        events.push_back({1u,state.r[1],state.lr,state.r[3],
            state.r[10],state.r[31]});
        state.r[10]=0xabcddcba00000011ull;
    }
    void RtlUnwind(GuestMemory&,shared::Registers& state) override
    {
        events.push_back({2u,state.r[1],state.lr,state.r[3],
            state.r[10],state.r[31]});
        state.r[10]=0xabcddcba00000022ull;
        state.cr1.gt=1u;
    }
};
SharedExtra* active_extra=nullptr;

shared::Dependencies Deps(Services& stream,IndexService& index,
    Host& host,wrapper_oracle::WrapperUnlock& unlock,SharedExtra& extra)
{
    const auto accepted=StreamDeps(stream,index);
    crt_wide_stream_output::Dependencies output{accepted,extra};
    crt_float_formatting::Dependencies floating{{stream,stream},host};
    crt_formatter::Dependencies formatter{output,floating,extra,stream,extra};
    crt_format_stream::Dependencies format{formatter};
    crt_stream_close_error::Dependencies close{accepted,extra};
    crt_stream_close_pipeline::Dependencies pipeline{close,format,host};
    crt_stream_bulk_close_routes::Dependencies bulk{pipeline,extra};
    return {bulk,wrapper_oracle::Deps(stream,index,host,unlock),extra};
}

void SeedShared(GuestWindow& window,Route route)
{
    SeedOpen(window,Scenario::BinarySuccess);
    auto memory=window.Memory();
    memory.WriteU32(0x83215360u,0x62000u); // initialized indexed lock 1
    memory.WriteU8(ModeString,'x');
    memory.WriteU8(ModeString+1u,0u);
    if(route==Route::EmptyTable||route==Route::ScanEmpty)
    {
        memory.WriteU32(Count,0u);
        memory.WriteU32(Blocks,0u);
    }
}
PPCContext Initial(Route route)
{
    PPCContext context=InitialOpen(Scenario::BinarySuccess);
    context.cr1.gt=1; context.cr7.lt=1;
    context.f13.u64=0x4024000000000000ull;
    context.f30.u64=0x4034000000000000ull;
    context.r3.u64=route==Route::NullPath?0u:Path;
    context.r4.u64=route==Route::InvalidMode?ModeString:Path;
    context.r5.u64=64u;
    context.r6.u64=Record;
    if(route==Route::OpenNullOutput)
    {
        context.r3.u64=0u; // becomes the open wrapper's output pointer
        context.r4.u64=Path;
        context.r5.u64=0x8000u;
        context.r6.u64=16u;
        context.r7.u64=OutputSlot;
    }
    return context;
}
void RunOriginal(GuestAddress entry,PPCContext& context,
    std::uint8_t* bytes)
{
    switch(entry)
    {
    case 0x82df2150u:__imp__sub_82DF2150(context,bytes);return;
    case 0x82df1fc0u:__imp__sub_82DF1FC0(context,bytes);return;
    case 0x82df4898u:__imp__sub_82DF4898(context,bytes);return;
    case 0x82df3030u:__imp__sub_82DF3030(context,bytes);return;
    case 0x82df4638u:__imp__sub_82DF4638(context,bytes);return;
    case 0x82df6908u:__imp__sub_82DF6908(context,bytes);return;
    default:throw std::runtime_error("unknown close shared oracle entry");
    }
}

void Check(const SharedCase& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedShared(original,item.route);SeedShared(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    SharedExtra expected_extra,actual_extra;
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;current_index=&expected_index;
    current_host=&expected_host;active=&expected;
    wrapper_oracle::original_unlock=&expected_unlock;
    active_extra=&expected_extra;
    RunOriginal(item.entry,context,original.Bytes());
    current=nullptr;current_index=nullptr;current_host=nullptr;active=nullptr;
    wrapper_oracle::original_unlock=nullptr;active_extra=nullptr;
    if(!shared::Apply(item.entry,actual.memory,
            Deps(actual,actual_index,actual_host,actual_unlock,actual_extra),
            state))
        throw std::runtime_error("missing recovered close shared body");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_index.events!=actual_index.events||
        expected_host.events!=actual_host.events||
        expected_unlock.events!=actual_unlock.events||
        expected_extra.events!=actual_extra.events||
        expected.traps!=actual.traps||
        expected.converted_errors!=actual.converted_errors)
    {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i])
                std::fprintf(stderr,"case %u state[%u] %llx/%llx\n",
                    ordinal,i,static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        for(const auto region:OpenRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=
                    recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",
                    ordinal,static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
        throw std::runtime_error("full context/RAM/ordered callbacks mismatch");
    }
    switch(item.route)
    {
    case Route::NullPath:case Route::InvalidMode:
        if(context.r3.u32!=0u||expected.memory.ReadU32(0x83215210u)!=22u)
            throw std::runtime_error("missed invalid close argument path");
        break;
    case Route::EmptyTable:
        if(context.r3.u32!=0u||expected.memory.ReadU32(0x83215210u)!=24u||
            expected_index.events.size()!=1u)
            throw std::runtime_error("missed empty-table parent path");
        break;
    case Route::ScanEmpty:
        if(context.r3.u32!=0u||expected_index.events.size()!=1u)
            throw std::runtime_error("missed empty-table scan/unlock path");
        break;
    case Route::OpenNullOutput:
        if(context.r3.u32!=22u||expected.traps!=1u)
            throw std::runtime_error("missed actual open wrapper path");
        break;
    case Route::ReturningUnwind:
        if(expected_extra.events.size()!=1u||
            expected_extra.events[0][0]!=2u||
            expected_extra.events[0][2]!=0x82df3048u)
            throw std::runtime_error("missed returning unwind boundary");
        break;
    }
}
} // namespace close_shared_oracle

void OriginalCloseAccepted(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
            current->memory,
            close_shared_oracle::Deps(*current,*current_index,*current_host,
                *wrapper_oracle::original_unlock,
                *close_shared_oracle::active_extra),state))
        throw std::runtime_error("missing accepted close shared lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalCloseNativeEnter(PPCContext& context,std::uint8_t* bytes)
{
    if(context.lr!=0x82df49e0u)
    {OriginalOpenNative(0x830d9c6cu,context,bytes);return;}
    auto state=crt_full_oracle::FromPpc(context);
    close_shared_oracle::active_extra->EnterCriticalSection(
        current->memory,state);
    crt_full_oracle::ToPpc(context,state);
}
void OriginalCloseNativeUnwind(PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    close_shared_oracle::active_extra->RtlUnwind(current->memory,state);
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(unsigned i=0;i<close_shared_oracle::SharedCases.size();++i)
    {
        const auto& item=close_shared_oracle::SharedCases[i];
        try{close_shared_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-stream-close-shared-lower %zu actual PPC cases\n",
        close_shared_oracle::SharedCases.size());
    std::puts("LIMIT selected full context; native unwind may return in fixture; deep accepted guest/native, faults, MMIO and concurrency retain their boundaries");
    return 0;
}

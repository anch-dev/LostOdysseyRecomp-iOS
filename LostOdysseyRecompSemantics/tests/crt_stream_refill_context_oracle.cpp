#pragma push_macro("main")
#undef main
#define main RefillSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main
#pragma pop_macro("main")

#include "lo_semantics/crt_stream_refill_context.h"

namespace refill_oracle
{
namespace refill = crt_stream_refill_context;
using Full = refill::Registers;
using RefillTrace = std::array<std::uint64_t, 8>;
enum class RefillRoute {MinusTwo, OutOfRange, Oversized, Empty, Binary};
struct RefillCase {const char* name;RefillRoute route;};
constexpr std::array RefillCases{
    RefillCase{"minus-two-error",RefillRoute::MinusTwo},
    RefillCase{"out-of-range-handle",RefillRoute::OutOfRange},
    RefillCase{"oversized-count",RefillRoute::Oversized},
    RefillCase{"valid-empty-read-unlock",RefillRoute::Empty},
    RefillCase{"valid-binary-read-unlock",RefillRoute::Binary}};
constexpr GuestAddress Destination=0x40000u;

struct RefillHost final : routes::GuestServices,
    crt_stream_position_routes::NativeServices,
    crt_async_status_transfer::NativeServices,
    crt_utf8_conversion_routes::NativeServices,
    heap_allocation_context::BoundaryServices,
    heap_free_context::BoundaryServices,
    crt_free_context::LowerCalls,
    crt_stream_close_error::GuestServices,
    crt_float_environment::NativeServices
{
    std::vector<RefillTrace> events;
    [[noreturn]] static void Unselected()
    {throw std::runtime_error("unselected refill guest/native boundary");}
    void KeTlsGetValue(GuestMemory&,routes::Registers&) override {Unselected();}
    void KeTlsSetValue(GuestMemory&,routes::Registers&) override {Unselected();}
    void AllocateCrtRecord(GuestMemory&,routes::Registers&) override {Unselected();}
    void EnterCriticalSection(GuestMemory&,routes::Registers&) override
    {Unselected();}
    void LeaveCriticalSection(GuestMemory&,routes::Registers&) override
    {Unselected();}
    void InitAnsiString(GuestMemory&,routes::Registers&) override {Unselected();}
    void CallOpenFile(GuestAddress,GuestMemory&,routes::Registers&) override
    {Unselected();}
    void NtQueryInformationFile(GuestMemory&,Full&) override {Unselected();}
    void NtSetInformationFile(GuestMemory&,Full&) override {Unselected();}
    void CallIndirect(GuestAddress target,GuestMemory& memory,Full& state) override
    {
        if(target!=0x2400u||state.lr!=0x82be2ed8u)
            throw std::runtime_error("unexpected refill async target");
        events.push_back({target,state.r[1],state.lr,state.r[3],state.r[7],
            state.r[8],state.r[9],state.r[12]});
        for(unsigned index=0;index<4u;++index)
            memory.WriteU8(Address(state.r[8]+index),
                static_cast<std::uint8_t>("ABCD"[index]));
        memory.WriteU32(Address(state.r[1]+84u),4u);
        state.r[3]=0u;
        state.r[10]=0x1122334455667788ull;
        state.fpr_bits[3]=0x4008000000000000ull;
    }
    void NtWaitForSingleObjectEx(GuestMemory&,Full&) override {Unselected();}
    void NtStatusToDosError(GuestMemory&,Full&) override {Unselected();}
    void RtlMultiByteToUnicodeN(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override {Unselected();}
    void RtlNtStatusToDosError(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override {Unselected();}
    void CallDirect(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override {Unselected();}
    void CallNative(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override {Unselected();}
    void Call(GuestAddress,GuestMemory&,
        crt_free_context::Registers&) override {Unselected();}
    void CallIndirect(GuestMemory&,GuestAddress,
        crt_stream_close_error::Registers&) override {Unselected();}
    void CallDebugMonitor(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
    void CallExceptionHandler(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
    void BugCheck(GuestMemory&,
        crt_float_environment::Registers&) override {Unselected();}
};

refill::Dependencies Deps(Services& stream,IndexService& index,
    RefillHost& host,wrapper_oracle::WrapperUnlock& unlock,
    close_shared_oracle::SharedExtra& extra)
{
    const auto accepted=StreamDeps(stream,index);
    crt_wide_stream_output::Dependencies output{accepted,extra};
    crt_float_formatting::Dependencies floating{{stream,stream},host};
    crt_formatter::Dependencies formatter{output,floating,extra,stream,extra};
    crt_format_stream::Dependencies format{formatter};
    crt_stream_close_error::Dependencies close{accepted,extra};
    crt_stream_close_pipeline::Dependencies pipeline{close,format,host};
    crt_stream_bulk_close_routes::Dependencies bulk{pipeline,extra};
    crt_stream_open_pipeline::Dependencies open{accepted,host,
        {accepted,host,host,host},{accepted,host,host,host,host},host,host};
    crt_stream_open_wrapper::Dependencies wrapper{open,unlock};
    return {bulk,wrapper,extra};
}

void SeedRefill(GuestWindow& window)
{
    SeedOpen(window,Scenario::BinarySuccess);
    auto memory=window.Memory();
    memory.WriteU32(Count,16u);
    memory.WriteU32(Blocks,0x30000u);
    memory.WriteU8(Record+4u,1u);
    memory.WriteU8(Record+40u,0u);
    memory.WriteU32(0x34010u,0x2401u);
    memory.WriteU32(0x83215360u,0x62000u);
    for(unsigned index=0;index<4u;++index)
        memory.WriteU8(Destination+index,0x55u);
}
PPCContext InitialRefill(RefillRoute route)
{
    auto context=InitialOpen(Scenario::BinarySuccess);
    context.cr1.gt=1u;context.cr7.lt=1u;
    context.r3.u64=route==RefillRoute::MinusTwo?UINT64_MAX-1u:
        route==RefillRoute::OutOfRange?16u:
        0x1234567800000005ull;
    context.r4.u64=0x4567000000000000ull|Destination;
    context.r5.u64=route==RefillRoute::Oversized?0x80000000u:
        route==RefillRoute::Empty?0u:4u;
    return context;
}

Services* original_stream=nullptr;
IndexService* original_index=nullptr;
RefillHost* original_host=nullptr;
wrapper_oracle::WrapperUnlock* original_unlock=nullptr;
close_shared_oracle::SharedExtra* original_extra=nullptr;

void Check(const RefillCase& item)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedRefill(original);SeedRefill(recovered);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    RefillHost expected_host,actual_host;
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    auto context=InitialRefill(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_stream=&expected;original_index=&expected_index;
    original_host=&expected_host;original_unlock=&expected_unlock;
    original_extra=&expected_extra;
    __imp__sub_82B85A80(context,original.Bytes());
    active=nullptr;original_stream=nullptr;original_index=nullptr;
    original_host=nullptr;original_unlock=nullptr;original_extra=nullptr;
    if(!refill::Apply(0x82b85a80u,actual.memory,
            Deps(actual,actual_index,actual_host,actual_unlock,actual_extra),state))
        throw std::runtime_error("missing recovered refill body");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_index.events!=actual_index.events||
        expected_host.events!=actual_host.events||
        expected_unlock.events!=actual_unlock.events||
        expected_extra.events!=actual_extra.events||
        expected.traps!=actual.traps)
    {
        for(unsigned index=0;index<before.size();++index)
            if(before[index]!=after[index])
                std::fprintf(stderr,"refill state[%u] %llx/%llx\n",index,
                    static_cast<unsigned long long>(before[index]),
                    static_cast<unsigned long long>(after[index]));
        throw std::runtime_error("refill full state/RAM/ordered event mismatch");
    }
    const bool valid=item.route==RefillRoute::Empty||
        item.route==RefillRoute::Binary;
    if(expected_unlock.events.size()!=(valid?1u:0u)||
        expected_host.events.size()!=(item.route==RefillRoute::Binary?1u:0u))
        throw std::runtime_error("refill lock/read/unlock route missed");
    if(item.route==RefillRoute::Binary)
    {
        if(context.r3.u32!=4u||expected.memory.ReadU8(Destination)!='A'||
            expected.memory.ReadU8(Destination+3u)!='D'||
            expected_host.events[0][2]!=0x82be2ed8u)
            throw std::runtime_error("refill binary transfer route missed");
    }
    else if(item.route==RefillRoute::Empty)
    {
        if(context.r3.u32!=0u)
            throw std::runtime_error("refill empty transfer route missed");
    }
    else if(context.r3.u32!=UINT32_MAX)
        throw std::runtime_error("refill error route missed");
}
} // namespace refill_oracle

void OriginalRefillLower(GuestAddress entry,PPCContext& context,std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_refill_context::ApplyAcceptedLower(entry,active->memory,
        refill_oracle::Deps(*refill_oracle::original_stream,
            *refill_oracle::original_index,*refill_oracle::original_host,
            *refill_oracle::original_unlock,*refill_oracle::original_extra),state))
        throw std::runtime_error("missing accepted refill lower");
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(const auto& item:refill_oracle::RefillCases)
    {
        try{refill_oracle::Check(item);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::puts("CRT stream refill: 5 focused actual PPC cases passed");
    return 0;
}

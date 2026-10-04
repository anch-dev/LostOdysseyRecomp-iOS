// The accepted shared-close fixture supplies full-state services and the
// actual open PPC pin. Its main is only a helper in this independent oracle.
#define main CloseFileSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main

#include "lo_semantics/crt_stream_close_file_lower.h"

namespace close_file_oracle
{
namespace file = crt_stream_close_file_lower;
enum class FileRoute {NullUpper,ActiveStaticRecord,NullInner,
    MinusTwoDescriptor,OutOfRangeDescriptor,DirectPointerUnlock};
struct FileCase {const char* name;GuestAddress entry;FileRoute route;};
constexpr std::array FileCases{
    FileCase{"upper-null-stream",0x82df1cd0u,FileRoute::NullUpper},
    FileCase{"upper-static-record-invalid-descriptor",0x82df1cd0u,
        FileRoute::ActiveStaticRecord},
    FileCase{"inner-null-stream",0x82df1aa0u,FileRoute::NullInner},
    FileCase{"descriptor-minus-two",0x82df44a8u,
        FileRoute::MinusTwoDescriptor},
    FileCase{"descriptor-out-of-range",0x82df44a8u,
        FileRoute::OutOfRangeDescriptor},
    FileCase{"direct-pointer-unlock",0x82df4600u,
        FileRoute::DirectPointerUnlock}};
constexpr GuestAddress StaticRecord=0x83214af0u;

void SeedFile(GuestWindow& window)
{
    close_shared_oracle::SeedShared(window,
        close_shared_oracle::Route::NullPath);
    auto memory=window.Memory();
    memory.WriteU32(StaticRecord+4u,0u);
    memory.WriteU32(StaticRecord+12u,0u);
    memory.WriteU32(StaticRecord+16u,UINT32_MAX);
    memory.WriteU32(0x832153d8u,0x62100u); // initialized indexed lock 16
}
PPCContext Initial(FileRoute route)
{
    auto context=close_shared_oracle::Initial(
        close_shared_oracle::Route::NullPath);
    switch(route)
    {
    case FileRoute::NullUpper:case FileRoute::NullInner:
        context.r3.u64=0u;break;
    case FileRoute::ActiveStaticRecord:
        context.r3.u64=StaticRecord;break;
    case FileRoute::MinusTwoDescriptor:
        context.r3.u64=0xfffffffffffffffeull;break;
    case FileRoute::OutOfRangeDescriptor:
        context.r3.u64=0x1000u;break;
    case FileRoute::DirectPointerUnlock:
        context.r3.u64=0x1234000000000000ull;
        context.r30.u64=0u;
        context.r12.u64=context.r1.u64;break;
    }
    return context;
}
void RunOriginal(GuestAddress entry,PPCContext& context,
    std::uint8_t* bytes)
{
    switch(entry)
    {
    case 0x82df1cd0u:__imp__sub_82DF1CD0(context,bytes);return;
    case 0x82df1aa0u:__imp__sub_82DF1AA0(context,bytes);return;
    case 0x82df44a8u:__imp__sub_82DF44A8(context,bytes);return;
    case 0x82df4600u:__imp__sub_82DF4600(context,bytes);return;
    default:throw std::runtime_error("unknown file close oracle entry");
    }
}
void Check(const FileCase& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedFile(original);SeedFile(recovered);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;current_index=&expected_index;
    current_host=&expected_host;active=&expected;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    RunOriginal(item.entry,context,original.Bytes());
    current=nullptr;current_index=nullptr;current_host=nullptr;active=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;
    if(!file::Apply(item.entry,actual.memory,
            close_shared_oracle::Deps(actual,actual_index,actual_host,
                actual_unlock,actual_extra),state))
        throw std::runtime_error("missing recovered close file body");
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
    case FileRoute::NullUpper:case FileRoute::NullInner:
        if(context.r3.u32!=UINT32_MAX||
            expected.memory.ReadU32(0x83215210u)!=22u)
            throw std::runtime_error("missed null stream argument path");
        break;
    case FileRoute::ActiveStaticRecord:
        if(context.r3.u32!=UINT32_MAX||
            expected.memory.ReadU32(0x83215210u)!=9u||
            expected_index.events.empty())
            throw std::runtime_error("missed locked record/descriptor path");
        break;
    case FileRoute::MinusTwoDescriptor:case FileRoute::OutOfRangeDescriptor:
        if(context.r3.u32!=UINT32_MAX||
            expected.memory.ReadU32(0x83215210u)!=9u)
            throw std::runtime_error("missed invalid descriptor path");
        break;
    case FileRoute::DirectPointerUnlock:
        if(expected_unlock.events.size()!=1u)
            throw std::runtime_error("missed accepted pointer unlock path");
        break;
    }
}
} // namespace close_file_oracle

void OriginalFileAccepted(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_close_file_lower::ApplyAcceptedLower(entry,current->memory,
            close_shared_oracle::Deps(*current,*current_index,*current_host,
                *wrapper_oracle::original_unlock,
                *close_shared_oracle::active_extra),state))
        throw std::runtime_error("missing accepted close file lower");
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(unsigned i=0;i<close_file_oracle::FileCases.size();++i)
    {
        const auto& item=close_file_oracle::FileCases[i];
        try{close_file_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-stream-close-file-lower %zu actual PPC cases\n",
        close_file_oracle::FileCases.size());
    std::puts("LIMIT selected full state; positive descriptor I/O and deeper native/guest, faults, MMIO and concurrency remain bounded by accepted lower APIs");
    return 0;
}

// Appended after the five complete original stream-slot/open/lock bodies.
#include "lo_semantics/crt_stream_open_routes_context.h"
#include "crt_stream_oracle_fixture.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace routes=lo::semantic::gpu::crt_stream_open_routes_context;
using Trace=std::array<std::uint64_t,5>;

struct IndexService final : crt_stream_index_unlock::NativeServices
{
    std::vector<Trace> events;
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_index_unlock::Registers& s) override
    {events.push_back({1u,s.sp,s.lr,s.r3,s.r29});}
};

enum class Path {SlotValid,SlotInvalid,OpenExisting,OpenCreate,
    InitializeExisting,RestoreLoop};
constexpr std::array Paths{Path::SlotValid,Path::SlotInvalid,
    Path::OpenExisting,Path::OpenCreate,Path::InitializeExisting,
    Path::RestoreLoop};
constexpr std::array<test::Region,6> OpenRegions{{{0,0x120000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x83245000u,0x1000u},{0x832d3000u,0x2000u},
    {0x83378000u,0x3000u}}};

struct Guest final : routes::GuestServices
{
    Path path;
    std::vector<Trace> events;
    explicit Guest(Path value):path(value){}
    void KeTlsGetValue(GuestMemory&,routes::Registers&) override
    {throw std::runtime_error("unexpected open-route TLS get");}
    void KeTlsSetValue(GuestMemory&,routes::Registers&) override
    {throw std::runtime_error("unexpected open-route TLS set");}
    void AllocateCrtRecord(GuestMemory&,routes::Registers& s) override
    {
        events.push_back({2u,s.sp,s.lr,s.r[3],s.r[4]});
        s.r[3]=0x38000u;
    }
    void EnterCriticalSection(GuestMemory&,routes::Registers& s) override
    {events.push_back({3u,s.sp,s.lr,s.r[3],s.r[30]});}
    void LeaveCriticalSection(GuestMemory&,routes::Registers& s) override
    {events.push_back({4u,s.sp,s.lr,s.r[3],s.r[30]});}
    void InitAnsiString(GuestMemory& memory,routes::Registers& s) override
    {
        events.push_back({5u,s.sp,s.lr,s.r[3],s.r[4]});
        auto length=std::uint16_t{0};
        while(memory.ReadU8(Address(s.r[4])+length)!=0u)++length;
        memory.WriteU16(Address(s.r[3]),length);
        memory.WriteU16(Address(s.r[3]+2u),
            static_cast<std::uint16_t>(length+1u));
        memory.WriteU32(Address(s.r[3]+4u),Address(s.r[4]));
    }
    void CallOpenFile(GuestAddress target,GuestMemory& memory,
        routes::Registers& s) override
    {
        events.push_back({6u,s.sp,s.lr,target,s.r[4]});
        if(target!=0x2600u)
            throw std::runtime_error("unexpected open-file target");
        memory.WriteU32(Address(s.r[3]),0x59000u);
        memory.WriteU32(Address(s.r[6]+4u),
            path==Path::OpenExisting?3u:0u);
        s.r[3]=0u;
    }
};

Services* current=nullptr;
IndexService* index_current=nullptr;
Guest* guest_current=nullptr;

family::Dependencies AcceptedDeps(Services& s,IndexService& index)
{
    return {s,s,s,s,{s,s,s,s,s,s,index,s},s,s};
}

GuestAddress Entry(Path path)
{
    switch(path)
    {
    case Path::SlotValid:case Path::SlotInvalid:return 0x82b86108u;
    case Path::OpenExisting:case Path::OpenCreate:return 0x82be2be0u;
    case Path::InitializeExisting:return 0x82b86420u;
    case Path::RestoreLoop:return 0x82b86654u;
    }
    return 0u;
}

void SeedPath(GuestWindow& window,Path path)
{
    Seed(window,Mode::LockedWrite);
    auto memory=window.Memory();
    if(path==Path::SlotValid)
        memory.WriteU32(Record,UINT32_MAX);
    if(path==Path::OpenExisting || path==Path::OpenCreate)
    {
        memory.WriteU32(0x3400cu,0x2601u);
        if(path==Path::OpenCreate)
        {
            memory.WriteU8(0x40000u,'C');
            memory.WriteU8(0x40001u,':');
            memory.WriteU8(0x40002u,'\\');
            memory.WriteU8(0x40003u,0u);
        }
        else memory.WriteU8(0x40003u,0u);
    }
    if(path==Path::InitializeExisting || path==Path::RestoreLoop)
    {
        memory.WriteU32(0x83245708u,0x10000u);
        memory.WriteU32(0x832153b0u,0x65000u);
        memory.WriteU32(0x832153a8u,0x65100u);
        memory.WriteU32(0x30000u,UINT32_MAX);
        memory.WriteU8(0x30004u,0u);
        memory.WriteU32(0x30008u,1u);
        memory.WriteU32(Stack-176u+84u,0u);
        memory.WriteU32(Stack-176u+88u,0x30000u);
        memory.WriteU32(Stack-176u+92u,0u);
    }
}

PPCContext Initial(Path path)
{
    PPCContext c{};
    PPCRegister* fields[]={&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,
        &c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,
        &c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,
        &c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,
        &c.r30,&c.r31};
    for(unsigned index=0;index<32;++index)
        fields[index]->u64=0x1122334400000000ull+index;
    c.r1.u64=0x8877665500000000ull|Stack;
    c.r13.u64=0xaabbccdd00000000ull|Environment;
    c.lr=0xabcdef0181234567ull;
    c.ctr.u64=0x5566778899aabbccull;
    c.xer.so=1;c.xer.ca=1;c.cr0.lt=1;c.cr6.gt=1;
    if(path==Path::SlotValid || path==Path::SlotInvalid)
    {
        c.r3.u64=0xaabbccdd00000000ull|
            (path==Path::SlotValid?5u:16u);
        c.r4.u64=0x9988776600001234ull;
    }
    if(path==Path::OpenExisting || path==Path::OpenCreate)
    {
        c.r3.u64=0xaabbccdd00040000ull;
        c.r4.u64=0u;c.r5.u64=0u;
        c.r7.u64=path==Path::OpenExisting?2u:4u;
        c.r8.u64=path==Path::OpenCreate?0x04000000u:0u;
    }
    if(path==Path::RestoreLoop)
    {
        c.r1.u64-=176u;
        c.r12.u64=0x8877665500000000ull|Stack;
        c.r31.u64=c.r1.u64;
    }
    return c;
}

bool Independent(Path path,const PPCContext& c,const Services& s,
    const IndexService& index,const Guest& guest)
{
    switch(path)
    {
    case Path::SlotValid:
        return c.r3.u32==0u && s.memory.ReadU32(Record)==0x1234u &&
            index.events.empty() && guest.events.empty();
    case Path::SlotInvalid:
        return c.r3.u64==UINT64_MAX && s.memory.ReadU32(0x83215210u)==9u &&
            s.memory.ReadU32(0x83215214u)==0u;
    case Path::OpenExisting:
        return c.r3.u32==0x59000u && s.memory.ReadU32(ErrorState+352u)==183u &&
            guest.events.size()==2u && guest.events[0][0]==5u &&
            guest.events[1][0]==6u;
    case Path::OpenCreate:
        return c.r3.u32==0x59000u && s.memory.ReadU32(ErrorState+352u)==0u &&
            guest.events.size()==2u && guest.events[0][0]==5u &&
            guest.events[1][0]==6u;
    case Path::InitializeExisting:
        return c.r3.u32==0u && s.memory.ReadU8(0x30004u)==1u &&
            !index.events.empty() && guest.events.size()==1u &&
            guest.events[0][0]==3u;
    case Path::RestoreLoop:
        return !index.events.empty() && guest.events.empty() &&
            c.r24.u64==UINT64_MAX && c.r26.u64==1u &&
            c.r29.u32==0x83378d80u;
    }
    return false;
}

void Check(Path path)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    SeedPath(original,path);SeedPath(recovered,path);
    Services expected(original,Mode::LockedWrite);
    Services actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Guest expected_guest(path),actual_guest(path);
    auto raw=Initial(path);
    auto state=FromPpc(raw);
    current=&expected;index_current=&expected_index;
    guest_current=&expected_guest;active=&expected;
    switch(Entry(path))
    {
    case 0x82b86108u:__imp__sub_82B86108(raw,original.Bytes());break;
    case 0x82be2be0u:__imp__sub_82BE2BE0(raw,original.Bytes());break;
    case 0x82b86420u:__imp__sub_82B86420(raw,original.Bytes());break;
    case 0x82b86654u:__imp__sub_82B86654(raw,original.Bytes());break;
    default:throw std::runtime_error("unknown stream open entry");
    }
    current=nullptr;index_current=nullptr;guest_current=nullptr;active=nullptr;
    if(!Independent(path,raw,expected,expected_index,expected_guest))
    {
        std::fprintf(stderr,"EXPECT open-routes path=%u r3=%llX "
            "errno=%08X last=%08X index=%zu guest=%zu\n",
            static_cast<unsigned>(path),
            static_cast<unsigned long long>(raw.r3.u64),
            expected.memory.ReadU32(0x83215210u),
            expected.memory.ReadU32(ErrorState+352u),
            expected_index.events.size(),expected_guest.events.size());
        throw std::runtime_error("independent stream open path");
    }
    if(!routes::Apply(Entry(path),actual.memory,
        AcceptedDeps(actual,actual_index),actual_guest,state))
        throw std::runtime_error("missing stream open selected entry");
    const auto observed=FromPpc(raw);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events ||
        expected_index.events!=actual_index.events ||
        expected_guest.events!=actual_guest.events)
    {
        std::fprintf(stderr,"FAIL open-routes path=%u state=%u RAM=%u "
            "accepted=%zu/%zu index=%zu/%zu guest=%zu/%zu\n",
            static_cast<unsigned>(path),same_state,same_ram,
            expected.events.size(),actual.events.size(),
            expected_index.events.size(),actual_index.events.size(),
            expected_guest.events.size(),actual_guest.events.size());
        for(unsigned index=0;index<32u;++index)
            if(observed.r[index]!=state.r[index])
                std::fprintf(stderr," r%u=%llX/%llX\n",index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        throw std::runtime_error("stream open selected context differs");
    }
}
} // namespace

void OriginalOpenLower(GuestAddress address,PPCContext& c,std::uint8_t*)
{
    auto state=FromPpc(c);
    if(!routes::ApplyAcceptedLower(address,current->memory,
        AcceptedDeps(*current,*index_current),*guest_current,state))
        throw std::runtime_error("missing accepted open-route lower");
    ToPpc(c,state);
}
void OriginalOpenAllocation(PPCContext& c,std::uint8_t*)
{
    auto state=FromPpc(c);
    guest_current->AllocateCrtRecord(current->memory,state);
    ToPpc(c,state);
}
void OriginalOpenNative(GuestAddress entry,PPCContext& c,std::uint8_t*)
{
    auto state=FromPpc(c);
    switch(entry)
    {
    case 0x830d9c6cu:
        guest_current->EnterCriticalSection(current->memory,state);break;
    case 0x830d9c7cu:
        guest_current->LeaveCriticalSection(current->memory,state);break;
    case 0x830d9dfcu:
        guest_current->InitAnsiString(current->memory,state);break;
    default:throw std::runtime_error("unknown native open-route import");
    }
    ToPpc(c,state);
}
void OriginalOpenIndirect(GuestAddress target,PPCContext& c,std::uint8_t*)
{
    auto state=FromPpc(c);
    guest_current->CallOpenFile(target,current->memory,state);
    ToPpc(c,state);
}

int main()
{
    try
    {
        for(const auto path:Paths)Check(path);
        std::printf("PASS crt-stream-open-routes-context %zu original PPC cases\n",
            Paths.size());
        std::puts("LIMIT five full selected bodies; accepted lower selected ABI and allocation guest boundary, native/indirect internals, unselected CR, faults, MMIO, concurrency and runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}

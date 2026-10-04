#include "crt_stream_oracle_fixture.h"

namespace
{
constexpr Case Cases[]={{0x82b81b88u,Mode::CountZero},
    {0x82b81b88u,Mode::NullBuffer},{0x82b81b88u,Mode::OddMode},
    {0x82b81b88u,Mode::Binary},{0x82b81b88u,Mode::Text},
    {0x82b81b88u,Mode::ShortWrite},
    {0x82b81b88u,Mode::NativeFailureFive},
    {0x82b85ea8u,Mode::Seek},
    {0x82b85ea8u,Mode::NegativeSeek},
    {0x82b81de0u,Mode::LockedWrite},
    {0x82b85f70u,Mode::LockedSeek},
    {0x82b81de0u,Mode::LockedInvalidWrite},
    {0x82b85f70u,Mode::LockedInvalidSeek},
    {0x82b81de0u,Mode::InvalidAfterLock},
    {0x82b82558u,Mode::BufferedChar},
    {0x82b82558u,Mode::FlushChar}};

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0xabcdef01u;
    c.r13.u64=Environment;
    c.r3.u64=item.address==0x82b82558u?0x51u:5u;
    c.r4.u64=item.address==0x82b82558u?Stream:0x40000u;
    c.r5.u64=3;
    if (item.mode==Mode::CountZero) c.r5.u64=0;
    if (item.mode==Mode::NullBuffer) c.r4.u64=0;
    if (item.mode==Mode::LockedInvalidWrite ||
        item.mode==Mode::LockedInvalidSeek)
        c.r3.u64=UINT64_MAX-1u;
    if (item.mode==Mode::Seek || item.mode==Mode::LockedSeek)
    {c.r4.u64=0x0000000100000004ull;c.r5.u64=0;}
    if (item.mode==Mode::NegativeSeek)
    {c.r4.u64=0xabcdef0100000004ull;c.r5.u64=0;}
    if (item.mode==Mode::LockedInvalidSeek)
    {c.r4.u64=0x0000000100000004ull;c.r5.u64=0;}
    return c;
}

bool ExpectedPath(const Case& item,const Services& s)
{
    switch (item.mode)
    {
    case Mode::CountZero:return s.writes==0 && s.seeks==0;
    case Mode::NullBuffer:return s.traps==1 && s.writes==0;
    case Mode::OddMode:return s.traps==1 && s.writes==0;
    case Mode::Binary:return s.writes==1 && s.seeks==0;
    case Mode::Text:return s.writes==1 && s.seeks==0 &&
        s.verified_text_buffers==1;
    case Mode::ShortWrite:return s.writes==1;
    case Mode::NativeFailureFive:return s.writes==1 &&
        s.converted_errors==1 &&
        s.memory.ReadU32(ErrorState+352u)==5u &&
        s.memory.ReadU32(0x83215210u)==9u &&
        s.memory.ReadU32(0x83215214u)==5u;
    case Mode::Seek:return s.seeks==1;
    case Mode::NegativeSeek:return s.seeks==0 &&
        s.memory.ReadU32(ErrorState+352u)==131u &&
        s.memory.ReadU32(0x83215210u)==22u &&
        s.memory.ReadU32(0x83215214u)==131u;
    case Mode::LockedWrite:return s.locks==1 && s.unlocks==1 && s.writes==1;
    case Mode::LockedSeek:return s.locks==1 && s.unlocks==1 && s.seeks==1;
    case Mode::LockedInvalidWrite:
    case Mode::LockedInvalidSeek:
        return s.locks==0 && s.unlocks==0 && s.writes==0 && s.seeks==0;
    case Mode::InvalidAfterLock:return s.locks==1 && s.unlocks==1 && s.writes==0;
    case Mode::BufferedChar:return s.writes==0 && s.seeks==0;
    case Mode::FlushChar:return s.locks==1 && s.unlocks==1 && s.writes==1;
    }
    return false;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,item.mode);Seed(recovered,item.mode);
    Services expected(original,item.mode),actual(recovered,item.mode);
    PPCContext raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    switch (item.address)
    {
    case 0x82b85ea8u:__imp__sub_82B85EA8(raw,original.Bytes());break;
    case 0x82b81b88u:__imp__sub_82B81B88(raw,original.Bytes());break;
    case 0x82b81de0u:__imp__sub_82B81DE0(raw,original.Bytes());break;
    case 0x82b85f70u:__imp__sub_82B85F70(raw,original.Bytes());break;
    case 0x82b82558u:__imp__sub_82B82558(raw,original.Bytes());break;
    default:throw std::runtime_error("unknown selected entry");
    }
    active=nullptr;
    if (!ExpectedPath(item,expected))
    {
        std::fprintf(stderr,"fixture path %08X mode=%u writes=%u seeks=%u "
            "locks=%u unlocks=%u traps=%u text=%u converted=%u "
            "last=%08X errno=%08X crt=%08X r3=%llX\n",
            item.address,static_cast<unsigned>(item.mode),expected.writes,
            expected.seeks,expected.locks,expected.unlocks,expected.traps,
            expected.verified_text_buffers,expected.converted_errors,
            expected.memory.ReadU32(ErrorState+352u),
            expected.memory.ReadU32(0x83215210u),
            expected.memory.ReadU32(0x83215214u),
            static_cast<unsigned long long>(raw.r3.u64));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if (!family::Apply(item.address,actual.memory,Dependencies(actual),state))
        throw std::runtime_error("recovered entry missing");
    const auto was=FromPpc(raw);
    if (!Same(was,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL stream-operations %08X mode=%u "
            "r3=%llx/%llx sp=%llx/%llx lr=%llx/%llx "
            "events=%zu/%zu RAM=%u\n",item.address,
            static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(was.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(was.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(was.lr),
            static_cast<unsigned long long>(state.lr),
            expected.events.size(),actual.events.size(),
            original.EqualCommitted(recovered));
        return false;
    }
    return true;
}
} // namespace

int main()
{
    try
    {
        for (const auto& item:Cases)
            if (!Check(item)) return 1;
        GuestWindow window(Regions),baseline(Regions);
        Seed(window,Mode::Binary);Seed(baseline,Mode::Binary);
        Services services(window,Mode::Binary);
        auto state=FromPpc(Initial(Cases[0]));
        const auto saved=state;
        if (family::Apply(0xffffffffu,services.memory,
                Dependencies(services),state) || !Same(saved,state) ||
            !services.events.empty() || !window.EqualCommitted(baseline))
            throw std::runtime_error("unknown entry changed state");
        std::printf("PASS crt-stream-operations %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT selected state/RAM; accepted lower ABI and native internals, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}

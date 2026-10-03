#include "lo_semantics/crt_stream_io.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"
#include "lo_semantics/thread_state.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_stream_io;
namespace abi = recovery_abi;
namespace test = lo::semantic::gpu::test;

constexpr GuestAddress Stack = 0x10000u, MovedStack = 0x18000u;
constexpr GuestAddress Count = 0x20000u, IoStatus = 0x21000u;
constexpr GuestAddress Table = 0x22000u, Environment = 0x30000u;
constexpr GuestAddress ErrorState = 0x31000u;
constexpr std::array<test::Region, 2> Regions{{{0, 0x50000},
    {0x831e0000u, 0x10000}}};
constexpr std::uint64_t HighSp = 0x1234567800000000ull;

enum class Mode
{
    ExternalSuccess, ExternalPending, ExternalFailure,
    LocalSuccess, LocalPending, WaitFailure,
    Flag0, Flag1, Flag2, Flag3, Negative, Odd,
    ReadFailure, WriteFailure, ReportZero, LiveFrame
};
constexpr std::array<Mode, 16> Cases{{Mode::ExternalSuccess,
    Mode::ExternalPending, Mode::ExternalFailure, Mode::LocalSuccess,
    Mode::LocalPending, Mode::WaitFailure, Mode::Flag0, Mode::Flag1,
    Mode::Flag2, Mode::Flag3, Mode::Negative, Mode::Odd,
    Mode::ReadFailure, Mode::WriteFailure, Mode::ReportZero,
    Mode::LiveFrame}};

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers s{};
    s.sp=c.r1.u64; s.lr=c.lr; s.ctr=c.ctr.u64;
    s.r3=c.r3.u64; s.r4=c.r4.u64; s.r5=c.r5.u64;
    s.r6=c.r6.u64; s.r7=c.r7.u64; s.r8=c.r8.u64;
    s.r9=c.r9.u64; s.r10=c.r10.u64; s.r11=c.r11.u64;
    s.r12=c.r12.u64; s.r13=c.r13.u64;
    s.r28=c.r28.u64; s.r29=c.r29.u64;
    s.r30=c.r30.u64; s.r31=c.r31.u64;
    s.xer_so=c.xer.so;
    s.cr0={c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};
    s.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};
    return s;
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    c.r1.u64=s.sp; c.lr=s.lr; c.ctr.u64=s.ctr;
    c.r3.u64=s.r3; c.r4.u64=s.r4; c.r5.u64=s.r5;
    c.r6.u64=s.r6; c.r7.u64=s.r7; c.r8.u64=s.r8;
    c.r9.u64=s.r9; c.r10.u64=s.r10; c.r11.u64=s.r11;
    c.r12.u64=s.r12; c.r13.u64=s.r13;
    c.r28.u64=s.r28; c.r29.u64=s.r29;
    c.r30.u64=s.r30; c.r31.u64=s.r31;
    c.xer.so=s.xer_so;
    c.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,{s.cr0.so}};
    c.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,{s.cr6.so}};
}

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.sp==b.sp && a.lr==b.lr && a.ctr==b.ctr &&
        a.r3==b.r3 && a.r4==b.r4 && a.r5==b.r5 &&
        a.r6==b.r6 && a.r7==b.r7 && a.r8==b.r8 &&
        a.r9==b.r9 && a.r10==b.r10 && a.r11==b.r11 &&
        a.r12==b.r12 && a.r13==b.r13 &&
        a.r28==b.r28 && a.r29==b.r29 &&
        a.r30==b.r30 && a.r31==b.r31 &&
        a.xer_so==b.xer_so &&
        a.cr0.lt==b.cr0.lt && a.cr0.gt==b.cr0.gt &&
        a.cr0.eq==b.cr0.eq && a.cr0.so==b.cr0.so &&
        a.cr6.lt==b.cr6.lt && a.cr6.gt==b.cr6.gt &&
        a.cr6.eq==b.cr6.eq && a.cr6.so==b.cr6.so;
}

struct Services final : family::NativeServices
{
    Mode mode;
    unsigned writes = 0, waits = 0, indirects = 0, converts = 0;
    explicit Services(Mode value) : mode(value) {}

    void NtWriteFile(GuestMemory& memory, family::Registers& state) override
    {
        ++writes;
        if (state.r3 != 0xfedcba9800003400ull ||
            state.r8 != 0xabcdef0100004000ull ||
            state.r9 != 0x3456789000000040ull)
            throw std::runtime_error("write argument registers");
        const bool external = abi::Address(state.r7) == IoStatus;
        if (external)
        {
            if (state.lr != 0x82be2884u ||
                state.r7 != 0xbbbbbbbb00021000ull ||
                state.r10 != state.sp + 88u ||
                (state.r6 != 0 && state.r6 != state.r7) ||
                state.cr0.eq != (mode == Mode::ExternalSuccess))
                throw std::runtime_error("external write ABI");
            memory.WriteU32(IoStatus + 4u, 0x49u);
            state.r3 = mode == Mode::ExternalPending ? 259u :
                mode == Mode::ExternalFailure ? 0xffffffffc0000017ull : 0u;
        }
        else
        {
            if (state.lr != 0x82be28d0u ||
                state.r7 != state.sp + 80u ||
                state.r4 != 0 || state.r5 != 0 || state.r6 != 0 || state.r10 != 0)
                throw std::runtime_error("local write ABI");
            memory.WriteU32(abi::Address(state.sp + 80u), 0);
            memory.WriteU32(abi::Address(state.sp + 84u), 0x35u);
            state.r3 = mode == Mode::LocalPending || mode == Mode::WaitFailure ?
                259u : 0u;
        }
    }

    void NtWaitForSingleObjectEx(GuestMemory& memory,
        family::Registers& state) override
    {
        ++waits;
        if (state.lr != 0x82be28ecu ||
            state.r3 != 0xfedcba9800003400ull || state.r4 != 1 ||
            state.r5 != 0 || state.r6 != 0)
            throw std::runtime_error("wait ABI");
        memory.WriteU32(abi::Address(state.sp + 80u), 0);
        state.r3 = mode == Mode::WaitFailure ?
            0xffffffff80000005ull : 0u;
    }

    void CallIndirect(GuestMemory& memory, GuestAddress target,
        family::Registers& state) override
    {
        ++indirects;
        const bool read = target == 0x2400u;
        if ((!read && target != 0x2500u) ||
            state.r3 != 0xabcdefff00003500ull ||
            state.r4 != state.sp + 88u)
            throw std::runtime_error("indirect target/base ABI");
        if (read)
        {
            const bool wide = mode == Mode::Flag2 || mode == Mode::LiveFrame;
            if (state.lr != (wide ? 0x82be29a8u : 0x82be29e8u) ||
                state.r6 != (wide ? 56u : 8u) ||
                state.r7 != (wide ? 34u : 14u) ||
                state.r5 != state.sp + (wide ? 96u : 80u) ||
                (mode == Mode::Flag2 && !state.cr6.lt) ||
                (mode == Mode::Flag3 && !state.cr6.eq))
                throw std::runtime_error("indirect read ABI");
            if (mode == Mode::ReadFailure)
                state.r3 = 0xffffffffc0000017ull;
            else
            {
                if (mode == Mode::LiveFrame)
                    state.sp = 0x9876543200000000ull | (MovedStack - 192u);
                abi::WriteU64(memory, abi::Address(state.sp + (wide ? 136u : 80u)), 0x20u);
                state.r3 = 0;
            }
        }
        else
        {
            if (state.lr != 0x82be2a54u || state.r6 != 8 || state.r7 != 14 ||
                state.r5 != state.sp + 80u)
                throw std::runtime_error("indirect write ABI");
            memory.WriteU32(abi::Address(state.sp + 84u),
                mode == Mode::ReportZero ? 0xffffffffu : 0x31u);
            state.r3 = mode == Mode::WriteFailure ?
                0xffffffffc0000017ull : 0u;
        }
    }

    void NtStatusToDosError(GuestMemory&, crt_status_error::Registers& state) override
    {
        ++converts;
        if (state.lr != 0x827ca638u)
            throw std::runtime_error("status conversion LR");
        state.r3 = 0x1234567800000042ull;
    }
};

Services* active = nullptr;

void Seed(test::GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto m = window.Memory();
    m.WriteU32(0x831e7df4u, Table);
    m.WriteU32(Table + 32u, 0x2403u);
    m.WriteU32(Table + 36u, 0x2501u);
    m.WriteU32(Environment + 336u, 0);
    m.WriteU32(Environment + 256u, ErrorState);
    m.WriteU32(ErrorState + 352u, 0xdeadbeefu);
    m.WriteU32(Count, 0);
    m.WriteU32(IoStatus + 8u, 0x1111u);
    m.WriteU32(IoStatus + 12u, 0x2222u);
    m.WriteU32(IoStatus + 16u, mode == Mode::ExternalSuccess ? 0u : 1u);
    if (mode == Mode::LiveFrame)
    {
        abi::WriteU64(m, MovedStack - 40u, 0x456789abcdef0001ull);
        abi::WriteU64(m, MovedStack - 32u, 0x456789abcdef0002ull);
        abi::WriteU64(m, MovedStack - 24u, 0x456789abcdef0003ull);
        abi::WriteU64(m, MovedStack - 16u, 0x456789abcdef0004ull);
        m.WriteU32(MovedStack - 8u, 0x76543210u);
    }
}

PPCContext Initial(Mode mode)
{
    PPCContext c{};
    const bool write_file = mode <= Mode::WaitFailure;
    c.r1.u64 = HighSp | Stack;
    c.lr = 0x1234567887654321ull;
    c.r3.u64 = write_file ? 0xfedcba9800003400ull :
        0xabcdefff00003500ull;
    c.r4.u64 = write_file ? 0xabcdef0100004000ull :
        mode == Mode::Negative ? 0xdeadbeefffffffffull :
        mode == Mode::Odd ? 0xdeadbeef00000003ull :
        0xdeadbeef00000010ull;
    c.r5.u64 = write_file ? 0x3456789000000040ull :
        mode == Mode::Negative || mode == Mode::Odd ? 0 :
        0xaabbccdd00020000ull;
    c.r6.u64 = write_file ? 0xaaaaaaaa00020000ull :
        mode == Mode::Flag1 || mode == Mode::ReadFailure ? 1u :
        mode == Mode::Flag2 || mode == Mode::LiveFrame ? 2u :
        mode == Mode::Flag3 ? 3u : 0u;
    c.r7.u64 = write_file && mode <= Mode::ExternalFailure ?
        0xbbbbbbbb00021000ull : 0;
    c.r13.u64 = 0xcafebabe00030000ull;
    c.r28.u64 = 0x1122334455667788ull;
    c.r29.u64 = 0x2233445566778899ull;
    c.r30.u64 = 0x33445566778899aaull;
    c.r31.u64 = 0x445566778899aabbull;
    c.r8.u64 = 0xaaaa111122223333ull;
    c.r9.u64 = 0xbbbb111122223333ull;
    c.r10.u64 = 0xcccc111122223333ull;
    c.r11.u64 = 0xdddd111122223333ull;
    c.r12.u64 = 0xeeee111122223333ull;
    c.ctr.u64 = 0xffff111122223333ull;
    c.xer.so = 1;
    return c;
}

bool Run(Mode mode)
{
    test::GuestWindow raw_window(Regions), recovered_window(Regions);
    Seed(raw_window, mode);
    for (const auto region : Regions)
        std::memcpy(recovered_window.Bytes() + region.base,
            raw_window.Bytes() + region.base, region.size);
    auto original = Initial(mode);
    const auto initial = original;
    Services raw_services(mode), recovered_services(mode);
    active = &raw_services;
    const auto entry = mode <= Mode::WaitFailure ? 0x82be2810u : 0x82be2938u;
    if (entry == 0x82be2810u)
        __imp__sub_82BE2810(original, raw_window.Bytes());
    else
        __imp__sub_82BE2938(original, raw_window.Bytes());
    active = &recovered_services;
    auto state = FromPpc(initial);
    auto memory = recovered_window.Memory();
    if (!family::Apply(entry, memory, recovered_services, state))
        throw std::runtime_error("known entry rejected");
    active = nullptr;
    if (!Same(FromPpc(original), state) ||
        !raw_window.EqualCommitted(recovered_window) ||
        raw_services.writes != recovered_services.writes ||
        raw_services.waits != recovered_services.waits ||
        raw_services.indirects != recovered_services.indirects ||
        raw_services.converts != recovered_services.converts)
    {
        std::fprintf(stderr, "FAIL crt-stream-io mode %u\n", unsigned(mode));
        return false;
    }
    return true;
}
} // namespace

void OriginalSave28(PPCContext& c, std::uint8_t* bytes)
{
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    abi::WriteU64(m, abi::Address(c.r1.u64 - 40u), c.r28.u64);
    abi::WriteU64(m, abi::Address(c.r1.u64 - 32u), c.r29.u64);
    abi::WriteU64(m, abi::Address(c.r1.u64 - 24u), c.r30.u64);
    abi::WriteU64(m, abi::Address(c.r1.u64 - 16u), c.r31.u64);
    m.WriteU32(abi::Address(c.r1.u64 - 8u), abi::Address(c.r12.u64));
}
void OriginalSave29(PPCContext& c, std::uint8_t* bytes)
{
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    abi::WriteU64(m, abi::Address(c.r1.u64 - 32u), c.r29.u64);
    abi::WriteU64(m, abi::Address(c.r1.u64 - 24u), c.r30.u64);
    abi::WriteU64(m, abi::Address(c.r1.u64 - 16u), c.r31.u64);
    m.WriteU32(abi::Address(c.r1.u64 - 8u), abi::Address(c.r12.u64));
}
void OriginalRest28(PPCContext& c, std::uint8_t* bytes)
{
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    c.r28.u64=abi::ReadU64(m, abi::Address(c.r1.u64 - 40u));
    c.r29.u64=abi::ReadU64(m, abi::Address(c.r1.u64 - 32u));
    c.r30.u64=abi::ReadU64(m, abi::Address(c.r1.u64 - 24u));
    c.r31.u64=abi::ReadU64(m, abi::Address(c.r1.u64 - 16u));
    c.r12.u64=m.ReadU32(abi::Address(c.r1.u64 - 8u));
    c.lr=c.r12.u64;
}
void OriginalRest29(PPCContext& c, std::uint8_t* bytes)
{
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    c.r29.u64=abi::ReadU64(m, abi::Address(c.r1.u64 - 32u));
    c.r30.u64=abi::ReadU64(m, abi::Address(c.r1.u64 - 24u));
    c.r31.u64=abi::ReadU64(m, abi::Address(c.r1.u64 - 16u));
    c.r12.u64=m.ReadU32(abi::Address(c.r1.u64 - 8u));
    c.lr=c.r12.u64;
}
void OriginalWrite(PPCContext& c, std::uint8_t* bytes)
{
    auto s=FromPpc(c);
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    active->NtWriteFile(m, s);
    ToPpc(c, s);
}
void OriginalWait(PPCContext& c, std::uint8_t* bytes)
{
    auto s=FromPpc(c);
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    active->NtWaitForSingleObjectEx(m, s);
    ToPpc(c, s);
}
void OriginalIndirect(PPCContext& c, std::uint8_t* bytes,
    std::uint32_t target)
{
    auto s=FromPpc(c);
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    active->CallIndirect(m, target, s);
    ToPpc(c, s);
}
void OriginalConvert(PPCContext& c, std::uint8_t* bytes)
{
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    crt_status_error::Registers s{c.r1.u64, c.lr, c.r3.u64,
        c.r11.u64, c.r12.u64, c.r13.u64, c.xer.so,
        {c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so}};
    if (!crt_status_error::Apply(0x827ca628u, m, *active, s))
        throw std::runtime_error("lower status entry rejected");
    c.r1.u64=s.sp; c.lr=s.lr; c.r3.u64=s.r3;
    c.r11.u64=s.r11; c.r12.u64=s.r12; c.r13.u64=s.r13;
    c.xer.so=s.xer_so;
    c.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,{s.cr6.so}};
}
void OriginalStore(PPCContext& c, std::uint8_t* bytes)
{
    GuestMemory m(0, std::span<std::uint8_t>(bytes, test::GuestWindow::Space));
    c.r11.u64=m.ReadU32(abi::Address(c.r13.u64 + 336u));
    const bool clear=abi::Address(c.r11.u64)==0;
    c.cr6={0,static_cast<std::uint8_t>(!clear),
        static_cast<std::uint8_t>(clear),{c.xer.so}};
    if (clear) c.r11.u64=m.ReadU32(abi::Address(c.r13.u64 + 256u));
    StoreThreadFailureCode(m, abi::Address(c.r13.u64), c.r3.u32);
}
void OriginalReport(PPCContext& c, std::uint8_t* bytes)
{
    OriginalStore(c, bytes);
}

int main()
{
    try
    {
        for (const auto mode : Cases)
            if (!Run(mode)) return 1;
        std::array<std::uint8_t, 64> bytes{};
        GuestMemory memory(0, std::span<std::uint8_t>(bytes));
        auto state = FromPpc(Initial(Mode::Flag0));
        const auto saved = state;
        Services services(Mode::Flag0);
        if (family::Apply(0xffffffffu, memory, services, state) ||
            !Same(state, saved) || bytes != std::array<std::uint8_t, 64>{} ||
            services.writes || services.waits || services.indirects || services.converts)
            throw std::runtime_error("unknown entry effects");
        std::puts("PASS crt-stream-io 16 original PPC cases + unknown");
        std::puts("LIMIT selected native/indirect callback profile and ordinary RAM; native internals, faults/MMIO/concurrency/runtime unverified");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

#include "lo_semantics/crt_formatting_support.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_formatting_support;
using test::GuestWindow;
constexpr test::Region Regions[] = {
    {0, 0x60000}, {0x83216000u, 0x1000},
    {0x832D3000u, 0x1000}, {0x821A8000u, 0x1000},
};
constexpr std::uint64_t Stack = 0x1234567800010000ull;
constexpr std::uint64_t MovedFrame = 0xABCDEF1200030000ull;
constexpr std::uint64_t MovedSaved31 = 0xFEDCBA9800000031ull;
constexpr GuestAddress Byte = 0x20000u, Destination = 0x21000u;
constexpr GuestAddress Unicode = 0x22000u, Ansi = 0x23000u;
enum class Mode { NullInput, ZeroExtent, NulByte, ByteOutput, ByteCount,
    TaggedMatch, TaggedMiss, TaggedUnderflow, UnicodeSuccess,
    UnicodeFailure, UnicodeMovedStack };
struct Case { GuestAddress entry; Mode mode; };
constexpr Case Cases[] = {
    {0x822A07A0u, Mode::NullInput}, {0x822A07A0u, Mode::ZeroExtent},
    {0x822A07A0u, Mode::NulByte}, {0x822A07A0u, Mode::ByteOutput},
    {0x822A07A0u, Mode::ByteCount}, {0x82B85420u, Mode::TaggedMatch},
    {0x82B85420u, Mode::TaggedMiss}, {0x82B85420u, Mode::TaggedUnderflow},
    {0x82BE4700u, Mode::UnicodeSuccess}, {0x82BE4700u, Mode::UnicodeFailure},
    {0x82BE4700u, Mode::UnicodeMovedStack},
};
using Event = std::array<std::uint64_t, 6>;

void Require(bool condition, const char* message)
{ if (!condition) throw std::runtime_error(message); }

struct Services final : family::NativeServices
{
    Mode mode;
    std::vector<Event> events;
    explicit Services(Mode selected) : mode(selected) {}
    void InitializeUnicodeString(GuestMemory& memory, family::Registers& s) override
    {
        events.push_back({1, s.r3, s.r4, s.r5, s.sp, s.lr});
        Require(s.r3 == Stack - 24u && s.r4 == 0x1122334400022000ull &&
            s.lr == 0x82BE471Cu, "Unicode initializer arguments");
        memory.WriteU16(recovery_abi::Address(s.r3), 4);
        memory.WriteU16(recovery_abi::Address(s.r3 + 2u), 6);
        memory.WriteU32(recovery_abi::Address(s.r3 + 4u), Unicode);
        s.r8 = 0xEEFF001122334488ull;
        s.r10 = 0x0123456789ABCDEFull;
        s.cr6 = {1, 0, 0, 0};
        if (mode == Mode::UnicodeMovedStack)
        {
            s.sp = MovedFrame;
            memory.WriteU16(recovery_abi::Address(s.sp + 88u), 4);
            memory.WriteU16(recovery_abi::Address(s.sp + 90u), 6);
            memory.WriteU32(recovery_abi::Address(s.sp + 92u), Unicode);
            recovery_abi::WriteU64(memory, recovery_abi::Address(s.sp + 96u),
                MovedSaved31);
            memory.WriteU32(recovery_abi::Address(s.sp + 104u), 0x76543210u);
        }
    }
    void UnicodeStringToAnsiString(GuestMemory& memory, family::Registers& s) override
    {
        events.push_back({2, s.r3, s.r4, s.r5, s.sp, s.lr});
        const auto frame = mode == Mode::UnicodeMovedStack ? MovedFrame : Stack - 112u;
        Require(s.sp == frame && s.r3 == frame + 80u && s.r4 == frame + 88u &&
            s.r5 == 1 && s.lr == 0x82BE472Cu &&
            memory.ReadU32(recovery_abi::Address(s.r4 + 4u)) == Unicode,
            "converter must use live Unicode descriptor");
        memory.WriteU16(recovery_abi::Address(s.r3), 2);
        memory.WriteU16(recovery_abi::Address(s.r3 + 2u), 3);
        memory.WriteU32(recovery_abi::Address(s.r3 + 4u), Ansi);
        s.r7 = 0x8877665544332211ull;
        s.r9 = 0xAABBCCDD00000009ull;
        s.xer_so = 1;
        s.r3 = mode == Mode::UnicodeFailure ?
            0xF1234567C0000001ull : 0xF123456700000000ull;
    }
    void FreeAnsiString(GuestMemory&, family::Registers& s) override
    {
        events.push_back({5, s.r3, s.r4, s.r5, s.sp, s.lr});
        Require(s.r3 == s.sp + 80u && s.lr == 0x82BE4758u,
            "free must use live ANSI descriptor");
        s.r3 = 0x5566778899AABBCCull; // Preserve this void import's residual r3.
        s.r11 = 0x1122334455667788ull;
        s.lr = 0xABCDEF00ABCDEF00ull;
    }
    void InitAnsiString(GuestMemory& memory, GuestAddress descriptor,
        GuestAddress message) override
    {
        events.push_back({3, descriptor, message});
        const auto expected = mode == Mode::UnicodeFailure ? 0x821A8F04u : Ansi;
        Require(message == expected, "converted or fallback error string");
        std::uint16_t length = 0;
        while (memory.ReadU8(message + length) != 0)
        {
            Require(length < 32, "unterminated fixture string");
            ++length;
        }
        memory.WriteU16(descriptor, length);
        memory.WriteU16(descriptor + 2u, static_cast<std::uint16_t>(length + 1u));
        memory.WriteU32(descriptor + 4u, message);
    }
    std::uint64_t WriteAnsi(GuestAddress buffer, std::uint16_t length) override
    {
        events.push_back({4, buffer, length});
        return 0xA1B2C3D4E5F60718ull;
    }
};
Services* active = nullptr;
GuestMemory* active_memory = nullptr;

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers s{};
    s.sp=c.r1.u64; s.lr=c.lr;
    s.r3=c.r3.u64; s.r4=c.r4.u64; s.r5=c.r5.u64;
    s.r6=c.r6.u64; s.r7=c.r7.u64; s.r8=c.r8.u64;
    s.r9=c.r9.u64; s.r10=c.r10.u64; s.r11=c.r11.u64;
    s.r12=c.r12.u64; s.r13=c.r13.u64; s.r31=c.r31.u64;
    s.xer_so=c.xer.so;
    s.cr0={c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};
    s.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};
    return s;
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    c.r1.u64=s.sp; c.lr=s.lr;
    c.r3.u64=s.r3; c.r4.u64=s.r4; c.r5.u64=s.r5;
    c.r6.u64=s.r6; c.r7.u64=s.r7; c.r8.u64=s.r8;
    c.r9.u64=s.r9; c.r10.u64=s.r10; c.r11.u64=s.r11;
    c.r12.u64=s.r12; c.r13.u64=s.r13; c.r31.u64=s.r31;
    c.xer.so=s.xer_so;
    c.cr0.lt=s.cr0.lt; c.cr0.gt=s.cr0.gt; c.cr0.eq=s.cr0.eq; c.cr0.so=s.cr0.so;
    c.cr6.lt=s.cr6.lt; c.cr6.gt=s.cr6.gt; c.cr6.eq=s.cr6.eq; c.cr6.so=s.cr6.so;
}
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=Stack; c.lr=0x9988776687654321ull;
    c.r3.u64=0xAABBCCDD00021000ull;
    c.r4.u64=0x1122334400020000ull;
    c.r5.u64=0x1234567800000001ull;
    c.r6.u64=6; c.r7.u64=7; c.r8.u64=8; c.r9.u64=9;
    c.r10.u64=0x123456780000000Aull;
    c.r11.u64=0x123456780000000Bull;
    c.r12.u64=0x123456780000000Cull; c.r13.u64=13;
    c.r31.u64=0x1234567800000031ull;
    c.xer.so=1; c.cr0.gt=1; c.cr6.lt=1;
    if (item.mode == Mode::NullInput) c.r4.u64=0xDEADBEEF00000000ull;
    if (item.mode == Mode::ZeroExtent) c.r5.u64=0xDEADBEEF00000000ull;
    if (item.mode == Mode::ByteCount) c.r3.u64=0xDEADBEEF00000000ull;
    if (item.entry == 0x82BE4700u) c.r3.u64=0x1122334400022000ull;
    return c;
}
void Seed(GuestWindow& window, const Case& item)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU8(Byte, item.mode == Mode::NulByte ? 0 : 0xE9u);
    memory.WriteU16(Destination, 0xABCDu);
    memory.WriteU32(0x83216000u, item.mode == Mode::TaggedMatch ? 0xFFFFFFFEu : 2);
    memory.WriteU32(0x832D3CE8u, item.mode == Mode::TaggedMatch ? 0xFFFFFFFFu :
        item.mode == Mode::TaggedUnderflow ? 9 : 1);
    memory.WriteU16(Unicode, 'o'); memory.WriteU16(Unicode + 2u, 'k');
    memory.WriteU8(Ansi, 'o'); memory.WriteU8(Ansi + 1u, 'k');
    const char fallback[] = "conversion failed";
    for (unsigned i=0; i<sizeof(fallback); ++i)
        memory.WriteU8(0x821A8F04u + i, static_cast<std::uint8_t>(fallback[i]));
}
void Check(const Case& item)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original,item); Seed(recovered,item);
    auto original_memory=original.Memory(), recovered_memory=recovered.Memory();
    Services left(item.mode), right(item.mode);
    auto c=Initial(item);
    auto s=FromPpc(c);
    active=&left; active_memory=&original_memory;
    switch(item.entry)
    {
    case 0x822A07A0u: __imp__sub_822A07A0(c,original.Bytes()); break;
    case 0x82B85420u: __imp__sub_82B85420(c,original.Bytes()); break;
    case 0x82BE4700u: __imp__sub_82BE4700(c,original.Bytes()); break;
    default: throw std::runtime_error("invalid fixture entry");
    }
    Require(family::Apply(item.entry,recovered_memory,right,s), "entry dispatch");
    if (!(FromPpc(c)==s) || !original.EqualCommitted(recovered) || left.events!=right.events)
    {
        std::fprintf(stderr,"mismatch %08X mode=%u r3=%llX/%llX r11=%llX/%llX\n",
            item.entry, static_cast<unsigned>(item.mode), c.r3.u64,s.r3,c.r11.u64,s.r11);
        throw std::runtime_error("original PPC comparison");
    }
    if (item.entry == 0x822A07A0u)
    {
        const bool output = item.mode == Mode::ByteOutput || item.mode == Mode::ByteCount;
        Require(s.r3 == (output ? 1u : 0u), "code-unit presence result");
        const auto expected = item.mode == Mode::NulByte ? 0u :
            item.mode == Mode::ByteOutput ? 0xE9u : 0xABCDu;
        Require(recovered_memory.ReadU16(Destination)==expected, "code-unit output");
    }
    else if (item.entry == 0x82B85420u)
        Require(s.r3 == (item.mode == Mode::TaggedMatch ? 1u : 0u), "tagged equality");
    else
    {
        const bool failed=item.mode == Mode::UnicodeFailure;
        Require(right.events.size()==(failed?4u:5u), "error output/free call sequence");
        Require(right.events[3][2]==(failed?17u:2u), "error sink byte length");
        Require(s.r3==(failed?0xA1B2C3D4E5F60718ull:0x5566778899AABBCCull),
            "output or void-free residual return");
        if (item.mode == Mode::UnicodeMovedStack)
            Require(s.sp==MovedFrame+112u && s.r31==MovedSaved31 && s.lr==0x76543210u,
                "epilogue must restore from live native stack");
    }
}
} // namespace

void OriginalInitUnicode(PPCContext& c, std::uint8_t*)
{ auto s=FromPpc(c); active->InitializeUnicodeString(*active_memory,s); ToPpc(c,s); }
void OriginalConvertUnicode(PPCContext& c, std::uint8_t*)
{ auto s=FromPpc(c); active->UnicodeStringToAnsiString(*active_memory,s); ToPpc(c,s); }
void OriginalFreeAnsi(PPCContext& c, std::uint8_t*)
{ auto s=FromPpc(c); active->FreeAnsiString(*active_memory,s); ToPpc(c,s); }
void OriginalOutputAnsi(PPCContext& c, std::uint8_t*)
{
    c.r3.u64=OutputCrtErrorMessage(*active_memory,*active,c.r3.u32,c.r1.u32);
}

int main()
{
    try
    {
        for(const auto& item:Cases) Check(item);
        GuestWindow window(Regions), baseline(Regions);
        Seed(window,Cases[0]); Seed(baseline,Cases[0]);
        auto memory=window.Memory();
        auto state=FromPpc(Initial(Cases[0])); const auto saved=state;
        Services services(Mode::NullInput);
        Require(!family::Apply(0xFFFFFFFFu,memory,services,state) && state==saved &&
            window.EqualCommitted(baseline) && services.events.empty(), "unknown entry mutation");
        std::printf("PASS crt-formatting-support %zu original PPC cases + unknown\n",std::size(Cases));
        std::puts("LIMIT selected full entry state and ordinary RAM; accepted ANSI-output descriptor/result contract; explicit mutable native services; no generic lower ABI/fault/MMIO/concurrency/runtime proof");
        return 0;
    }
    catch(const std::exception& error)
    { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}

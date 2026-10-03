#include "lo_semantics/crt_stream_error.h"
#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/crt_allocation.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::crt_stream_error;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress ThreadData = 0x22000u;
constexpr GuestAddress NextData = 0x23000u;
constexpr GuestAddress Environment = 0x9000u;

enum class Mode { SlotFound, SlotMissing, HandleValid, HandleMinusTwo,
    HandleNegative, HandleRange, HandleUnallocated, SetMissingFound,
    SetFoundMissing };
struct Case { GuestAddress address; Mode mode; std::uint64_t input; };
constexpr Case Cases[] = {
    {0x82B7FDB0u, Mode::SlotFound, 0x1122334455667788ull},
    {0x82B7FDB0u, Mode::SlotMissing, 0x1122334455667788ull},
    {0x82B86228u, Mode::HandleValid, 0xAABBCCDD00000021ull},
    {0x82B86228u, Mode::HandleMinusTwo, 0xAABBCCDDFFFFFFFEull},
    {0x82B86228u, Mode::HandleNegative, 0xAABBCCDDFFFFFFFDull},
    {0x82B86228u, Mode::HandleRange, 0xAABBCCDD00000041ull},
    {0x82B86228u, Mode::HandleUnallocated, 0xAABBCCDD00000022ull},
    {0x82B7FDE8u, Mode::SetMissingFound, 0xAABBCCDD000004D2ull},
    {0x82B7FDE8u, Mode::SetFoundMissing, 0xAABBCCDD000004D2ull},
};
struct Region { GuestAddress base; std::size_t size; };
constexpr Region Regions[] = {
    {0, 0x90000}, {0x83215000u, 0x3000},
    {0x83378000u, 0x2000}, {0x83214000u, 0x3000},
};
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve stream guest RAM");
        for (const auto region : Regions)
            if (!VirtualAlloc(bytes + region.base, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit stream guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}
std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}
void Seed(Window& window)
{
    for (const auto region : Regions)
        std::memset(window.bytes + region.base, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Environment + 336u, 1);
    memory.WriteU32(0x83214D74u, 0x34000u);
    memory.WriteU32(0x83214D78u, 5);
    memory.WriteU32(0x83378E80u, 0x7203u);
    memory.WriteU32(0x83378D68u, 65);
    memory.WriteU32(0x83378D84u, 0x30000u); // block for indices 32-63
    memory.WriteU32(0x30000u + 0x40u, 0x35000u);
    memory.WriteU8(0x30000u + 0x44u, 1);
    memory.WriteU8(0x30000u + 0x84u, 0);
    // 82B7FD10's real 45-row table, not a stubbed mapping.
    memory.WriteU32(0x832150A8u, 0x4D2u);
    memory.WriteU32(0x832150ACu, 0x3Bu);
}

using Event = std::array<std::uint64_t, 5>;
struct Services final : CrtThreadDataServices, InvalidParameterServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    unsigned lookups = 0;
    Services(Window& window, Mode selected)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)), mode(selected) {}
    static std::uint64_t Unexpected()
    { throw std::runtime_error("unexpected lower service"); }
    std::uint64_t GetTlsValue(std::uint32_t index) override
    { events.push_back({1, index}); return 0x7100u; }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { (void)Unexpected(); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { return Unexpected(); }
    std::uint64_t CallThreadDataGetterWithState(GuestAddress target,
        std::uint64_t context, CrtThreadDataCall& call) override
    {
        events.push_back({2, target, context, call.thread_environment,
            lookups});
        const unsigned index = lookups++;
        if (mode == Mode::SlotMissing || mode == Mode::SetMissingFound)
            return index == 0 ? 0 : 0xAABBCCDD00023000ull;
        if (mode == Mode::SetFoundMissing)
            return index == 0 ? 0xAABBCCDD00022000ull : 0;
        return 0xAABBCCDD00022000ull;
    }
    std::uint64_t AllocateThreadData(std::uint32_t count,
        std::uint32_t bytes) override
    { events.push_back({4, count, bytes}); return 0; }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override { return Unexpected(); }
    void FreeThreadData(std::uint64_t) override { (void)Unexpected(); }
    void CallHandler(GuestMemory&, GuestAddress target,
        InvalidParameterCall& call) override
    {
        events.push_back({3, target, call.arguments[0], call.arguments[7],
            call.thread_environment});
        if (target != 0x7200u || call.arguments[0] != 0)
            throw std::runtime_error("unexpected invalid handle callback");
        call.arguments[0] = 0xABCDEF0000000007ull;
    }
    void Trap(const InvalidParameterCall&) override { (void)Unexpected(); }
};
Services* active = nullptr;

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers s{};
    s.sp=c.r1.u64; s.lr=c.lr;
    s.r3=c.r3.u64; s.r4=c.r4.u64; s.r5=c.r5.u64;
    s.r6=c.r6.u64; s.r7=c.r7.u64; s.r8=c.r8.u64;
    s.r9=c.r9.u64; s.r10=c.r10.u64; s.r11=c.r11.u64;
    s.r12=c.r12.u64; s.r13=c.r13.u64;
    s.r30=c.r30.u64; s.r31=c.r31.u64;
    s.xer_so=c.xer.so; s.xer_ca=c.xer.ca;
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
    c.r12.u64=s.r12; c.r13.u64=s.r13;
    c.r30.u64=s.r30; c.r31.u64=s.r31;
    c.xer.so=s.xer_so; c.xer.ca=s.xer_ca;
    c.cr0.lt=s.cr0.lt; c.cr0.gt=s.cr0.gt;
    c.cr0.eq=s.cr0.eq; c.cr0.so=s.cr0.so;
    c.cr6.lt=s.cr6.lt; c.cr6.gt=s.cr6.gt;
    c.cr6.eq=s.cr6.eq; c.cr6.so=s.cr6.so;
}
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=Stack; c.lr=0x1234567887654321ull;
    c.r3.u64=item.input; c.r4.u64=0xAABBCCDD00004000ull;
    c.r30.u64=0x1234567800000030ull;
    c.r31.u64=0xAABBCCDD00000031ull;
    c.r13.u64=0x1122334400009000ull;
    c.r9.u64=0x3344556600000009ull;
    c.r10.u64=0x334455660000000Aull;
    c.r11.u64=0x334455660000000Bull;
    c.xer.so=1; c.xer.ca=1;
    return c;
}
bool SameMemory(const Window& a, const Window& b)
{
    for (const auto region : Regions)
        if (std::memcmp(a.bytes + region.base,
                b.bytes + region.base, region.size) != 0) return false;
    return true;
}
bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.sp==b.sp && a.lr==b.lr &&
        a.r3==b.r3 && a.r4==b.r4 && a.r5==b.r5 &&
        a.r6==b.r6 && a.r7==b.r7 && a.r8==b.r8 &&
        a.r9==b.r9 && a.r10==b.r10 && a.r11==b.r11 &&
        a.r12==b.r12 && a.r13==b.r13 &&
        a.r30==b.r30 && a.r31==b.r31 &&
        a.xer_so==b.xer_so && a.xer_ca==b.xer_ca &&
        a.cr0.lt==b.cr0.lt && a.cr0.gt==b.cr0.gt &&
        a.cr0.eq==b.cr0.eq && a.cr0.so==b.cr0.so &&
        a.cr6.lt==b.cr6.lt && a.cr6.gt==b.cr6.gt &&
        a.cr6.eq==b.cr6.eq && a.cr6.so==b.cr6.so;
}
bool Check(const Case& item)
{
    Window original, recovered;
    Seed(original); Seed(recovered);
    Services expected(original,item.mode), actual(recovered,item.mode);
    PPCContext raw=Initial(item);
    auto translated=FromPpc(raw);
    active=&expected;
    switch(item.address)
    {
    case 0x82B7FDB0u: __imp__sub_82B7FDB0(raw,original.bytes); break;
    case 0x82B86228u: __imp__sub_82B86228(raw,original.bytes); break;
    case 0x82B7FDE8u: __imp__sub_82B7FDE8(raw,original.bytes); break;
    default: throw std::runtime_error("unexpected stream entry");
    }
    if (!family::Apply(item.address,actual.memory,actual,actual,translated))
        throw std::runtime_error("missing recovered stream entry");
    const auto original_state=FromPpc(raw);
    const bool same=Same(original_state,translated) &&
        expected.events==actual.events && SameMemory(original,recovered);
    if (!same)
        std::fprintf(stderr,"FAIL stream-error %08X mode=%u r3=%llx/%llx "
            "r11=%llx/%llx r12=%llx/%llx events=%zu/%zu\n",
            item.address,static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(original_state.r3),
            static_cast<unsigned long long>(translated.r3),
            static_cast<unsigned long long>(original_state.r11),
            static_cast<unsigned long long>(translated.r11),
            static_cast<unsigned long long>(original_state.r12),
            static_cast<unsigned long long>(translated.r12),
            expected.events.size(),actual.events.size());
    return same;
}
} // namespace

void OriginalThread(PPCContext& c, std::uint8_t*)
{
    CrtThreadDataCall call{c.r13.u64};
    c.r3.u64=GetCrtThreadData(active->memory,*active,call);
    c.r13.u64=call.thread_environment;
}
struct OriginalErrorAdapter final : AllocationFailureServices
{
    PPCContext& context;
    explicit OriginalErrorAdapter(PPCContext& c) : context(c) {}
    std::uint64_t GetThreadData() override
    { OriginalThread(context,nullptr); return context.r3.u64; }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { return Services::Unexpected(); }
    std::uint64_t BugCheck(std::uint32_t) override
    { return Services::Unexpected(); }
    std::uint64_t CallNewHandler(GuestAddress,std::uint64_t) override
    { return Services::Unexpected(); }
};
void OriginalErrorAddress(PPCContext& c,std::uint8_t*)
{
    auto s=FromPpc(c);
    active->memory.WriteU32(static_cast<GuestAddress>(s.sp-8u),
        static_cast<GuestAddress>(s.lr));
    active->memory.WriteU32(static_cast<GuestAddress>(s.sp-96u),
        static_cast<GuestAddress>(s.sp));
    c.r1.u64-=96u;
    OriginalErrorAdapter adapter(c);
    c.r3.u64=GetAllocationErrorAddress(adapter);
    c.r1.u64+=96u;
    c.r12.u64=active->memory.ReadU32(static_cast<GuestAddress>(c.r1.u64-8u));
    c.lr=c.r12.u64;
}
void OriginalInvalid(PPCContext& c,std::uint8_t*)
{
    InvalidParameterCall call{{{c.r3.u64,c.r4.u64,c.r5.u64,c.r6.u64,
        c.r7.u64,c.r8.u64,c.r9.u64,c.r10.u64}},c.r13.u64};
    c.r3.u64=ReportInvalidParameter(active->memory,*active,call);
    c.r4.u64=call.arguments[1]; c.r5.u64=call.arguments[2];
    c.r6.u64=call.arguments[3]; c.r7.u64=call.arguments[4];
    c.r8.u64=call.arguments[5]; c.r9.u64=call.arguments[6];
    c.r10.u64=call.arguments[7]; c.r13.u64=call.thread_environment;
}
void OriginalTranslate(PPCContext& c,std::uint8_t*)
{ c.r3.u64=TranslateCrtError(active->memory,c.r3.u32); }

int main()
{
    try
    {
        for (const auto& item : Cases) if (!Check(item)) return 1;
        Window window;
        Seed(window);
        Services services(window,Mode::SlotFound);
        auto state=FromPpc(Initial(Cases[0]));
        const auto saved=state;
        if (family::Apply(0xFFFFFFFFu,services.memory,services,services,state) ||
            !Same(state,saved) || !services.events.empty())
            throw std::runtime_error("unknown address changed state");
        std::printf("PASS crt-stream-error %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT selected full entry state and RAM; accepted lower models with bounded generic ABI/native contracts; no faults/MMIO/concurrency/runtime proof");
        return 0;
    }
    catch(const std::exception& error)
    { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}

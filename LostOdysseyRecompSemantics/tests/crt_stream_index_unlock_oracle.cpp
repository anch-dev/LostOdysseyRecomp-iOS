#include "lo_semantics/crt_stream_index_unlock.h"

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
namespace family = lo::semantic::gpu::crt_stream_index_unlock;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Table = 0x83215358u;
constexpr std::uint64_t Stack = 0x1122334400030000ull;
constexpr std::uint64_t ParentFrame = 0x5566778800020000ull;
constexpr GuestAddress RedirectFrame = 0x28000u;
enum class Mode { LeafZero, LeafTen, Wrapper, WrapperRedirect };
constexpr std::array Cases{Mode::LeafZero, Mode::LeafTen,
    Mode::Wrapper, Mode::WrapperRedirect};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes ||
            !VirtualAlloc(bytes + 0x10000u, 0x50000u, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83215000u, 0x1000u, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve indexed-lock guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

using Event = std::array<std::uint64_t, 8>;
struct Services final : family::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(Window& window, Mode selected)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)), mode(selected) {}
    void LeaveCriticalSection(GuestMemory&, family::Registers& r) override
    {
        events.push_back({r.sp, r.lr, r.r3, r.r10, r.r11,
            r.r12, r.r29, r.r31});
        r.r3 = 0xaabbccdd77665544ull;
        r.r11 = 0x8877665544332211ull;
        if (mode == Mode::WrapperRedirect)
        {
            r.r31 = 0x9988776600028000ull;
            r.sp = 0x1234567800031000ull;
            r.r29 = 0x1111111122222222ull;
            r.r12 = 0x3333333344444444ull;
            r.lr = 0x5555555566666666ull;
            memory.WriteU32(0x31000u, 0x32000u);
            memory.WriteU32(0x32000u - 8u, 0xface1234u);
            memory.WriteU32(RedirectFrame + 148u, 0xbeef1234u);
            memory.WriteU32(RedirectFrame + 80u, 0xcafe5678u);
        }
    }
};
Services* active = nullptr;

family::Registers FromPpc(const PPCContext& c)
{ return {c.r1.u64,c.lr,c.r3.u64,c.r10.u64,c.r11.u64,c.r12.u64,
    c.r29.u64,c.r31.u64}; }
void ToPpc(PPCContext& c, const family::Registers& r)
{
    c.r1.u64=r.sp; c.lr=r.lr; c.r3.u64=r.r3; c.r10.u64=r.r10;
    c.r11.u64=r.r11; c.r12.u64=r.r12; c.r29.u64=r.r29; c.r31.u64=r.r31;
}

void Seed(Window& window)
{
    std::memset(window.bytes + 0x10000u, 0xbd, 0x50000u);
    std::memset(window.bytes + 0x83215000u, 0xbd, 0x1000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Table, 0x12345678u);
    memory.WriteU32(Table + 80u, 0xdeadbeefu);
    memory.WriteU32(static_cast<GuestAddress>(ParentFrame) + 148u, 0x76543210u);
    memory.WriteU32(static_cast<GuestAddress>(ParentFrame) + 80u, 0xabcdef12u);
}

bool Check(Mode mode)
{
    Window before, after;
    Seed(before); Seed(after);
    Services expected(before,mode), actual(after,mode);
    PPCContext raw{};
    raw.r1.u64=Stack;
    raw.lr=0x123456789abcdef0ull;
    raw.r3.u64=mode==Mode::LeafTen ? 0x667788990000000aull :
        mode==Mode::LeafZero ? 0xaabbccdd00000000ull :
        0x1234567898765432ull;
    raw.r10.u64=0x1111222233334444ull;
    raw.r11.u64=0x5555666677778888ull;
    raw.r12.u64=0x9999aaaabbbbccccull;
    raw.r29.u64=0xddddeeeeffff0000ull;
    raw.r31.u64=ParentFrame;
    family::Registers recovered=FromPpc(raw);
    active=&expected;
    if (mode==Mode::LeafZero || mode==Mode::LeafTen)
        __imp__sub_82B819C8(raw,before.bytes);
    else __imp__sub_82B863B8(raw,before.bytes);
    const auto address=(mode==Mode::LeafZero || mode==Mode::LeafTen)
        ? 0x82b819c8u : 0x82b863b8u;
    if (!family::Apply(address,actual.memory,actual,recovered))
        throw std::runtime_error("indexed lock exit entry missing");
    const auto observed=FromPpc(raw);
    const bool same=observed.sp==recovered.sp && observed.lr==recovered.lr &&
        observed.r3==recovered.r3 && observed.r10==recovered.r10 &&
        observed.r11==recovered.r11 && observed.r12==recovered.r12 &&
        observed.r29==recovered.r29 && observed.r31==recovered.r31 &&
        expected.events==actual.events &&
        std::memcmp(before.bytes+0x10000u,after.bytes+0x10000u,0x50000u)==0 &&
        std::memcmp(before.bytes+0x83215000u,after.bytes+0x83215000u,0x1000u)==0;
    if (!same) std::fprintf(stderr,"FAIL crt-stream-index-unlock mode=%d raw-r3=%llx model-r3=%llx\n",
        static_cast<int>(mode),static_cast<unsigned long long>(raw.r3.u64),
        static_cast<unsigned long long>(recovered.r3));
    return same;
}
} // namespace

void OriginalLeave(PPCContext& context,std::uint8_t*)
{
    auto registers=FromPpc(context);
    active->LeaveCriticalSection(active->memory,registers);
    ToPpc(context,registers);
}

int main()
{
    try
    {
        for (Mode mode:Cases) if (!Check(mode)) return 1;
        Window window;
        Services services(window,Mode::LeafZero);
        family::Registers registers{}; registers.r3=42;
        if (family::Apply(0xffffffffu,services.memory,services,registers) ||
            registers.r3!=42 || !services.events.empty())
            throw std::runtime_error("unknown address changed state");
        std::printf("PASS crt-stream-index-unlock %zu original PPC cases + unknown\n",
            Cases.size());
        std::puts("LIMIT selected full PPC registers and committed ordinary RAM; native RtlLeaveCriticalSection explicit, faults/MMIO/concurrency/runtime unverified");
        return 0;
    }
    catch (const std::exception& e)
    { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}

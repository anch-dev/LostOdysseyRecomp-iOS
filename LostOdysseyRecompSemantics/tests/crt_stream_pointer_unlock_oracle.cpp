#include "lo_semantics/crt_stream_pointer_unlock.h"

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
namespace family = lo::semantic::gpu::crt_stream_pointer_unlock;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Table = 0x83378d80u;
enum class Mode { LeafSigned, FirstWrapper, SecondWrapper, RedirectedSave,
    DirectLock, DirectLockAlias };
struct Case { GuestAddress address; Mode mode; std::uint64_t index; };
constexpr Case Cases[] = {
    {0x82b863f0u, Mode::LeafSigned, UINT64_MAX},
    {0x82b81f38u, Mode::FirstWrapper, 10u},
    {0x82b860ccu, Mode::SecondWrapper, 77u},
    {0x82b860ccu, Mode::RedirectedSave, 10u},
    {0x82b81af8u, Mode::DirectLock, 0xaabbccdd00000005ull},
    {0x82b81af8u, Mode::DirectLockAlias, 0xaabbccdd00000006ull},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000,
                MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83378000u, 0x3000,
                MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve stream pointer guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

using Event = std::array<std::uint64_t, 8>;
struct Services final : family::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(Window& window, Mode selected)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)), mode(selected) {}

    void LeaveCriticalSection(GuestMemory& guest,
        family::Registers& registers) override
    {
        events.push_back({registers.sp, registers.lr, registers.r3,
            registers.r10, registers.r11, registers.r12, registers.r30,
            registers.r31});
        const bool direct_lock = mode == Mode::DirectLock ||
            mode == Mode::DirectLockAlias;
        const auto expected_sp = mode == Mode::LeafSigned ? Stack :
            direct_lock ? Stack - 96u : Stack - 112u;
        const auto expected_lr = mode == Mode::LeafSigned ?
            0x1122334455667788ull : direct_lock ? 0x82b81b10ull :
            mode == Mode::FirstWrapper ? 0x82b81f58ull : 0x82b860ecull;
        const auto expected_index = mode == Mode::LeafSigned ? UINT64_MAX :
            mode == Mode::SecondWrapper ? 77u : 10u;
        const auto slot = mode == Mode::LeafSigned ? Table - 4u :
            mode == Mode::SecondWrapper ? Table + 8u : Table;
        const std::uint64_t expected_r3 =
            direct_lock ? 0x69000u :
            0x50000u + (mode == Mode::LeafSigned ? 0x7c0u :
                mode == Mode::SecondWrapper ? 0x340u : 0x280u) + 12u;
        if (&guest != &memory || registers.sp != expected_sp ||
            registers.lr != expected_lr || registers.r3 != expected_r3 ||
            registers.r30 != (direct_lock ? 0x3344556600056000ull :
                mode == Mode::LeafSigned ? 0x3344556600000030ull :
                0xaabbccdd00000000ull + expected_index) ||
            (!direct_lock && memory.ReadU32(slot) != 0x50000u) ||
            (mode == Mode::LeafSigned && registers.xer_ca != 1u))
            throw std::runtime_error("incorrect stream native tail boundary");
        registers.r3 = 0x8877665500001000ull +
            static_cast<unsigned>(mode);
        registers.r9 = 0xaabbccdd00000009ull;
        registers.r10 = 0xaabbccdd00000010ull;
        registers.r11 = 0xaabbccdd00000011ull;
        if (mode == Mode::RedirectedSave || mode == Mode::DirectLockAlias)
        {
            registers.sp = Stack - 0x100u;
            memory.WriteU32(static_cast<GuestAddress>(registers.sp), 0x76000u);
            WriteU64(memory, 0x76000u - 8u,
                direct_lock ? 0x99aabbccddeeff00ull : 0x1122334455667788ull);
            if (direct_lock)
                memory.WriteU32(0x76000u - 16u, 0xface1234u);
            else
            {
                WriteU64(memory, 0x76000u - 16u, 0x99aabbccddeeff00ull);
                memory.WriteU32(0x76000u - 24u, 0xface1234u);
            }
            registers.r30 = 0;
            registers.r31 = 0;
            registers.r12 = 0;
            registers.lr = 0;
        }
    }
};
Services* active = nullptr;

family::Registers FromPpc(const PPCContext& c)
{
    return {c.r1.u64, c.lr, c.r3.u64, c.r9.u64, c.r10.u64,
        c.r11.u64, c.r12.u64, c.r30.u64, c.r31.u64, c.xer.ca};
}

void ToPpc(PPCContext& c, const family::Registers& r)
{
    c.r1.u64 = r.sp; c.lr = r.lr; c.r3.u64 = r.r3;
    c.r9.u64 = r.r9; c.r10.u64 = r.r10; c.r11.u64 = r.r11;
    c.r12.u64 = r.r12; c.r30.u64 = r.r30; c.r31.u64 = r.r31;
    c.xer.ca = r.xer_ca;
}

void Seed(Window& window)
{
    std::memset(window.bytes, 0, 0x90000u);
    std::memset(window.bytes + 0x83378000u, 0, 0x3000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Table - 4u, 0x50000u);
    memory.WriteU32(Table, 0x50000u);
    memory.WriteU32(Table + 8u, 0x50000u);
    memory.WriteU32(0x56000u + 80u, 0x69000u);
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64 = Stack;
    c.lr = 0x1122334455667788ull;
    c.r3.u64 = item.index;
    c.r12.u64 = 0xaabbccdd00070100ull;
    c.r30.u64 = item.mode == Mode::DirectLock ||
        item.mode == Mode::DirectLockAlias ? 0x3344556600056000ull :
        item.mode == Mode::LeafSigned ? 0x3344556600000030ull :
        0xaabbccdd00000000ull + item.index;
    c.r31.u64 = 0x1234567800000031ull;
    c.xer.ca = 0;
    return c;
}

bool Check(const Case& item)
{
    Window original, recovered;
    Seed(original); Seed(recovered);
    Services expected(original, item.mode), actual(recovered, item.mode);
    PPCContext raw = Initial(item), initial = raw;
    active = &expected;
    switch (item.address)
    {
    case 0x82b863f0u: __imp__sub_82B863F0(raw, original.bytes); break;
    case 0x82b81f38u: __imp__sub_82B81F38(raw, original.bytes); break;
    case 0x82b860ccu: __imp__sub_82B860CC(raw, original.bytes); break;
    case 0x82b81af8u: __imp__sub_82B81AF8(raw, original.bytes); break;
    default: throw std::runtime_error("unexpected stream entry");
    }
    auto registers = FromPpc(initial);
    if (!family::Apply(item.address, actual.memory, actual, registers))
        throw std::runtime_error("stream pointer unlock entry missing");
    PPCContext translated = initial;
    ToPpc(translated, registers);
    const auto expected_sp = item.mode == Mode::RedirectedSave ||
        item.mode == Mode::DirectLockAlias ?
        0x76000u : item.mode == Mode::LeafSigned ? Stack :
        static_cast<GuestAddress>(Stack);
    const bool same = std::memcmp(&raw, &translated, sizeof(PPCContext)) == 0 &&
        std::memcmp(original.bytes, recovered.bytes, 0x90000u) == 0 &&
        std::memcmp(original.bytes + 0x83378000u,
            recovered.bytes + 0x83378000u, 0x3000u) == 0 &&
        expected.events == actual.events && expected.events.size() == 1u &&
        raw.r1.u64 == expected_sp &&
        raw.r3.u64 == 0x8877665500001000ull +
            static_cast<unsigned>(item.mode);
    if (!same)
        std::fprintf(stderr,
            "FAIL stream-pointer-unlock %08x mode=%u r3=%llx/%llx "
            "SP=%llx/%llx LR=%llx/%llx events=%zu/%zu\n",
            item.address, static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(registers.r3),
            static_cast<unsigned long long>(raw.r1.u64),
            static_cast<unsigned long long>(registers.sp),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(registers.lr),
            expected.events.size(), actual.events.size());
    return same;
}
} // namespace

void OriginalNative(PPCContext& context, std::uint8_t*)
{
    auto registers = FromPpc(context);
    active->LeaveCriticalSection(active->memory, registers);
    ToPpc(context, registers);
}

int main()
{
    try
    {
        for (const auto& item : Cases) if (!Check(item)) return 1;
        Window window;
        Services services(window, Mode::LeafSigned);
        family::Registers registers = FromPpc(Initial(Cases[0]));
        const auto saved = registers;
        if (family::Apply(0xffffffffu, services.memory, services, registers) ||
            std::memcmp(&saved, &registers, sizeof(saved)) != 0 ||
            !services.events.empty())
            throw std::runtime_error("unknown stream address changed state");
        std::puts("PASS crt-stream-pointer-unlock 6 original PPC cases + unknown");
        std::puts("LIMIT selected registers, actual four bodies and ordinary RAM; native tail/mutable frame explicit; other native ABI, faults, MMIO, concurrency and runtime unverified");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}

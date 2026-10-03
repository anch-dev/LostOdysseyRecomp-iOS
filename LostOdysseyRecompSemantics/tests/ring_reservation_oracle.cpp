#include "lo_semantics/ring_reservation.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace ring = lo::semantic::gpu::ring_reservation;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Ring = 0x9000u, Descriptor = 0xA000u;
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x40000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve ring guest window");
        std::memset(bytes, 0xBD, 0x40000);
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

// The generated PPC baseline has no lwsync operation. This is intentionally
// its ordinary-memory model, not a purported real hardware barrier.
struct GeneratedSynchronization final : ring::SynchronizationServices
{
    void LightweightSync() override {}
};

bool Compare(unsigned mode)
{
    Window expected, actual;
    const GuestAddress descriptor = mode == 3 ? Ring - 8u : Descriptor;
    const auto setup = [mode](Window& window)
    {
        GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
        memory.WriteU32(Ring, 0x20000u);
        memory.WriteU32(Ring + 4u, 0x22000u);
        memory.WriteU32(Ring + 8u, mode == 2 ? 0x21FF0u : 0x20100u);
        memory.WriteU32(Ring + 12u, 0xDEADBEEFu);
        memory.WriteU32(Ring + 16u, 0);
        memory.WriteU32(Ring + 20u, mode == 1 ? 0x20300u : 0x20080u);
        memory.WriteU32(Ring + 24u, 16);
    };
    setup(expected);
    setup(actual);
    PPCContext raw{};
    raw.r1.u64 = 0x1122334400018000ull;
    raw.r3.u64 = 0x2233445500000000ull | descriptor;
    raw.r4.u64 = 0x6677889900000000ull | Ring;
    raw.r5.u64 = 0x99AABBCC00000011ull;
    raw.lr = 0xDEADBEEF01234567ull;
    raw.r7.u64 = 7; raw.r8.u64 = 8; raw.r9.u64 = 9;
    raw.r10.u64 = 10; raw.r11.u64 = 11;
    const PPCContext initial = raw;
    __imp__sub_82290AB8(raw, expected.bytes);
    GuestMemory memory(0, std::span<std::uint8_t>(actual.bytes, Space));
    GeneratedSynchronization synchronization;
    ring::Registers registers{initial.r7.u64, initial.r8.u64, initial.r9.u64,
                              initial.r10.u64, initial.r11.u64};
    std::uint64_t result = 99;
    if (!ring::Apply(0x82290AB8u, memory, synchronization, initial.r3.u64,
        initial.r4.u64, initial.r5.u64, registers, result))
        throw std::runtime_error("ring reservation not mapped");
    const bool same = raw.r1.u64 == initial.r1.u64 && raw.lr == initial.lr &&
        raw.r3.u64 == result && raw.r4.u64 == initial.r4.u64 && raw.r5.u64 == initial.r5.u64 &&
        raw.r7.u64 == registers.r7 && raw.r8.u64 == registers.r8 &&
        raw.r9.u64 == registers.r9 && raw.r10.u64 == registers.r10 &&
        raw.r11.u64 == registers.r11 && !std::memcmp(expected.bytes, actual.bytes, 0x40000);
    if (!same) std::fprintf(stderr, "FAIL ring reservation finite path %u\n", mode);
    return same;
}
} // namespace

int main()
{
    try
    {
        for (unsigned mode = 0; mode < 4; ++mode) if (!Compare(mode)) return 1;
        std::array<std::uint8_t, 32> bytes{};
        GuestMemory memory(0, bytes);
        GeneratedSynchronization synchronization;
        ring::Registers registers{1, 2, 3, 4, 5};
        std::uint64_t result = 99;
        if (ring::Apply(0xFFFFFFFFu, memory, synchronization, 0, 0, 0, registers, result) ||
            result != 99 || registers.r7 != 1 || registers.r8 != 2 || registers.r9 != 3 ||
            registers.r10 != 4 || registers.r11 != 5 || bytes != std::array<std::uint8_t, 32>{})
            throw std::runtime_error("unknown ring entry changed state");
        std::puts("PASS ring-reservation 1 exact body, 4 finite original-PPC paths + unknown");
        std::puts("LIMIT generated PPC omits lwsync; ordinary RAM and selected GPRs only, "
                  "concurrency, wait wakeups, faults/MMIO and real synchronization excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

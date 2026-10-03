#include "lo_semantics/instance_linker_composed_family.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace linker = lo::semantic::gpu::instance_linker_composed_family;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Stack = 0x7000u;
constexpr GuestAddress Object = 0x1200u;
constexpr GuestAddress GlobalPage = 0x83235000u;
constexpr GuestAddress FirstGlobal = 0x83235aa4u;
constexpr GuestAddress SecondGlobal = 0x83235aa0u;
constexpr GuestAddress ThirdGlobal = 0x83235aacu;

enum class Mode { Ordinary, Null, GlobalAlias, SpillAlias };
struct Case
{
    GuestAddress address;
    PPCFunc* original;
    Mode mode;
};
constexpr Case Cases[] = {
    {0x823f34b8u, __imp__sub_823F34B8, Mode::Ordinary},
    {0x824190f8u, __imp__sub_824190F8, Mode::Ordinary},
    {0x82419230u, __imp__sub_82419230, Mode::Ordinary},
    {0x824190f8u, __imp__sub_824190F8, Mode::Null},
    {0x82419230u, __imp__sub_82419230, Mode::Null},
    {0x823f34b8u, __imp__sub_823F34B8, Mode::GlobalAlias},
    {0x824190f8u, __imp__sub_824190F8, Mode::GlobalAlias},
    {0x824190f8u, __imp__sub_824190F8, Mode::SpillAlias},
    {0x82419230u, __imp__sub_82419230, Mode::SpillAlias},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x9000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + GlobalPage, 0x2000, MEM_COMMIT,
                PAGE_READWRITE))
            throw std::runtime_error("reserve linker guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct NoResize final : ArrayResizeServices
{
    void InitializeManager() override
    { throw std::runtime_error("unexpected InitializeManager"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected ResizeStorage"); }
};

void Initialize(Window& window)
{
    std::memset(window.bytes, 0xbd, 0x9000);
    std::memset(window.bytes + GlobalPage, 0xbd, 0x2000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(FirstGlobal, 0x11112222u);
    memory.WriteU32(SecondGlobal, 0x33334444u);
    memory.WriteU32(ThirdGlobal, 0x55556666u);
}

bool SameMemory(const Window& expected, const Window& actual)
{
    return std::memcmp(expected.bytes, actual.bytes, 0x9000) == 0 &&
        std::memcmp(expected.bytes + GlobalPage,
                    actual.bytes + GlobalPage, 0x2000) == 0;
}

bool Compare(const Case& test)
{
    Window expected, actual;
    Initialize(expected);
    Initialize(actual);
    const GuestAddress object = test.mode == Mode::Null ? 0u :
        test.mode == Mode::GlobalAlias ? SecondGlobal - 4u :
        test.mode == Mode::SpillAlias ? Stack - 192u : Object;
    PPCContext raw{};
    raw.r3.u64 = 0x1234567800000000ull | object;
    raw.r1.u64 = Stack;
    raw.lr = 0x1122334455667788ull;
    raw.r30.u64 = 0x99aabbccddeeff00ull;
    raw.r31.u64 = 0x8877665544332211ull;
    PPCContext recovered = raw;
    test.original(raw, expected.bytes);

    GuestMemory memory(0, std::span<std::uint8_t>(actual.bytes, Space));
    NoResize services;
    linker::FrameRegisters frame{recovered.lr, recovered.r30.u64,
                                 recovered.r31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!linker::Apply(test.address, memory, services, recovered.r3.u64,
                       Stack, frame, result))
        throw std::runtime_error("Linker composed entry unmapped");
    recovered.r3.u64 = result;
    recovered.lr = frame.lr;
    recovered.r30.u64 = frame.r30;
    recovered.r31.u64 = frame.r31;
    const bool same = raw.r3.u64 == recovered.r3.u64 &&
        raw.r1.u64 == recovered.r1.u64 && raw.lr == recovered.lr &&
        raw.r30.u64 == recovered.r30.u64 &&
        raw.r31.u64 == recovered.r31.u64 && SameMemory(expected, actual);
    if (!same)
        std::fprintf(stderr,
            "FAIL linker %08x mode %d raw-r3 %016llx recovered-r3 %016llx "
            "raw-r31 %016llx recovered-r31 %016llx\n", test.address,
            static_cast<int>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(recovered.r3.u64),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(recovered.r31.u64));
    if (test.mode == Mode::SpillAlias &&
        raw.r3.u64 != 0x00000000000000f0ull)
        throw std::runtime_error("spill alias did not change live subobject");
    return same;
}
} // namespace

PPC_FUNC(sub_82B7BC40) { __imp__sub_82B7BC40(ctx, base); }
PPC_FUNC(sub_82419178) { __imp__sub_82419178(ctx, base); }
PPC_FUNC(sub_823F34B8) { __imp__sub_823F34B8(ctx, base); }

int main()
{
    try
    {
        for (const Case& test : Cases)
            if (!Compare(test))
                return 1;
        std::array<std::uint8_t, 32> untouched{};
        GuestMemory memory(0, untouched);
        NoResize services;
        linker::FrameRegisters frame{1, 2, 3};
        std::uint64_t result = 0xdeadbeefcafef00dull;
        if (linker::Apply(0xffffffffu, memory, services,
                          0x1234567800001200ull, Stack, frame, result) ||
            result != 0xdeadbeefcafef00dull || frame.lr != 1 ||
            frame.r30 != 2 || frame.r31 != 3 ||
            untouched != std::array<std::uint8_t, 32>{})
            throw std::runtime_error("unknown Linker entry changed state");
        std::printf("PASS instance-linker 3 exact bodies, %zu bounded comparisons\n",
                    std::size(Cases) + 1);
        std::puts("LIMIT reused lower Linker/Fill semantics; generic lower ABI "
                  "and volatile GPR/CR excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

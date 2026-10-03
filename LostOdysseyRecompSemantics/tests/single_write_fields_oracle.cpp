// The runner prepends the exact generated PPC bodies selected by the manifest.
#include "lo_semantics/single_write_fields.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <windows.h>

namespace
{

using lo::semantic::gpu::GuestMemory;
using lo::semantic::single_write_fields::Registers;

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr std::size_t kGuestSize = std::size_t{1} << 32;
constexpr std::uint32_t kPageSize = 4096;
constexpr unsigned kCases = 6;
constexpr std::uint32_t kObject = 0x20000;
constexpr std::uint32_t kInput = 0x60000;
constexpr std::uint32_t kOutput = 0x70000;
constexpr std::uint32_t kPointer = 0x80000;
constexpr std::uint32_t kNeighbor = 0x90000;

struct TestEntry
{
    std::uint32_t address;
    void (*original)(PPCContext&, std::uint8_t*);
};

// TEST_ENTRIES

struct PageImage
{
    std::uint32_t address;
    std::array<std::uint8_t, kPageSize> bytes;
};

std::uint32_t Global(std::int32_t upper, std::int32_t offset)
{
    return static_cast<std::uint32_t>(upper * 65536 + offset);
}

class SparseGuestSpace
{
public:
    SparseGuestSpace()
    {
        base_ = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, kGuestSize, MEM_RESERVE, PAGE_NOACCESS));
        if (!base_) throw std::runtime_error("reserve guest address space");
    }
    ~SparseGuestSpace() { VirtualFree(base_, 0, MEM_RELEASE); }
    SparseGuestSpace(const SparseGuestSpace&) = delete;
    SparseGuestSpace& operator=(const SparseGuestSpace&) = delete;

    [[nodiscard]] std::uint8_t* data() const { return base_; }
    [[nodiscard]] GuestMemory memory() const { return GuestMemory(0, {base_, kGuestSize}); }

    void BeginCase()
    {
        pages_.clear();
        TrackRange(0, 0x10000);
        TrackRange(kObject, 0x32000);
        TrackRange(kInput, 0x3000);
        TrackRange(kOutput, 0x3000);
        TrackRange(kPointer, 0x3000);
        TrackPage(kPointer + 23856);
        TrackRange(kNeighbor, 0x3000);
        for (const auto [upper, offset] : std::array{
                 std::pair{-31951, -18904}, std::pair{-31951, 24500},
                 std::pair{-31964, -19532}, std::pair{-31950, -30908},
                 std::pair{-31956, 9268}, std::pair{-31969, 7008},
                 std::pair{-31955, -24352 + 5584},
                 std::pair{-31953, -13592}, std::pair{-31953, -10572},
                 std::pair{-32232, 4136}, std::pair{-31969, -28140},
                 std::pair{-31969, -28056 + 8},
                 std::pair{-31967, 16208}, std::pair{-31955, 11584}})
            TrackPage(Global(upper, offset));
    }

    void TrackRange(std::uint32_t address, std::uint32_t size)
    {
        for (std::uint32_t page = address; page < address + size; page += kPageSize)
            TrackPage(page);
    }

    void TrackPage(std::uint32_t address)
    {
        const auto page = address & ~(kPageSize - 1u);
        for (const auto known : pages_)
            if (known == page) return;
        if (!VirtualAlloc(base_ + page, kPageSize, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest test page");
        std::memset(base_ + page, 0, kPageSize);
        pages_.push_back(page);
    }

    void Put32(std::uint32_t address, std::uint32_t value)
    {
        memory().WriteU32(address, value);
    }

    void Put8(std::uint32_t address, std::uint8_t value)
    {
        memory().WriteU8(address, value);
    }

    [[nodiscard]] std::vector<PageImage> Snapshot() const
    {
        std::vector<PageImage> images;
        for (const auto page : pages_)
        {
            PageImage image{page, {}};
            std::memcpy(image.bytes.data(), base_ + page, kPageSize);
            images.push_back(image);
        }
        return images;
    }

    void Restore(const std::vector<PageImage>& images)
    {
        for (const auto& image : images)
            std::memcpy(base_ + image.address, image.bytes.data(), kPageSize);
    }

private:
    std::uint8_t* base_ = nullptr;
    std::vector<std::uint32_t> pages_;
};

void FillContext(PPCContext& context, std::uint32_t address, unsigned case_index)
{
    auto* bytes = reinterpret_cast<std::uint8_t*>(&context);
    for (std::size_t i = 0; i < sizeof(context); ++i)
        bytes[i] = static_cast<std::uint8_t>(i * 29u + address + case_index * 17u);
    const std::uint64_t high = case_index & 1 ?
        0xfedcba9800000000ull : 0x1234567800000000ull;
    context.r3.u64 = high | kObject;
    context.r4.u64 = high | kInput;
    context.r5.u64 = high | kOutput;
    context.r6.u64 = high | kNeighbor;
}

void Seed(const TestEntry& entry, unsigned case_index,
          PPCContext& context, SparseGuestSpace& space)
{
    const auto index = case_index < 4 ? case_index : case_index - 4;
    space.Put32(kObject + 0, kPointer);
    space.Put32(kObject + 4, 3 + index);
    space.Put32(kObject + 16, case_index == 2 ? 0xffffffffu : 0x12345678u);
    space.Put32(kObject + 32, case_index == 3 ? 0xffffffffu : 37u);
    space.Put32(kObject + 48, case_index & 1 ? 3u : 4u);
    space.Put32(kObject + 52,
                case_index == 3 ? 0xffffffffu : 0x80000000u + case_index);
    space.Put32(kObject + 60, 0xa5a50000u + case_index);
    space.Put32(kObject + 128, case_index & 1 ? 0x0fffffffu : 0xffffffffu);
    space.Put32(kObject + 132, kPointer);
    space.Put32(kObject + 148, kPointer);
    space.Put32(kObject + 196, kPointer);
    space.Put32(kObject + 200, 0x10203040u);
    space.Put32(kObject + 440, kPointer);
    space.Put32(kObject + 480, 0x12345678u);
    space.Put32(kObject + 744, 0xffffffffu);
    space.Put32(kObject + 1120, kPointer);
    space.Put32(kObject + 20320, kPointer);
    space.Put32(kObject + 196608 + 7632, 0x80000000u);
    space.Put32(kInput + 4, 0x22334455u);
    space.Put32(kInput + 248, 0x80000000u);
    space.Put32(kPointer + 24, 0x01234567u);
    space.Put32(kPointer + 104, 0x89abcdefu);
    space.Put32(kPointer + 208, 0xfedcba98u);
    space.Put32(kPointer + 736, 0xa5a5a5a5u);
    space.Put32(Global(-31951, -18904), 0x80000000u);
    space.Put32(Global(-31951, 24500), kPointer);
    space.Put32(Global(-31964, -19532), 0x12345678u);
    space.Put32(Global(-31950, -30908), kPointer);
    space.Put32(kPointer + 80, kInput);
    space.Put32(kInput + 60, kOutput);
    space.Put32(kOutput, kNeighbor);
    space.Put32(kNeighbor + 712, 0x01020304u);
    space.Put32(Global(-31956, 9268), case_index == 3 ? 0 : 9u);
    space.Put32(Global(-31969, 7008), kPointer);
    space.Put32(Global(-31967, 16208), 0x80001234u);
    space.Put32(Global(-31969, -28140), 0x55667788u);
    space.Put32(Global(-32232, 4136 + (3 + index) * 16), 0x66778899u);
    for (std::uint32_t i = 0; i < 8; ++i)
    {
        space.Put32(kPointer + i * 8, 0x11223300u + i);
        space.Put32(kPointer + i * 8 + 4, 0x44556600u + i);
    }

    switch (entry.address)
    {
    case 0x8251B300u:
    case 0x829062E0u:
    case 0x829062F8u:
    case 0x8295FEA0u:
    case 0x82965048u:
    case 0x82965060u:
        context.r4.u64 = (context.r4.u64 & 0xffffffff00000000ull) | (case_index & 1);
        break;
    case 0x82872868u:
    case 0x82B9E1F8u:
    case 0x82D93C90u:
    case 0x830642C0u:
    case 0x830642D8u:
    case 0x83098528u:
        context.r4.u64 = (context.r4.u64 & 0xffffffff00000000ull) | index;
        break;
    case 0x82D18958u:
        context.r6.u64 = (context.r6.u64 & 0xffffffff00000000ull) | index;
        break;
    case 0x82AE10E8u:
        if (case_index == 5)
            context.r4.u64 = (context.r4.u64 & 0xffffffff00000000ull) | 1u;
        break;
    case 0x82DE9470u:
        if (case_index == 5)
            context.r3.u64 &= 0xffffffff00000000ull;
        break;
    case 0x83081570u:
        context.r4.u64 = (context.r4.u64 & 0xffffffff00000000ull) | index;
        context.r5.u64 &= 0xffffffff00000000ull;
        break;
    case 0x82746848u:
        space.Put8(kObject + 148, case_index & 1 ? 5 : 4);
        break;
    case 0x828B6320u:
        context.r4.u64 = (context.r4.u64 & 0xffffffff00000000ull) |
                         (case_index & 1);
        context.r5.u64 = (context.r5.u64 & 0xffffffff00000000ull) |
                         (case_index & 1);
        break;
    case 0x82E02128u:
        context.r4.u64 = (context.r4.u64 & 0xffffffff00000000ull) | index;
        if (case_index == 5)
            space.Put32(kPointer + index * 16, 0);
        break;
    default:
        break;
    }

    if (entry.address == 0x82B9E1F8u && case_index == 5)
    {
        // The output overwrites the pointer field before its required reload.
        context.r5.u64 = context.r3.u64 + 196;
        space.Put32(kPointer + index * 8, kInput);
    }
}

bool Test(const TestEntry& entry, unsigned case_index, SparseGuestSpace& space)
{
    space.BeginCase();
    PPCContext original{};
    FillContext(original, entry.address, case_index);
    Seed(entry, case_index, original, space);
    const auto before = space.Snapshot();
    PPCContext recovered{};
    std::memcpy(&recovered, &original, sizeof(original));

    entry.original(original, space.data());
    const auto expected = space.Snapshot();
    space.Restore(before);

    Registers registers{recovered.r3.u64, recovered.r4.u64,
                        recovered.r5.u64, recovered.r6.u64,
                        recovered.r9.u64, recovered.r10.u64,
                        recovered.r11.u64};
    auto memory = space.memory();
    if (!lo::semantic::single_write_fields::Apply(entry.address, registers, memory))
    {
        std::fprintf(stderr, "unmapped %08X\n", entry.address);
        return false;
    }
    recovered.r3.u64 = registers.r3;
    recovered.r4.u64 = registers.r4;
    recovered.r5.u64 = registers.r5;
    recovered.r6.u64 = registers.r6;
    recovered.r9.u64 = registers.r9;
    recovered.r10.u64 = registers.r10;
    recovered.r11.u64 = registers.r11;
    if (std::memcmp(&original, &recovered, sizeof(original)) != 0)
    {
        std::fprintf(stderr, "FAIL context %08X case %u: r3 %016llX/%016llX"
                             " r9 %016llX/%016llX r10 %016llX/%016llX"
                             " r11 %016llX/%016llX\n",
                     entry.address, case_index,
                     static_cast<unsigned long long>(original.r3.u64),
                     static_cast<unsigned long long>(recovered.r3.u64),
                     static_cast<unsigned long long>(original.r9.u64),
                     static_cast<unsigned long long>(recovered.r9.u64),
                     static_cast<unsigned long long>(original.r10.u64),
                     static_cast<unsigned long long>(recovered.r10.u64),
                     static_cast<unsigned long long>(original.r11.u64),
                     static_cast<unsigned long long>(recovered.r11.u64));
        return false;
    }
    const auto actual = space.Snapshot();
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (expected[i].address != actual[i].address ||
            expected[i].bytes != actual[i].bytes)
        {
            std::fprintf(stderr, "FAIL memory %08X case %u page %08X\n",
                         entry.address, case_index, expected[i].address);
            return false;
        }
    return true;
}

} // namespace

int main()
{
    SparseGuestSpace space;
    for (const auto& entry : kTestEntries)
        for (unsigned i = 0; i < kCases; ++i)
            if (!Test(entry, i, space)) return 1;

    std::array<std::uint8_t, 4> bytes{};
    GuestMemory memory(0, bytes);
    Registers unknown{1, 2, 3, 4, 5, 6, 7};
    if (lo::semantic::single_write_fields::Apply(0, unknown, memory) ||
        unknown.r3 != 1 || unknown.r4 != 2 || unknown.r5 != 3 ||
        unknown.r6 != 4 || unknown.r9 != 5 || unknown.r10 != 6 ||
        unknown.r11 != 7)
        return 1;

    std::printf("PASS single-write-fields %zu entries %zu cases\n",
                std::size(kTestEntries), std::size(kTestEntries) * kCases);
    return 0;
}

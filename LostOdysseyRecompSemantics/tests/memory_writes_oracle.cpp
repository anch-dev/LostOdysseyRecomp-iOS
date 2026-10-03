// The runner prepends the original generated PPC implementations.
#include "lo_semantics/memory_writes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <windows.h>

namespace
{

using lo::semantic::gpu::GuestMemory;
using lo::semantic::memory_writes::Registers;

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr std::size_t kGuestSize = std::size_t{1} << 32;
constexpr std::uint32_t kPageSize = 4096;
constexpr unsigned kCases = 6;
constexpr std::uint32_t kObject = 0x20000;
constexpr std::uint32_t kInput = 0x30000;
constexpr std::uint32_t kOutput = 0x40000;
constexpr std::uint32_t kPointer = 0x50000;
constexpr std::uint32_t kNeighbor = 0x60000;

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
        TrackRange(kObject, 0x7000);
        TrackRange(kInput, 0x2000);
        TrackRange(kOutput, 0x2000);
        TrackRange(kPointer, 0x2000);
        TrackRange(kNeighbor, 0x2000);
    }

    void TrackRange(std::uint32_t address, std::uint32_t size)
    {
        for (auto page = address & ~(kPageSize - 1u);
             page < address + size; page += kPageSize)
        {
            if (!VirtualAlloc(base_ + page, kPageSize, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit guest test page");
            std::memset(base_ + page, static_cast<int>((page >> 8) ^ 0xa5), kPageSize);
            pages_.push_back(page);
        }
    }

    void Put32(std::uint32_t address, std::uint32_t value)
    {
        memory().WriteU32(address, value);
    }

    [[nodiscard]] std::vector<PageImage> Snapshot() const
    {
        std::vector<PageImage> images;
        for (auto page : pages_)
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

std::uint32_t At(std::uint64_t base, std::uint32_t offset = 0)
{
    return static_cast<std::uint32_t>(base) + offset;
}

void FillContext(PPCContext& context, std::uint32_t address, unsigned case_index)
{
    auto* bytes = reinterpret_cast<std::uint8_t*>(&context);
    for (std::size_t i = 0; i < sizeof(context); ++i)
        bytes[i] = static_cast<std::uint8_t>(i * 29u + address + case_index * 17u);
    const std::uint64_t high = (case_index & 1) ?
        0xfedcba9800000000ull : 0x1234567800000000ull;
    context.r3.u64 = high | kObject;
    context.r4.u64 = high | kInput;
    context.r5.u64 = high | kOutput;
    context.r6.u64 = (high ^ 0x5555555500000000ull) | 0x12345678u;
    context.r7.u64 = (high ^ 0xaaaaaaaa00000000ull) | 0x87654321u;
}

void Seed(const TestEntry& entry, unsigned case_index,
          PPCContext& context, SparseGuestSpace& space)
{
    // Vary actual memory words separately from address and register values.
    space.Put32(kObject + 4, 0xabcdef00u + case_index);
    space.Put32(kInput + 72, case_index & 1 ? 0xffffffffu : 0x12345678u);
    space.Put32(kInput + 76, case_index & 1 ? 0x80000000u : 0x7fffffffu);
    space.Put32(kInput + 124, 0x87654321u + case_index);
    space.Put32(kInput + 128, 0xfedcba98u + case_index);
    space.Put32(kInput + 224, 0x80000000u + case_index);
    switch (entry.address)
    {
    case 0x8234B4B8u:
        space.Put32(At(context.r4.u64, 12), kPointer);
        if (case_index == 5)
            context.r5.u64 = context.r4.u64 + 12;
        break;
    case 0x825207B8u:
        space.Put32(At(context.r4.u64, 516), kPointer);
        break;
    case 0x8257A208u:
        space.Put32(At(context.r4.u64, 120), kPointer);
        context.r5.u64 = (context.r5.u64 & 0xffffffff00000000ull) | case_index;
        break;
    case 0x8257A230u:
        space.Put32(At(context.r3.u64, 120), kPointer);
        context.r4.u64 = (context.r4.u64 & 0xffffffff00000000ull) | case_index;
        break;
    case 0x82C2FB88u:
        space.Put32(At(context.r3.u64, 444), kPointer);
        break;
    case 0x82C2FEB0u:
        space.Put32(At(context.r3.u64), kPointer);
        break;
    case 0x83084F88u:
        space.Put32(At(context.r3.u64, 4),
                    case_index == 5 ? kObject - 4 : kPointer);
        space.Put32(At(context.r3.u64, 8), kNeighbor);
        break;
    case 0x82EBAF20u:
    case 0x82EBAF80u:
    case 0x82EBFD30u:
        if (case_index == 5)
        {
            const std::uint32_t source = entry.address == 0x82EBAF20u ? 320 :
                (entry.address == 0x82EBAF80u ? 332 : 336);
            context.r4.u64 = context.r3.u64 + source + 4;
        }
        break;
    case 0x82884018u:
        if (case_index == 5)
        {
            context.r4.u64 = context.r3.u64 + 24424;
            context.r5.u64 = context.r3.u64 + 24420;
        }
        break;
    default: break;
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

    Registers registers{recovered.r3.u64, recovered.r4.u64, recovered.r5.u64,
                        recovered.r6.u64, recovered.r7.u64, recovered.r8.u64,
                        recovered.r9.u64, recovered.r10.u64, recovered.r11.u64};
    auto memory = space.memory();
    if (!lo::semantic::memory_writes::Apply(entry.address, registers, memory))
    {
        std::fprintf(stderr, "unmapped %08X\n", entry.address);
        return false;
    }
    recovered.r3.u64 = registers.r3;
    recovered.r4.u64 = registers.r4;
    recovered.r5.u64 = registers.r5;
    recovered.r6.u64 = registers.r6;
    recovered.r7.u64 = registers.r7;
    recovered.r8.u64 = registers.r8;
    recovered.r9.u64 = registers.r9;
    recovered.r10.u64 = registers.r10;
    recovered.r11.u64 = registers.r11;
    if (std::memcmp(&original, &recovered, sizeof(original)) != 0)
    {
        std::fprintf(stderr, "FAIL context %08X case %u:"
                             " r3 %016llX/%016llX r10 %016llX/%016llX"
                             " r11 %016llX/%016llX\n",
            entry.address, case_index,
            static_cast<unsigned long long>(original.r3.u64),
            static_cast<unsigned long long>(recovered.r3.u64),
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
    Registers unknown{1, 2, 3, 4, 5, 6, 7, 8, 9};
    if (lo::semantic::memory_writes::Apply(0, unknown, memory) ||
        unknown.r3 != 1 || unknown.r4 != 2 || unknown.r5 != 3 ||
        unknown.r6 != 4 || unknown.r7 != 5 || unknown.r8 != 6 ||
        unknown.r9 != 7 || unknown.r10 != 8 || unknown.r11 != 9)
        return 1;

    std::printf("PASS memory-writes %zu entries %zu cases\n",
                std::size(kTestEntries), std::size(kTestEntries) * kCases);
    return 0;
}

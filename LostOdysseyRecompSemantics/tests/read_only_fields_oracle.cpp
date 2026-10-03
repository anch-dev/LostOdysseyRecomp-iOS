// Appended after the exact original generated PPC bodies by the batch runner.
#include "lo_semantics/guest_memory.h"
#include "lo_semantics/read_only_fields.h"

#include <array>
#include <bit>
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
using lo::semantic::read_only_fields::Registers;

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr std::size_t kGuestSize = std::size_t{1} << 32;
constexpr std::uint32_t kPageSize = 4096;
constexpr unsigned kCases = 6;
constexpr std::uint32_t kPointer = 0x00050000u;
constexpr std::uint32_t kSecond = 0x00060000u;
constexpr std::uint32_t kArray = 0x00070000u;

struct TestEntry
{
    std::uint32_t address;
    void (*original)(PPCContext&, std::uint8_t*);
};

// TEST_ENTRIES

[[nodiscard]] std::uint32_t Low(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint32_t At(std::uint64_t base, std::int32_t offset)
{
    return Low(base) + static_cast<std::uint32_t>(offset);
}

[[nodiscard]] std::uint32_t High(std::int32_t upper)
{
    return static_cast<std::uint32_t>(upper * std::int64_t{65536});
}

[[nodiscard]] std::uint32_t Sample(unsigned index)
{
    constexpr std::uint32_t values[kCases] = {
        0u, 1u, 0x80000000u, 0xffffffffu, 0x12345678u, 6u,
    };
    return values[index % kCases];
}

struct PageImage
{
    std::uint32_t address;
    std::array<std::uint8_t, kPageSize> bytes;
    bool operator==(const PageImage&) const = default;
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

    void BeginCase() { pages_.clear(); }

    void Track(std::uint32_t address, unsigned width)
    {
        for (unsigned i = 0; i < width; ++i)
        {
            const auto page = (address + i) & ~(kPageSize - 1u);
            bool seen = false;
            for (auto existing : pages_) if (existing == page) { seen = true; break; }
            if (seen) continue;
            if (!VirtualAlloc(base_ + page, kPageSize, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit guest test page");
            std::memset(base_ + page, 0xa5, kPageSize);
            pages_.push_back(page);
        }
    }

    void Put8(std::uint32_t address, std::uint8_t value)
    {
        Track(address, 1);
        memory().WriteU8(address, value);
    }
    void Put16(std::uint32_t address, std::uint16_t value)
    {
        Track(address, 2);
        memory().WriteU16(address, value);
    }
    void Put32(std::uint32_t address, std::uint32_t value)
    {
        Track(address, 4);
        memory().WriteU32(address, value);
    }

    [[nodiscard]] std::vector<PageImage> Snapshot() const
    {
        std::vector<PageImage> result;
        for (auto address : pages_)
        {
            PageImage image{address, {}};
            std::memcpy(image.bytes.data(), base_ + address, kPageSize);
            result.push_back(image);
        }
        return result;
    }

private:
    std::uint8_t* base_ = nullptr;
    std::vector<std::uint32_t> pages_;
};

void Seed(const TestEntry& entry, unsigned index, const PPCContext& c,
          SparseGuestSpace& space)
{
    const std::uint32_t root = c.r3.u32;
    const std::uint32_t value = Sample(index);
    const std::uint32_t scalar = index == 3 ? 0xffffffffu : value;

    switch (entry.address)
    {
    case 0x822A7F38u: case 0x82906310u:
        space.Put32(root + 196608u + 7632u, value);
        break;
    case 0x822B5430u:
        space.Put32(High(-31953) - 10472u +
                    (std::rotl(root, 8) & 0xfcu), value);
        break;
    case 0x822D7C38u:
        space.Put32(root + 280u, value);
        space.Put32(root + 252u, Sample(index + 1));
        break;
    case 0x823214C0u:
        space.Put8(root + 6u, static_cast<std::uint8_t>(value));
        space.Put8(root + 1u, static_cast<std::uint8_t>(value >> 8));
        break;
    case 0x823588A0u: case 0x82B7CFC0u: case 0x82B7CFE0u:
        space.Put32(High(-31967) + 21024u + 200u, kArray);
        space.Put16(kArray + (root << 1), static_cast<std::uint16_t>(value));
        break;
    case 0x82B7A680u:
        space.Put32(High(-31967) + 21248u, kPointer);
        space.Put32(kPointer + 200u, kArray);
        space.Put16(kArray + ((root << 1) & 0x1feu),
                    static_cast<std::uint16_t>(value));
        break;
    case 0x8237CB68u:
        space.Put32(High(-31950) - 30908u, kPointer);
        space.Put32(kPointer + 476u, scalar);
        break;
    case 0x823A3C40u:
        space.Put8(High(-31955) - 16464u, static_cast<std::uint8_t>(value));
        break;
    case 0x823B8F28u:
        space.Put32(root, kPointer);
        space.Put32(root + 4u, value);
        space.Put32(kPointer, Sample(index + 1));
        break;
    case 0x82498EF0u:
        for (auto offset : {-28928, -28916, -28904, -28892})
            space.Put32(At(c.r9.u64, offset), value + static_cast<std::uint32_t>(offset));
        space.Put32(root, value);
        break;
    case 0x82520BF8u:
        space.Put32(High(-31951) + 24500u, kPointer);
        space.Put32(kPointer + 736u, value);
        break;
    case 0x8255C520u:
        space.Put32(root + 316u, value);
        space.Put32(root + 252u, Sample(index + 1));
        break;
    case 0x825B80F8u:
        space.Put32(root + 148u, kPointer);
        space.Put32(kPointer + 80u, value);
        break;
    case 0x825D28B8u:
        space.Put32(root + 548u, value);
        break;
    case 0x82631480u:
        space.Put8(root + c.r4.u32 + 424u, static_cast<std::uint8_t>(value));
        break;
    case 0x827B3A90u:
        space.Put8(root + 10556u, static_cast<std::uint8_t>(value));
        break;
    case 0x827B3D20u: case 0x827B3DF0u: case 0x827B3EE0u:
        space.Put8(root + c.r4.u32 + 11994u, static_cast<std::uint8_t>(value));
        break;
    case 0x827BFB30u:
        space.Put32((root + 535953408u) << 2, value);
        break;
    case 0x82805430u:
        space.Put32(root + 152u, kPointer);
        space.Put8(kPointer + 80u, static_cast<std::uint8_t>(value));
        break;
    case 0x82820C00u:
        space.Put32(High(-31962) + 17748u, kPointer);
        space.Put8(kPointer + 54u, static_cast<std::uint8_t>(value));
        break;
    case 0x82851090u:
        space.Put32(High(-31962) + 19792u, value);
        break;
    case 0x828571B8u:
        space.Put8(root + 3u, static_cast<std::uint8_t>(value));
        break;
    case 0x82AE4FB8u:
        space.Put8(High(-31955) - 24352u + 5793u,
                   static_cast<std::uint8_t>(value));
        break;
    case 0x82B5D188u:
        space.Put32(root + ((c.r4.u32 * 5u) << 2) + 3232u, value);
        break;
    case 0x82C4FF50u:
        space.Put32(root + 232u, value);
        break;
    case 0x82CC6F80u:
        space.Put8(c.r13.u32 + 268u, static_cast<std::uint8_t>(index));
        space.Put32(High(-31954) + 336u, kArray);
        space.Put32(kArray + index * 8u + 12u, value);
        break;
    case 0x82E26E50u:
        space.Put32(root + 44u, value);
        break;
    case 0x82E753D0u:
        space.Put32(High(-31953) - 10572u, value);
        break;
    case 0x82E7D258u: case 0x82E7D2C0u:
        space.Put32(root + 312u, value);
        break;
    case 0x82F6D6A0u: case 0x82F6D6C8u:
    {
        space.Put32(root + 4u, kPointer);
        space.Put32(root + 12u, value);
        space.Put32(kPointer + 32u, kArray);
        const std::uint32_t index_address =
            ((((value << 2) & 0xffffffe0u) + (value & 7u) +
              (entry.address == 0x82F6D6A0u ? 21u : 25u)) << 2) & 0xfffffffcu;
        space.Put32(kArray + index_address, Sample(index + 1));
        break;
    }
    case 0x82F8A7C8u:
        space.Put32(root + 4u, kPointer);
        space.Put32(kPointer + 40u, kSecond);
        space.Put32(kSecond, kArray);
        space.Put32(kArray + 36u, value);
        break;
    case 0x83081618u:
        space.Put8(root + (((c.r4.u32 + 32u) << 2) & 0xfffffffcu) +
                   c.r5.u32, static_cast<std::uint8_t>(value));
        break;
    default:
        throw std::runtime_error("unhandled seed entry");
    }
}

void Fill(PPCContext& c, std::uint32_t address, unsigned index)
{
    auto* bytes = reinterpret_cast<std::uint8_t*>(&c);
    for (std::size_t i = 0; i < sizeof(c); ++i)
        bytes[i] = static_cast<std::uint8_t>(i * 37u + address + index * 19u);
    const std::uint64_t high = index & 1u ? 0xfedcba9800000000ull :
                                          0x1234567800000000ull;
    c.r3.u64 = high | (0x00020000u + index * 0x101u);
    c.r4.u64 = high | (index * 3u);
    c.r5.u64 = (high ^ 0x7777777700000000ull) | 0x00080000u;
    c.r9.u64 = high | 0x00090000u;
    c.r13.u64 = high | 0x000a0000u;
}

bool Test(const TestEntry& entry, unsigned index, SparseGuestSpace& space)
{
    space.BeginCase();
    PPCContext original{};
    Fill(original, entry.address, index);
    Seed(entry, index, original, space);
    const auto before = space.Snapshot();
    PPCContext recovered{};
    std::memcpy(&recovered, &original, sizeof(original));

    entry.original(original, space.data());
    if (space.Snapshot() != before)
    {
        std::fprintf(stderr, "FAIL original read-only memory %08X case %u\n",
                     entry.address, index);
        return false;
    }

    Registers r{recovered.r3.u64, recovered.r4.u64, recovered.r5.u64,
                recovered.r8.u64, recovered.r9.u64, recovered.r10.u64,
                recovered.r11.u64, recovered.r13.u64, recovered.r18.u64};
    auto memory = space.memory();
    if (!lo::semantic::read_only_fields::Apply(entry.address, r, memory))
    {
        std::fprintf(stderr, "unmapped %08X\n", entry.address);
        return false;
    }
    recovered.r3.u64 = r.r3;
    recovered.r4.u64 = r.r4;
    recovered.r5.u64 = r.r5;
    recovered.r8.u64 = r.r8;
    recovered.r9.u64 = r.r9;
    recovered.r10.u64 = r.r10;
    recovered.r11.u64 = r.r11;
    recovered.r13.u64 = r.r13;
    recovered.r18.u64 = r.r18;
    if (std::memcmp(&original, &recovered, sizeof(original)) != 0)
    {
        std::fprintf(stderr, "FAIL context %08X case %u: r3 %016llX/%016llX "
                             "r10 %016llX/%016llX r11 %016llX/%016llX\n",
                     entry.address, index,
                     static_cast<unsigned long long>(original.r3.u64),
                     static_cast<unsigned long long>(recovered.r3.u64),
                     static_cast<unsigned long long>(original.r10.u64),
                     static_cast<unsigned long long>(recovered.r10.u64),
                     static_cast<unsigned long long>(original.r11.u64),
                     static_cast<unsigned long long>(recovered.r11.u64));
        return false;
    }
    if (space.Snapshot() != before)
    {
        std::fprintf(stderr, "FAIL recovered read-only memory %08X case %u\n",
                     entry.address, index);
        return false;
    }
    return true;
}

} // namespace

int main()
{
    SparseGuestSpace space;
    for (const auto& entry : kTestEntries)
        for (unsigned index = 0; index < kCases; ++index)
            if (!Test(entry, index, space)) return 1;

    std::array<std::uint8_t, 4> bytes{};
    GuestMemory memory(0, bytes);
    Registers unknown{1, 2, 3, 4, 5, 6, 7, 8, 9};
    if (lo::semantic::read_only_fields::Apply(0, unknown, memory) ||
        unknown.r3 != 1 || unknown.r4 != 2 || unknown.r5 != 3 ||
        unknown.r8 != 4 || unknown.r9 != 5 || unknown.r10 != 6 ||
        unknown.r11 != 7 || unknown.r13 != 8 || unknown.r18 != 9)
        return 1;

    std::printf("PASS read-only-fields %zu entries %zu cases\n",
                std::size(kTestEntries), std::size(kTestEntries) * kCases);
    return 0;
}

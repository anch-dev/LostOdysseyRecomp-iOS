// Appended after exact original generated PPC bodies by the batch runner.
#include "lo_semantics/field_arithmetic.h"
#include "lo_semantics/guest_memory.h"

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

using lo::semantic::field_arithmetic::Registers;
using lo::semantic::gpu::GuestMemory;

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr std::size_t kGuestSize = std::size_t{1} << 32;
constexpr std::uint32_t kPageSize = 4096;
constexpr unsigned kCases = 6;

enum class Family
{
    AddressFromPointerFieldAndIndex, AdjustWordField, ComposeTaggedPointer,
    ExchangeWordField, ExtractFieldBitsPlusConstant, MultiplyWordFieldByStride,
    ReadCursorAndAdvance, ReadFieldIndexedPointerMember, ReadIndexedPointerField,
    ReadLargeOffsetField, RoundWordFieldUnits, ScaleBiasedWordField,
    SumScaledWordFields, SumWordFields,
};
enum Feature : std::uint16_t
{
    HasGlobal = 1 << 0, HasPointer = 1 << 1, HasInner = 1 << 2,
    HasShift1 = 1 << 3, HasField3 = 1 << 4, HasField4 = 1 << 5,
    HasStatus = 1 << 6, HasDelta = 1 << 7, HasReplacement = 1 << 8,
    HasReturnBias = 1 << 9,
};
struct TestParameters
{
    std::int32_t field, field1, field2, field3, field4;
    std::int32_t pointer_field, inner_pointer_field, base_field, index_field;
    std::int32_t tag_field, cursor_field, member, offset_low;
    std::int32_t global_high, global_low, delta, replacement, status;
    std::int32_t bias, return_bias, region, rounding_bias, unit_bias;
    std::int32_t advance, shift, shift1, shift2;
};
struct TestEntry
{
    std::uint32_t address;
    void (*original)(PPCContext&, std::uint8_t*);
    Family family;
    TestParameters p;
    std::uint16_t flags;
};

// TEST_ENTRIES

[[nodiscard]] bool Has(const TestEntry& entry, Feature feature)
{
    return (entry.flags & feature) != 0;
}

[[nodiscard]] std::uint32_t Address(std::uint64_t base, std::int32_t offset)
{
    return static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(offset);
}

[[nodiscard]] std::uint32_t Scale(std::uint64_t value, std::uint32_t shift)
{
    return static_cast<std::uint32_t>(value) << shift;
}

[[nodiscard]] std::uint32_t Sample(unsigned index)
{
    constexpr std::uint32_t values[kCases] = {
        0u, 0xffffffffu, 0x80000000u, 0x7fffffffu, 0x12345678u, 0xfffffffcu,
    };
    return values[index % kCases];
}

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

    void Restore(const std::vector<PageImage>& images)
    {
        for (const auto& image : images)
            std::memcpy(base_ + image.address, image.bytes.data(), kPageSize);
    }

private:
    std::uint8_t* base_ = nullptr;
    std::vector<std::uint32_t> pages_;
};

void PutMember(SparseGuestSpace& space, std::uint32_t address,
               std::uint32_t value, unsigned width)
{
    if (width == 8) space.Put8(address, static_cast<std::uint8_t>(value));
    else if (width == 16) space.Put16(address, static_cast<std::uint16_t>(value));
    else space.Put32(address, value);
}

void Seed(const TestEntry& entry, unsigned case_index, const PPCContext& context,
          SparseGuestSpace& space)
{
    const auto& p = entry.p;
    const std::uint32_t root = context.r3.u32;
    constexpr std::uint32_t pointer = 0x00050000u;
    constexpr std::uint32_t inner = 0x00060000u;
    constexpr std::uint32_t array = 0x00070000u;
    const std::uint32_t value = Sample(case_index);

    switch (entry.family)
    {
    case Family::AddressFromPointerFieldAndIndex:
        space.Put32(Address(root, p.pointer_field), array);
        break;
    case Family::AdjustWordField:
        if (Has(entry, HasGlobal))
        {
            const auto base = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(p.global_high) * 65536 + p.global_low);
            space.Put32(Address(base, p.field), value);
        }
        else if (Has(entry, HasPointer))
        {
            space.Put32(Address(root, p.pointer_field), pointer);
            space.Put32(Address(pointer, p.field), value);
        }
        else if (entry.address == 0x829A4948u)
            space.Put32(Address(context.r4.u64, p.field), value);
        else space.Put32(Address(root, p.field), value);
        break;
    case Family::ComposeTaggedPointer:
        if (!Has(entry, HasPointer))
            space.Put32(Address(root, p.tag_field), value);
        else
        {
            space.Put32(Address(root, p.pointer_field), pointer);
            space.Put32(Address(root, p.base_field), Sample(case_index + 2));
            if (Has(entry, HasInner))
                space.Put32(Address(pointer, p.inner_pointer_field), inner);
            space.Put32(Address(Has(entry, HasInner) ? inner : pointer, p.tag_field), value);
        }
        break;
    case Family::ExchangeWordField:
        if (Has(entry, HasGlobal))
            space.Put32(Address(static_cast<std::uint64_t>(
                static_cast<std::int64_t>(p.global_high) * 65536), p.field), value);
        else space.Put32(Address(root, p.field), value);
        break;
    case Family::ReadCursorAndAdvance:
    {
        const std::uint32_t cursor = case_index == 5 ?
            Address(root, p.cursor_field) : pointer;
        space.Put32(Address(root, p.cursor_field), cursor);
        if (case_index != 5)
            PutMember(space, cursor, value, static_cast<unsigned>(p.advance) * 8);
        break;
    }
    case Family::ReadFieldIndexedPointerMember:
    {
        space.Put32(Address(root, p.index_field), value);
        space.Put32(Address(root, p.pointer_field), array);
        const auto scaled = Scale(static_cast<std::uint64_t>(value) +
                                  Scale(value, p.shift1), p.shift2);
        space.Put32(Address(static_cast<std::uint32_t>(array + scaled), p.member),
                    Sample(case_index + 1));
        break;
    }
    case Family::ReadIndexedPointerField:
    {
        space.Put32(Address(root, p.pointer_field), pointer);
        std::uint32_t indexed_base = pointer;
        if (Has(entry, HasInner))
        {
            space.Put32(Address(pointer, p.inner_pointer_field), inner);
            indexed_base = inner;
        }
        const auto index = Has(entry, HasShift1) ?
            Scale(context.r4.u64 + Scale(context.r4.u64, p.shift1), p.shift2) :
            Scale(context.r4.u64, p.shift);
        const auto target = Address(static_cast<std::uint32_t>(indexed_base + index), p.member);
        const unsigned width = entry.address == 0x825988B0u ||
            entry.address == 0x826827C8u || entry.address == 0x82682EB8u ||
            entry.address == 0x82684ED0u ? 8u : 32u;
        PutMember(space, target, Sample(case_index + 1), width);
        break;
    }
    case Family::ReadLargeOffsetField:
        PutMember(space, Address(root, p.offset_low), value,
                  entry.address == 0x82BCE398u ? 8u : 32u);
        break;
    case Family::SumScaledWordFields:
    case Family::SumWordFields:
        space.Put32(Address(root, p.field1), value);
        space.Put32(Address(root, p.field2), Sample(case_index + 1));
        if (Has(entry, HasField3))
            space.Put32(Address(root, p.field3), Sample(case_index + 2));
        if (Has(entry, HasField4))
            space.Put32(Address(root, p.field4), Sample(case_index + 3));
        break;
    default:
        space.Put32(Address(root, p.field), value);
        break;
    }
}

void FillContext(PPCContext& context, std::uint32_t address, unsigned case_index)
{
    auto* bytes = reinterpret_cast<std::uint8_t*>(&context);
    for (std::size_t i = 0; i < sizeof(context); ++i)
        bytes[i] = static_cast<std::uint8_t>(i * 37u + address + case_index * 19u);
    const std::uint64_t high = case_index & 1u ? 0xfedcba9800000000ull :
                                                0x1234567800000000ull;
    context.r3.u64 = high | (0x00020000u + case_index * 0x100u);
    context.r4.u64 = high | Sample(case_index);
    context.r5.u64 = (high ^ 0x7777777700000000ull) | Sample(case_index + 2);
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
                        recovered.r8.u64, recovered.r9.u64, recovered.r10.u64,
                        recovered.r11.u64};
    auto memory = space.memory();
    if (!lo::semantic::field_arithmetic::Apply(entry.address, registers, memory))
    {
        std::fprintf(stderr, "unmapped %08X\n", entry.address);
        return false;
    }
    recovered.r3.u64 = registers.r3;
    recovered.r4.u64 = registers.r4;
    recovered.r5.u64 = registers.r5;
    recovered.r8.u64 = registers.r8;
    recovered.r9.u64 = registers.r9;
    recovered.r10.u64 = registers.r10;
    recovered.r11.u64 = registers.r11;

    if (std::memcmp(&original, &recovered, sizeof(original)) != 0)
    {
        std::fprintf(stderr, "FAIL context %08X case %u: r3 %016llX/%016llX "
                             "r10 %016llX/%016llX r11 %016llX/%016llX\n",
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
    Registers unknown{1, 2, 3, 4, 5, 6, 7};
    if (lo::semantic::field_arithmetic::Apply(0, unknown, memory) ||
        unknown.r3 != 1 || unknown.r4 != 2 || unknown.r5 != 3 ||
        unknown.r8 != 4 || unknown.r9 != 5 || unknown.r10 != 6 ||
        unknown.r11 != 7)
        return 1;

    std::printf("PASS field-arithmetic %zu entries %zu cases\n",
                std::size(kTestEntries), std::size(kTestEntries) * kCases);
    return 0;
}

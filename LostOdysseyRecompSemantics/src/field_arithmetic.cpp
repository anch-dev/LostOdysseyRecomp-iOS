#include "lo_semantics/field_arithmetic.h"

#include <array>
#include <bit>
#include <cstdint>

namespace lo::semantic::field_arithmetic
{
namespace
{

enum class Family : std::uint8_t
{
    AddressFromPointerFieldAndIndex,
    AdjustWordField,
    ComposeTaggedPointer,
    ExchangeWordField,
    ExtractFieldBitsPlusConstant,
    MultiplyWordFieldByStride,
    ReadCursorAndAdvance,
    ReadFieldIndexedPointerMember,
    ReadIndexedPointerField,
    ReadLargeOffsetField,
    RoundWordFieldUnits,
    ScaleBiasedWordField,
    SumScaledWordFields,
    SumWordFields,
};

enum Feature : std::uint16_t
{
    HasGlobal = 1 << 0,
    HasPointer = 1 << 1,
    HasInner = 1 << 2,
    HasShift1 = 1 << 3,
    HasField3 = 1 << 4,
    HasField4 = 1 << 5,
    HasStatus = 1 << 6,
    HasDelta = 1 << 7,
    HasReplacement = 1 << 8,
    HasReturnBias = 1 << 9,
};

// Each row names object fields and fixed arithmetic parameters. The source
// address and complete ordered register effects are recorded in the manifest.
struct Parameters
{
    std::int32_t field = 0;
    std::int32_t field1 = 0;
    std::int32_t field2 = 0;
    std::int32_t field3 = 0;
    std::int32_t field4 = 0;
    std::int32_t pointer_field = 0;
    std::int32_t inner_pointer_field = 0;
    std::int32_t base_field = 0;
    std::int32_t index_field = 0;
    std::int32_t tag_field = 0;
    std::int32_t cursor_field = 0;
    std::int32_t member = 0;
    std::int32_t offset_low = 0;
    std::int32_t global_high = 0;
    std::int32_t global_low = 0;
    std::int32_t delta = 0;
    std::int32_t replacement = 0;
    std::int32_t status = 0;
    std::int32_t bias = 0;
    std::int32_t return_bias = 0;
    std::int32_t region = 0;
    std::int32_t rounding_bias = 0;
    std::int32_t unit_bias = 0;
    std::int32_t advance = 0;
    std::int32_t shift = 0;
    std::int32_t shift1 = 0;
    std::int32_t shift2 = 0;
};

struct Entry
{
    std::uint32_t address;
    Family family;
    Parameters parameters;
    std::uint16_t features;
};

constexpr std::array<Entry, 89> kEntries{{
    {0x822D9078u, Family::ScaleBiasedWordField, {.field = 252, .bias = 84, .shift = 2}, 0},
    {0x82322348u, Family::ExchangeWordField, {.field = -32684, .global_high = -31950, .replacement = 0}, HasGlobal | HasReplacement},
    {0x823CB338u, Family::AdjustWordField, {.field = 32, .global_high = -31964, .global_low = -27980, .delta = 1}, HasGlobal | HasDelta},
    {0x823F1140u, Family::SumWordFields, {.field1 = 52, .field2 = 44}, 0},
    {0x823F13A0u, Family::AdjustWordField, {.field = 324, .delta = 1, .status = 0}, HasStatus | HasDelta},
    {0x8240C800u, Family::AdjustWordField, {.field = 112}, 0},
    {0x825988B0u, Family::ReadIndexedPointerField, {.pointer_field = 28, .member = 16, .shift1 = 2, .shift2 = 2}, HasPointer | HasShift1},
    {0x825DE940u, Family::ScaleBiasedWordField, {.field = 252, .bias = 72, .shift = 2}, 0},
    {0x825DFD70u, Family::ScaleBiasedWordField, {.field = 252, .bias = 68, .shift = 2}, 0},
    {0x826827C8u, Family::ReadIndexedPointerField, {.pointer_field = 8, .member = 16, .shift1 = 2, .shift2 = 2}, HasPointer | HasShift1},
    {0x82682EB8u, Family::ReadIndexedPointerField, {.pointer_field = 8, .member = 28, .shift = 5}, HasPointer},
    {0x82684ED0u, Family::ReadIndexedPointerField, {.pointer_field = 8, .member = 76, .shift1 = 2, .shift2 = 4}, HasPointer | HasShift1},
    {0x826B2688u, Family::MultiplyWordFieldByStride, {.field = 8, .shift1 = 2, .shift2 = 3}, HasShift1},
    {0x826B2F88u, Family::SumScaledWordFields, {.field1 = 84, .field2 = 120, .field3 = 108, .field4 = 96, .shift = 4}, HasField3 | HasField4},
    {0x826D30E0u, Family::SumScaledWordFields, {.field1 = 152, .field2 = 140, .shift = 2}, 0},
    {0x826DCE28u, Family::MultiplyWordFieldByStride, {.field = 8, .shift1 = 1, .shift2 = 4}, HasShift1},
    {0x826F4680u, Family::ScaleBiasedWordField, {.field = 252, .bias = 76, .shift = 2}, 0},
    {0x8270D248u, Family::ScaleBiasedWordField, {.field = 252, .bias = 232, .shift = 2}, 0},
    {0x82715E18u, Family::ScaleBiasedWordField, {.field = 252, .bias = 80, .shift = 2}, 0},
    {0x82755A70u, Family::AddressFromPointerFieldAndIndex, {.pointer_field = 28, .member = 48, .shift1 = 2, .shift2 = 6}, HasPointer | HasShift1},
    {0x8275CE38u, Family::ScaleBiasedWordField, {.field = 252, .bias = 120, .shift = 2}, 0},
    {0x8278DD58u, Family::ScaleBiasedWordField, {.field = 252, .bias = 100, .shift = 2}, 0},
    {0x827AC488u, Family::ExtractFieldBitsPlusConstant, {.field = 44, .return_bias = 1, .shift = 26}, HasReturnBias},
    {0x827B6430u, Family::AdjustWordField, {.field = 60, .delta = 1}, HasDelta},
    {0x827CAFE0u, Family::ExchangeWordField, {.field = 18540, .global_high = -31945}, HasGlobal},
    {0x828510A8u, Family::AdjustWordField, {.field = 19792, .global_high = -31962, .delta = 1}, HasGlobal | HasDelta},
    {0x828510C0u, Family::AdjustWordField, {.field = 19792, .global_high = -31962, .delta = -1}, HasGlobal | HasDelta},
    {0x82881168u, Family::ReadLargeOffsetField, {.offset_low = 45160}, 0},
    {0x82881178u, Family::ReadLargeOffsetField, {.offset_low = 45152}, 0},
    {0x82881188u, Family::ReadLargeOffsetField, {.offset_low = 48640}, 0},
    {0x82881198u, Family::ReadLargeOffsetField, {.offset_low = 50460}, 0},
    {0x82890F58u, Family::ExchangeWordField, {.field = 20, .replacement = -1}, HasReplacement},
    {0x828B6340u, Family::ReadFieldIndexedPointerMember, {.pointer_field = 132, .index_field = 56, .member = 4, .shift1 = 1, .shift2 = 4}, HasPointer | HasShift1},
    {0x828C3678u, Family::ReadFieldIndexedPointerMember, {.pointer_field = 132, .index_field = 56, .member = 4, .shift1 = 1, .shift2 = 3}, HasPointer | HasShift1},
    {0x828C4738u, Family::ReadFieldIndexedPointerMember, {.pointer_field = 192, .index_field = 56, .member = 4, .shift1 = 1, .shift2 = 3}, HasPointer | HasShift1},
    {0x828C4758u, Family::ReadFieldIndexedPointerMember, {.pointer_field = 192, .index_field = 56, .member = 8, .shift1 = 1, .shift2 = 3}, HasPointer | HasShift1},
    {0x828CC7D0u, Family::ReadFieldIndexedPointerMember, {.pointer_field = 180, .index_field = 56, .member = 4, .shift1 = 1, .shift2 = 3}, HasPointer | HasShift1},
    {0x828CDE80u, Family::ReadFieldIndexedPointerMember, {.pointer_field = 176, .index_field = 56, .member = 4, .shift1 = 1, .shift2 = 3}, HasPointer | HasShift1},
    {0x828CEFB0u, Family::ReadFieldIndexedPointerMember, {.pointer_field = 168, .index_field = 56, .member = 4, .shift1 = 1, .shift2 = 3}, HasPointer | HasShift1},
    {0x829A4948u, Family::AdjustWordField, {.field = 1692, .delta = 4, .status = 0}, HasStatus | HasDelta},
    {0x82A680D0u, Family::ExchangeWordField, {.field = 132, .replacement = 0}, HasReplacement},
    {0x82AF7FB8u, Family::AdjustWordField, {.field = 52, .pointer_field = 24, .delta = 9}, HasPointer | HasDelta},
    {0x82AF7FD0u, Family::AdjustWordField, {.field = 52, .pointer_field = 24, .delta = 2}, HasPointer | HasDelta},
    {0x82B63DE8u, Family::ExchangeWordField, {.field = 10328, .global_high = -31955}, HasGlobal},
    {0x82B6B840u, Family::ReadIndexedPointerField, {.pointer_field = 8, .member = -4, .shift = 2}, HasPointer},
    {0x82B6B858u, Family::AddressFromPointerFieldAndIndex, {.pointer_field = 0, .shift = 16}, HasPointer},
    {0x82B797D8u, Family::AdjustWordField, {.field = 8, .delta = -1}, HasDelta},
    {0x82B8DBA0u, Family::AddressFromPointerFieldAndIndex, {.pointer_field = 68, .shift = 5}, HasPointer},
    {0x82B94C60u, Family::ReadIndexedPointerField, {.pointer_field = 40, .inner_pointer_field = 24, .member = 12, .shift = 4}, HasPointer | HasInner},
    {0x82B9B4B0u, Family::AdjustWordField, {.field = 4, .status = 1}, HasStatus},
    {0x82BC51C0u, Family::AddressFromPointerFieldAndIndex, {.pointer_field = -152, .shift1 = 3, .shift2 = 2}, HasPointer | HasShift1},
    {0x82BC8B48u, Family::ExchangeWordField, {.field = 16, .replacement = 0}, HasReplacement},
    {0x82BC8B60u, Family::ExchangeWordField, {.field = 12, .replacement = 0}, HasReplacement},
    {0x82BCE398u, Family::ReadLargeOffsetField, {.offset_low = 32852}, 0},
    {0x82BCE658u, Family::ReadLargeOffsetField, {.offset_low = 32868}, 0},
    {0x82BCE6F0u, Family::ReadLargeOffsetField, {.offset_low = 32872}, 0},
    {0x82BDB340u, Family::MultiplyWordFieldByStride, {.field = 4, .return_bias = 8, .shift = 5}, HasReturnBias},
    {0x82BDB630u, Family::MultiplyWordFieldByStride, {.field = 4, .shift1 = 2, .shift2 = 2}, HasShift1},
    {0x82BDB648u, Family::MultiplyWordFieldByStride, {.field = 4, .return_bias = 32, .shift1 = 2, .shift2 = 2}, HasShift1 | HasReturnBias},
    {0x82BDBD78u, Family::MultiplyWordFieldByStride, {.field = 4, .shift1 = 3, .shift2 = 2}, HasShift1},
    {0x82BDBEC0u, Family::MultiplyWordFieldByStride, {.field = 4, .return_bias = 8, .shift1 = 3, .shift2 = 2}, HasShift1 | HasReturnBias},
    {0x82BDC1F0u, Family::MultiplyWordFieldByStride, {.field = 4, .shift1 = 1, .shift2 = 3}, HasShift1},
    {0x82BDC820u, Family::MultiplyWordFieldByStride, {.field = 4, .return_bias = 32, .shift1 = 1, .shift2 = 3}, HasShift1 | HasReturnBias},
    {0x82BDE550u, Family::ReadCursorAndAdvance, {.cursor_field = 4, .advance = 1}, 0},
    {0x82BDE568u, Family::ReadCursorAndAdvance, {.cursor_field = 4, .advance = 2}, 0},
    {0x82BDE580u, Family::ReadCursorAndAdvance, {.cursor_field = 4, .advance = 4}, 0},
    {0x82C3A458u, Family::RoundWordFieldUnits, {.field = 4, .rounding_bias = 3, .unit_bias = 2}, 0},
    {0x82CC6118u, Family::ReadIndexedPointerField, {.pointer_field = 8, .member = 76, .shift1 = 1, .shift2 = 5}, HasPointer | HasShift1},
    {0x82CC66A0u, Family::AdjustWordField, {.field = 4, .delta = 1}, HasDelta},
    {0x82CD5940u, Family::AdjustWordField, {.field = 8, .delta = 1}, HasDelta},
    {0x82CEE848u, Family::AdjustWordField, {.field = 320, .delta = 1, .status = 0}, HasStatus | HasDelta},
    {0x82CFA670u, Family::ExchangeWordField, {.field = 92}, 0},
    {0x82DFBB88u, Family::AdjustWordField, {.field = 172, .delta = 1}, HasDelta},
    {0x82F4C7A8u, Family::ComposeTaggedPointer, {.tag_field = 48, .region = 16}, 0},
    {0x82F51D58u, Family::ComposeTaggedPointer, {.pointer_field = 4, .inner_pointer_field = 8, .base_field = 8, .tag_field = 48, .region = 128}, HasPointer | HasInner},
    {0x82F591C8u, Family::ComposeTaggedPointer, {.pointer_field = 4, .base_field = 8, .tag_field = 48, .region = 32}, HasPointer},
    {0x82F5A5D0u, Family::ComposeTaggedPointer, {.pointer_field = 4, .base_field = 8, .tag_field = 48, .region = 64}, HasPointer},
    {0x82F5ABB0u, Family::ComposeTaggedPointer, {.pointer_field = 4, .base_field = 8, .tag_field = 48, .region = 80}, HasPointer},
    {0x82F5C350u, Family::ComposeTaggedPointer, {.pointer_field = 4, .inner_pointer_field = 0, .base_field = 8, .tag_field = 48, .region = 48}, HasPointer | HasInner},
    {0x82F5D0F0u, Family::ComposeTaggedPointer, {.pointer_field = 4, .inner_pointer_field = 0, .base_field = 8, .tag_field = 48, .region = 144}, HasPointer | HasInner},
    {0x82F5E088u, Family::ComposeTaggedPointer, {.pointer_field = 4, .inner_pointer_field = 0, .base_field = 8, .tag_field = 48, .region = 176}, HasPointer | HasInner},
    {0x82F5ECD8u, Family::ComposeTaggedPointer, {.pointer_field = 4, .inner_pointer_field = 0, .base_field = 8, .tag_field = 48, .region = 160}, HasPointer | HasInner},
    {0x82F6DAD0u, Family::AddressFromPointerFieldAndIndex, {.pointer_field = 64, .shift = 4}, HasPointer},
    {0x82F71A68u, Family::AddressFromPointerFieldAndIndex, {.pointer_field = 68, .shift1 = 2, .shift2 = 2}, HasPointer | HasShift1},
    {0x82F917E0u, Family::AdjustWordField, {.field = 120, .delta = 1}, HasDelta},
    {0x830765D0u, Family::SumWordFields, {.field1 = 908, .field2 = 900}, 0},
    {0x830765E0u, Family::SumWordFields, {.field1 = 916, .field2 = 908, .field3 = 900}, HasField3},
    {0x830765F8u, Family::SumWordFields, {.field1 = 916, .field2 = 924, .field3 = 908, .field4 = 900}, HasField3 | HasField4},
    {0x83076988u, Family::SumWordFields, {.field1 = 32, .field2 = 12}, 0},
}};

[[nodiscard]] bool Has(const Entry& entry, Feature feature)
{
    return (entry.features & feature) != 0;
}

[[nodiscard]] std::uint32_t Address(std::uint64_t base, std::int32_t displacement)
{
    return static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(displacement);
}

[[nodiscard]] std::uint64_t AddSigned(std::uint64_t value, std::int64_t increment)
{
    return value + static_cast<std::uint64_t>(increment);
}

[[nodiscard]] std::uint64_t ShiftWord(std::uint64_t value, std::uint32_t shift)
{
    return static_cast<std::uint32_t>(value) << shift;
}

[[nodiscard]] std::uint64_t ReadMember(gpu::GuestMemory& memory,
                                       std::uint64_t base, std::int32_t offset,
                                       std::uint32_t width)
{
    const auto address = Address(base, offset);
    if (width == 8) return memory.ReadU8(address);
    if (width == 16) return memory.ReadU16(address);
    return memory.ReadU32(address);
}

void AddressFromPointerFieldAndIndex(const Entry& entry, Registers& r,
                                     gpu::GuestMemory& memory)
{
    const auto& p = entry.parameters;
    if (Has(entry, HasShift1))
    {
        if (entry.address == 0x82755A70u)
        {
            r.r11 = ShiftWord(r.r4, p.shift1);
            r.r10 = memory.ReadU32(Address(r.r3, p.pointer_field));
            r.r11 += r.r4;
            r.r11 = ShiftWord(r.r11, p.shift2);
            r.r11 += r.r10;
            r.r3 = AddSigned(r.r11, p.member);
        }
        else
        {
            r.r10 = ShiftWord(r.r4, p.shift1);
            r.r11 = memory.ReadU32(Address(r.r3, p.pointer_field));
            r.r10 += r.r4;
            r.r10 = ShiftWord(r.r10, p.shift2);
            r.r3 = r.r11 + r.r10;
        }
        return;
    }

    if (entry.address == 0x82B6B858u)
    {
        r.r10 = memory.ReadU32(Address(r.r3, p.pointer_field));
        r.r11 = ShiftWord(r.r4, p.shift);
        r.r3 = r.r11 + r.r10;
    }
    else
    {
        r.r11 = memory.ReadU32(Address(r.r3, p.pointer_field));
        r.r10 = ShiftWord(r.r4, p.shift);
        r.r3 = r.r11 + r.r10;
    }
}

void AdjustWordField(const Entry& entry, Registers& r, gpu::GuestMemory& memory)
{
    const auto& p = entry.parameters;
    if (entry.address == 0x8240C800u || entry.address == 0x829A4948u ||
        entry.address == 0x82DFBB88u)
    {
        const auto base = entry.address == 0x829A4948u ? r.r4 : r.r3;
        r.r11 = memory.ReadU32(Address(base, p.field));
        if (entry.address == 0x829A4948u) r.r3 = 0;
        r.r11 = entry.address == 0x8240C800u ? r.r5 + r.r11 :
                AddSigned(r.r11, p.delta);
        memory.WriteU32(Address(base, p.field), static_cast<std::uint32_t>(r.r11));
        return;
    }

    if (Has(entry, HasGlobal))
        r.r11 = AddSigned(static_cast<std::uint64_t>(
            static_cast<std::int64_t>(p.global_high) * 65536), p.global_low);
    else if (Has(entry, HasPointer))
        r.r11 = memory.ReadU32(Address(r.r3, p.pointer_field));
    else
        r.r11 = r.r3;

    if (Has(entry, HasStatus)) r.r3 = static_cast<std::uint64_t>(p.status);
    r.r10 = memory.ReadU32(Address(r.r11, p.field));
    const auto adjusted = Has(entry, HasDelta) ? AddSigned(r.r10, p.delta) : r.r5 + r.r10;
    if (Has(entry, HasGlobal) || Has(entry, HasPointer) || Has(entry, HasStatus))
    {
        r.r10 = adjusted;
        memory.WriteU32(Address(r.r11, p.field), static_cast<std::uint32_t>(r.r10));
    }
    else
    {
        r.r3 = adjusted;
        memory.WriteU32(Address(r.r11, p.field), static_cast<std::uint32_t>(r.r3));
    }
}

void ComposeTaggedPointer(const Entry& entry, Registers& r, gpu::GuestMemory& memory)
{
    const auto& p = entry.parameters;
    if (!Has(entry, HasPointer))
    {
        r.r11 = memory.ReadU32(Address(r.r3, p.tag_field));
        r.r10 = ShiftWord(r.r11 & 0x3fu, 26);
        r.r11 += r.r10;
    }
    else
    {
        r.r11 = memory.ReadU32(Address(r.r3, p.pointer_field));
        r.r10 = memory.ReadU32(Address(r.r3, p.base_field));
        if (Has(entry, HasInner))
            r.r11 = memory.ReadU32(Address(r.r11, p.inner_pointer_field));
        r.r11 = memory.ReadU32(Address(r.r11, p.tag_field));
        r.r11 = ShiftWord(r.r11 & 0x3fu, 26);
        r.r11 += r.r10;
    }
    r.r3 = AddSigned(r.r11, static_cast<std::int64_t>(p.region) * 65536);
}

void ExchangeWordField(const Entry& entry, Registers& r, gpu::GuestMemory& memory)
{
    const auto& p = entry.parameters;
    if (Has(entry, HasGlobal))
    {
        r.r11 = static_cast<std::uint64_t>(static_cast<std::int64_t>(p.global_high) * 65536);
        if (Has(entry, HasReplacement)) r.r9 = static_cast<std::uint64_t>(p.replacement);
        else r.r10 = r.r3;
    }
    else
    {
        r.r11 = r.r3;
        if (Has(entry, HasReplacement)) r.r10 = static_cast<std::uint64_t>(p.replacement);
    }
    r.r3 = memory.ReadU32(Address(r.r11, p.field));
    const auto replacement = entry.address == 0x82322348u ? r.r9 :
                             Has(entry, HasReplacement) || Has(entry, HasGlobal) ? r.r10 : r.r4;
    memory.WriteU32(Address(r.r11, p.field), static_cast<std::uint32_t>(replacement));
}

void MultiplyWordFieldByStride(const Entry& entry, Registers& r,
                               gpu::GuestMemory& memory)
{
    const auto& p = entry.parameters;
    r.r11 = memory.ReadU32(Address(r.r3, p.field));
    if (Has(entry, HasShift1))
    {
        r.r10 = ShiftWord(r.r11, p.shift1);
        r.r11 += r.r10;
        if (Has(entry, HasReturnBias))
        {
            r.r11 = ShiftWord(r.r11, p.shift2);
            r.r3 = AddSigned(r.r11, p.return_bias);
        }
        else r.r3 = ShiftWord(r.r11, p.shift2);
    }
    else
    {
        r.r11 = ShiftWord(r.r11, p.shift);
        r.r3 = AddSigned(r.r11, p.return_bias);
    }
}

void ReadIndexedPointerField(const Entry& entry, Registers& r,
                             gpu::GuestMemory& memory)
{
    const auto& p = entry.parameters;
    if (Has(entry, HasShift1))
    {
        r.r11 = ShiftWord(r.r4, p.shift1);
        r.r10 = memory.ReadU32(Address(r.r3, p.pointer_field));
        r.r11 += r.r4;
        r.r11 = ShiftWord(r.r11, p.shift2);
        r.r11 += r.r10;
    }
    else if (entry.address == 0x82B94C60u)
    {
        r.r11 = memory.ReadU32(Address(r.r3, p.pointer_field));
        r.r10 = ShiftWord(r.r4, p.shift);
        r.r11 = memory.ReadU32(Address(r.r11, p.inner_pointer_field));
        r.r11 += r.r10;
    }
    else
    {
        r.r10 = memory.ReadU32(Address(r.r3, p.pointer_field));
        r.r11 = ShiftWord(r.r4, p.shift);
        r.r11 += r.r10;
    }
    const std::uint32_t width = entry.address == 0x825988B0u ||
        entry.address == 0x826827C8u || entry.address == 0x82682EB8u ||
        entry.address == 0x82684ED0u ? 8u : 32u;
    r.r3 = ReadMember(memory, r.r11, p.member, width);
}

void SumWordFields(const Entry& entry, Registers& r, gpu::GuestMemory& memory)
{
    const auto& p = entry.parameters;
    const auto base = r.r3;
    if (Has(entry, HasField4))
    {
        r.r10 = memory.ReadU32(Address(base, p.field1));
        r.r11 = memory.ReadU32(Address(base, p.field2));
        r.r9 = memory.ReadU32(Address(base, p.field3));
        r.r11 += r.r10;
        r.r10 = memory.ReadU32(Address(base, p.field4));
        r.r11 += r.r9;
        r.r3 = r.r11 + r.r10;
    }
    else if (Has(entry, HasField3))
    {
        r.r11 = memory.ReadU32(Address(base, p.field1));
        r.r9 = memory.ReadU32(Address(base, p.field2));
        r.r10 = memory.ReadU32(Address(base, p.field3));
        r.r11 += r.r9;
        r.r3 = r.r11 + r.r10;
    }
    else
    {
        r.r11 = memory.ReadU32(Address(base, p.field1));
        r.r10 = memory.ReadU32(Address(base, p.field2));
        r.r3 = r.r11 + r.r10;
    }
}

} // namespace

bool Apply(std::uint32_t address, Registers& r, gpu::GuestMemory& memory)
{
    const Entry* entry = nullptr;
    for (const auto& candidate : kEntries)
        if (candidate.address == address) { entry = &candidate; break; }
    if (!entry) return false;

    const auto& p = entry->parameters;
    switch (entry->family)
    {
    case Family::AddressFromPointerFieldAndIndex:
        AddressFromPointerFieldAndIndex(*entry, r, memory);
        break;
    case Family::AdjustWordField:
        AdjustWordField(*entry, r, memory);
        break;
    case Family::ComposeTaggedPointer:
        ComposeTaggedPointer(*entry, r, memory);
        break;
    case Family::ExchangeWordField:
        ExchangeWordField(*entry, r, memory);
        break;
    case Family::ExtractFieldBitsPlusConstant:
        r.r11 = memory.ReadU32(Address(r.r3, p.field));
        r.r11 = std::rotl(static_cast<std::uint32_t>(r.r11), p.shift) & 0xfu;
        r.r3 = AddSigned(r.r11, p.return_bias);
        break;
    case Family::MultiplyWordFieldByStride:
        MultiplyWordFieldByStride(*entry, r, memory);
        break;
    case Family::ReadCursorAndAdvance:
        r.r11 = r.r3;
        r.r10 = memory.ReadU32(Address(r.r11, p.cursor_field));
        r.r9 = AddSigned(r.r10, p.advance);
        r.r3 = ReadMember(memory, r.r10, 0, static_cast<std::uint32_t>(p.advance) * 8);
        memory.WriteU32(Address(r.r11, p.cursor_field), static_cast<std::uint32_t>(r.r9));
        break;
    case Family::ReadFieldIndexedPointerMember:
        r.r11 = memory.ReadU32(Address(r.r3, p.index_field));
        r.r10 = memory.ReadU32(Address(r.r3, p.pointer_field));
        r.r9 = ShiftWord(r.r11, p.shift1);
        r.r11 += r.r9;
        r.r11 = ShiftWord(r.r11, p.shift2);
        r.r11 += r.r10;
        r.r3 = memory.ReadU32(Address(r.r11, p.member));
        break;
    case Family::ReadIndexedPointerField:
        ReadIndexedPointerField(*entry, r, memory);
        break;
    case Family::ReadLargeOffsetField:
        r.r11 = 0;
        r.r11 |= static_cast<std::uint32_t>(p.offset_low);
        r.r3 = ReadMember(memory, r.r3, static_cast<std::int32_t>(r.r11),
                          entry->address == 0x82BCE398u ? 8u : 32u);
        break;
    case Family::RoundWordFieldUnits:
        r.r11 = memory.ReadU32(Address(r.r3, p.field));
        r.r11 = AddSigned(r.r11, p.rounding_bias);
        r.r11 = static_cast<std::uint32_t>(r.r11) >> 2;
        r.r3 = AddSigned(r.r11, p.unit_bias);
        break;
    case Family::ScaleBiasedWordField:
        r.r11 = memory.ReadU32(Address(r.r3, p.field));
        r.r11 = AddSigned(r.r11, p.bias);
        r.r3 = ShiftWord(r.r11, p.shift);
        break;
    case Family::SumScaledWordFields:
    {
        const auto base = r.r3;
        r.r11 = memory.ReadU32(Address(base, p.field1));
        if (Has(*entry, HasField4))
        {
            r.r8 = memory.ReadU32(Address(base, p.field2));
            r.r11 = ShiftWord(r.r11, p.shift);
            r.r9 = memory.ReadU32(Address(base, p.field3));
            r.r10 = memory.ReadU32(Address(base, p.field4));
            r.r11 += r.r8;
            r.r11 += r.r9;
        }
        else
        {
            r.r10 = memory.ReadU32(Address(base, p.field2));
            r.r11 = ShiftWord(r.r11, p.shift);
        }
        r.r3 = r.r11 + r.r10;
        break;
    }
    case Family::SumWordFields:
        SumWordFields(*entry, r, memory);
        break;
    }
    return true;
}

} // namespace lo::semantic::field_arithmetic

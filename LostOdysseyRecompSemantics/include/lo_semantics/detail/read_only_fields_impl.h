#pragma once

#include <bit>
#include <cstdint>

namespace lo::semantic::read_only_fields
{
namespace detail
{

[[nodiscard]] inline std::uint32_t Low(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] inline std::uint32_t At(std::uint64_t base, std::int32_t offset)
{
    return Low(base) + static_cast<std::uint32_t>(offset);
}

[[nodiscard]] inline std::uint64_t High(std::int32_t upper)
{
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(upper) * 65536);
}

[[nodiscard]] inline std::uint64_t RotMask(std::uint64_t value, int rotate,
                                    std::uint32_t mask)
{
    return std::rotl(Low(value), rotate) & mask;
}

[[nodiscard]] inline std::uint64_t Insert(std::uint64_t target, std::uint64_t value,
                                   int rotate, std::uint32_t mask)
{
    return RotMask(value, rotate, mask) | (target & ~std::uint64_t{mask});
}

[[nodiscard]] inline std::uint64_t IsZero(std::uint64_t value)
{
    // The original cntlzw/rlwinm sequence leaves its boolean in the full GPR.
    return Low(value) == 0 ? 1u : 0u;
}

[[nodiscard]] inline std::uint64_t LeadingZeroes(std::uint64_t value)
{
    return std::countl_zero(Low(value));
}

template <typename Memory>
[[nodiscard]] inline std::uint32_t Word(Memory& memory, std::uint64_t base,
                                 std::int32_t offset = 0)
{
    return memory.ReadU32(At(base, offset));
}

template <typename Memory>
[[nodiscard]] inline std::uint16_t Half(Memory& memory, std::uint64_t base,
                                 std::int32_t offset = 0)
{
    return memory.ReadU16(At(base, offset));
}

template <typename Memory>
[[nodiscard]] inline std::uint8_t Byte(Memory& memory, std::uint64_t base,
                                std::int32_t offset = 0)
{
    return memory.ReadU8(At(base, offset));
}

// Three original leaf functions read different bits of one indexed byte.
template <typename Memory>
inline void ReadIndexedByteBit(Registers& r, Memory& memory, int bit)
{
    r.r11 = r.r3 + r.r4;
    r.r11 = Byte(memory, r.r11, 11994);
    r.r3 = (r.r11 >> bit) & 1u;
}

// The three table probes differ only in their selected 16-bit flag.
template <typename Memory>
inline void ReadTableHalfwordFlag(Registers& r, Memory& memory,
                           std::uint32_t index_mask, std::uint32_t flag,
                           bool indirect_global)
{
    r.r11 = High(-31967);
    r.r10 = RotMask(r.r3, 1, index_mask);
    if (indirect_global)
        r.r11 = Word(memory, r.r11, 21248);
    else
        r.r11 += 21024;
    r.r11 = Word(memory, r.r11, 200);
    r.r11 = Half(memory, Low(r.r10) + Low(r.r11));
    r.r3 = r.r11 & flag;
}

} // namespace detail

template <typename Memory>
bool ApplyWith(std::uint32_t address, Registers& r, Memory& memory)
{
    using namespace detail;
    switch (address)
    {
    case 0x822A7F38u: // Read the first flag of the large object field.
        r.r11 = High(3);
        r.r11 |= 7632u;
        r.r11 = Word(memory, Low(r.r3) + Low(r.r11));
        r.r3 = (r.r11 >> 31) & 1u;
        return true;

    case 0x822B5430u: // Look up the object in a global pointer table.
        r.r11 = High(-31953);
        r.r10 = RotMask(r.r3, 8, 0xfcu);
        r.r11 -= 10472u;
        r.r3 = Word(memory, Low(r.r10) + Low(r.r11));
        return true;

    case 0x822D7C38u: // Locate a four-byte member in a twelve-byte record.
        r.r11 = Word(memory, r.r3, 280);
        r.r10 = Word(memory, r.r3, 252);
        r.r9 = RotMask(r.r11, 1, 0xfffffffeu);
        r.r11 += r.r9;
        r.r11 = RotMask(r.r11, 2, 0xfffffffcu);
        r.r11 += r.r10;
        r.r11 += 76u;
        r.r3 = RotMask(r.r11, 2, 0xfffffffcu);
        return true;

    case 0x823214C0u: // Pack two object bytes into caller-supplied words.
        r.r11 = r.r3;
        r.r3 = r.r5;
        r.r10 = Byte(memory, r.r11, 6);
        r.r11 = Byte(memory, r.r11, 1);
        r.r10 = RotMask(r.r10, 16, 0x10000u);
        r.r11 = r.r10 | r.r11;
        r.r4 = Insert(r.r4, r.r11, 4, 0xfffffff0u);
        r.r3 = Insert(r.r3, r.r4, 8, 0xffffff00u);
        return true;

    case 0x823588A0u:
        ReadTableHalfwordFlag(r, memory, 0xfffffffeu, 0x4u, false);
        return true;

    case 0x8237CB68u: // Compare the global object's mode with six.
        r.r11 = High(-31950);
        r.r11 = Word(memory, r.r11, -30908);
        r.r11 = Word(memory, r.r11, 476);
        r.r11 -= 6u;
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x823A3C40u:
        r.r11 = High(-31955);
        r.r11 = Byte(memory, r.r11, -16464);
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x823B8F28u: // Test a mask against the referenced object's flags.
        r.r11 = Word(memory, r.r3);
        r.r10 = Word(memory, r.r3, 4);
        r.r11 = Word(memory, r.r11);
        r.r11 &= r.r10;
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x82498EF0u: // Preserve the four original reads into r18.
        r.r18 = Word(memory, r.r9, -28928);
        r.r18 = Word(memory, r.r9, -28916);
        r.r18 = Word(memory, r.r9, -28904);
        r.r18 = Word(memory, r.r9, -28892);
        r.r11 = Word(memory, r.r3);
        r.r3 = (r.r11 >> 28) & 1u;
        return true;

    case 0x82520BF8u:
        r.r11 = High(-31951);
        r.r11 = Word(memory, r.r11, 24500);
        r.r11 = Word(memory, r.r11, 736);
        r.r3 = (r.r11 >> 28) & 1u;
        return true;

    case 0x8255C520u: // Locate a member in a twelve-byte record.
        r.r11 = Word(memory, r.r3, 316);
        r.r10 = Word(memory, r.r3, 252);
        r.r9 = RotMask(r.r11, 1, 0xfffffffeu);
        r.r11 += r.r9;
        r.r11 += r.r10;
        r.r11 += 80u;
        r.r3 = RotMask(r.r11, 2, 0xfffffffcu);
        return true;

    case 0x825B80F8u:
        r.r11 = Word(memory, r.r3, 148);
        r.r11 = Word(memory, r.r11, 80);
        r.r11 = LeadingZeroes(r.r11);
        r.r3 = r.r11 == 32u ? 1u : 0u;
        return true;

    case 0x825D28B8u: // Decode the four compact count bits.
        r.r11 = Word(memory, r.r3, 548);
        r.r10 = RotMask(r.r11, 10, 0x2u);
        r.r8 = RotMask(r.r11, 12, 0x1u);
        r.r9 = RotMask(r.r11, 13, 0x1u);
        r.r8 = r.r10 + r.r8;
        r.r10 = RotMask(r.r11, 11, 0x1u);
        r.r11 = RotMask(r.r8, 1, 0xfffffffeu);
        r.r11 += r.r9;
        r.r11 = RotMask(r.r11, 1, 0xfffffffeu);
        r.r3 = r.r11 + r.r10;
        return true;

    case 0x82631480u:
        r.r11 = r.r4 + r.r3;
        r.r11 = Byte(memory, r.r11, 424);
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x827B3A90u: // Clear two reserved bits in an object byte.
        r.r11 = Byte(memory, r.r3, 10556);
        r.r10 = r.r11;
        r.r10 = Insert(r.r10, r.r11, 0, 0xfcu);
        r.r3 = r.r10 & 0xffu;
        return true;

    case 0x827B3D20u:
        ReadIndexedByteBit(r, memory, 1);
        return true;
    case 0x827B3DF0u:
        ReadIndexedByteBit(r, memory, 0);
        return true;
    case 0x827B3EE0u:
        ReadIndexedByteBit(r, memory, 2);
        return true;

    case 0x827BFB30u: // Read a word from a scaled large object table.
        r.r11 = r.r3 + 535953408u;
        r.r11 = RotMask(r.r11, 2, 0xfffffffcu);
        r.r3 = Word(memory, r.r11);
        return true;

    case 0x82805430u: // Compare a referenced byte with one.
        r.r11 = Word(memory, r.r3, 152);
        r.r11 = Byte(memory, r.r11, 80);
        r.r11 -= 1u;
        r.r11 = LeadingZeroes(r.r11);
        r.r3 = r.r11 == 32u ? 1u : 0u;
        return true;

    case 0x82820C00u:
        r.r11 = High(-31962);
        r.r11 = Word(memory, r.r11, 17748);
        r.r11 = Byte(memory, r.r11, 54);
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x82851090u:
        r.r11 = High(-31962);
        r.r11 = Word(memory, r.r11, 19792);
        r.r11 = LeadingZeroes(r.r11);
        r.r3 = r.r11 == 32u ? 1u : 0u;
        return true;

    case 0x828571B8u:
        r.r11 = Byte(memory, r.r3, 3);
        r.r3 = std::rotl(Low(r.r11), 11);
        return true;

    case 0x82906310u:
        r.r11 = High(3);
        r.r11 |= 7632u;
        r.r11 = Word(memory, Low(r.r3) + Low(r.r11));
        r.r3 = (r.r11 >> 30) & 1u;
        return true;

    case 0x82AE4FB8u:
        r.r11 = High(-31955);
        r.r11 -= 24352u;
        r.r11 = Byte(memory, r.r11, 5793);
        r.r11 = LeadingZeroes(r.r11);
        r.r3 = r.r11 == 32u ? 1u : 0u;
        return true;

    case 0x82B5D188u: // Read a twenty-byte table element.
        r.r11 = RotMask(r.r4, 2, 0xfffffffcu);
        r.r11 += r.r4;
        r.r11 = RotMask(r.r11, 2, 0xfffffffcu);
        r.r11 += r.r3;
        r.r3 = Word(memory, r.r11, 3232);
        return true;

    case 0x82B7A680u:
        ReadTableHalfwordFlag(r, memory, 0x1feu, 0xffff8000u, true);
        return true;
    case 0x82B7CFC0u:
        ReadTableHalfwordFlag(r, memory, 0xfffffffeu, 0x80u, false);
        return true;
    case 0x82B7CFE0u:
        ReadTableHalfwordFlag(r, memory, 0xfffffffeu, 0x8u, false);
        return true;

    case 0x82C4FF50u:
        r.r11 = Word(memory, r.r3, 232);
        r.r3 = r.r11 & r.r4;
        return true;

    case 0x82CC6F80u: // Select a pointer from the thread-local eight-byte table.
        r.r11 = Byte(memory, r.r13, 268);
        r.r10 = std::rotl(Low(r.r11), 3);
        r.r11 = High(-31954);
        r.r11 = Word(memory, r.r11, 336);
        r.r11 += r.r10;
        r.r3 = Word(memory, r.r11, 12);
        return true;

    case 0x82E26E50u:
        r.r11 = Word(memory, r.r3, 44);
        r.r11 &= r.r4;
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x82E753D0u:
        r.r11 = High(-31953);
        r.r11 = Word(memory, r.r11, -10572);
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x82E7D258u:
        r.r11 = Word(memory, r.r3, 312);
        r.r11 &= r.r4;
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x82E7D2C0u:
        r.r11 = Word(memory, r.r3, 312);
        r.r11 &= 7u;
        r.r11 = IsZero(r.r11);
        r.r3 = r.r11 ^ 1u;
        return true;

    case 0x82F6D6A0u:
    case 0x82F6D6C8u: // Read one of two adjacent words in a packed table.
        r.r10 = Word(memory, r.r3, 4);
        r.r11 = Word(memory, r.r3, 12);
        r.r9 = Word(memory, r.r10, 32);
        r.r10 = RotMask(r.r11, 2, 0xffffffe0u);
        r.r11 &= 7u;
        r.r11 += r.r10;
        r.r11 += address == 0x82F6D6A0u ? 21u : 25u;
        r.r11 = RotMask(r.r11, 2, 0xfffffffcu);
        r.r3 = Word(memory, Low(r.r11) + Low(r.r9));
        return true;

    case 0x82F8A7C8u: // Resolve a record pointer and scale the caller's index.
        r.r11 = Word(memory, r.r3, 4);
        r.r10 = RotMask(r.r4, 3, 0xfffffff8u);
        r.r10 += r.r4;
        r.r10 = RotMask(r.r10, 2, 0xfffffffcu);
        r.r11 = Word(memory, r.r11, 40);
        r.r11 = Word(memory, r.r11);
        r.r11 = Word(memory, r.r11, 36);
        r.r3 = r.r11 + r.r10;
        return true;

    case 0x83081618u: // Read an indexed byte from a two-level array.
        r.r11 = r.r4 + 32u;
        r.r11 = RotMask(r.r11, 2, 0xfffffffcu);
        r.r11 += r.r5;
        r.r3 = Byte(memory, Low(r.r11) + Low(r.r3));
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::read_only_fields

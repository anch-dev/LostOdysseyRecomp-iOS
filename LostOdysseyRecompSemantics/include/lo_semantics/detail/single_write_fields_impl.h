#pragma once

#include <bit>
#include <cstdint>

namespace lo::semantic::single_write_fields
{
namespace detail
{

inline std::uint32_t Low(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value);
}

inline std::uint32_t At(std::uint64_t base, std::int32_t offset = 0)
{
    return Low(base) + static_cast<std::uint32_t>(offset);
}

inline std::uint64_t High(std::int32_t upper)
{
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(upper) * 65536);
}

inline std::uint64_t RotMask(std::uint64_t value, int rotate, std::uint64_t mask)
{
    // The generated PPC rotate duplicates the low word into both halves.
    return std::rotl(std::uint64_t{Low(value)} | (value << 32), rotate) & mask;
}

inline std::uint64_t Insert(std::uint64_t old_word, std::uint64_t source,
                     int rotate, std::uint32_t mask)
{
    return RotMask(source, rotate, mask) | (old_word & ~std::uint64_t{mask});
}

inline std::uint64_t LeadingZeroes(std::uint64_t value)
{
    return std::countl_zero(Low(value));
}

template <typename Memory>
inline std::uint64_t Word(Memory& memory, std::uint64_t base,
                   std::int32_t offset = 0)
{
    return memory.ReadU32(At(base, offset));
}

template <typename Memory>
inline void PutWord(Memory& memory, std::uint64_t base,
             std::int32_t offset, std::uint64_t value)
{
    memory.WriteU32(At(base, offset), Low(value));
}

template <typename Memory>
inline void PutByte(Memory& memory, std::uint64_t base,
             std::int32_t offset, std::uint64_t value)
{
    memory.WriteU8(At(base, offset), static_cast<std::uint8_t>(value));
}

} // namespace detail

template <typename Memory>
bool ApplyWith(std::uint32_t address, Registers& r, Memory& memory)
{
    using namespace detail;
    switch (address)
    {
    case 0x822CA1A0u: // Set a persistent global flag.
        r.r11 = High(-31951);
        r.r10 = Word(memory, r.r11, -18904);
        r.r10 |= 2u;
        PutWord(memory, r.r11, -18904, r.r10);
        return true;

    case 0x822D2C80u: // Advance an object's count and report success.
        r.r11 = r.r3 + 28u;
        r.r3 = 1;
        r.r10 = Word(memory, r.r11, 4);
        ++r.r10;
        PutWord(memory, r.r11, 4, r.r10);
        return true;

    case 0x82306AF0u: // Consume bit 28 of the object's flags.
        r.r11 = r.r3;
        r.r10 = Word(memory, r.r11, 128);
        r.r3 = RotMask(r.r10, 4, 1u);
        r.r10 = RotMask(r.r10, 0, 0xffffffffefffffffull);
        PutWord(memory, r.r11, 128, r.r10);
        return true;

    case 0x823F35F0u: // Preserve the failure path's fixed-address word store.
        r.r11 = 13;
        PutWord(memory, 3, 0, r.r11);
        return true;

    case 0x8245C7A8u: // Replace the five-bit tag in the object's word.
        r.r11 = Word(memory, r.r3, 480);
        r.r10 = 19;
        r.r11 = Insert(r.r11, r.r10, 27, 0xf8000000u);
        PutWord(memory, r.r3, 480, r.r11);
        return true;

    case 0x8251B300u: // Update the selected global object's flag.
        r.r11 = High(-31951);
        r.r11 = Word(memory, r.r11, 24500);
        r.r10 = Word(memory, r.r11, 736);
        r.r10 = Insert(r.r10, r.r4, 19, 0x00080000u);
        PutWord(memory, r.r11, 736, r.r10);
        return true;

    case 0x82583430u: // Copy a global pointer into the caller's slot.
        r.r11 = High(-31964);
        r.r11 = Word(memory, r.r11, -19532);
        PutWord(memory, r.r3, 0, r.r11);
        return true;

    case 0x8267A190u: // Copy a linked object's field into this object.
        r.r11 = Word(memory, r.r3, 20320);
        r.r11 = Word(memory, r.r11, 104);
        PutWord(memory, r.r3, 104, r.r11);
        return true;

    case 0x82746848u: // Reflect byte state five into bit 30.
        r.r11 = memory.ReadU8(At(r.r3, 148));
        r.r10 = Word(memory, r.r3, 60);
        r.r11 -= 5;
        r.r11 = LeadingZeroes(r.r11);
        r.r11 = RotMask(r.r11, 27, 1u);
        r.r10 = Insert(r.r10, r.r11, 30, 0x40000000u);
        PutWord(memory, r.r3, 60, r.r10);
        return true;

    case 0x82747B80u: // Follow the global object chain and set a status flag.
        r.r11 = High(-31950);
        r.r3 = 1;
        r.r11 = Word(memory, r.r11, -30908);
        r.r11 = Word(memory, r.r11, 80);
        r.r11 = Word(memory, r.r11, 60);
        r.r11 = Word(memory, r.r11);
        r.r10 = Word(memory, r.r11, 712);
        r.r10 |= 0x00800000u;
        PutWord(memory, r.r11, 712, r.r10);
        return true;

    case 0x8282F1A8u: // Copy one linked field to its adjacent slot.
        r.r11 = Word(memory, r.r3, 1120);
        r.r10 = Word(memory, r.r11, 208);
        PutWord(memory, r.r11, 212, r.r10);
        return true;

    case 0x82872868u: // Write an indexed word with the original alignment.
        r.r11 = r.r4 + 3279u;
        r.r11 = RotMask(r.r11, 2, 0xfffffffcu);
        PutWord(memory, r.r11 + r.r3, 0, r.r5);
        return true;

    case 0x828B6320u: // Set the leading bit of an indexed record word.
        r.r10 = RotMask(r.r4, 1, 0xfffffffeu);
        r.r11 = Word(memory, r.r3, 132);
        r.r10 += r.r4;
        r.r10 = RotMask(r.r10, 4, 0xfffffff0u);
        r.r9 = Word(memory, r.r11 + r.r10);
        r.r9 = Insert(r.r9, r.r5, 31, 0x80000000u);
        PutWord(memory, r.r11 + r.r10, 0, r.r9);
        return true;

    case 0x829062E0u: // Set bit 31 in the large object field.
    case 0x829062F8u: // Set bit 30 in the same field.
        r.r11 = r.r3 + 196608u;
        r.r11 += 7632u;
        r.r10 = Word(memory, r.r11);
        r.r10 = address == 0x829062E0u ?
            Insert(r.r10, r.r4, 31, 0x80000000u) :
            Insert(r.r10, r.r4, 30, 0x40000000u);
        PutWord(memory, r.r11, 0, r.r10);
        return true;

    case 0x82942040u: // Add the input delta to the saved field.
        r.r11 = Word(memory, r.r4, 248);
        r.r11 += r.r5;
        PutWord(memory, r.r6, 0, r.r11);
        return true;

    case 0x8295FEA0u: // Set bit 30 from the input byte.
        r.r11 = Word(memory, r.r3, 744);
        r.r10 = Low(r.r4) & 0xffu;
        r.r11 = Insert(r.r11, r.r10, 30, 0x40000000u);
        PutWord(memory, r.r3, 744, r.r11);
        return true;

    case 0x82965048u: // Update bit 31 and report success.
        r.r11 = r.r3;
        r.r3 = 1;
        r.r10 = Word(memory, r.r11, 60);
        r.r10 = Insert(r.r10, r.r4, 31, 0x80000000u);
        PutWord(memory, r.r11, 60, r.r10);
        return true;

    case 0x82965060u: // Update bit 30 from an input byte.
        r.r11 = r.r3;
        r.r9 = Low(r.r4) & 0xffu;
        r.r3 = 1;
        r.r10 = Word(memory, r.r11, 60);
        r.r10 = Insert(r.r10, r.r9, 30, 0x40000000u);
        PutWord(memory, r.r11, 60, r.r10);
        return true;

    case 0x829E6DD0u: // Collapse a global word to its zero predicate.
        r.r11 = High(-31956);
        r.r10 = Word(memory, r.r11, 9268);
        r.r10 = LeadingZeroes(r.r10);
        r.r10 = RotMask(r.r10, 27, 1u);
        PutWord(memory, r.r11, 9268, r.r10);
        return true;

    case 0x829FD978u: // Save the first global-linked field.
    case 0x829FD988u: // Save the adjacent global-linked field.
        r.r11 = High(-31969);
        r.r11 = Word(memory, r.r11, 7008);
        PutWord(memory, r.r11,
                address == 0x829FD978u ? 23856 : 23860, r.r3);
        return true;

    case 0x82AE10E8u: // Publish whether the input equals one.
        r.r11 = r.r4 - 1u;
        r.r10 = LeadingZeroes(r.r11);
        r.r11 = High(-31955);
        r.r10 = RotMask(r.r10, 27, 1u);
        r.r11 -= 24352u;
        PutByte(memory, r.r11, 5584, r.r10);
        return true;

    case 0x82B9E1F8u: // Write a table word, then read the next word and base.
        r.r10 = Word(memory, r.r3, 196);
        r.r11 = RotMask(r.r4, 3, 0xfffffff8u);
        r.r10 = Word(memory, r.r11 + r.r10);
        PutWord(memory, r.r5, 0, r.r10);
        // These are deliberately reloaded after the store: the output may alias.
        r.r9 = Word(memory, r.r3, 196);
        r.r10 = Word(memory, r.r3, 200);
        r.r11 += r.r9;
        r.r11 = Word(memory, r.r11, 4);
        r.r3 = r.r11 + r.r10;
        return true;

    case 0x82CB0C58u: // Install a fixed guest pointer into a linked object.
        r.r11 = High(-32053);
        r.r10 = Word(memory, r.r3, 440);
        r.r11 += 2728u;
        PutWord(memory, r.r10, 0, r.r11);
        return true;

    case 0x82CB5DE8u: // Choose one of two indexed output slots.
        r.r11 = Word(memory, r.r3, 48);
        r.r11 -= 3u;
        r.r11 = LeadingZeroes(r.r11);
        r.r11 = RotMask(r.r11, 27, 1u);
        r.r11 += 21u;
        r.r11 = RotMask(r.r11, 2, 0xfffffffcu);
        PutWord(memory, r.r11 + r.r3, 0, r.r4);
        return true;

    case 0x82CCC270u: // Write the object's byte and return zero.
        r.r11 = r.r3;
        r.r3 = 0;
        PutByte(memory, r.r11, 26, r.r4);
        return true;

    case 0x82CCE118u: // Round an input size and accumulate the field.
        r.r11 = r.r4 + 3u;
        r.r10 = Word(memory, r.r3, 16);
        r.r11 = RotMask(r.r11, 0, 0xfffffffcu);
        r.r11 += r.r10;
        PutWord(memory, r.r3, 16, r.r11);
        return true;

    case 0x82CD5A28u: // Write the address of the preceding object word.
        r.r11 = r.r3 - 4u;
        PutWord(memory, r.r4, 0, r.r11);
        return true;

    case 0x82CEED38u: // Accumulate an unsigned word and return zero.
        r.r11 = r.r3;
        r.r10 = Low(r.r4);
        r.r3 = 0;
        r.r9 = Word(memory, r.r11, 52);
        r.r10 += r.r9;
        PutWord(memory, r.r11, 52, r.r10);
        return true;

    case 0x82CFB338u: // Clear the object's byte and return zero.
        r.r11 = r.r3;
        r.r10 = 0;
        r.r3 = 0;
        PutByte(memory, r.r11, 512, r.r10);
        return true;

    case 0x82D18958u: // Store one halfword in an indexed array.
        r.r10 = RotMask(r.r6, 1, 0xfffffffeu);
        memory.WriteU16(At(r.r10 + r.r4), static_cast<std::uint16_t>(r.r3));
        return true;

    case 0x82D93C90u: // Store one byte at the indexed member.
        r.r10 = r.r3 + r.r4;
        PutByte(memory, r.r10, 13, r.r5);
        return true;

    case 0x82DD7418u: // Mark a linked record's high status bit.
        r.r11 = Word(memory, r.r3, 148);
        r.r10 = Word(memory, r.r11, 24);
        r.r10 |= 0x80000000u;
        PutWord(memory, r.r11, 24, r.r10);
        return true;

    case 0x82DE9470u: // Publish whether the input is nonzero.
        r.r11 = LeadingZeroes(r.r3);
        r.r10 = High(-31953);
        r.r11 = RotMask(r.r11, 27, 1u);
        r.r11 ^= 1u;
        PutWord(memory, r.r10, -13592, r.r11);
        return true;

    case 0x82E02128u: // Decrement an indexed table word.
        r.r10 = Word(memory, r.r3);
        r.r11 = RotMask(r.r4, 4, 0xfffffff0u);
        r.r9 = Word(memory, r.r11 + r.r10);
        --r.r9;
        PutWord(memory, r.r11 + r.r10, 0, r.r9);
        return true;

    case 0x82E44640u: // Describe the first fixed-size object member.
    case 0x82E44658u: // Describe the second fixed-size object member.
        r.r11 = address == 0x82E44640u ? 128u : 256u;
        r.r3 += address == 0x82E44640u ? 448u : 1672u;
        PutWord(memory, r.r4, 0, r.r11);
        return true;

    case 0x82E753C0u: // Copy an object word into global state.
        r.r11 = Word(memory, r.r3, 20);
        r.r10 = High(-31953);
        PutWord(memory, r.r10, -10572, r.r11);
        return true;

    case 0x830642C0u: // Form a sixteen-byte indexed address.
    case 0x830642D8u: // Form a 128-byte indexed address.
        r.r11 = address == 0x830642C0u ?
            RotMask(r.r4, 4, 0xfffffff0u) :
            RotMask(r.r4, 7, 0xffffff80u);
        r.r3 = 1;
        r.r11 += r.r5;
        PutWord(memory, r.r6, 0, r.r11);
        return true;

    case 0x8307E0B0u: // Replace an object's word using a global lookup table.
        r.r10 = Word(memory, r.r3, 4);
        r.r11 = High(-32232);
        r.r11 += 4136u;
        r.r10 = RotMask(r.r10, 4, 0xfffffff0u);
        r.r11 = Word(memory, r.r10 + r.r11);
        PutWord(memory, r.r3, 4, r.r11);
        return true;

    case 0x83081570u: // Write an indexed byte with a 32-element bias.
        r.r11 = r.r4 + 32u;
        r.r11 = RotMask(r.r11, 2, 0xfffffffcu);
        r.r11 += r.r5;
        PutByte(memory, r.r11 + r.r3, 0, r.r6);
        return true;

    case 0x83098528u: // Write an indexed array word.
        r.r11 = RotMask(r.r4, 2, 0xfffffffcu);
        PutWord(memory, r.r11 + r.r5, 0, r.r3);
        return true;

    case 0x830BF810u: // Copy one global word to the adjacent state slot.
        r.r11 = High(-31969);
        r.r10 = High(-31969);
        r.r10 -= 28056u;
        r.r11 = Word(memory, r.r11, -28140);
        PutWord(memory, r.r10, 8, r.r11);
        return true;

    case 0x830D1DD0u: // Copy one global pointer to another state group.
        r.r11 = High(-31967);
        r.r10 = High(-31955);
        r.r11 = Word(memory, r.r11, 16208);
        PutWord(memory, r.r10, 11584, r.r11);
        return true;

    default:
        return false;
    }
}

} // namespace lo::semantic::single_write_fields

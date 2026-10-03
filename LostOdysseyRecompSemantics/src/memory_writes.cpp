#include "lo_semantics/memory_writes.h"

#include <bit>
#include <initializer_list>

namespace lo::semantic::memory_writes
{
namespace
{

using gpu::GuestMemory;

std::uint32_t At(std::uint64_t base, std::uint32_t offset = 0)
{
    return static_cast<std::uint32_t>(base) + offset;
}

std::uint64_t Word(GuestMemory& memory, std::uint64_t base, std::uint32_t offset = 0)
{
    return memory.ReadU32(At(base, offset));
}

void PutWord(GuestMemory& memory, std::uint64_t base, std::uint32_t offset,
             std::uint64_t value)
{
    memory.WriteU32(At(base, offset), static_cast<std::uint32_t>(value));
}

std::uint64_t Byte(GuestMemory& memory, std::uint64_t base, std::uint32_t offset = 0)
{
    return memory.ReadU8(At(base, offset));
}

std::uint64_t Half(GuestMemory& memory, std::uint64_t base, std::uint32_t offset = 0)
{
    return memory.ReadU16(At(base, offset));
}

void PutByte(GuestMemory& memory, std::uint64_t base, std::uint32_t offset,
             std::uint64_t value)
{
    memory.WriteU8(At(base, offset), static_cast<std::uint8_t>(value));
}

void PutHalf(GuestMemory& memory, std::uint64_t base, std::uint32_t offset,
             std::uint64_t value)
{
    memory.WriteU16(At(base, offset), static_cast<std::uint16_t>(value));
}

// XenonRecomp implements PPC word rotates by duplicating the low word into
// both halves, rotating as a 64-bit value, then applying the generated mask.
std::uint64_t RotatedWord(std::uint64_t value, int shift)
{
    const std::uint64_t word = static_cast<std::uint32_t>(value);
    return std::rotl(word | (word << 32), shift);
}

std::uint64_t SignedWord(std::int32_t value)
{
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
}

void CopyThreeWordsAndAdvanceCursor(Registers& r, GuestMemory& memory)
{
    r.r11 = Word(memory, r.r4, 12);
    r.r10 = Word(memory, r.r11);
    PutWord(memory, r.r5, 0, r.r10);
    r.r10 = Word(memory, r.r11, 4);
    PutWord(memory, r.r5, 4, r.r10);
    r.r11 = Word(memory, r.r11, 8);
    PutWord(memory, r.r5, 8, r.r11);
    // Reload after the stores: the output can alias the cursor field.
    r.r11 = Word(memory, r.r4, 12);
    r.r11 += 12;
    PutWord(memory, r.r4, 12, r.r11);
}

void CopyThreeWordsFromPointerField(Registers& r, GuestMemory& memory)
{
    r.r11 = Word(memory, r.r4, 516);
    r.r11 += 272;
    r.r10 = Word(memory, r.r11);
    r.r9 = Word(memory, r.r11, 4);
    r.r11 = Word(memory, r.r11, 8);
    PutWord(memory, r.r3, 0, r.r10);
    PutWord(memory, r.r3, 4, r.r9);
    PutWord(memory, r.r3, 8, r.r11);
}

void CopyThreeWordsAndReadFlag(Registers& r, GuestMemory& memory,
                               std::uint32_t source_offset,
                               std::uint32_t flag_offset, unsigned bit)
{
    r.r11 = r.r3 + source_offset;
    r.r10 = Word(memory, r.r11);
    PutWord(memory, r.r4, 0, r.r10);
    r.r10 = Word(memory, r.r11, 4);
    PutWord(memory, r.r4, 4, r.r10);
    r.r11 = Word(memory, r.r11, 8);
    PutWord(memory, r.r4, 8, r.r11);
    r.r11 = Word(memory, r.r3, flag_offset);
    r.r3 = (r.r11 >> bit) & 1u;
}

void CopyFourWordsWithinObject(Registers& r, GuestMemory& memory)
{
    r.r11 = r.r3 + 172;
    r.r10 = r.r3 + 188;
    r.r9 = Word(memory, r.r11);
    r.r8 = Word(memory, r.r11, 4);
    r.r7 = Word(memory, r.r11, 8);
    r.r11 = Word(memory, r.r11, 12);
    PutWord(memory, r.r10, 0, r.r9);
    PutWord(memory, r.r10, 4, r.r8);
    PutWord(memory, r.r10, 8, r.r7);
    PutWord(memory, r.r10, 12, r.r11);
}

void ReadIndexedPair(Registers& r, GuestMemory& memory)
{
    r.r10 = static_cast<std::uint32_t>(r.r5) << 1;
    r.r11 = Word(memory, r.r4, 120);
    r.r10 += r.r5;
    r.r10 = static_cast<std::uint32_t>(r.r10) << 2;
    r.r11 += r.r10;
    r.r10 = Word(memory, r.r11);
    r.r11 = Word(memory, r.r11, 4);
    PutWord(memory, r.r3, 0, r.r10);
    PutWord(memory, r.r3, 4, r.r11);
}

void WriteIndexedPair(Registers& r, GuestMemory& memory)
{
    r.r10 = static_cast<std::uint32_t>(r.r4) << 1;
    r.r11 = Word(memory, r.r3, 120);
    r.r9 = Word(memory, r.r5);
    r.r10 += r.r4;
    r.r10 = static_cast<std::uint32_t>(r.r10) << 2;
    r.r11 += r.r10;
    PutWord(memory, r.r11, 0, r.r9);
    r.r10 = Word(memory, r.r5, 4);
    PutWord(memory, r.r11, 4, r.r10);
}

void WriteRangeEndpoints(Registers& r, GuestMemory& memory,
                         std::uint32_t x_extent, std::uint32_t y_extent)
{
    r.r11 = Word(memory, r.r4, 76);
    r.r10 = Word(memory, r.r4, 72);
    if (x_extent)
        r.r9 = Word(memory, r.r4, x_extent);
    else
        r.r9 = r.r11 + 64;
    if (y_extent)
        r.r8 = Word(memory, r.r4, y_extent);
    else
        r.r8 = r.r10 + 64;
    if (x_extent)
        r.r9 += r.r11;
    if (y_extent)
        r.r8 += r.r10;
    PutWord(memory, r.r3, 4, r.r11);
    PutWord(memory, r.r3, 0, r.r10);
    PutWord(memory, r.r3, 12, r.r9);
    PutWord(memory, r.r3, 8, r.r8);
}

void DrainThreeStatusWords(Registers& r, GuestMemory& memory)
{
    r.r11 = r.r3;
    r.r10 = 0;
    r.r9 = Word(memory, r.r11, 24420);
    r.r3 = Word(memory, r.r11, 24416);
    PutWord(memory, r.r4, 0, r.r9);
    r.r9 = Word(memory, r.r11, 24424);
    PutWord(memory, r.r5, 0, r.r9);
    PutWord(memory, r.r11, 24416, r.r10);
    PutWord(memory, r.r11, 24420, r.r10);
    PutWord(memory, r.r11, 24424, r.r10);
}

void ClearObjectAndLinkedState(Registers& r, GuestMemory& memory)
{
    r.r11 = 0;
    r.r10 = Word(memory, r.r3, 444);
    PutWord(memory, r.r3, 220, 0);
    PutWord(memory, r.r3, 148, 0);
    PutWord(memory, r.r3, 420, 0);
    PutWord(memory, r.r10, 12, 0);
    PutWord(memory, r.r10, 16, 0);
    PutWord(memory, r.r10, 24, 0);
    PutWord(memory, r.r10, 164, 0);
}

void InitializeEmbeddedLists(Registers& r, GuestMemory& memory)
{
    r.r11 = 0;
    r.r10 = r.r3 + 8;
    r.r9 = r.r3 + 28;
    for (std::uint32_t offset : {0u, 4u, 8u})
        PutWord(memory, r.r10, offset, r.r11);
    for (std::uint32_t offset : {0u, 4u, 8u})
        PutWord(memory, r.r9, offset, r.r11);
    PutWord(memory, r.r3, 0, r.r10);
    PutWord(memory, r.r3, 4, r.r9);
}

void InitializeSelfLinkedNodes(Registers& r, GuestMemory& memory)
{
    r.r10 = 0;
    PutWord(memory, r.r3, 0, r.r3);
    r.r11 = r.r3 + 12;
    PutWord(memory, r.r3, 4, r.r3);
    PutWord(memory, r.r3, 8, r.r10);
    PutWord(memory, r.r11, 8, r.r10);
    PutWord(memory, r.r11, 0, r.r11);
    PutWord(memory, r.r11, 4, r.r11);
}

void RelinkNeighbors(Registers& r, GuestMemory& memory)
{
    r.r11 = Word(memory, r.r3, 4);
    r.r10 = Word(memory, r.r3, 8);
    PutWord(memory, r.r11, 8, r.r10);
    // These loads must occur after the first link update if nodes alias.
    r.r11 = Word(memory, r.r3, 8);
    r.r10 = Word(memory, r.r3, 4);
    PutWord(memory, r.r11, 4, r.r10);
}

void ConsumeByteAndAdvance(Registers& r, GuestMemory& memory, bool write_word)
{
    r.r11 = Word(memory, r.r4, 12);
    r.r11 = Byte(memory, r.r11);
    if (write_word)
        PutWord(memory, r.r5, 0, r.r11);
    else
        PutByte(memory, r.r5, 0, r.r11);
    // The output may alias r4+12, so reload it after the byte was written.
    r.r11 = Word(memory, r.r4, 12);
    r.r11 += 1;
    PutWord(memory, r.r4, 12, r.r11);
}

void ClearObjectCountersAndAdvance(Registers& r, GuestMemory& memory,
                                   std::uint32_t first, std::uint32_t second)
{
    r.r11 = r.r3;
    r.r10 = 0;
    r.r3 = 0;
    r.r9 = Word(memory, r.r11, 3372);
    PutWord(memory, r.r11, first, 0);
    PutWord(memory, r.r11, second, 0);
    PutWord(memory, r.r11, 3368, r.r9);
    r.r11 = Word(memory, r.r4, 1692);
    r.r11 += 2;
    PutWord(memory, r.r4, 1692, r.r11);
}

void SetGlobalBooleanOptions(Registers& r, GuestMemory& memory,
                             std::uint32_t flag_offset)
{
    r.r11 = SignedWord(-2094202880);
    r.r10 = 1;
    r.r11 += SignedWord(-24352);
    r.r9 = static_cast<std::uint32_t>(r.r4) == 0 ? 32u :
        std::countl_zero(static_cast<std::uint32_t>(r.r4));
    r.r9 = RotatedWord(r.r9, 27) & 1u;
    PutByte(memory, r.r11, 5130, r.r10);
    r.r10 = 0;
    PutWord(memory, r.r11, 5132, 0);
    r.r10 = r.r9 ^ 1u;
    PutByte(memory, r.r11, flag_offset, r.r10);
}

} // namespace

bool Apply(std::uint32_t address, Registers& r, GuestMemory& memory)
{
    switch (address)
    {
    case 0x8234B4B8u: CopyThreeWordsAndAdvanceCursor(r, memory); break;
    case 0x825207B8u: CopyThreeWordsFromPointerField(r, memory); break;
    case 0x8257A208u: ReadIndexedPair(r, memory); break;
    case 0x8257A230u: WriteIndexedPair(r, memory); break;
    case 0x82871898u: CopyFourWordsWithinObject(r, memory); break;
    case 0x82EBAF20u: CopyThreeWordsAndReadFlag(r, memory, 320, 356, 1); break;
    case 0x82EBAF80u: CopyThreeWordsAndReadFlag(r, memory, 332, 356, 2); break;
    case 0x82EBFD30u: CopyThreeWordsAndReadFlag(r, memory, 336, 332, 1); break;
    case 0x825BD998u: WriteRangeEndpoints(r, memory, 128, 124); break;
    case 0x825BFF20u: WriteRangeEndpoints(r, memory, 128, 224); break;
    case 0x825C0F80u: WriteRangeEndpoints(r, memory, 0, 0); break;
    case 0x82884018u: DrainThreeStatusWords(r, memory); break;
    case 0x82C2FB88u: ClearObjectAndLinkedState(r, memory); break;
    case 0x82ACD3C0u:
        r.r10 = Word(memory, r.r4, 100);
        r.r11 = 0;
        r.r10 &= 0x3fffffffu;
        PutWord(memory, r.r4, 88, 0);
        PutWord(memory, r.r4, 92, 0);
        PutWord(memory, r.r4, 96, 0);
        PutWord(memory, r.r4, 100, r.r10);
        break;
    case 0x82F2EBC8u:
        r.r11 = 0;
        PutWord(memory, r.r3, 44, 0);
        PutWord(memory, r.r3, 48, 0);
        r.r11 = Word(memory, r.r3, 4);
        r.r11 = (r.r11 | (r.r11 << 32)) & ~std::uint64_t{12};
        PutWord(memory, r.r3, 4, r.r11);
        break;
    case 0x82E52FA8u: InitializeEmbeddedLists(r, memory); break;
    case 0x82CD6C70u: InitializeSelfLinkedNodes(r, memory); break;
    case 0x83084F88u: RelinkNeighbors(r, memory); break;
    case 0x828F4860u:
        r.r11 = r.r3;
        r.r10 = 0;
        r.r3 = 1;
        PutWord(memory, r.r11, 4, 0);
        PutWord(memory, r.r11, 8, 0);
        break;
    case 0x82BE4878u:
        r.r11 = r.r3;
        r.r3 = 1;
        PutWord(memory, r.r11, 0, r.r4);
        PutWord(memory, r.r11, 4, r.r5);
        PutWord(memory, r.r11, 8, r.r6);
        PutWord(memory, r.r11, 12, r.r7);
        break;
    case 0x82CF0748u:
        r.r11 = r.r3;
        r.r3 = 0;
        PutWord(memory, r.r11, 500, r.r4);
        PutWord(memory, r.r11, 504, r.r5);
        break;
    case 0x82CFAC30u:
        r.r11 = r.r3;
        r.r3 = 0;
        memory.WriteU8(At(r.r11, 1524), static_cast<std::uint8_t>(r.r4));
        PutWord(memory, r.r11, 1528, r.r5);
        break;
    case 0x82CFAD08u:
        r.r11 = r.r3;
        r.r10 = 0;
        r.r3 = 0;
        memory.WriteU8(At(r.r11, 1524), 0);
        PutWord(memory, r.r11, 1528, 0);
        break;
    case 0x82D7DF10u:
        r.r11 = r.r3;
        r.r10 = 1;
        r.r9 = 0;
        r.r3 = 0;
        PutWord(memory, r.r11, 15560, 1);
        PutWord(memory, r.r11, 15564, 0);
        PutWord(memory, r.r11, 15536, 1);
        PutWord(memory, r.r11, 456, 1);
        PutWord(memory, r.r11, 3452, 1);
        break;
    case 0x82D815E8u:
        r.r10 = Word(memory, r.r3, 248);
        r.r11 = 0;
        PutWord(memory, r.r3, 280, 0);
        PutWord(memory, r.r3, 472, 0);
        PutWord(memory, r.r3, 3980, 0);
        PutWord(memory, r.r3, 3988, r.r10);
        break;
    case 0x82C3A438u:
        r.r10 = 0;
        PutWord(memory, r.r3, 0, r.r4);
        r.r11 = r.r3 + 8;
        PutWord(memory, r.r3, 4, 0);
        PutWord(memory, r.r3, 8, 0);
        PutWord(memory, r.r3, 12, r.r11);
        break;
    case 0x830882A8u:
        r.r11 = static_cast<std::uint8_t>(r.r4);
        PutWord(memory, r.r3, 168, r.r4);
        for (std::uint32_t offset : {132u, 133u, 134u, 135u})
            memory.WriteU8(At(r.r3, offset), static_cast<std::uint8_t>(r.r11));
        break;
    case 0x82C2FEB0u:
        r.r10 = Word(memory, r.r3);
        r.r11 = 0;
        PutWord(memory, r.r10, 108, 0);
        r.r10 = Word(memory, r.r3);
        PutWord(memory, r.r10, 20, 0);
        break;
    case 0x822D06D0u:
        r.r11 = Word(memory, r.r6);
        r.r10 = r.r11 + r.r5;
        r.r11 += 160;
        PutWord(memory, r.r6, 0, r.r11);
        PutWord(memory, r.r7, 0, r.r10);
        break;
    case 0x822D1FA0u:
        r.r11 = Word(memory, r.r6);
        r.r10 = ~std::uint64_t{0};
        PutWord(memory, r.r7, 0, r.r11);
        PutWord(memory, r.r8, 0, r.r10);
        break;
    case 0x822FD7B0u: ConsumeByteAndAdvance(r, memory, false); break;
    case 0x823AF248u: ConsumeByteAndAdvance(r, memory, true); break;
    case 0x82306AC8u:
        r.r10 = Word(memory, r.r3, 4);
        r.r9 = RotatedWord(r.r4, 29) & 0xe0000000u;
        r.r11 = SignedWord(-2113536000);
        r.r10 = static_cast<std::uint32_t>(r.r10) & 0x0fffffffu;
        r.r11 += 20860;
        r.r10 |= r.r9;
        r.r10 = RotatedWord(r.r10, 0) & 0xffffffffefffffffull;
        PutWord(memory, r.r3, 0, r.r11);
        PutWord(memory, r.r3, 4, r.r10);
        break;
    case 0x82376A38u:
        r.r11 = SignedWord(-2113536000);
        r.r10 = Word(memory, r.r3, 4);
        r.r11 += 20860;
        r.r10 = static_cast<std::uint32_t>(r.r10) & 0x0fffffffu;
        PutWord(memory, r.r3, 0, r.r11);
        r.r11 = Byte(memory, r.r4, 96);
        r.r11 = RotatedWord(r.r11, 29) & 0xffffffe0u;
        r.r11 |= r.r10;
        r.r11 = RotatedWord(r.r11, 0) & 0xffffffffefffffffull;
        PutWord(memory, r.r3, 4, r.r11);
        break;
    case 0x82405FB0u:
        r.r10 = Word(memory, r.r3, 112);
        r.r11 = Word(memory, r.r3, 116);
        r.r10 = r.r4 + r.r10;
        r.r11 = r.r5 + r.r11;
        PutWord(memory, r.r3, 112, r.r10);
        PutWord(memory, r.r3, 116, r.r11);
        break;
    case 0x8240CE40u:
        r.r11 = Word(memory, r.r3, 76);
        PutWord(memory, r.r4, 64, r.r11);
        PutWord(memory, r.r3, 76, r.r4);
        break;
    case 0x824A05A0u:
        r.r11 = 0;
        r.r10 = SignedWord(-2093940736);
        PutWord(memory, r.r10, static_cast<std::uint32_t>(-18908), 0);
        r.r11 = Word(memory, r.r4, 12);
        r.r11 -= 1;
        PutWord(memory, r.r4, 12, r.r11);
        break;
    case 0x825BB9D0u:
        r.r11 = Word(memory, r.r3, 24);
        r.r10 = 385;
        PutWord(memory, r.r11, 40, r.r10);
        PutWord(memory, r.r3, 240, r.r4);
        break;
    case 0x826AA1E8u:
        r.r11 = SignedWord(-2093875200);
        r.r10 = Word(memory, r.r3, 52);
        r.r9 = 1;
        r.r11 += SignedWord(-29280);
        PutWord(memory, r.r11, 4, r.r10);
        PutWord(memory, r.r3, 80, r.r9);
        break;
    case 0x826AA330u:
        r.r11 = SignedWord(-2093875200);
        r.r10 = Word(memory, r.r3, 52);
        r.r11 += SignedWord(-29280);
        PutWord(memory, r.r11, 8, r.r10);
        r.r11 = 2;
        r.r10 = Word(memory, r.r3, 140);
        r.r10 |= 67108864u;
        PutWord(memory, r.r3, 80, r.r11);
        PutWord(memory, r.r3, 140, r.r10);
        break;
    case 0x826AA3C8u:
        r.r11 = SignedWord(-2093875200);
        r.r10 = Word(memory, r.r3, 52);
        r.r9 = 3;
        r.r11 += SignedWord(-29280);
        PutWord(memory, r.r11, 12, r.r10);
        PutWord(memory, r.r3, 80, r.r9);
        break;
    case 0x82735E60u:
        r.r11 = Word(memory, r.r3, 28);
        r.r10 = 0;
        r.r9 = r.r11 + 1;
        PutWord(memory, r.r3, 28, r.r9);
        PutByte(memory, r.r11, 0, 0);
        break;
    case 0x8285C288u:
        r.r11 = SignedWord(-2094661632);
        r.r11 += 20576;
        r.r10 = Word(memory, r.r11, 388);
        r.r9 = r.r10 + 1;
        PutWord(memory, r.r11, 388, r.r9);
        PutWord(memory, r.r3, 4, r.r10);
        break;
    case 0x82994F70u:
        r.r11 = Word(memory, r.r3, 120);
        r.r10 = 4;
        r.r11 = (RotatedWord(r.r4, 26) & 0x04000000u) |
                (r.r11 & 0xfffffffffbffffffull);
        PutWord(memory, r.r3, 356, r.r10);
        PutWord(memory, r.r3, 120, r.r11);
        break;
    case 0x829DC588u:
        r.r11 = SignedWord(-2094202880);
        r.r11 += SignedWord(-27200);
        r.r10 = Word(memory, r.r11, 564);
        PutWord(memory, r.r4, 0, r.r10);
        r.r11 = Word(memory, r.r11, 568);
        PutWord(memory, r.r3, 0, r.r11);
        break;
    case 0x82A43308u:
        r.r11 = SignedWord(-2095120384);
        r.r10 = 1;
        r.r3 = 0;
        r.r11 = Word(memory, r.r11, 7008);
        PutWord(memory, r.r11, 3764, r.r10);
        r.r11 = Word(memory, r.r4, 1692);
        r.r11 += 2;
        PutWord(memory, r.r4, 1692, r.r11);
        break;
    case 0x82A515D0u: ClearObjectCountersAndAdvance(r, memory, 3580, 3612); break;
    case 0x82A51600u: ClearObjectCountersAndAdvance(r, memory, 3644, 3676); break;
    case 0x82AE4AE0u:
        r.r10 = static_cast<std::uint32_t>(r.r5) == 0 ? 32u :
            std::countl_zero(static_cast<std::uint32_t>(r.r5));
        r.r11 = SignedWord(-2094202880);
        r.r9 = RotatedWord(r.r10, 27) & 1u;
        r.r11 += SignedWord(-16292);
        r.r10 = 0;
        PutWord(memory, r.r11, 96, r.r4);
        PutByte(memory, r.r11, 101, 0);
        r.r10 = r.r9 ^ 1u;
        PutByte(memory, r.r11, 100, r.r10);
        break;
    case 0x82AE6FC8u: SetGlobalBooleanOptions(r, memory, 5129); break;
    case 0x82AE70F8u: SetGlobalBooleanOptions(r, memory, 5128); break;
    case 0x82AF7518u:
        r.r11 = Word(memory, r.r3, 24);
        r.r10 = 1;
        r.r9 = Word(memory, r.r11, 64);
        r.r9 = (RotatedWord(r.r10, 26) & 0x0c000000u) |
                (r.r9 & 0xfffffffff3ffffffull);
        PutWord(memory, r.r11, 64, r.r9);
        r.r11 = Word(memory, r.r3, 24);
        r.r10 = Word(memory, r.r11, 52);
        r.r10 += 1;
        PutWord(memory, r.r11, 52, r.r10);
        break;
    case 0x82B69780u:
        r.r10 = Half(memory, r.r3, 4);
        r.r11 = RotatedWord(r.r4, 14) & 0xffffc000u;
        r.r9 = Half(memory, r.r3, 6);
        r.r10 ^= r.r11;
        r.r9 = (RotatedWord(r.r4, 15) & 0xffff8000u) |
               (r.r9 & 0xffffffff00007fffull);
        r.r10 = static_cast<std::uint32_t>(r.r10) & 0x7fffu;
        r.r11 = r.r10 ^ r.r11;
        PutHalf(memory, r.r3, 6, r.r9);
        PutHalf(memory, r.r3, 4, r.r11);
        break;
    case 0x82B697A8u:
        r.r11 = static_cast<std::uint32_t>(r.r4) & 0xffffu;
        r.r9 = RotatedWord(r.r5, 15) & 0xffff8000u;
        r.r10 = RotatedWord(r.r5, 14) & 0x8000u;
        PutHalf(memory, r.r3, 2, r.r11);
        PutHalf(memory, r.r3, 6, r.r9);
        PutHalf(memory, r.r3, 0, r.r11);
        PutHalf(memory, r.r3, 4, r.r10);
        break;
    case 0x82B72D90u:
        r.r11 = RotatedWord(r.r4, 8) & 0xffffff00u;
        PutWord(memory, r.r3, 60, r.r7);
        r.r11 |= r.r5;
        r.r11 = RotatedWord(r.r11, 6) & 0xffffffc0u;
        r.r11 |= r.r6;
        PutHalf(memory, r.r3, 56, r.r11);
        break;
    case 0x82BD8FA8u:
        r.r10 = Word(memory, r.r3, 4);
        r.r11 = Word(memory, r.r3, 8);
        r.r11 = r.r10 ^ r.r11;
        PutWord(memory, r.r3, 4, r.r11);
        r.r10 = Word(memory, r.r3, 8);
        r.r11 = r.r10 ^ r.r11;
        PutWord(memory, r.r3, 8, r.r11);
        r.r11 = Word(memory, r.r3, 4);
        r.r10 = Word(memory, r.r3, 8);
        r.r11 ^= r.r10;
        PutWord(memory, r.r3, 4, r.r11);
        break;
    case 0x82BF0188u:
        r.r11 = Word(memory, r.r3, 24);
        r.r3 = 1;
        r.r10 = Word(memory, r.r11, 28);
        r.r9 = Word(memory, r.r11, 32);
        PutWord(memory, r.r11, 0, r.r10);
        PutWord(memory, r.r11, 4, r.r9);
        break;
    case 0x82CBBA90u:
        r.r10 = Word(memory, r.r3, 316);
        r.r11 = Word(memory, r.r3, 456);
        PutWord(memory, r.r11, 92, r.r10);
        r.r10 = Word(memory, r.r3, 116);
        PutWord(memory, r.r11, 96, r.r10);
        break;
    case 0x82CBD3F0u:
        r.r11 = Word(memory, r.r3, 456);
        r.r10 = 0;
        PutWord(memory, r.r11, 36, 0);
        r.r10 = Word(memory, r.r3, 116);
        PutWord(memory, r.r11, 44, r.r10);
        break;
    case 0x82CC3698u:
        r.r11 = r.r3;
        r.r3 = 0;
        r.r10 = r.r11 + 20;
        r.r9 = Word(memory, r.r10);
        PutWord(memory, r.r4, 0, r.r9);
        r.r10 = Word(memory, r.r10, 4);
        PutWord(memory, r.r4, 4, r.r10);
        r.r11 = Word(memory, r.r11, 28);
        PutWord(memory, r.r4, 8, r.r11);
        break;
    case 0x82CCD378u:
        r.r11 = 4;
        r.r3 = 0;
        PutByte(memory, r.r4, 0, r.r11);
        r.r11 = SignedWord(-2112815104);
        r.r11 = Half(memory, r.r11, static_cast<std::uint32_t>(-28200));
        PutHalf(memory, r.r4, 2, r.r11);
        break;
    case 0x82CFCB30u:
        r.r11 = Word(memory, r.r3, 20);
        r.r3 = 0;
        r.r10 = Word(memory, r.r4, 20);
        r.r9 = Word(memory, r.r11, 68);
        PutWord(memory, r.r10, 4, r.r9);
        PutWord(memory, r.r11, 68, r.r4);
        break;
    case 0x82D18990u:
        r.r11 = RotatedWord(r.r3, 4) & 0xfffffff0u;
        PutWord(memory, r.r1, static_cast<std::uint32_t>(-16), r.r11);
        r.r11 = RotatedWord(r.r6, 1) & 0xfffffffeu;
        r.r10 = Half(memory, r.r1, static_cast<std::uint32_t>(-16));
        r.r11 = r.r6 + r.r11;
        r.r9 = Byte(memory, r.r1, static_cast<std::uint32_t>(-14));
        r.r11 += r.r4;
        PutHalf(memory, r.r11, 0, r.r10);
        PutByte(memory, r.r11, 2, r.r9);
        break;
    case 0x82E441A8u:
        r.r11 = Word(memory, r.r3, 244);
        PutWord(memory, r.r4, 0, r.r11);
        PutWord(memory, r.r3, 244, r.r4);
        break;
    case 0x82E447B0u:
        r.r11 = Word(memory, r.r3, 768);
        PutWord(memory, r.r3, 632, r.r4);
        r.r11 |= 8;
        PutWord(memory, r.r3, 636, r.r5);
        PutWord(memory, r.r3, 640, r.r6);
        PutWord(memory, r.r3, 768, r.r11);
        break;
    case 0x82E447D0u:
        r.r11 = Word(memory, r.r3, 768);
        PutByte(memory, r.r3, 644, r.r4);
        r.r11 |= 8;
        PutWord(memory, r.r3, 768, r.r11);
        break;
    case 0x82F0A3B0u:
        r.r10 = Word(memory, r.r3, 8);
        r.r11 = r.r3 + 131072;
        r.r10 += 1;
        r.r11 += 5028;
        PutWord(memory, r.r3, 8, r.r10);
        r.r10 = Word(memory, r.r11);
        PutWord(memory, r.r11, 4, r.r10);
        break;
    case 0x82F59338u:
        r.r11 = Word(memory, r.r3, 4);
        PutByte(memory, r.r3, 24, r.r4);
        r.r11 = Word(memory, r.r11, 292);
        PutWord(memory, r.r3, 12, r.r11);
        break;
    case 0x82F73C08u:
        r.r11 = Word(memory, r.r3, 4);
        r.r10 = Word(memory, r.r11, 12);
        PutWord(memory, r.r3, 8, r.r10);
        r.r11 = Word(memory, r.r11, 16);
        PutWord(memory, r.r3, 12, r.r11);
        break;
    case 0x82F917F8u:
        r.r11 = Word(memory, r.r3, 112);
        PutWord(memory, r.r11, 272, r.r4);
        r.r11 = Half(memory, r.r3, 140);
        r.r11 += 1;
        PutHalf(memory, r.r3, 140, r.r11);
        break;
    case 0x83054980u:
        r.r11 = 0;
        for (std::uint32_t offset : {0u, 4u, 8u, 12u, 16u})
            PutWord(memory, r.r3, offset, 0);
        r.r11 = Word(memory, r.r3);
        PutWord(memory, r.r3, 16, r.r4);
        r.r11 |= 0x80000000u;
        PutWord(memory, r.r3, 0, r.r11);
        PutWord(memory, r.r4, 4, r.r3);
        break;
    default: return false;
    }
    return true;
}

} // namespace lo::semantic::memory_writes

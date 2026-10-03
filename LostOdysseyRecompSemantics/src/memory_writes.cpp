#include "lo_semantics/memory_writes.h"

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
    default: return false;
    }
    return true;
}

} // namespace lo::semantic::memory_writes

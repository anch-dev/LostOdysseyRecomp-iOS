#include "lo_semantics/legacy_float_digit_accumulator.h"
#include "lo_semantics/memory_move.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::legacy_float_digit_accumulator
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
std::uint64_t& R(Registers& state,unsigned index) { return state.r[index]; }
std::int32_t SignedWord(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(Address(value)); }
std::int64_t SignedByte(std::uint64_t value)
{ return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value)); }
void CompareUnsigned(Condition& cr,std::uint32_t a,std::uint32_t b,std::uint8_t so)
{ cr={std::uint8_t(a<b),std::uint8_t(a>b),std::uint8_t(a==b),so}; }
void CompareSigned(Condition& cr,std::int32_t a,std::int32_t b,std::uint8_t so)
{ cr={std::uint8_t(a<b),std::uint8_t(a>b),std::uint8_t(a==b),so}; }
void Save27(GuestMemory& memory,Registers& state)
{
    for(unsigned i=27;i<=31;++i)
        WriteU64(memory,Address(R(state,1)-8u*(33u-i)),R(state,i));
    memory.WriteU32(Address(R(state,1)-8u),Address(R(state,12)));
}
void Restore27(GuestMemory& memory,Registers& state)
{
    for(unsigned i=27;i<=31;++i)
        R(state,i)=ReadU64(memory,Address(R(state,1)-8u*(33u-i)));
    R(state,12)=memory.ReadU32(Address(R(state,1)-8u));
    state.lr=R(state,12);
}
void Copy12(GuestMemory& memory,Registers& state)
{
    // All callers here request exactly twelve bytes. Track the source pointer,
    // CTR and CR1/CR7 that survive this caller's own register operations.
    const auto destination=R(state,3), source=R(state,4);
    const auto prefix=(Address(destination)&7u) ? 8u-(Address(destination)&7u) : 0u;
    const auto remaining=12u-prefix;
    const auto aligned_source=source+prefix;
    const auto source_alignment=Address(aligned_source)&7u;
    const auto copied=CopyGuestMemory(memory,destination,Address(source),12u,Address(R(state,1)));
    if(source_alignment==0u)
    {
        const auto tail=remaining&7u;
        R(state,4)=tail==0u || tail==4u ? aligned_source-8u+(remaining/8u)*8u : source+11u;
        CompareUnsigned(state.cr[1],tail,4u,state.xer_so);
    }
    else if(source_alignment==4u)
    {
        R(state,4)=(remaining&3u)==0u ? aligned_source-4u+(remaining/4u)*4u : source+11u;
        CompareUnsigned(state.cr[1],remaining/4u,0u,state.xer_so);
    }
    else
    {
        R(state,4)=source+11u;
        CompareUnsigned(state.cr[1],source_alignment,0u,state.xer_so);
    }
    CompareUnsigned(state.cr[7],remaining,128u,state.xer_so);
    state.ctr=0u;
    R(state,3)=copied;
}
void Accumulate(GuestMemory& memory,Registers& state)
{
    std::uint64_t temporary=0;

	// mflr r12
	R(state,12) = state.lr;
	// bl 0x82b7a6e4
	state.lr = 0x82297F40;
	Save27(memory,state);
	// stwu r1,-144(r1)
	temporary = R(state,1) + uint64_t(-144);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,1)));
	R(state,1) = temporary;
	// mr r31,r5
	R(state,31) = R(state,5);
	// li r30,0
	R(state,30) = 0;
	// mr r28,r4
	R(state,28) = R(state,4);
	// mr r29,r3
	R(state,29) = R(state,3);
	// li r27,16462
	R(state,27) = 16462;
	// cmplwi cr6,r28,0
	CompareUnsigned(state.cr[6],Address(R(state,28)), 0,state.xer_so);
	// stw r30,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,30)));
	// stw r30,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,30)));
	// stw r30,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,30)));
	// beq cr6,0x82298124
	if (state.cr[6].eq) goto loc_82298124;
loc_82297F6C:
	// addi r3,r1,80
	R(state,3) = R(state,1) + 80;
	// li r5,12
	R(state,5) = 12;
	// mr r4,r31
	R(state,4) = R(state,31);
	// bl 0x82b7a0b0
	state.lr = 0x82297F7C;
	Copy12(memory,state);
	// lwz r10,4(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 4));
	// lwz r9,0(r31)
	R(state,9) = memory.ReadU32(Address(Address(R(state,31)) + 0));
	// mr r6,r30
	R(state,6) = R(state,30);
	// lwz r11,8(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 8));
	// rlwinm r8,r10,1,31,31
	R(state,8) = std::rotl(Address(R(state,10)) | (R(state,10) << 32), 1) & 0x1;
	// rlwinm r7,r9,1,0,30
	R(state,7) = std::rotl(Address(R(state,9)) | (R(state,9) << 32), 1) & 0xFFFFFFFE;
	// rlwinm r9,r11,1,31,31
	R(state,9) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 1) & 0x1;
	// rlwinm r10,r10,1,0,30
	R(state,10) = std::rotl(Address(R(state,10)) | (R(state,10) << 32), 1) & 0xFFFFFFFE;
	// rlwinm r11,r11,1,0,30
	R(state,11) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 1) & 0xFFFFFFFE;
	// or r9,r10,r9
	R(state,9) = R(state,10) | R(state,9);
	// or r8,r7,r8
	R(state,8) = R(state,7) | R(state,8);
	// rlwinm r10,r11,1,0,30
	R(state,10) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 1) & 0xFFFFFFFE;
	// rlwinm r7,r11,1,31,31
	R(state,7) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 1) & 0x1;
	// stw r11,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,11)));
	// rlwinm r11,r9,1,31,31
	R(state,11) = std::rotl(Address(R(state,9)) | (R(state,9) << 32), 1) & 0x1;
	// stw r9,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,9)));
	// rotlwi r9,r9,0
	R(state,9) = std::rotl(Address(R(state,9)), 0);
	// rlwinm r5,r8,1,0,30
	R(state,5) = std::rotl(Address(R(state,8)) | (R(state,8) << 32), 1) & 0xFFFFFFFE;
	// stw r8,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,8)));
	// rlwinm r9,r9,1,0,30
	R(state,9) = std::rotl(Address(R(state,9)) | (R(state,9) << 32), 1) & 0xFFFFFFFE;
	// stw r10,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,10)));
	// or r8,r5,r11
	R(state,8) = R(state,5) | R(state,11);
	// or r9,r9,r7
	R(state,9) = R(state,9) | R(state,7);
	// lwz r7,88(r1)
	R(state,7) = memory.ReadU32(Address(Address(R(state,1)) + 88));
	// add r11,r10,r7
	R(state,11) = R(state,10) + R(state,7);
	// cmplw cr6,r11,r10
	CompareUnsigned(state.cr[6],Address(R(state,11)), Address(R(state,10)),state.xer_so);
	// stw r8,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,8)));
	// stw r9,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,9)));
	// blt cr6,0x82297ff8
	if (state.cr[6].lt) goto loc_82297FF8;
	// cmplw cr6,r11,r7
	CompareUnsigned(state.cr[6],Address(R(state,11)), Address(R(state,7)),state.xer_so);
	// bge cr6,0x82297ffc
	if (!state.cr[6].lt) goto loc_82297FFC;
loc_82297FF8:
	// li r6,1
	R(state,6) = 1;
loc_82297FFC:
	// cmpwi cr6,r6,0
	CompareSigned(state.cr[6],SignedWord(R(state,6)), 0,state.xer_so);
	// stw r11,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,11)));
	// beq cr6,0x82298038
	if (state.cr[6].eq) goto loc_82298038;
	// addi r10,r9,1
	R(state,10) = R(state,9) + 1;
	// mr r7,r30
	R(state,7) = R(state,30);
	// cmplw cr6,r10,r9
	CompareUnsigned(state.cr[6],Address(R(state,10)), Address(R(state,9)),state.xer_so);
	// blt cr6,0x82298020
	if (state.cr[6].lt) goto loc_82298020;
	// cmplwi cr6,r10,1
	CompareUnsigned(state.cr[6],Address(R(state,10)), 1,state.xer_so);
	// bge cr6,0x82298024
	if (!state.cr[6].lt) goto loc_82298024;
loc_82298020:
	// li r7,1
	R(state,7) = 1;
loc_82298024:
	// cmpwi cr6,r7,0
	CompareSigned(state.cr[6],SignedWord(R(state,7)), 0,state.xer_so);
	// stw r10,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,10)));
	// beq cr6,0x82298038
	if (state.cr[6].eq) goto loc_82298038;
	// addi r10,r8,1
	R(state,10) = R(state,8) + 1;
	// stw r10,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,10)));
loc_82298038:
	// lwz r10,4(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 4));
	// mr r8,r30
	R(state,8) = R(state,30);
	// lwz r7,84(r1)
	R(state,7) = memory.ReadU32(Address(Address(R(state,1)) + 84));
	// add r9,r10,r7
	R(state,9) = R(state,10) + R(state,7);
	// cmplw cr6,r9,r10
	CompareUnsigned(state.cr[6],Address(R(state,9)), Address(R(state,10)),state.xer_so);
	// blt cr6,0x82298058
	if (state.cr[6].lt) goto loc_82298058;
	// cmplw cr6,r9,r7
	CompareUnsigned(state.cr[6],Address(R(state,9)), Address(R(state,7)),state.xer_so);
	// bge cr6,0x8229805c
	if (!state.cr[6].lt) goto loc_8229805C;
loc_82298058:
	// li r8,1
	R(state,8) = 1;
loc_8229805C:
	// cmpwi cr6,r8,0
	CompareSigned(state.cr[6],SignedWord(R(state,8)), 0,state.xer_so);
	// stw r9,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,9)));
	// beq cr6,0x82298074
	if (state.cr[6].eq) goto loc_82298074;
	// lwz r10,0(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 0));
	// addi r10,r10,1
	R(state,10) = R(state,10) + 1;
	// stw r10,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,10)));
loc_82298074:
	// lwz r8,0(r31)
	R(state,8) = memory.ReadU32(Address(Address(R(state,31)) + 0));
	// rlwinm r10,r11,1,31,31
	R(state,10) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 1) & 0x1;
	// lwz r7,80(r1)
	R(state,7) = memory.ReadU32(Address(Address(R(state,1)) + 80));
	// rlwinm r9,r9,1,31,31
	R(state,9) = std::rotl(Address(R(state,9)) | (R(state,9) << 32), 1) & 0x1;
	// lwz r6,4(r31)
	R(state,6) = memory.ReadU32(Address(Address(R(state,31)) + 4));
	// rlwinm r11,r11,1,0,30
	R(state,11) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 1) & 0xFFFFFFFE;
	// add r8,r8,r7
	R(state,8) = R(state,8) + R(state,7);
	// rlwinm r6,r6,1,0,30
	R(state,6) = std::rotl(Address(R(state,6)) | (R(state,6) << 32), 1) & 0xFFFFFFFE;
	// rlwinm r5,r8,1,0,30
	R(state,5) = std::rotl(Address(R(state,8)) | (R(state,8) << 32), 1) & 0xFFFFFFFE;
	// or r8,r6,r10
	R(state,8) = R(state,6) | R(state,10);
	// or r6,r5,r9
	R(state,6) = R(state,5) | R(state,9);
	// stw r11,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,11)));
	// mr r7,r30
	R(state,7) = R(state,30);
	// stw r8,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,8)));
	// stw r6,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,6)));
	// lbz r10,0(r29)
	R(state,10) = memory.ReadU8(Address(Address(R(state,29)) + 0));
	// extsb r9,r10
	R(state,9) = SignedByte(R(state,10));
	// add r10,r11,r9
	R(state,10) = R(state,11) + R(state,9);
	// cmplw cr6,r10,r11
	CompareUnsigned(state.cr[6],Address(R(state,10)), Address(R(state,11)),state.xer_so);
	// stw r9,88(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 88), Address(R(state,9)));
	// blt cr6,0x822980d0
	if (state.cr[6].lt) goto loc_822980D0;
	// cmplw cr6,r10,r9
	CompareUnsigned(state.cr[6],Address(R(state,10)), Address(R(state,9)),state.xer_so);
	// bge cr6,0x822980d4
	if (!state.cr[6].lt) goto loc_822980D4;
loc_822980D0:
	// li r7,1
	R(state,7) = 1;
loc_822980D4:
	// cmpwi cr6,r7,0
	CompareSigned(state.cr[6],SignedWord(R(state,7)), 0,state.xer_so);
	// stw r10,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,10)));
	// beq cr6,0x82298110
	if (state.cr[6].eq) goto loc_82298110;
	// addi r11,r8,1
	R(state,11) = R(state,8) + 1;
	// mr r10,r30
	R(state,10) = R(state,30);
	// cmplw cr6,r11,r8
	CompareUnsigned(state.cr[6],Address(R(state,11)), Address(R(state,8)),state.xer_so);
	// blt cr6,0x822980f8
	if (state.cr[6].lt) goto loc_822980F8;
	// cmplwi cr6,r11,1
	CompareUnsigned(state.cr[6],Address(R(state,11)), 1,state.xer_so);
	// bge cr6,0x822980fc
	if (!state.cr[6].lt) goto loc_822980FC;
loc_822980F8:
	// li r10,1
	R(state,10) = 1;
loc_822980FC:
	// cmpwi cr6,r10,0
	CompareSigned(state.cr[6],SignedWord(R(state,10)), 0,state.xer_so);
	// stw r11,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,11)));
	// beq cr6,0x82298110
	if (state.cr[6].eq) goto loc_82298110;
	// addi r11,r6,1
	R(state,11) = R(state,6) + 1;
	// stw r11,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,11)));
loc_82298110:
	// lwz r11,0(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 0));
	// addic. r28,r28,-1
	state.xer_ca = Address(R(state,28)) > 0;
	R(state,28) = R(state,28) + -1;
	CompareSigned(state.cr[0],SignedWord(R(state,28)), 0,state.xer_so);
	// addi r29,r29,1
	R(state,29) = R(state,29) + 1;
	// bne 0x82297f6c
	if (!state.cr[0].eq) goto loc_82297F6C;
	// b 0x82298160
	goto loc_82298160;
loc_82298124:
	// clrlwi r9,r27,16
	R(state,9) = Address(R(state,27)) & 0xFFFF;
	// lwz r11,4(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 4));
	// lwz r10,8(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 8));
	// addis r9,r9,1
	R(state,9) = R(state,9) + 65536;
	// rlwinm r8,r10,16,16,31
	R(state,8) = std::rotl(Address(R(state,10)) | (R(state,10) << 32), 16) & 0xFFFF;
	// addi r9,r9,-16
	R(state,9) = R(state,9) + -16;
	// rlwinm r10,r10,16,0,15
	R(state,10) = std::rotl(Address(R(state,10)) | (R(state,10) << 32), 16) & 0xFFFF0000;
	// clrlwi r27,r9,16
	R(state,27) = Address(R(state,9)) & 0xFFFF;
	// rlwinm r9,r11,16,0,15
	R(state,9) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 16) & 0xFFFF0000;
	// rlwinm r11,r11,16,16,31
	R(state,11) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 16) & 0xFFFF;
	// or r9,r9,r8
	R(state,9) = R(state,9) | R(state,8);
	// stw r10,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,10)));
	// stw r11,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,11)));
	// rotlwi r11,r11,0
	R(state,11) = std::rotl(Address(R(state,11)), 0);
	// stw r9,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,9)));
loc_82298160:
	// cmplwi cr6,r11,0
	CompareUnsigned(state.cr[6],Address(R(state,11)), 0,state.xer_so);
	// beq cr6,0x82298124
	if (state.cr[6].eq) goto loc_82298124;
	// b 0x822981b0
	goto loc_822981B0;
loc_8229816C:
	// clrlwi r9,r27,16
	R(state,9) = Address(R(state,27)) & 0xFFFF;
	// lwz r11,8(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 8));
	// lwz r8,0(r31)
	R(state,8) = memory.ReadU32(Address(Address(R(state,31)) + 0));
	// addis r9,r9,1
	R(state,9) = R(state,9) + 65536;
	// lwz r10,4(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 4));
	// rlwinm r7,r8,1,0,30
	R(state,7) = std::rotl(Address(R(state,8)) | (R(state,8) << 32), 1) & 0xFFFFFFFE;
	// addi r9,r9,-1
	R(state,9) = R(state,9) + -1;
	// rlwinm r8,r10,1,31,31
	R(state,8) = std::rotl(Address(R(state,10)) | (R(state,10) << 32), 1) & 0x1;
	// clrlwi r27,r9,16
	R(state,27) = Address(R(state,9)) & 0xFFFF;
	// rlwinm r9,r11,1,31,31
	R(state,9) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 1) & 0x1;
	// rlwinm r11,r11,1,0,30
	R(state,11) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 1) & 0xFFFFFFFE;
	// rlwinm r10,r10,1,0,30
	R(state,10) = std::rotl(Address(R(state,10)) | (R(state,10) << 32), 1) & 0xFFFFFFFE;
	// stw r11,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,11)));
	// or r11,r10,r9
	R(state,11) = R(state,10) | R(state,9);
	// or r10,r7,r8
	R(state,10) = R(state,7) | R(state,8);
	// stw r11,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,11)));
	// stw r10,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,10)));
loc_822981B0:
	// lwz r11,0(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 0));
	// rlwinm. r11,r11,0,16,16
	R(state,11) = std::rotl(Address(R(state,11)) | (R(state,11) << 32), 0) & 0x8000;
	CompareSigned(state.cr[0],SignedWord(R(state,11)), 0,state.xer_so);
	// beq 0x8229816c
	if (state.cr[0].eq) goto loc_8229816C;
	// sth r27,0(r31)
	memory.WriteU16(Address(Address(R(state,31)) + 0), static_cast<std::uint16_t>(R(state,27)));
	// addi r1,r1,144
	R(state,1) = R(state,1) + 144;
	// b 0x82b7a734
	Restore27(memory,state);
	return;

}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Registers& state)
{ if(entry!=0x82297f38u) return false; Accumulate(memory,state); return true; }
}

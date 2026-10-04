#include "lo_semantics/legacy_float_text_parser.h"

#include "lo_semantics/memory_move.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::legacy_float_text_parser
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index)
{ return index == 1u ? state.sp : state.r[index]; }
std::uint32_t Low32(std::uint64_t value)
{ return static_cast<std::uint32_t>(value); }
std::uint16_t Low16(std::uint64_t value)
{ return static_cast<std::uint16_t>(value); }
std::uint8_t Low8(std::uint64_t value)
{ return static_cast<std::uint8_t>(value); }
std::int32_t Signed32(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(Low32(value)); }
std::int8_t Signed8(std::uint64_t value)
{ return std::bit_cast<std::int8_t>(Low8(value)); }
std::int16_t Signed16(std::uint64_t value)
{ return std::bit_cast<std::int16_t>(Low16(value)); }
std::int32_t ArithmeticShift32(std::uint32_t value, unsigned shift)
{
    const auto shifted = value >> shift;
    const auto fill = (value & 0x80000000u) ?
        (0xffffffffu << (32u - shift)) : 0u;
    return std::bit_cast<std::int32_t>(shifted | fill);
}
void CompareUnsigned(Condition& cr, std::uint64_t left,
    std::uint32_t right, std::uint8_t so)
{
    const auto word = Low32(left);
    cr = {std::uint8_t(word < right), std::uint8_t(word > right),
        std::uint8_t(word == right), so};
}
void CompareSigned(Condition& cr, std::uint64_t left,
    std::int32_t right, std::uint8_t so)
{
    const auto word = Signed32(left);
    cr = {std::uint8_t(word < right), std::uint8_t(word > right),
        std::uint8_t(word == right), so};
}
std::uint8_t LoadU8(GuestMemory& memory, std::uint64_t address)
{ return memory.ReadU8(Address(address)); }
std::uint16_t LoadU16(GuestMemory& memory, std::uint64_t address)
{ return memory.ReadU16(Address(address)); }
std::uint32_t LoadU32(GuestMemory& memory, std::uint64_t address)
{ return memory.ReadU32(Address(address)); }
std::uint64_t LoadU64(GuestMemory& memory, std::uint64_t address)
{ return ReadU64(memory, Address(address)); }
void StoreU8(GuestMemory& memory, std::uint64_t address,
    std::uint64_t value)
{ memory.WriteU8(Address(address), Low8(value)); }
void StoreU16(GuestMemory& memory, std::uint64_t address,
    std::uint64_t value)
{ memory.WriteU16(Address(address), Low16(value)); }
void StoreU32(GuestMemory& memory, std::uint64_t address,
    std::uint64_t value)
{ memory.WriteU32(Address(address), Low32(value)); }
void StoreU64(GuestMemory& memory, std::uint64_t address,
    std::uint64_t value)
{ WriteU64(memory, Address(address), value); }
void SaveRegs(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 17u; index <= 31u; ++index)
        StoreU64(memory, state.sp - 8u * (33u - index), R(state, index));
    StoreU32(memory, state.sp - 8u, R(state, 12));
}
void RestoreRegs(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 17u; index <= 31u; ++index)
        R(state, index) = LoadU64(memory,
            state.sp - 8u * (33u - index));
    R(state, 12) = LoadU32(memory, state.sp - 8u);
    state.lr = R(state, 12);
}
void CallCopy(GuestMemory& memory, Registers& state)
{
    R(state, 3) = CopyGuestMemory(memory, R(state, 3),
        Address(R(state, 4)), R(state, 5), Address(state.sp));
}

// Complete 12-state PPC control flow. Guest table bytes and the low 32-bit
// address window remain observable, including on paths not selected by the
// focused oracle. The only pending direct PPC callee is 82297F38.
void Parse(GuestMemory& memory, PpcBoundaryServices& services,
    Registers& state)
{
	std::uint64_t temp = 0;
	// mflr r12
	R(state, 12) = state.lr;
	// bl 0x82b7a6bc
	state.lr = 0x822975B8;
	SaveRegs(memory, state);
	// stwu r1,-288(r1)
	temp = R(state, 1) + uint64_t(-288);
	StoreU32(memory, Low32(temp), Low32(R(state, 1)));
	R(state, 1) = temp;
	// li r21,0
	R(state, 21) = 0;
	// mr r19,r3
	R(state, 19) = R(state, 3);
	// mr r23,r6
	R(state, 23) = R(state, 6);
	// mr r25,r7
	R(state, 25) = R(state, 7);
	// mr r24,r8
	R(state, 24) = R(state, 8);
	// addi r3,r1,128
	R(state, 3) = R(state, 1) + 128;
	// mr r18,r21
	R(state, 18) = R(state, 21);
	// li r27,1
	R(state, 27) = 1;
	// mr r6,r21
	R(state, 6) = R(state, 21);
	// mr r30,r21
	R(state, 30) = R(state, 21);
	// mr r26,r21
	R(state, 26) = R(state, 21);
	// mr r28,r21
	R(state, 28) = R(state, 21);
	// mr r29,r21
	R(state, 29) = R(state, 21);
	// mr r31,r21
	R(state, 31) = R(state, 21);
	// mr r11,r21
	R(state, 11) = R(state, 21);
	// cmplwi cr6,r10,0
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 0, state.xer_so);
	// bne cr6,0x82297630
	if (!state.cr6.eq) goto loc_82297630;
	// bl 0x82b7fd78
	state.lr = 0x82297604;
	services.InvalidArgument82B7FD78(memory, state);
	// mr r11,r3
	R(state, 11) = R(state, 3);
	// li r10,22
	R(state, 10) = 22;
	// li r7,0
	R(state, 7) = 0;
	// li r6,0
	R(state, 6) = 0;
	// li r5,0
	R(state, 5) = 0;
	// li r4,0
	R(state, 4) = 0;
	// li r3,0
	R(state, 3) = 0;
	// stw r10,0(r11)
	StoreU32(memory, Low32(R(state, 11)) + 0, Low32(R(state, 10)));
	// bl 0x82b7fec0
	state.lr = 0x82297628;
	services.InvalidParameter82B7FEC0(memory, state);
	// li r3,0
	R(state, 3) = 0;
	// b 0x82297f2c
	goto loc_82297F2C;
loc_82297630:
	// mr r8,r5
	R(state, 8) = R(state, 5);
loc_82297634:
	// lhz r10,0(r8)
	R(state, 10) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// cmplwi cr6,r10,32
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 32, state.xer_so);
	// beq cr6,0x82297658
	if (state.cr6.eq) goto loc_82297658;
	// cmplwi cr6,r10,9
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 9, state.xer_so);
	// beq cr6,0x82297658
	if (state.cr6.eq) goto loc_82297658;
	// cmplwi cr6,r10,10
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 10, state.xer_so);
	// beq cr6,0x82297658
	if (state.cr6.eq) goto loc_82297658;
	// cmplwi cr6,r10,13
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 13, state.xer_so);
	// bne cr6,0x82297660
	if (!state.cr6.eq) goto loc_82297660;
loc_82297658:
	// addi r8,r8,2
	R(state, 8) = R(state, 8) + 2;
	// b 0x82297634
	goto loc_82297634;
loc_82297660:
	// lis r10,-31967
	R(state, 10) = -2094989312;
	// lis r7,0
	R(state, 7) = 0;
	// ori r20,r7,32768
	R(state, 20) = R(state, 7) | 32768;
	// lwz r7,22088(r10)
	R(state, 7) = LoadU32(memory, Low32(R(state, 10)) + 22088);
loc_82297670:
	// lhz r10,0(r8)
	R(state, 10) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// cmplwi cr6,r11,11
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 11, state.xer_so);
	// addi r8,r8,2
	R(state, 8) = R(state, 8) + 2;
	// bgt cr6,0x822979f0
	if (state.cr6.gt) goto loc_822979F0;
	// lis r12,-32243
	R(state, 12) = -2113077248;
	// addi r12,r12,14896
	R(state, 12) = R(state, 12) + 14896;
	// lbzx r0,r12,r11
	R(state, 0) = LoadU8(memory, Low32(R(state, 12)) + Low32(R(state, 11)));
	// rlwinm r0,r0,2,0,29
	R(state, 0) = std::rotl(Low32(R(state, 0)) | (R(state, 0) << 32), 2) & 0xFFFFFFFC;
	// lis r12,-32215
	R(state, 12) = -2111242240;
	// addi r12,r12,30376
	R(state, 12) = R(state, 12) + 30376;
	// add r12,r12,r0
	R(state, 12) = R(state, 12) + R(state, 0);
	// mtctr r12
	state.ctr = R(state, 12);
	// nop
	// bctr
	switch (Low32(R(state, 11))) {
	case 0:
		goto loc_822976A8;
	case 1:
		goto loc_8229771C;
	case 2:
		goto loc_82297798;
	case 3:
		goto loc_82297814;
	case 4:
		goto loc_82297888;
	case 5:
		goto loc_82297900;
	case 6:
		goto loc_82297920;
	case 7:
		goto loc_822979A4;
	case 8:
		goto loc_82297968;
	case 9:
		goto loc_822979FC;
	case 10:
		goto loc_822979F0;
	case 11:
		goto loc_822979BC;
	default:
		throw std::logic_error("unreachable decimal parser state");
	}
loc_822976A8:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// cmplwi cr6,r11,49
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 49, state.xer_so);
	// blt cr6,0x822976c8
	if (state.cr6.lt) goto loc_822976C8;
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// bgt cr6,0x822976c8
	if (state.cr6.gt) goto loc_822976C8;
loc_822976BC:
	// li r11,3
	R(state, 11) = 3;
loc_822976C0:
	// addi r8,r8,-2
	R(state, 8) = R(state, 8) + -2;
	// b 0x82297670
	goto loc_82297670;
loc_822976C8:
	// lwz r10,0(r7)
	R(state, 10) = LoadU32(memory, Low32(R(state, 7)) + 0);
	// lbz r10,0(r10)
	R(state, 10) = LoadU8(memory, Low32(R(state, 10)) + 0);
	// extsb r10,r10
	R(state, 10) = Signed8(R(state, 10));
	// cmpw cr6,r11,r10
	CompareSigned(state.cr6, Signed32(R(state, 11)), Signed32(R(state, 10)), state.xer_so);
	// bne cr6,0x822976e4
	if (!state.cr6.eq) goto loc_822976E4;
loc_822976DC:
	// li r11,5
	R(state, 11) = 5;
	// b 0x82297670
	goto loc_82297670;
loc_822976E4:
	// cmpwi cr6,r11,43
	CompareSigned(state.cr6, Signed32(R(state, 11)), 43, state.xer_so);
	// beq cr6,0x82297710
	if (state.cr6.eq) goto loc_82297710;
	// cmpwi cr6,r11,45
	CompareSigned(state.cr6, Signed32(R(state, 11)), 45, state.xer_so);
	// beq cr6,0x82297704
	if (state.cr6.eq) goto loc_82297704;
	// cmpwi cr6,r11,48
	CompareSigned(state.cr6, Signed32(R(state, 11)), 48, state.xer_so);
	// bne cr6,0x8229799c
	if (!state.cr6.eq) goto loc_8229799C;
loc_822976FC:
	// li r11,1
	R(state, 11) = 1;
	// b 0x82297670
	goto loc_82297670;
loc_82297704:
	// li r11,2
	R(state, 11) = 2;
	// mr r18,r20
	R(state, 18) = R(state, 20);
	// b 0x82297670
	goto loc_82297670;
loc_82297710:
	// li r11,2
	R(state, 11) = 2;
	// mr r18,r21
	R(state, 18) = R(state, 21);
	// b 0x82297670
	goto loc_82297670;
loc_8229771C:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// li r30,1
	R(state, 30) = 1;
	// cmplwi cr6,r11,49
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 49, state.xer_so);
	// blt cr6,0x82297734
	if (state.cr6.lt) goto loc_82297734;
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// ble cr6,0x822976bc
	if (!state.cr6.gt) goto loc_822976BC;
loc_82297734:
	// lwz r10,0(r7)
	R(state, 10) = LoadU32(memory, Low32(R(state, 7)) + 0);
	// lbz r10,0(r10)
	R(state, 10) = LoadU8(memory, Low32(R(state, 10)) + 0);
	// extsb r10,r10
	R(state, 10) = Signed8(R(state, 10));
	// cmpw cr6,r11,r10
	CompareSigned(state.cr6, Signed32(R(state, 11)), Signed32(R(state, 10)), state.xer_so);
	// bne cr6,0x82297750
	if (!state.cr6.eq) goto loc_82297750;
loc_82297748:
	// li r11,4
	R(state, 11) = 4;
	// b 0x82297670
	goto loc_82297670;
loc_82297750:
	// cmpwi cr6,r11,43
	CompareSigned(state.cr6, Signed32(R(state, 11)), 43, state.xer_so);
	// beq cr6,0x8229778c
	if (state.cr6.eq) goto loc_8229778C;
	// cmpwi cr6,r11,45
	CompareSigned(state.cr6, Signed32(R(state, 11)), 45, state.xer_so);
	// beq cr6,0x8229778c
	if (state.cr6.eq) goto loc_8229778C;
	// cmpwi cr6,r11,48
	CompareSigned(state.cr6, Signed32(R(state, 11)), 48, state.xer_so);
	// beq cr6,0x822976fc
	if (state.cr6.eq) goto loc_822976FC;
loc_82297768:
	// cmpwi cr6,r11,67
	CompareSigned(state.cr6, Signed32(R(state, 11)), 67, state.xer_so);
	// ble cr6,0x8229799c
	if (!state.cr6.gt) goto loc_8229799C;
	// cmpwi cr6,r11,69
	CompareSigned(state.cr6, Signed32(R(state, 11)), 69, state.xer_so);
	// ble cr6,0x82297784
	if (!state.cr6.gt) goto loc_82297784;
	// addi r11,r11,-100
	R(state, 11) = R(state, 11) + -100;
	// cmplwi cr6,r11,1
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 1, state.xer_so);
	// bgt cr6,0x8229799c
	if (state.cr6.gt) goto loc_8229799C;
loc_82297784:
	// li r11,6
	R(state, 11) = 6;
	// b 0x82297670
	goto loc_82297670;
loc_8229778C:
	// addi r8,r8,-2
	R(state, 8) = R(state, 8) + -2;
	// li r11,11
	R(state, 11) = 11;
	// b 0x82297670
	goto loc_82297670;
loc_82297798:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// cmplwi cr6,r11,49
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 49, state.xer_so);
	// blt cr6,0x822977ac
	if (state.cr6.lt) goto loc_822977AC;
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// ble cr6,0x822976bc
	if (!state.cr6.gt) goto loc_822976BC;
loc_822977AC:
	// lwz r10,0(r7)
	R(state, 10) = LoadU32(memory, Low32(R(state, 7)) + 0);
	// lbz r10,0(r10)
	R(state, 10) = LoadU8(memory, Low32(R(state, 10)) + 0);
	// extsb r10,r10
	R(state, 10) = Signed8(R(state, 10));
	// cmpw cr6,r11,r10
	CompareSigned(state.cr6, Signed32(R(state, 11)), Signed32(R(state, 10)), state.xer_so);
	// beq cr6,0x822976dc
	if (state.cr6.eq) goto loc_822976DC;
	// cmpwi cr6,r11,48
	CompareSigned(state.cr6, Signed32(R(state, 11)), 48, state.xer_so);
	// beq cr6,0x822976fc
	if (state.cr6.eq) goto loc_822976FC;
loc_822977C8:
	// mr r8,r5
	R(state, 8) = R(state, 5);
loc_822977CC:
	// cmpwi cr6,r30,0
	CompareSigned(state.cr6, Signed32(R(state, 30)), 0, state.xer_so);
	// stw r8,0(r4)
	StoreU32(memory, Low32(R(state, 4)) + 0, Low32(R(state, 8)));
	// beq cr6,0x82297ec8
	if (state.cr6.eq) goto loc_82297EC8;
	// cmplwi cr6,r6,24
	CompareUnsigned(state.cr6, Low32(R(state, 6)), 24, state.xer_so);
	// ble cr6,0x82297804
	if (!state.cr6.gt) goto loc_82297804;
	// lbz r11,151(r1)
	R(state, 11) = LoadU8(memory, Low32(R(state, 1)) + 151);
	// extsb r11,r11
	R(state, 11) = Signed8(R(state, 11));
	// cmpwi cr6,r11,5
	CompareSigned(state.cr6, Signed32(R(state, 11)), 5, state.xer_so);
	// blt cr6,0x822977f8
	if (state.cr6.lt) goto loc_822977F8;
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// stb r11,151(r1)
	StoreU8(memory, Low32(R(state, 1)) + 151, Low8(R(state, 11)));
loc_822977F8:
	// li r6,24
	R(state, 6) = 24;
	// addi r3,r3,-1
	R(state, 3) = R(state, 3) + -1;
	// addi r31,r31,1
	R(state, 31) = R(state, 31) + 1;
loc_82297804:
	// cmplwi cr6,r6,0
	CompareUnsigned(state.cr6, Low32(R(state, 6)), 0, state.xer_so);
	// beq cr6,0x82297eb4
	if (state.cr6.eq) goto loc_82297EB4;
	// addi r11,r3,-1
	R(state, 11) = R(state, 3) + -1;
	// b 0x82297a78
	goto loc_82297A78;
loc_82297814:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// li r30,1
	R(state, 30) = 1;
	// b 0x82297854
	goto loc_82297854;
loc_82297820:
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// bgt cr6,0x8229785c
	if (state.cr6.gt) goto loc_8229785C;
	// cmplwi cr6,r6,25
	CompareUnsigned(state.cr6, Low32(R(state, 6)), 25, state.xer_so);
	// bge cr6,0x82297844
	if (!state.cr6.lt) goto loc_82297844;
	// addi r11,r11,-48
	R(state, 11) = R(state, 11) + -48;
	// addi r6,r6,1
	R(state, 6) = R(state, 6) + 1;
	// stb r11,0(r3)
	StoreU8(memory, Low32(R(state, 3)) + 0, Low8(R(state, 11)));
	// addi r3,r3,1
	R(state, 3) = R(state, 3) + 1;
	// b 0x82297848
	goto loc_82297848;
loc_82297844:
	// addi r31,r31,1
	R(state, 31) = R(state, 31) + 1;
loc_82297848:
	// lhz r10,0(r8)
	R(state, 10) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// addi r8,r8,2
	R(state, 8) = R(state, 8) + 2;
	// mr r11,r10
	R(state, 11) = R(state, 10);
loc_82297854:
	// cmplwi cr6,r11,48
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 48, state.xer_so);
	// bge cr6,0x82297820
	if (!state.cr6.lt) goto loc_82297820;
loc_8229785C:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// lwz r10,0(r7)
	R(state, 10) = LoadU32(memory, Low32(R(state, 7)) + 0);
	// lbz r10,0(r10)
	R(state, 10) = LoadU8(memory, Low32(R(state, 10)) + 0);
	// extsb r10,r10
	R(state, 10) = Signed8(R(state, 10));
	// cmpw cr6,r11,r10
	CompareSigned(state.cr6, Signed32(R(state, 11)), Signed32(R(state, 10)), state.xer_so);
	// beq cr6,0x82297748
	if (state.cr6.eq) goto loc_82297748;
loc_82297874:
	// cmpwi cr6,r11,43
	CompareSigned(state.cr6, Signed32(R(state, 11)), 43, state.xer_so);
	// beq cr6,0x8229778c
	if (state.cr6.eq) goto loc_8229778C;
	// cmpwi cr6,r11,45
	CompareSigned(state.cr6, Signed32(R(state, 11)), 45, state.xer_so);
	// beq cr6,0x8229778c
	if (state.cr6.eq) goto loc_8229778C;
	// b 0x82297768
	goto loc_82297768;
loc_82297888:
	// li r30,1
	R(state, 30) = 1;
	// li r26,1
	R(state, 26) = 1;
	// cmplwi cr6,r6,0
	CompareUnsigned(state.cr6, Low32(R(state, 6)), 0, state.xer_so);
	// bne cr6,0x822978b8
	if (!state.cr6.eq) goto loc_822978B8;
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// cmplwi cr6,r11,48
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 48, state.xer_so);
	// bne cr6,0x822978b8
	if (!state.cr6.eq) goto loc_822978B8;
loc_822978A4:
	// lhz r10,0(r8)
	R(state, 10) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// addi r31,r31,-1
	R(state, 31) = R(state, 31) + -1;
	// addi r8,r8,2
	R(state, 8) = R(state, 8) + 2;
	// cmplwi cr6,r10,48
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 48, state.xer_so);
	// beq cr6,0x822978a4
	if (state.cr6.eq) goto loc_822978A4;
loc_822978B8:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// b 0x822978f0
	goto loc_822978F0;
loc_822978C0:
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// bgt cr6,0x822978f8
	if (state.cr6.gt) goto loc_822978F8;
	// cmplwi cr6,r6,25
	CompareUnsigned(state.cr6, Low32(R(state, 6)), 25, state.xer_so);
	// bge cr6,0x822978e4
	if (!state.cr6.lt) goto loc_822978E4;
	// addi r11,r11,-48
	R(state, 11) = R(state, 11) + -48;
	// addi r6,r6,1
	R(state, 6) = R(state, 6) + 1;
	// addi r31,r31,-1
	R(state, 31) = R(state, 31) + -1;
	// stb r11,0(r3)
	StoreU8(memory, Low32(R(state, 3)) + 0, Low8(R(state, 11)));
	// addi r3,r3,1
	R(state, 3) = R(state, 3) + 1;
loc_822978E4:
	// lhz r10,0(r8)
	R(state, 10) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// addi r8,r8,2
	R(state, 8) = R(state, 8) + 2;
	// mr r11,r10
	R(state, 11) = R(state, 10);
loc_822978F0:
	// cmplwi cr6,r11,48
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 48, state.xer_so);
	// bge cr6,0x822978c0
	if (!state.cr6.lt) goto loc_822978C0;
loc_822978F8:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// b 0x82297874
	goto loc_82297874;
loc_82297900:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// li r26,1
	R(state, 26) = 1;
	// cmplwi cr6,r11,48
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 48, state.xer_so);
	// blt cr6,0x822977c8
	if (state.cr6.lt) goto loc_822977C8;
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// bgt cr6,0x822977c8
	if (state.cr6.gt) goto loc_822977C8;
	// li r11,4
	R(state, 11) = 4;
	// b 0x822976c0
	goto loc_822976C0;
loc_82297920:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// addi r5,r8,-4
	R(state, 5) = R(state, 8) + -4;
	// cmplwi cr6,r11,49
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 49, state.xer_so);
	// blt cr6,0x82297940
	if (state.cr6.lt) goto loc_82297940;
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// bgt cr6,0x82297940
	if (state.cr6.gt) goto loc_82297940;
loc_82297938:
	// li r11,9
	R(state, 11) = 9;
	// b 0x822976c0
	goto loc_822976C0;
loc_82297940:
	// cmpwi cr6,r11,43
	CompareSigned(state.cr6, Signed32(R(state, 11)), 43, state.xer_so);
	// beq cr6,0x82297960
	if (state.cr6.eq) goto loc_82297960;
	// cmpwi cr6,r11,45
	CompareSigned(state.cr6, Signed32(R(state, 11)), 45, state.xer_so);
	// beq cr6,0x822979dc
	if (state.cr6.eq) goto loc_822979DC;
loc_82297950:
	// cmpwi cr6,r11,48
	CompareSigned(state.cr6, Signed32(R(state, 11)), 48, state.xer_so);
	// bne cr6,0x822977c8
	if (!state.cr6.eq) goto loc_822977C8;
	// li r11,8
	R(state, 11) = 8;
	// b 0x82297670
	goto loc_82297670;
loc_82297960:
	// li r11,7
	R(state, 11) = 7;
	// b 0x82297670
	goto loc_82297670;
loc_82297968:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// li r28,1
	R(state, 28) = 1;
	// cmplwi cr6,r11,48
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 48, state.xer_so);
	// bne cr6,0x82297988
	if (!state.cr6.eq) goto loc_82297988;
loc_82297978:
	// lhz r10,0(r8)
	R(state, 10) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// addi r8,r8,2
	R(state, 8) = R(state, 8) + 2;
	// cmplwi cr6,r10,48
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 48, state.xer_so);
	// beq cr6,0x82297978
	if (state.cr6.eq) goto loc_82297978;
loc_82297988:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// cmplwi cr6,r11,49
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 49, state.xer_so);
	// blt cr6,0x8229799c
	if (state.cr6.lt) goto loc_8229799C;
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// ble cr6,0x82297938
	if (!state.cr6.gt) goto loc_82297938;
loc_8229799C:
	// addi r8,r8,-2
	R(state, 8) = R(state, 8) + -2;
	// b 0x822977cc
	goto loc_822977CC;
loc_822979A4:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// cmplwi cr6,r11,49
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 49, state.xer_so);
	// blt cr6,0x82297950
	if (state.cr6.lt) goto loc_82297950;
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// ble cr6,0x82297938
	if (!state.cr6.gt) goto loc_82297938;
	// b 0x82297950
	goto loc_82297950;
loc_822979BC:
	// cmpwi cr6,r9,0
	CompareSigned(state.cr6, Signed32(R(state, 9)), 0, state.xer_so);
	// beq cr6,0x822979e8
	if (state.cr6.eq) goto loc_822979E8;
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// addi r5,r8,-2
	R(state, 5) = R(state, 8) + -2;
	// cmpwi cr6,r11,43
	CompareSigned(state.cr6, Signed32(R(state, 11)), 43, state.xer_so);
	// beq cr6,0x82297960
	if (state.cr6.eq) goto loc_82297960;
	// cmpwi cr6,r11,45
	CompareSigned(state.cr6, Signed32(R(state, 11)), 45, state.xer_so);
	// bne cr6,0x822977c8
	if (!state.cr6.eq) goto loc_822977C8;
loc_822979DC:
	// li r11,7
	R(state, 11) = 7;
	// li r27,-1
	R(state, 27) = -1;
	// b 0x82297670
	goto loc_82297670;
loc_822979E8:
	// li r11,10
	R(state, 11) = 10;
	// addi r8,r8,-2
	R(state, 8) = R(state, 8) + -2;
loc_822979F0:
	// cmpwi cr6,r11,10
	CompareSigned(state.cr6, Signed32(R(state, 11)), 10, state.xer_so);
	// bne cr6,0x82297670
	if (!state.cr6.eq) goto loc_82297670;
	// b 0x822977cc
	goto loc_822977CC;
loc_822979FC:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// li r28,1
	R(state, 28) = 1;
	// mr r9,r21
	R(state, 9) = R(state, 21);
	// b 0x82297a34
	goto loc_82297A34;
loc_82297A0C:
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// bgt cr6,0x82297a44
	if (state.cr6.gt) goto loc_82297A44;
	// mulli r9,r9,10
	R(state, 9) = R(state, 9) * 10;
	// add r11,r9,r11
	R(state, 11) = R(state, 9) + R(state, 11);
	// addi r9,r11,-48
	R(state, 9) = R(state, 11) + -48;
	// cmpwi cr6,r9,5200
	CompareSigned(state.cr6, Signed32(R(state, 9)), 5200, state.xer_so);
	// bgt cr6,0x82297a40
	if (state.cr6.gt) goto loc_82297A40;
	// lhz r10,0(r8)
	R(state, 10) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// addi r8,r8,2
	R(state, 8) = R(state, 8) + 2;
	// mr r11,r10
	R(state, 11) = R(state, 10);
loc_82297A34:
	// cmplwi cr6,r11,48
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 48, state.xer_so);
	// bge cr6,0x82297a0c
	if (!state.cr6.lt) goto loc_82297A0C;
	// b 0x82297a44
	goto loc_82297A44;
loc_82297A40:
	// li r9,5201
	R(state, 9) = 5201;
loc_82297A44:
	// clrlwi r11,r10,16
	R(state, 11) = Low32(R(state, 10)) & 0xFFFF;
	// mr r29,r9
	R(state, 29) = R(state, 9);
	// b 0x82297a60
	goto loc_82297A60;
loc_82297A50:
	// cmplwi cr6,r11,57
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 57, state.xer_so);
	// bgt cr6,0x8229799c
	if (state.cr6.gt) goto loc_8229799C;
	// lhz r11,0(r8)
	R(state, 11) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// addi r8,r8,2
	R(state, 8) = R(state, 8) + 2;
loc_82297A60:
	// cmplwi cr6,r11,48
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 48, state.xer_so);
	// bge cr6,0x82297a50
	if (!state.cr6.lt) goto loc_82297A50;
	// b 0x8229799c
	goto loc_8229799C;
loc_82297A6C:
	// addi r11,r11,-1
	R(state, 11) = R(state, 11) + -1;
	// addi r6,r6,-1
	R(state, 6) = R(state, 6) + -1;
	// addi r31,r31,1
	R(state, 31) = R(state, 31) + 1;
loc_82297A78:
	// lbz r10,0(r11)
	R(state, 10) = LoadU8(memory, Low32(R(state, 11)) + 0);
	// cmplwi cr6,r10,0
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 0, state.xer_so);
	// beq cr6,0x82297a6c
	if (state.cr6.eq) goto loc_82297A6C;
	// addi r5,r1,96
	R(state, 5) = R(state, 1) + 96;
	// mr r4,r6
	R(state, 4) = R(state, 6);
	// addi r3,r1,128
	R(state, 3) = R(state, 1) + 128;
	// bl 0x82297f38
	state.lr = 0x82297A94;
	services.DigitsToExtended82297F38(memory, state);
	// cmpwi cr6,r27,0
	CompareSigned(state.cr6, Signed32(R(state, 27)), 0, state.xer_so);
	// bge cr6,0x82297aa0
	if (!state.cr6.lt) goto loc_82297AA0;
	// neg r29,r29
	R(state, 29) = -R(state, 29);
loc_82297AA0:
	// add r11,r31,r29
	R(state, 11) = R(state, 31) + R(state, 29);
	// cmpwi cr6,r28,0
	CompareSigned(state.cr6, Signed32(R(state, 28)), 0, state.xer_so);
	// bne cr6,0x82297ab0
	if (!state.cr6.eq) goto loc_82297AB0;
	// add r11,r11,r25
	R(state, 11) = R(state, 11) + R(state, 25);
loc_82297AB0:
	// cmpwi cr6,r26,0
	CompareSigned(state.cr6, Signed32(R(state, 26)), 0, state.xer_so);
	// bne cr6,0x82297abc
	if (!state.cr6.eq) goto loc_82297ABC;
	// subf r11,r24,r11
	R(state, 11) = R(state, 11) - R(state, 24);
loc_82297ABC:
	// cmpwi cr6,r11,5200
	CompareSigned(state.cr6, Signed32(R(state, 11)), 5200, state.xer_so);
	// bgt cr6,0x82297ee0
	if (state.cr6.gt) goto loc_82297EE0;
	// cmpwi cr6,r11,-5200
	CompareSigned(state.cr6, Signed32(R(state, 11)), -5200, state.xer_so);
	// blt cr6,0x82297ef8
	if (state.cr6.lt) goto loc_82297EF8;
	// lis r10,-31967
	R(state, 10) = -2094989312;
	// mr r24,r11
	R(state, 24) = R(state, 11);
	// addi r10,r10,23680
	R(state, 10) = R(state, 10) + 23680;
	// cmpwi cr6,r11,0
	CompareSigned(state.cr6, Signed32(R(state, 11)), 0, state.xer_so);
	// addi r25,r10,-96
	R(state, 25) = R(state, 10) + -96;
	// beq cr6,0x82297ea0
	if (state.cr6.eq) goto loc_82297EA0;
	// bge cr6,0x82297af8
	if (!state.cr6.lt) goto loc_82297AF8;
	// lis r10,-31967
	R(state, 10) = -2094989312;
	// neg r24,r11
	R(state, 24) = -R(state, 11);
	// addi r10,r10,24032
	R(state, 10) = R(state, 10) + 24032;
	// addi r25,r10,-96
	R(state, 25) = R(state, 10) + -96;
loc_82297AF8:
	// cmplwi cr6,r23,0
	CompareUnsigned(state.cr6, Low32(R(state, 23)), 0, state.xer_so);
	// bne cr6,0x82297b04
	if (!state.cr6.eq) goto loc_82297B04;
	// sth r21,106(r1)
	StoreU16(memory, Low32(R(state, 1)) + 106, Low16(R(state, 21)));
loc_82297B04:
	// cmpwi cr6,r24,0
	CompareSigned(state.cr6, Signed32(R(state, 24)), 0, state.xer_so);
	// beq cr6,0x82297ea0
	if (state.cr6.eq) goto loc_82297EA0;
	// lis r11,0
	R(state, 11) = 0;
	// lis r22,-32768
	R(state, 22) = -2147483648;
	// ori r26,r11,65535
	R(state, 26) = R(state, 11) | 65535;
	// lis r11,32767
	R(state, 11) = 2147418112;
	// li r23,-32768
	R(state, 23) = -32768;
	// ori r17,r11,32768
	R(state, 17) = R(state, 11) | 32768;
loc_82297B24:
	// clrlwi. r11,r24,29
	R(state, 11) = Low32(R(state, 24)) & 0x7;
	CompareSigned(state.cr0, Signed32(R(state, 11)), 0, state.xer_so);
	// addi r25,r25,84
	R(state, 25) = R(state, 25) + 84;
	// srawi r24,r24,3
	state.xer_ca = (Signed32(R(state, 24)) < 0) && ((Low32(R(state, 24)) & 0x7) != 0);
	R(state, 24) = ArithmeticShift32(Low32(R(state, 24)), 3);
	// beq 0x82297e98
	if (state.cr0.eq) goto loc_82297E98;
	// mulli r11,r11,12
	R(state, 11) = R(state, 11) * 12;
	// add r4,r11,r25
	R(state, 4) = R(state, 11) + R(state, 25);
	// lhz r11,10(r4)
	R(state, 11) = LoadU16(memory, Low32(R(state, 4)) + 10);
	// cmplwi cr6,r11,32768
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 32768, state.xer_so);
	// blt cr6,0x82297b64
	if (state.cr6.lt) goto loc_82297B64;
	// addi r3,r1,112
	R(state, 3) = R(state, 1) + 112;
	// li r5,12
	R(state, 5) = 12;
	// bl 0x82b7a0b0
	state.lr = 0x82297B54;
	CallCopy(memory, state);
	// lwz r11,118(r1)
	R(state, 11) = LoadU32(memory, Low32(R(state, 1)) + 118);
	// addi r4,r1,112
	R(state, 4) = R(state, 1) + 112;
	// addi r11,r11,-1
	R(state, 11) = R(state, 11) + -1;
	// stw r11,118(r1)
	StoreU32(memory, Low32(R(state, 1)) + 118, Low32(R(state, 11)));
loc_82297B64:
	// stw r21,88(r1)
	StoreU32(memory, Low32(R(state, 1)) + 88, Low32(R(state, 21)));
	// mr r28,r21
	R(state, 28) = R(state, 21);
	// stw r21,84(r1)
	StoreU32(memory, Low32(R(state, 1)) + 84, Low32(R(state, 21)));
	// stw r21,80(r1)
	StoreU32(memory, Low32(R(state, 1)) + 80, Low32(R(state, 21)));
	// lhz r10,0(r4)
	R(state, 10) = LoadU16(memory, Low32(R(state, 4)) + 0);
	// lhz r11,96(r1)
	R(state, 11) = LoadU16(memory, Low32(R(state, 1)) + 96);
	// mr r8,r10
	R(state, 8) = R(state, 10);
	// xor r10,r11,r10
	R(state, 10) = R(state, 11) ^ R(state, 10);
	// clrlwi r11,r11,17
	R(state, 11) = Low32(R(state, 11)) & 0x7FFF;
	// rlwinm r27,r10,0,16,16
	R(state, 27) = std::rotl(Low32(R(state, 10)) | (R(state, 10) << 32), 0) & 0x8000;
	// clrlwi r10,r8,17
	R(state, 10) = Low32(R(state, 8)) & 0x7FFF;
	// cmplwi cr6,r11,32767
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 32767, state.xer_so);
	// add r9,r11,r10
	R(state, 9) = R(state, 11) + R(state, 10);
	// clrlwi r29,r9,16
	R(state, 29) = Low32(R(state, 9)) & 0xFFFF;
	// bge cr6,0x82297e80
	if (!state.cr6.lt) goto loc_82297E80;
	// cmplwi cr6,r10,32767
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 32767, state.xer_so);
	// bge cr6,0x82297e80
	if (!state.cr6.lt) goto loc_82297E80;
	// clrlwi r9,r29,16
	R(state, 9) = Low32(R(state, 29)) & 0xFFFF;
	// cmplwi cr6,r9,49149
	CompareUnsigned(state.cr6, Low32(R(state, 9)), 49149, state.xer_so);
	// bgt cr6,0x82297e80
	if (state.cr6.gt) goto loc_82297E80;
	// cmplwi cr6,r9,16319
	CompareUnsigned(state.cr6, Low32(R(state, 9)), 16319, state.xer_so);
	// bgt cr6,0x82297bc4
	if (state.cr6.gt) goto loc_82297BC4;
loc_82297BBC:
	// stw r21,96(r1)
	StoreU32(memory, Low32(R(state, 1)) + 96, Low32(R(state, 21)));
	// b 0x82297e90
	goto loc_82297E90;
loc_82297BC4:
	// cmplwi cr6,r11,0
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 0, state.xer_so);
	// bne cr6,0x82297c00
	if (!state.cr6.eq) goto loc_82297C00;
	// addi r11,r9,1
	R(state, 11) = R(state, 9) + 1;
	// lwz r9,96(r1)
	R(state, 9) = LoadU32(memory, Low32(R(state, 1)) + 96);
	// clrlwi r29,r11,16
	R(state, 29) = Low32(R(state, 11)) & 0xFFFF;
	// clrlwi. r9,r9,1
	R(state, 9) = Low32(R(state, 9)) & 0x7FFFFFFF;
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// bne 0x82297c00
	if (!state.cr0.eq) goto loc_82297C00;
	// lwz r11,100(r1)
	R(state, 11) = LoadU32(memory, Low32(R(state, 1)) + 100);
	// cmplwi cr6,r11,0
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 0, state.xer_so);
	// bne cr6,0x82297c00
	if (!state.cr6.eq) goto loc_82297C00;
	// lwz r11,104(r1)
	R(state, 11) = LoadU32(memory, Low32(R(state, 1)) + 104);
	// cmplwi cr6,r11,0
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 0, state.xer_so);
	// bne cr6,0x82297c00
	if (!state.cr6.eq) goto loc_82297C00;
	// sth r21,96(r1)
	StoreU16(memory, Low32(R(state, 1)) + 96, Low16(R(state, 21)));
	// b 0x82297e98
	goto loc_82297E98;
loc_82297C00:
	// cmplwi cr6,r10,0
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 0, state.xer_so);
	// bne cr6,0x82297c38
	if (!state.cr6.eq) goto loc_82297C38;
	// lwz r10,0(r4)
	R(state, 10) = LoadU32(memory, Low32(R(state, 4)) + 0);
	// clrlwi r11,r29,16
	R(state, 11) = Low32(R(state, 29)) & 0xFFFF;
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// clrlwi r29,r11,16
	R(state, 29) = Low32(R(state, 11)) & 0xFFFF;
	// clrlwi. r10,r10,1
	R(state, 10) = Low32(R(state, 10)) & 0x7FFFFFFF;
	CompareSigned(state.cr0, Signed32(R(state, 10)), 0, state.xer_so);
	// bne 0x82297c38
	if (!state.cr0.eq) goto loc_82297C38;
	// lwz r11,4(r4)
	R(state, 11) = LoadU32(memory, Low32(R(state, 4)) + 4);
	// cmplwi cr6,r11,0
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 0, state.xer_so);
	// bne cr6,0x82297c38
	if (!state.cr6.eq) goto loc_82297C38;
	// lwz r11,8(r4)
	R(state, 11) = LoadU32(memory, Low32(R(state, 4)) + 8);
	// cmplwi cr6,r11,0
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 0, state.xer_so);
	// beq cr6,0x82297bbc
	if (state.cr6.eq) goto loc_82297BBC;
loc_82297C38:
	// mr r30,r21
	R(state, 30) = R(state, 21);
	// addi r8,r1,86
	R(state, 8) = R(state, 1) + 86;
	// li r3,5
	R(state, 3) = 5;
loc_82297C44:
	// rlwinm r11,r30,1,0,30
	R(state, 11) = std::rotl(Low32(R(state, 30)) | (R(state, 30) << 32), 1) & 0xFFFFFFFE;
	// mr r31,r3
	R(state, 31) = R(state, 3);
	// cmpwi cr6,r3,0
	CompareSigned(state.cr6, Signed32(R(state, 3)), 0, state.xer_so);
	// ble cr6,0x82297cb4
	if (!state.cr6.gt) goto loc_82297CB4;
	// addi r10,r1,106
	R(state, 10) = R(state, 1) + 106;
	// addi r5,r4,2
	R(state, 5) = R(state, 4) + 2;
	// subf r6,r11,r10
	R(state, 6) = R(state, 10) - R(state, 11);
loc_82297C60:
	// lhz r10,0(r5)
	R(state, 10) = LoadU16(memory, Low32(R(state, 5)) + 0);
	// mr r7,r21
	R(state, 7) = R(state, 21);
	// lhz r9,0(r6)
	R(state, 9) = LoadU16(memory, Low32(R(state, 6)) + 0);
	// lwz r11,2(r8)
	R(state, 11) = LoadU32(memory, Low32(R(state, 8)) + 2);
	// mullw r9,r10,r9
	R(state, 9) = int64_t(Signed32(R(state, 10))) * int64_t(Signed32(R(state, 9)));
	// add r10,r11,r9
	R(state, 10) = R(state, 11) + R(state, 9);
	// cmplw cr6,r10,r11
	CompareUnsigned(state.cr6, Low32(R(state, 10)), Low32(R(state, 11)), state.xer_so);
	// blt cr6,0x82297c88
	if (state.cr6.lt) goto loc_82297C88;
	// cmplw cr6,r10,r9
	CompareUnsigned(state.cr6, Low32(R(state, 10)), Low32(R(state, 9)), state.xer_so);
	// bge cr6,0x82297c8c
	if (!state.cr6.lt) goto loc_82297C8C;
loc_82297C88:
	// li r7,1
	R(state, 7) = 1;
loc_82297C8C:
	// cmpwi cr6,r7,0
	CompareSigned(state.cr6, Signed32(R(state, 7)), 0, state.xer_so);
	// stw r10,2(r8)
	StoreU32(memory, Low32(R(state, 8)) + 2, Low32(R(state, 10)));
	// beq cr6,0x82297ca4
	if (state.cr6.eq) goto loc_82297CA4;
	// lhz r11,0(r8)
	R(state, 11) = LoadU16(memory, Low32(R(state, 8)) + 0);
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// sth r11,0(r8)
	StoreU16(memory, Low32(R(state, 8)) + 0, Low16(R(state, 11)));
loc_82297CA4:
	// addic. r31,r31,-1
	state.xer_ca = Low32(R(state, 31)) > 0;
	R(state, 31) = R(state, 31) + -1;
	CompareSigned(state.cr0, Signed32(R(state, 31)), 0, state.xer_so);
	// addi r6,r6,-2
	R(state, 6) = R(state, 6) + -2;
	// addi r5,r5,2
	R(state, 5) = R(state, 5) + 2;
	// bgt 0x82297c60
	if (state.cr0.gt) goto loc_82297C60;
loc_82297CB4:
	// addic. r3,r3,-1
	state.xer_ca = Low32(R(state, 3)) > 0;
	R(state, 3) = R(state, 3) + -1;
	CompareSigned(state.cr0, Signed32(R(state, 3)), 0, state.xer_so);
	// addi r8,r8,-2
	R(state, 8) = R(state, 8) + -2;
	// addi r30,r30,1
	R(state, 30) = R(state, 30) + 1;
	// bgt 0x82297c44
	if (state.cr0.gt) goto loc_82297C44;
	// clrlwi r11,r29,16
	R(state, 11) = Low32(R(state, 29)) & 0xFFFF;
	// addis r11,r11,1
	R(state, 11) = R(state, 11) + 65536;
	// addi r11,r11,-16382
	R(state, 11) = R(state, 11) + -16382;
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// extsh. r10,r11
	R(state, 10) = Signed16(R(state, 11));
	CompareSigned(state.cr0, Signed32(R(state, 10)), 0, state.xer_so);
	// lwz r10,88(r1)
	R(state, 10) = LoadU32(memory, Low32(R(state, 1)) + 88);
	// ble 0x82297d2c
	if (!state.cr0.gt) goto loc_82297D2C;
loc_82297CE0:
	// lwz r6,80(r1)
	R(state, 6) = LoadU32(memory, Low32(R(state, 1)) + 80);
	// rlwinm. r9,r6,0,0,0
	R(state, 9) = std::rotl(Low32(R(state, 6)) | (R(state, 6) << 32), 0) & 0x80000000;
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// bne 0x82297d2c
	if (!state.cr0.eq) goto loc_82297D2C;
	// lwz r7,84(r1)
	R(state, 7) = LoadU32(memory, Low32(R(state, 1)) + 84);
	// rlwinm r9,r10,1,31,31
	R(state, 9) = std::rotl(Low32(R(state, 10)) | (R(state, 10) << 32), 1) & 0x1;
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// rlwinm r8,r7,1,31,31
	R(state, 8) = std::rotl(Low32(R(state, 7)) | (R(state, 7) << 32), 1) & 0x1;
	// rlwinm r7,r7,1,0,30
	R(state, 7) = std::rotl(Low32(R(state, 7)) | (R(state, 7) << 32), 1) & 0xFFFFFFFE;
	// add r11,r11,r26
	R(state, 11) = R(state, 11) + R(state, 26);
	// or r9,r7,r9
	R(state, 9) = R(state, 7) | R(state, 9);
	// rlwinm r6,r6,1,0,30
	R(state, 6) = std::rotl(Low32(R(state, 6)) | (R(state, 6) << 32), 1) & 0xFFFFFFFE;
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// rlwinm r10,r10,1,0,30
	R(state, 10) = std::rotl(Low32(R(state, 10)) | (R(state, 10) << 32), 1) & 0xFFFFFFFE;
	// stw r9,84(r1)
	StoreU32(memory, Low32(R(state, 1)) + 84, Low32(R(state, 9)));
	// or r9,r6,r8
	R(state, 9) = R(state, 6) | R(state, 8);
	// stw r10,88(r1)
	StoreU32(memory, Low32(R(state, 1)) + 88, Low32(R(state, 10)));
	// stw r9,80(r1)
	StoreU32(memory, Low32(R(state, 1)) + 80, Low32(R(state, 9)));
	// extsh. r9,r11
	R(state, 9) = Signed16(R(state, 11));
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// bgt 0x82297ce0
	if (state.cr0.gt) goto loc_82297CE0;
loc_82297D2C:
	// extsh. r9,r11
	R(state, 9) = Signed16(R(state, 11));
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// bgt 0x82297dcc
	if (state.cr0.gt) goto loc_82297DCC;
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// add r11,r11,r26
	R(state, 11) = R(state, 11) + R(state, 26);
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// extsh. r9,r11
	R(state, 9) = Signed16(R(state, 11));
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// bge 0x82297dcc
	if (!state.cr0.lt) goto loc_82297DCC;
	// lwz r7,84(r1)
	R(state, 7) = LoadU32(memory, Low32(R(state, 1)) + 84);
	// lwz r6,80(r1)
	R(state, 6) = LoadU32(memory, Low32(R(state, 1)) + 80);
loc_82297D50:
	// lhz r9,90(r1)
	R(state, 9) = LoadU16(memory, Low32(R(state, 1)) + 90);
	// clrlwi. r9,r9,31
	R(state, 9) = Low32(R(state, 9)) & 0x1;
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// beq 0x82297d60
	if (state.cr0.eq) goto loc_82297D60;
	// addi r28,r28,1
	R(state, 28) = R(state, 28) + 1;
loc_82297D60:
	// clrlwi. r9,r6,31
	R(state, 9) = Low32(R(state, 6)) & 0x1;
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// mr r8,r22
	R(state, 8) = R(state, 22);
	// bne 0x82297d70
	if (!state.cr0.eq) goto loc_82297D70;
	// mr r8,r21
	R(state, 8) = R(state, 21);
loc_82297D70:
	// clrlwi. r9,r7,31
	R(state, 9) = Low32(R(state, 7)) & 0x1;
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// mr r9,r22
	R(state, 9) = R(state, 22);
	// bne 0x82297d80
	if (!state.cr0.eq) goto loc_82297D80;
	// mr r9,r21
	R(state, 9) = R(state, 21);
loc_82297D80:
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// rlwinm r10,r10,31,1,31
	R(state, 10) = std::rotl(Low32(R(state, 10)) | (R(state, 10) << 32), 31) & 0x7FFFFFFF;
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// or r10,r10,r9
	R(state, 10) = R(state, 10) | R(state, 9);
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// rlwinm r7,r7,31,1,31
	R(state, 7) = std::rotl(Low32(R(state, 7)) | (R(state, 7) << 32), 31) & 0x7FFFFFFF;
	// rlwinm r6,r6,31,1,31
	R(state, 6) = std::rotl(Low32(R(state, 6)) | (R(state, 6) << 32), 31) & 0x7FFFFFFF;
	// or r7,r7,r8
	R(state, 7) = R(state, 7) | R(state, 8);
	// stw r10,88(r1)
	StoreU32(memory, Low32(R(state, 1)) + 88, Low32(R(state, 10)));
	// extsh. r9,r11
	R(state, 9) = Signed16(R(state, 11));
	CompareSigned(state.cr0, Signed32(R(state, 9)), 0, state.xer_so);
	// blt 0x82297d50
	if (state.cr0.lt) goto loc_82297D50;
	// stw r7,84(r1)
	StoreU32(memory, Low32(R(state, 1)) + 84, Low32(R(state, 7)));
	// cmpwi cr6,r28,0
	CompareSigned(state.cr6, Signed32(R(state, 28)), 0, state.xer_so);
	// stw r6,80(r1)
	StoreU32(memory, Low32(R(state, 1)) + 80, Low32(R(state, 6)));
	// beq cr6,0x82297dcc
	if (state.cr6.eq) goto loc_82297DCC;
	// lhz r10,90(r1)
	R(state, 10) = LoadU16(memory, Low32(R(state, 1)) + 90);
	// ori r10,r10,1
	R(state, 10) = R(state, 10) | 1;
	// sth r10,90(r1)
	StoreU16(memory, Low32(R(state, 1)) + 90, Low16(R(state, 10)));
	// lwz r10,88(r1)
	R(state, 10) = LoadU32(memory, Low32(R(state, 1)) + 88);
loc_82297DCC:
	// lhz r9,90(r1)
	R(state, 9) = LoadU16(memory, Low32(R(state, 1)) + 90);
	// cmplwi cr6,r9,32768
	CompareUnsigned(state.cr6, Low32(R(state, 9)), 32768, state.xer_so);
	// bgt cr6,0x82297dec
	if (state.cr6.gt) goto loc_82297DEC;
	// lis r9,1
	R(state, 9) = 65536;
	// clrlwi r10,r10,15
	R(state, 10) = Low32(R(state, 10)) & 0x1FFFF;
	// ori r9,r9,32768
	R(state, 9) = R(state, 9) | 32768;
	// cmplw cr6,r10,r9
	CompareUnsigned(state.cr6, Low32(R(state, 10)), Low32(R(state, 9)), state.xer_so);
	// bne cr6,0x82297e4c
	if (!state.cr6.eq) goto loc_82297E4C;
loc_82297DEC:
	// lwz r10,86(r1)
	R(state, 10) = LoadU32(memory, Low32(R(state, 1)) + 86);
	// cmpwi cr6,r10,-1
	CompareSigned(state.cr6, Signed32(R(state, 10)), -1, state.xer_so);
	// bne cr6,0x82297e44
	if (!state.cr6.eq) goto loc_82297E44;
	// lwz r10,82(r1)
	R(state, 10) = LoadU32(memory, Low32(R(state, 1)) + 82);
	// stw r21,86(r1)
	StoreU32(memory, Low32(R(state, 1)) + 86, Low32(R(state, 21)));
	// cmpwi cr6,r10,-1
	CompareSigned(state.cr6, Signed32(R(state, 10)), -1, state.xer_so);
	// bne cr6,0x82297e38
	if (!state.cr6.eq) goto loc_82297E38;
	// lhz r10,80(r1)
	R(state, 10) = LoadU16(memory, Low32(R(state, 1)) + 80);
	// stw r21,82(r1)
	StoreU32(memory, Low32(R(state, 1)) + 82, Low32(R(state, 21)));
	// cmplwi cr6,r10,65535
	CompareUnsigned(state.cr6, Low32(R(state, 10)), 65535, state.xer_so);
	// bne cr6,0x82297e2c
	if (!state.cr6.eq) goto loc_82297E2C;
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// sth r20,80(r1)
	StoreU16(memory, Low32(R(state, 1)) + 80, Low16(R(state, 20)));
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// b 0x82297e4c
	goto loc_82297E4C;
loc_82297E2C:
	// addi r10,r10,1
	R(state, 10) = R(state, 10) + 1;
	// sth r10,80(r1)
	StoreU16(memory, Low32(R(state, 1)) + 80, Low16(R(state, 10)));
	// b 0x82297e4c
	goto loc_82297E4C;
loc_82297E38:
	// addi r10,r10,1
	R(state, 10) = R(state, 10) + 1;
	// stw r10,82(r1)
	StoreU32(memory, Low32(R(state, 1)) + 82, Low32(R(state, 10)));
	// b 0x82297e4c
	goto loc_82297E4C;
loc_82297E44:
	// addi r10,r10,1
	R(state, 10) = R(state, 10) + 1;
	// stw r10,86(r1)
	StoreU32(memory, Low32(R(state, 1)) + 86, Low32(R(state, 10)));
loc_82297E4C:
	// clrlwi r11,r11,16
	R(state, 11) = Low32(R(state, 11)) & 0xFFFF;
	// cmplwi cr6,r11,32767
	CompareUnsigned(state.cr6, Low32(R(state, 11)), 32767, state.xer_so);
	// bge cr6,0x82297e80
	if (!state.cr6.lt) goto loc_82297E80;
	// clrlwi r10,r27,16
	R(state, 10) = Low32(R(state, 27)) & 0xFFFF;
	// lhz r9,88(r1)
	R(state, 9) = LoadU16(memory, Low32(R(state, 1)) + 88);
	// or r11,r10,r11
	R(state, 11) = R(state, 10) | R(state, 11);
	// lwz r10,84(r1)
	R(state, 10) = LoadU32(memory, Low32(R(state, 1)) + 84);
	// sth r9,106(r1)
	StoreU16(memory, Low32(R(state, 1)) + 106, Low16(R(state, 9)));
	// stw r10,102(r1)
	StoreU32(memory, Low32(R(state, 1)) + 102, Low32(R(state, 10)));
	// lwz r10,80(r1)
	R(state, 10) = LoadU32(memory, Low32(R(state, 1)) + 80);
	// sth r11,96(r1)
	StoreU16(memory, Low32(R(state, 1)) + 96, Low16(R(state, 11)));
	// stw r10,98(r1)
	StoreU32(memory, Low32(R(state, 1)) + 98, Low32(R(state, 10)));
	// b 0x82297e98
	goto loc_82297E98;
loc_82297E80:
	// stw r23,96(r1)
	StoreU32(memory, Low32(R(state, 1)) + 96, Low32(R(state, 23)));
	// clrlwi. r11,r27,16
	R(state, 11) = Low32(R(state, 27)) & 0xFFFF;
	CompareSigned(state.cr0, Signed32(R(state, 11)), 0, state.xer_so);
	// bne 0x82297e90
	if (!state.cr0.eq) goto loc_82297E90;
	// stw r17,96(r1)
	StoreU32(memory, Low32(R(state, 1)) + 96, Low32(R(state, 17)));
loc_82297E90:
	// stw r21,104(r1)
	StoreU32(memory, Low32(R(state, 1)) + 104, Low32(R(state, 21)));
	// stw r21,100(r1)
	StoreU32(memory, Low32(R(state, 1)) + 100, Low32(R(state, 21)));
loc_82297E98:
	// cmpwi cr6,r24,0
	CompareSigned(state.cr6, Signed32(R(state, 24)), 0, state.xer_so);
	// bne cr6,0x82297b24
	if (!state.cr6.eq) goto loc_82297B24;
loc_82297EA0:
	// lhz r11,106(r1)
	R(state, 11) = LoadU16(memory, Low32(R(state, 1)) + 106);
	// lwz r8,102(r1)
	R(state, 8) = LoadU32(memory, Low32(R(state, 1)) + 102);
	// lwz r9,98(r1)
	R(state, 9) = LoadU32(memory, Low32(R(state, 1)) + 98);
	// lhz r10,96(r1)
	R(state, 10) = LoadU16(memory, Low32(R(state, 1)) + 96);
	// b 0x82297f0c
	goto loc_82297F0C;
loc_82297EB4:
	// mr r11,r21
	R(state, 11) = R(state, 21);
	// mr r10,r21
	R(state, 10) = R(state, 21);
	// mr r9,r21
	R(state, 9) = R(state, 21);
	// mr r8,r21
	R(state, 8) = R(state, 21);
	// b 0x82297f0c
	goto loc_82297F0C;
loc_82297EC8:
	// mr r11,r21
	R(state, 11) = R(state, 21);
	// mr r10,r21
	R(state, 10) = R(state, 21);
	// mr r9,r21
	R(state, 9) = R(state, 21);
	// mr r8,r21
	R(state, 8) = R(state, 21);
	// li r21,4
	R(state, 21) = 4;
	// b 0x82297f0c
	goto loc_82297F0C;
loc_82297EE0:
	// mr r8,r21
	R(state, 8) = R(state, 21);
	// mr r11,r21
	R(state, 11) = R(state, 21);
	// li r10,32767
	R(state, 10) = 32767;
	// lis r9,-32768
	R(state, 9) = -2147483648;
	// li r21,2
	R(state, 21) = 2;
	// b 0x82297f0c
	goto loc_82297F0C;
loc_82297EF8:
	// mr r11,r21
	R(state, 11) = R(state, 21);
	// mr r10,r21
	R(state, 10) = R(state, 21);
	// mr r9,r21
	R(state, 9) = R(state, 21);
	// mr r8,r21
	R(state, 8) = R(state, 21);
	// li r21,1
	R(state, 21) = 1;
loc_82297F0C:
	// clrlwi r10,r10,16
	R(state, 10) = Low32(R(state, 10)) & 0xFFFF;
	// sth r11,10(r19)
	StoreU16(memory, Low32(R(state, 19)) + 10, Low16(R(state, 11)));
	// clrlwi r7,r18,16
	R(state, 7) = Low32(R(state, 18)) & 0xFFFF;
	// stw r8,6(r19)
	StoreU32(memory, Low32(R(state, 19)) + 6, Low32(R(state, 8)));
	// mr r3,r21
	R(state, 3) = R(state, 21);
	// stw r9,2(r19)
	StoreU32(memory, Low32(R(state, 19)) + 2, Low32(R(state, 9)));
	// or r10,r10,r7
	R(state, 10) = R(state, 10) | R(state, 7);
	// sth r10,0(r19)
	StoreU16(memory, Low32(R(state, 19)) + 0, Low16(R(state, 10)));
loc_82297F2C:
	// addi r1,r1,288
	R(state, 1) = R(state, 1) + 288;
	// b 0x82b7a70c
	RestoreRegs(memory, state);
	return;
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    PpcBoundaryServices& services, Registers& registers)
{
    if (entry != 0x822975b0u) return false;
    Parse(memory, services, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_float_text_parser

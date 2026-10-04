#include "lo_semantics/memory_copy_context.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::memory_copy_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
std::uint64_t& R(Registers& s, unsigned i)
{ return i==1u?s.integer.sp:s.integer.r[i]; }
std::uint8_t LowByte(std::uint64_t value)
{ return static_cast<std::uint8_t>(value); }
void Compare(const Registers& s,crt_stream_operations::Condition& c,std::uint64_t a,std::uint64_t b)
{ const auto x=Address(a),y=Address(b);c={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.integer.xer_so}; }
void Copy(GuestMemory& memory, Registers& state)
{
    std::uint64_t temporary = 0;
	// std r3,-8(r1)
	WriteU64(memory, Address(Address(R(state,1)) + -8), R(state,3));
	// clrlwi r6,r3,29
	R(state,6) = Address(R(state,3)) & 0x7;
	// dcbt r0,r4
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// subfic r6,r6,8
	state.integer.xer_ca = Address(R(state,6)) <= 8;
	R(state,6) = 8 - R(state,6);
	// beq 0x82b7a114
	if (state.integer.cr0.eq) goto loc_82B7A114;
	// cmplw r5,r6
	Compare(state, state.integer.cr0, Address(R(state,5)), Address(R(state,6)));
	// ble 0x82b7a130
	if (!state.integer.cr0.gt) goto loc_82B7A130;
	// cmplwi r6,4
	Compare(state, state.integer.cr0, Address(R(state,6)), 4);
	// beq 0x82b7a100
	if (state.integer.cr0.eq) goto loc_82B7A100;
	// addi r3,r3,-1
	R(state,3) = R(state,3) + -1;
	// addi r4,r4,-1
	R(state,4) = R(state,4) + -1;
	// subf r5,r6,r5
	R(state,5) = R(state,5) - R(state,6);
	// mtctr r6
	state.integer.ctr = R(state,6);
loc_82B7A0E8:
	// lbzu r6,1(r4)
	temporary = R(state,4) + std::uint64_t(1);
	R(state,6) = memory.ReadU8(Address(Address(temporary)));
	R(state,4) = temporary;
	// stbu r6,1(r3)
	temporary = R(state,3) + std::uint64_t(1);
	memory.WriteU8(Address(Address(temporary)), LowByte(R(state,6)));
	R(state,3) = temporary;
	// bdnz 0x82b7a0e8
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A0E8;
	// addi r3,r3,1
	R(state,3) = R(state,3) + 1;
	// addi r4,r4,1
	R(state,4) = R(state,4) + 1;
	// b 0x82b7a114
	goto loc_82B7A114;
loc_82B7A100:
	// subf r5,r6,r5
	R(state,5) = R(state,5) - R(state,6);
	// lwz r6,0(r4)
	R(state,6) = memory.ReadU32(Address(Address(R(state,4)) + 0));
	// addi r4,r4,4
	R(state,4) = R(state,4) + 4;
	// stw r6,0(r3)
	memory.WriteU32(Address(Address(R(state,3)) + 0), Address(R(state,6)));
	// addi r3,r3,4
	R(state,3) = R(state,3) + 4;
loc_82B7A114:
	// clrlwi r6,r4,29
	R(state,6) = Address(R(state,4)) & 0x7;
	// cmplwi cr6,r6,4
	Compare(state, state.integer.cr6, Address(R(state,6)), 4);
	// cmplwi cr1,r6,0
	Compare(state, state.cr1, Address(R(state,6)), 0);
	// cmplwi cr7,r5,128
	Compare(state, state.cr7, Address(R(state,5)), 128);
	// beq cr6,0x82b7a2f8
	if (state.integer.cr6.eq) goto loc_82B7A2F8;
	// bne cr1,0x82b7a428
	if (!state.cr1.eq) goto loc_82B7A428;
	// bge cr7,0x82b7a1cc
	if (!state.cr7.lt) goto loc_82B7A1CC;
loc_82B7A130:
	// dcbtst r0,r3
	// addi r4,r4,-8
	R(state,4) = R(state,4) + -8;
	// addi r3,r3,-8
	R(state,3) = R(state,3) + -8;
loc_82B7A13C:
	// rlwinm r7,r5,29,28,31
	R(state,7) = WordRotateMask(R(state,5), 29, 0xFull);
	// clrlwi r6,r5,29
	R(state,6) = Address(R(state,5)) & 0x7;
	// cmplwi cr1,r7,0
	Compare(state, state.cr1, Address(R(state,7)), 0);
	// cmplwi cr6,r6,0
	Compare(state, state.integer.cr6, Address(R(state,6)), 0);
	// beq cr1,0x82b7a160
	if (state.cr1.eq) goto loc_82B7A160;
	// mtctr r7
	state.integer.ctr = R(state,7);
loc_82B7A154:
	// ldu r7,8(r4)
	temporary = R(state,4) + std::uint64_t(8);
	R(state,7) = ReadU64(memory, Address(Address(temporary)));
	R(state,4) = temporary;
	// stdu r7,8(r3)
	temporary = R(state,3) + std::uint64_t(8);
	WriteU64(memory, Address(Address(temporary)), R(state,7));
	R(state,3) = temporary;
	// bdnz 0x82b7a154
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A154;
loc_82B7A160:
	// cmplwi cr1,r6,4
	Compare(state, state.cr1, Address(R(state,6)), 4);
	// beq cr6,0x82b7a184
	if (state.integer.cr6.eq) goto loc_82B7A184;
	// beq cr1,0x82b7a18c
	if (state.cr1.eq) goto loc_82B7A18C;
	// addi r3,r3,7
	R(state,3) = R(state,3) + 7;
	// addi r4,r4,7
	R(state,4) = R(state,4) + 7;
	// mtctr r6
	state.integer.ctr = R(state,6);
loc_82B7A178:
	// lbzu r7,1(r4)
	temporary = R(state,4) + std::uint64_t(1);
	R(state,7) = memory.ReadU8(Address(Address(temporary)));
	R(state,4) = temporary;
	// stbu r7,1(r3)
	temporary = R(state,3) + std::uint64_t(1);
	memory.WriteU8(Address(Address(temporary)), LowByte(R(state,7)));
	R(state,3) = temporary;
	// bdnz 0x82b7a178
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A178;
loc_82B7A184:
	// ld r3,-8(r1)
	R(state,3) = ReadU64(memory, Address(Address(R(state,1)) + -8));
	// blr
	return;
loc_82B7A18C:
	// clrlwi r6,r3,30
	R(state,6) = Address(R(state,3)) & 0x3;
	// lwz r5,8(r4)
	R(state,5) = memory.ReadU32(Address(Address(R(state,4)) + 8));
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// bne 0x82b7a1a8
	if (!state.integer.cr0.eq) goto loc_82B7A1A8;
	// stw r5,8(r3)
	memory.WriteU32(Address(Address(R(state,3)) + 8), Address(R(state,5)));
	// ld r3,-8(r1)
	R(state,3) = ReadU64(memory, Address(Address(R(state,1)) + -8));
	// blr
	return;
loc_82B7A1A8:
	// lbz r8,8(r4)
	R(state,8) = memory.ReadU8(Address(Address(R(state,4)) + 8));
	// lbz r7,9(r4)
	R(state,7) = memory.ReadU8(Address(Address(R(state,4)) + 9));
	// lbz r6,10(r4)
	R(state,6) = memory.ReadU8(Address(Address(R(state,4)) + 10));
	// stb r8,8(r3)
	memory.WriteU8(Address(Address(R(state,3)) + 8), LowByte(R(state,8)));
	// stb r7,9(r3)
	memory.WriteU8(Address(Address(R(state,3)) + 9), LowByte(R(state,7)));
	// stb r6,10(r3)
	memory.WriteU8(Address(Address(R(state,3)) + 10), LowByte(R(state,6)));
	// stb r5,11(r3)
	memory.WriteU8(Address(Address(R(state,3)) + 11), LowByte(R(state,5)));
	// ld r3,-8(r1)
	R(state,3) = ReadU64(memory, Address(Address(R(state,1)) + -8));
	// blr
	return;
loc_82B7A1CC:
	// clrlwi r6,r3,25
	R(state,6) = Address(R(state,3)) & 0x7F;
	// addi r3,r3,-8
	R(state,3) = R(state,3) + -8;
	// addi r4,r4,-8
	R(state,4) = R(state,4) + -8;
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// subfic r6,r6,128
	state.integer.xer_ca = Address(R(state,6)) <= 128;
	R(state,6) = 128 - R(state,6);
	// beq 0x82b7a1fc
	if (state.integer.cr0.eq) goto loc_82B7A1FC;
	// rlwinm r7,r6,29,3,31
	R(state,7) = WordRotateMask(R(state,6), 29, 0x1FFFFFFFull);
	// subf r5,r6,r5
	R(state,5) = R(state,5) - R(state,6);
	// mtctr r7
	state.integer.ctr = R(state,7);
loc_82B7A1F0:
	// ldu r7,8(r4)
	temporary = R(state,4) + std::uint64_t(8);
	R(state,7) = ReadU64(memory, Address(Address(temporary)));
	R(state,4) = temporary;
	// stdu r7,8(r3)
	temporary = R(state,3) + std::uint64_t(8);
	WriteU64(memory, Address(Address(temporary)), R(state,7));
	R(state,3) = temporary;
	// bdnz 0x82b7a1f0
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A1F0;
loc_82B7A1FC:
	// rlwinm r6,r5,25,7,31
	R(state,6) = WordRotateMask(R(state,5), 25, 0x1FFFFFFull);
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// beq 0x82b7a13c
	if (state.integer.cr0.eq) goto loc_82B7A13C;
	// addi r10,r5,127
	R(state,10) = R(state,5) + 127;
	// clrlwi r8,r5,25
	R(state,8) = Address(R(state,5)) & 0x7F;
	// rlwinm r10,r10,25,7,31
	R(state,10) = WordRotateMask(R(state,10), 25, 0x1FFFFFFull);
	// cmplwi cr1,r8,0
	Compare(state, state.cr1, Address(R(state,8)), 0);
	// addi r10,r10,-1
	R(state,10) = R(state,10) + -1;
	// clrlwi r10,r10,29
	R(state,10) = Address(R(state,10)) & 0x7;
	// addi r10,r10,1
	R(state,10) = R(state,10) + 1;
	// li r9,8
	R(state,9) = 8;
	// mtctr r10
	state.integer.ctr = R(state,10);
loc_82B7A22C:
	// dcbt r9,r4
	// addi r9,r9,128
	R(state,9) = R(state,9) + 128;
	// bdnz 0x82b7a22c
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A22C;
	// add r12,r4,r5
	R(state,12) = R(state,4) + R(state,5);
	// li r10,8
	R(state,10) = 8;
	// subf r11,r9,r12
	R(state,11) = R(state,12) - R(state,9);
	// add r12,r3,r5
	R(state,12) = R(state,3) + R(state,5);
	// mtctr r6
	state.integer.ctr = R(state,6);
loc_82B7A24C:
	// ld r6,8(r4)
	R(state,6) = ReadU64(memory, Address(Address(R(state,4)) + 8));
	// ld r7,16(r4)
	R(state,7) = ReadU64(memory, Address(Address(R(state,4)) + 16));
	// ld r8,24(r4)
	R(state,8) = ReadU64(memory, Address(Address(R(state,4)) + 24));
	// std r6,8(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 8), R(state,6));
	// ld r6,32(r4)
	R(state,6) = ReadU64(memory, Address(Address(R(state,4)) + 32));
	// std r7,16(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 16), R(state,7));
	// ld r7,40(r4)
	R(state,7) = ReadU64(memory, Address(Address(R(state,4)) + 40));
	// std r8,24(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 24), R(state,8));
	// ld r8,48(r4)
	R(state,8) = ReadU64(memory, Address(Address(R(state,4)) + 48));
	// std r6,32(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 32), R(state,6));
	// ld r6,56(r4)
	R(state,6) = ReadU64(memory, Address(Address(R(state,4)) + 56));
	// std r7,40(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 40), R(state,7));
	// ld r7,64(r4)
	R(state,7) = ReadU64(memory, Address(Address(R(state,4)) + 64));
	// std r8,48(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 48), R(state,8));
	// ld r8,72(r4)
	R(state,8) = ReadU64(memory, Address(Address(R(state,4)) + 72));
	// std r6,56(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 56), R(state,6));
	// ld r6,80(r4)
	R(state,6) = ReadU64(memory, Address(Address(R(state,4)) + 80));
	// std r7,64(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 64), R(state,7));
	// ld r7,88(r4)
	R(state,7) = ReadU64(memory, Address(Address(R(state,4)) + 88));
	// std r8,72(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 72), R(state,8));
	// ld r8,96(r4)
	R(state,8) = ReadU64(memory, Address(Address(R(state,4)) + 96));
	// std r6,80(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 80), R(state,6));
	// ld r6,104(r4)
	R(state,6) = ReadU64(memory, Address(Address(R(state,4)) + 104));
	// std r7,88(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 88), R(state,7));
	// ld r7,112(r4)
	R(state,7) = ReadU64(memory, Address(Address(R(state,4)) + 112));
	// std r8,96(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 96), R(state,8));
	// ld r8,120(r4)
	R(state,8) = ReadU64(memory, Address(Address(R(state,4)) + 120));
	// std r6,104(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 104), R(state,6));
	// ldu r6,128(r4)
	temporary = R(state,4) + std::uint64_t(128);
	R(state,6) = ReadU64(memory, Address(Address(temporary)));
	R(state,4) = temporary;
	// std r7,112(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 112), R(state,7));
	// std r8,120(r3)
	WriteU64(memory, Address(Address(R(state,3)) + 120), R(state,8));
	// stdu r6,128(r3)
	temporary = R(state,3) + std::uint64_t(128);
	WriteU64(memory, Address(Address(temporary)), R(state,6));
	R(state,3) = temporary;
	// cmplw r4,r11
	Compare(state, state.integer.cr0, Address(R(state,4)), Address(R(state,11)));
	// bge 0x82b7a2e0
	if (!state.integer.cr0.lt) goto loc_82B7A2E0;
	// dcbt r9,r4
	// bdnz 0x82b7a24c
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A24C;
	// b 0x82b7a13c
	goto loc_82B7A13C;
loc_82B7A2E0:
	// beq cr1,0x82b7a2f0
	if (state.cr1.eq) goto loc_82B7A2F0;
	// li r8,-1
	R(state,8) = std::uint64_t{0} - 1u;
	// dcbtst r8,r12
	// cmplwi cr1,r8,0
	Compare(state, state.cr1, Address(R(state,8)), 0);
loc_82B7A2F0:
	// bdnz 0x82b7a24c
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A24C;
	// b 0x82b7a13c
	goto loc_82B7A13C;
loc_82B7A2F8:
	// addi r4,r4,-4
	R(state,4) = R(state,4) + -4;
	// bge cr7,0x82b7a350
	if (!state.cr7.lt) goto loc_82B7A350;
	// dcbtst r0,r3
	// addi r3,r3,-4
	R(state,3) = R(state,3) + -4;
loc_82B7A308:
	// rlwinm r7,r5,30,27,31
	R(state,7) = WordRotateMask(R(state,5), 30, 0x1Full);
	// clrlwi r6,r5,30
	R(state,6) = Address(R(state,5)) & 0x3;
	// cmplwi cr1,r7,0
	Compare(state, state.cr1, Address(R(state,7)), 0);
	// cmplwi cr6,r6,0
	Compare(state, state.integer.cr6, Address(R(state,6)), 0);
	// beq cr1,0x82b7a32c
	if (state.cr1.eq) goto loc_82B7A32C;
	// mtctr r7
	state.integer.ctr = R(state,7);
loc_82B7A320:
	// lwzu r7,4(r4)
	temporary = R(state,4) + std::uint64_t(4);
	R(state,7) = memory.ReadU32(Address(Address(temporary)));
	R(state,4) = temporary;
	// stwu r7,4(r3)
	temporary = R(state,3) + std::uint64_t(4);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,7)));
	R(state,3) = temporary;
	// bdnz 0x82b7a320
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A320;
loc_82B7A32C:
	// beq cr6,0x82b7a348
	if (state.integer.cr6.eq) goto loc_82B7A348;
	// addi r3,r3,3
	R(state,3) = R(state,3) + 3;
	// addi r4,r4,3
	R(state,4) = R(state,4) + 3;
	// mtctr r6
	state.integer.ctr = R(state,6);
loc_82B7A33C:
	// lbzu r7,1(r4)
	temporary = R(state,4) + std::uint64_t(1);
	R(state,7) = memory.ReadU8(Address(Address(temporary)));
	R(state,4) = temporary;
	// stbu r7,1(r3)
	temporary = R(state,3) + std::uint64_t(1);
	memory.WriteU8(Address(Address(temporary)), LowByte(R(state,7)));
	R(state,3) = temporary;
	// bdnz 0x82b7a33c
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A33C;
loc_82B7A348:
	// ld r3,-8(r1)
	R(state,3) = ReadU64(memory, Address(Address(R(state,1)) + -8));
	// blr
	return;
loc_82B7A350:
	// clrlwi r6,r3,25
	R(state,6) = Address(R(state,3)) & 0x7F;
	// addi r3,r3,-4
	R(state,3) = R(state,3) + -4;
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// subfic r6,r6,128
	state.integer.xer_ca = Address(R(state,6)) <= 128;
	R(state,6) = 128 - R(state,6);
	// beq 0x82b7a37c
	if (state.integer.cr0.eq) goto loc_82B7A37C;
	// rlwinm r7,r6,30,2,31
	R(state,7) = WordRotateMask(R(state,6), 30, 0x3FFFFFFFull);
	// subf r5,r6,r5
	R(state,5) = R(state,5) - R(state,6);
	// mtctr r7
	state.integer.ctr = R(state,7);
loc_82B7A370:
	// lwzu r7,4(r4)
	temporary = R(state,4) + std::uint64_t(4);
	R(state,7) = memory.ReadU32(Address(Address(temporary)));
	R(state,4) = temporary;
	// stwu r7,4(r3)
	temporary = R(state,3) + std::uint64_t(4);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,7)));
	R(state,3) = temporary;
	// bdnz 0x82b7a370
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A370;
loc_82B7A37C:
	// rlwinm r6,r5,25,7,31
	R(state,6) = WordRotateMask(R(state,5), 25, 0x1FFFFFFull);
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// beq 0x82b7a308
	if (state.integer.cr0.eq) goto loc_82B7A308;
	// addi r10,r5,127
	R(state,10) = R(state,5) + 127;
	// clrlwi r8,r5,25
	R(state,8) = Address(R(state,5)) & 0x7F;
	// rlwinm r10,r10,25,7,31
	R(state,10) = WordRotateMask(R(state,10), 25, 0x1FFFFFFull);
	// cmplwi cr1,r8,0
	Compare(state, state.cr1, Address(R(state,8)), 0);
	// addi r10,r10,-1
	R(state,10) = R(state,10) + -1;
	// clrlwi r10,r10,29
	R(state,10) = Address(R(state,10)) & 0x7;
	// addi r10,r10,1
	R(state,10) = R(state,10) + 1;
	// li r9,4
	R(state,9) = 4;
	// mtctr r10
	state.integer.ctr = R(state,10);
loc_82B7A3AC:
	// dcbt r9,r4
	// addi r9,r9,128
	R(state,9) = R(state,9) + 128;
	// bdnz 0x82b7a3ac
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A3AC;
	// add r12,r4,r5
	R(state,12) = R(state,4) + R(state,5);
	// li r10,8
	R(state,10) = 8;
	// subf r11,r9,r12
	R(state,11) = R(state,12) - R(state,9);
	// add r12,r3,r5
	R(state,12) = R(state,3) + R(state,5);
	// mtctr r6
	state.integer.ctr = R(state,6);
loc_82B7A3CC:
	// li r6,8
	R(state,6) = 8;
loc_82B7A3D0:
	// addi r6,r6,-1
	R(state,6) = R(state,6) + -1;
	// lwz r0,4(r4)
	R(state,0) = memory.ReadU32(Address(Address(R(state,4)) + 4));
	// lwz r7,8(r4)
	R(state,7) = memory.ReadU32(Address(Address(R(state,4)) + 8));
	// lwz r8,12(r4)
	R(state,8) = memory.ReadU32(Address(Address(R(state,4)) + 12));
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// stw r0,4(r3)
	memory.WriteU32(Address(Address(R(state,3)) + 4), Address(R(state,0)));
	// lwzu r0,16(r4)
	temporary = R(state,4) + std::uint64_t(16);
	R(state,0) = memory.ReadU32(Address(Address(temporary)));
	R(state,4) = temporary;
	// stw r7,8(r3)
	memory.WriteU32(Address(Address(R(state,3)) + 8), Address(R(state,7)));
	// stw r8,12(r3)
	memory.WriteU32(Address(Address(R(state,3)) + 12), Address(R(state,8)));
	// stwu r0,16(r3)
	temporary = R(state,3) + std::uint64_t(16);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,0)));
	R(state,3) = temporary;
	// bne 0x82b7a3d0
	if (!state.integer.cr0.eq) goto loc_82B7A3D0;
	// cmplw r4,r11
	Compare(state, state.integer.cr0, Address(R(state,4)), Address(R(state,11)));
	// bge 0x82b7a410
	if (!state.integer.cr0.lt) goto loc_82B7A410;
	// dcbt r9,r4
	// bdnz 0x82b7a3cc
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A3CC;
	// b 0x82b7a308
	goto loc_82B7A308;
loc_82B7A410:
	// beq cr1,0x82b7a420
	if (state.cr1.eq) goto loc_82B7A420;
	// li r8,-1
	R(state,8) = std::uint64_t{0} - 1u;
	// dcbtst r8,r12
	// cmplwi cr1,r8,0
	Compare(state, state.cr1, Address(R(state,8)), 0);
loc_82B7A420:
	// bdnz 0x82b7a3cc
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A3CC;
	// b 0x82b7a308
	goto loc_82B7A308;
loc_82B7A428:
	// addi r4,r4,-1
	R(state,4) = R(state,4) + -1;
	// bge cr7,0x82b7a45c
	if (!state.cr7.lt) goto loc_82B7A45C;
	// dcbtst r0,r3
	// addi r3,r3,-1
	R(state,3) = R(state,3) + -1;
loc_82B7A438:
	// clrlwi r6,r5,25
	R(state,6) = Address(R(state,5)) & 0x7F;
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// mtctr r6
	state.integer.ctr = R(state,6);
	// beq 0x82b7a454
	if (state.integer.cr0.eq) goto loc_82B7A454;
loc_82B7A448:
	// lbzu r6,1(r4)
	temporary = R(state,4) + std::uint64_t(1);
	R(state,6) = memory.ReadU8(Address(Address(temporary)));
	R(state,4) = temporary;
	// stbu r6,1(r3)
	temporary = R(state,3) + std::uint64_t(1);
	memory.WriteU8(Address(Address(temporary)), LowByte(R(state,6)));
	R(state,3) = temporary;
	// bdnz 0x82b7a448
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A448;
loc_82B7A454:
	// ld r3,-8(r1)
	R(state,3) = ReadU64(memory, Address(Address(R(state,1)) + -8));
	// blr
	return;
loc_82B7A45C:
	// clrlwi r6,r3,25
	R(state,6) = Address(R(state,3)) & 0x7F;
	// addi r3,r3,-1
	R(state,3) = R(state,3) + -1;
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// subfic r6,r6,128
	state.integer.xer_ca = Address(R(state,6)) <= 128;
	R(state,6) = 128 - R(state,6);
	// beq 0x82b7a484
	if (state.integer.cr0.eq) goto loc_82B7A484;
	// subf r5,r6,r5
	R(state,5) = R(state,5) - R(state,6);
	// mtctr r6
	state.integer.ctr = R(state,6);
loc_82B7A478:
	// lbzu r6,1(r4)
	temporary = R(state,4) + std::uint64_t(1);
	R(state,6) = memory.ReadU8(Address(Address(temporary)));
	R(state,4) = temporary;
	// stbu r6,1(r3)
	temporary = R(state,3) + std::uint64_t(1);
	memory.WriteU8(Address(Address(temporary)), LowByte(R(state,6)));
	R(state,3) = temporary;
	// bdnz 0x82b7a478
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A478;
loc_82B7A484:
	// rlwinm r6,r5,25,7,31
	R(state,6) = WordRotateMask(R(state,5), 25, 0x1FFFFFFull);
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// beq 0x82b7a438
	if (state.integer.cr0.eq) goto loc_82B7A438;
	// addi r10,r5,127
	R(state,10) = R(state,5) + 127;
	// clrlwi r8,r5,25
	R(state,8) = Address(R(state,5)) & 0x7F;
	// rlwinm r10,r10,25,7,31
	R(state,10) = WordRotateMask(R(state,10), 25, 0x1FFFFFFull);
	// cmplwi cr1,r8,0
	Compare(state, state.cr1, Address(R(state,8)), 0);
	// addi r10,r10,-1
	R(state,10) = R(state,10) + -1;
	// clrlwi r10,r10,29
	R(state,10) = Address(R(state,10)) & 0x7;
	// addi r10,r10,1
	R(state,10) = R(state,10) + 1;
	// li r9,1
	R(state,9) = 1;
	// mtctr r10
	state.integer.ctr = R(state,10);
loc_82B7A4B4:
	// dcbt r9,r4
	// addi r9,r9,128
	R(state,9) = R(state,9) + 128;
	// bdnz 0x82b7a4b4
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A4B4;
	// add r12,r4,r5
	R(state,12) = R(state,4) + R(state,5);
	// li r10,1
	R(state,10) = 1;
	// subf r11,r9,r12
	R(state,11) = R(state,12) - R(state,9);
	// add r12,r3,r5
	R(state,12) = R(state,3) + R(state,5);
	// mtctr r6
	state.integer.ctr = R(state,6);
loc_82B7A4D4:
	// li r6,32
	R(state,6) = 32;
loc_82B7A4D8:
	// lbz r7,4(r4)
	R(state,7) = memory.ReadU8(Address(Address(R(state,4)) + 4));
	// lbz r8,3(r4)
	R(state,8) = memory.ReadU8(Address(Address(R(state,4)) + 3));
	// addi r6,r6,-1
	R(state,6) = R(state,6) + -1;
	// rlwimi r7,r8,8,16,23
	R(state,7) = (WordRotateMask(R(state,8), 8, 0xFF00ull)) | (R(state,7) & 0xFFFFFFFFFFFF00FF);
	// lbz r9,2(r4)
	R(state,9) = memory.ReadU8(Address(Address(R(state,4)) + 2));
	// cmplwi r6,0
	Compare(state, state.integer.cr0, Address(R(state,6)), 0);
	// rlwimi r7,r9,16,8,15
	R(state,7) = (WordRotateMask(R(state,9), 16, 0xFF0000ull)) | (R(state,7) & 0xFFFFFFFFFF00FFFF);
	// lbz r10,1(r4)
	R(state,10) = memory.ReadU8(Address(Address(R(state,4)) + 1));
	// addi r4,r4,4
	R(state,4) = R(state,4) + 4;
	// rlwimi r7,r10,24,0,7
	R(state,7) = (WordRotateMask(R(state,10), 24, 0xFF000000ull)) | (R(state,7) & 0xFFFFFFFF00FFFFFF);
	// stw r7,1(r3)
	memory.WriteU32(Address(Address(R(state,3)) + 1), Address(R(state,7)));
	// addi r3,r3,4
	R(state,3) = R(state,3) + 4;
	// bne 0x82b7a4d8
	if (!state.integer.cr0.eq) goto loc_82B7A4D8;
	// cmplw r4,r11
	Compare(state, state.integer.cr0, Address(R(state,4)), Address(R(state,11)));
	// bge 0x82b7a520
	if (!state.integer.cr0.lt) goto loc_82B7A520;
	// dcbt r9,r4
	// bdnz 0x82b7a4d4
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A4D4;
	// b 0x82b7a438
	goto loc_82B7A438;
loc_82B7A520:
	// beq cr1,0x82b7a530
	if (state.cr1.eq) goto loc_82B7A530;
	// li r8,-1
	R(state,8) = std::uint64_t{0} - 1u;
	// dcbtst r8,r12
	// cmplwi cr1,r8,0
	Compare(state, state.cr1, Address(R(state,8)), 0);
loc_82B7A530:
	// bdnz 0x82b7a4d4
	--state.integer.ctr;
	if (Address(state.integer.ctr) != 0) goto loc_82B7A4D4;
	// b 0x82b7a438
	goto loc_82B7A438;
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Registers& state)
{
    if(entry!=0x82b7a0b0u)return false;
    Copy(memory,state);return true;
}
}

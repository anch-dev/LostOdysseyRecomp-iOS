#include "lo_semantics/heap_allocation_context.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::heap_allocation_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using std::int32_t;
using std::uint32_t;
using std::uint64_t;

std::uint32_t Low32(std::uint64_t value)
{return static_cast<std::uint32_t>(value);}
std::uint16_t Low16(std::uint64_t value)
{return static_cast<std::uint16_t>(value);}
std::uint8_t Low8(std::uint64_t value)
{return static_cast<std::uint8_t>(value);}
std::int32_t Signed32(std::uint64_t value)
{return std::bit_cast<std::int32_t>(Low32(value));}
std::uint64_t Rotate64(std::uint64_t value,int shift)
{return std::rotl(value,shift);}
int LeadingZeroes(std::uint32_t value)
{return std::countl_zero(value);}

template<typename T>
void Compare(object_child_float::Condition& condition,T left,T right,
    std::uint8_t so)
{
    condition={std::uint8_t(left<right),std::uint8_t(left>right),
        std::uint8_t(left==right),so};
}

void Save22(GuestMemory& memory,Registers& state)
{
    for(unsigned i=22u;i<=31u;++i)
        WriteU64(memory,Address(state.r[1]-8u*(33u-i)),state.r[i]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore22(GuestMemory& memory,Registers& state)
{
    for(unsigned i=22u;i<=31u;++i)
        state.r[i]=ReadU64(memory,Address(state.r[1]-8u*(33u-i)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}

#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(memory,Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a),Low8(v))
#define PPC_STORE_U16(a,v) memory.WriteU16(Address(a),Low16(v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a),Low32(v))
#define PPC_STORE_U64(a,v) WriteU64(memory,Address(a),(v))

void Cleanup(GuestMemory& memory,BoundaryServices& services,Registers& state);

void Allocate(GuestMemory& memory,BoundaryServices& services,Registers& state) {
	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6d0
	state.lr = 0x823ACCB8;
	Save22(memory,state);
	// addi r31,r1,-320
	state.r[31] = state.r[1] + -320;
	// stwu r1,-320(r1)
	temp = state.r[1] + uint64_t(-320);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r30,r3
	state.r[30] = state.r[3];
	// mr r29,r4
	state.r[29] = state.r[4];
	// mr r25,r5
	state.r[25] = state.r[5];
	// lwz r11,20(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 20);
	// mr r27,r30
	state.r[27] = state.r[30];
	// stw r27,128(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 128, Low32(state.r[27]));
	// li r24,0
	state.r[24] = 0;
	// stw r24,100(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 100, Low32(state.r[24]));
	// rlwinm. r11,r11,0,13,13
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x40000;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// mr r22,r24
	state.r[22] = state.r[24];
	// stw r22,104(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 104, Low32(state.r[22]));
	// beq 0x823acd18
	if (state.cr0.eq) goto loc_823ACD18;
	// bl 0x830da07c
	state.lr = 0x823ACCF4;
	services.CallNative(0x830DA07Cu,memory,state);
	// lbz r11,379(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 379);
	// cmpw cr6,r11,r3
	Compare<int32_t>(state.cr6, Signed32(state.r[11]), Signed32(state.r[3]), state.xer_so);
	// beq cr6,0x823acd18
	if (state.cr6.eq) goto loc_823ACD18;
	// mr r7,r25
	state.r[7] = state.r[25];
	// li r6,1459
	state.r[6] = 1459;
	// lwz r5,312(r31)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[31]) + 312);
	// mr r4,r30
	state.r[4] = state.r[30];
	// li r3,244
	state.r[3] = 244;
	// bl 0x830da06c
	state.lr = 0x823ACD18;
	services.CallNative(0x830DA06Cu,memory,state);
loc_823ACD18:
	// lwz r11,24(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 24);
	// cmplwi cr6,r25,0
	Compare<uint32_t>(state.cr6, Low32(state.r[25]), 0, state.xer_so);
	// or r23,r11,r29
	state.r[23] = state.r[11] | state.r[29];
	// li r28,1
	state.r[28] = 1;
	// mr r11,r25
	state.r[11] = state.r[25];
	// bne cr6,0x823acd34
	if (!state.cr6.eq) goto loc_823ACD34;
	// mr r11,r28
	state.r[11] = state.r[28];
loc_823ACD34:
	// addi r11,r11,31
	state.r[11] = state.r[11] + 31;
	// rlwinm r4,r11,0,0,27
	state.r[4] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFFFFFF0;
	// stw r4,88(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 88, Low32(state.r[4]));
	// rlwinm r29,r4,28,4,31
	state.r[29] = Rotate64(Low32(state.r[4]) | (state.r[4] << 32), 28) & 0xFFFFFFF;
	// mr r8,r8
	state.r[8] = state.r[8];
	// clrlwi. r11,r23,31
	state.r[11] = Low32(state.r[23]) & 0x1;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// bne 0x823acd64
	if (!state.cr0.eq) goto loc_823ACD64;
	// lwz r3,1408(r30)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[30]) + 1408);
	// bl 0x830d9c6c
	state.lr = 0x823ACD58;
	services.CallNative(0x830D9C6Cu,memory,state);
	// mr r22,r28
	state.r[22] = state.r[28];
	// stw r22,104(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 104, Low32(state.r[22]));
	// lwz r4,88(r31)
	state.r[4] = PPC_LOAD_U32(Low32(state.r[31]) + 88);
loc_823ACD64:
	// cmplwi cr6,r29,128
	Compare<uint32_t>(state.cr6, Low32(state.r[29]), 128, state.xer_so);
	// bge cr6,0x823acf5c
	if (!state.cr6.lt) goto loc_823ACF5C;
	// addi r11,r29,48
	state.r[11] = state.r[29] + 48;
	// rlwinm r11,r11,3,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r30
	state.r[11] = state.r[11] + state.r[30];
	// lwz r10,0(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// beq cr6,0x823ace20
	if (state.cr6.eq) goto loc_823ACE20;
	// lwz r11,4(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r11,r11,-8
	state.r[11] = state.r[11] + -8;
	// stw r11,92(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 92, Low32(state.r[11]));
	// lbz r6,5(r11)
	state.r[6] = PPC_LOAD_U8(Low32(state.r[11]) + 5);
	// stb r6,80(r31)
	PPC_STORE_U8(Low32(state.r[31]) + 80, Low8(state.r[6]));
	// lwz r10,12(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 12);
	// addi r7,r11,8
	state.r[7] = state.r[11] + 8;
	// lwz r9,8(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 8);
	// lwz r8,0(r10)
	state.r[8] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// lwz r5,4(r9)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[9]) + 4);
	// cmplw cr6,r8,r5
	Compare<uint32_t>(state.cr6, Low32(state.r[8]), Low32(state.r[5]), state.xer_so);
	// bne cr6,0x823acdf0
	if (!state.cr6.eq) goto loc_823ACDF0;
	// cmplw cr6,r8,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[8]), Low32(state.r[7]), state.xer_so);
	// bne cr6,0x823acdf0
	if (!state.cr6.eq) goto loc_823ACDF0;
	// cmplw cr6,r9,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[10]), state.xer_so);
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// stw r10,4(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 4, Low32(state.r[10]));
	// bne cr6,0x823acdf0
	if (!state.cr6.eq) goto loc_823ACDF0;
	// lhz r10,0(r11)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// rlwinm r9,r10,27,5,31
	state.r[9] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 27) & 0x7FFFFFF;
	// clrlwi r10,r10,27
	state.r[10] = Low32(state.r[10]) & 0x1F;
	// slw r8,r28,r10
	state.r[8] = Low8(state.r[10]) & 0x20 ? 0 : (Low32(state.r[28]) << (Low8(state.r[10]) & 0x3F));
	// addi r10,r9,88
	state.r[10] = state.r[9] + 88;
	// rlwinm r10,r10,2,0,29
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r10,r30
	state.r[9] = PPC_LOAD_U32(Low32(state.r[10]) + Low32(state.r[30]));
	// xor r9,r8,r9
	state.r[9] = state.r[8] ^ state.r[9];
	// stwx r9,r10,r30
	PPC_STORE_U32(Low32(state.r[10]) + Low32(state.r[30]), Low32(state.r[9]));
loc_823ACDF0:
	// lwz r10,48(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 48);
	// subf r10,r29,r10
	state.r[10] = state.r[10] - state.r[29];
	// stw r10,48(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 48, Low32(state.r[10]));
	// mr r26,r11
	state.r[26] = state.r[11];
	// stw r26,124(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 124, Low32(state.r[26]));
	// rlwimi r6,r28,0,28,26
	state.r[6] = (Rotate64(Low32(state.r[28]) | (state.r[28] << 32), 0) & 0xFFFFFFFFFFFFFFEF) | (state.r[6] & 0x10);
	// stb r6,5(r11)
	PPC_STORE_U8(Low32(state.r[11]) + 5, Low8(state.r[6]));
	// lwz r10,88(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 88);
	// subf r10,r25,r10
	state.r[10] = state.r[10] - state.r[25];
	// stb r10,6(r11)
	PPC_STORE_U8(Low32(state.r[11]) + 6, Low8(state.r[10]));
	// stb r24,7(r11)
	PPC_STORE_U8(Low32(state.r[11]) + 7, Low8(state.r[24]));
	// b 0x823ad3c8
	goto loc_823AD3C8;
loc_823ACE20:
	// clrlwi r8,r29,27
	state.r[8] = Low32(state.r[29]) & 0x1F;
	// rlwinm r11,r29,27,5,31
	state.r[11] = Rotate64(Low32(state.r[29]) | (state.r[29] << 32), 27) & 0x7FFFFFF;
	// addi r10,r11,88
	state.r[10] = state.r[11] + 88;
	// rlwinm r10,r10,2,0,29
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 2) & 0xFFFFFFFC;
	// add r9,r10,r30
	state.r[9] = state.r[10] + state.r[30];
	// stw r9,96(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 96, Low32(state.r[9]));
	// lwz r7,0(r9)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[9]) + 0);
	// slw r10,r28,r8
	state.r[10] = Low8(state.r[8]) & 0x20 ? 0 : (Low32(state.r[28]) << (Low8(state.r[8]) & 0x3F));
	// addi r10,r10,-1
	state.r[10] = state.r[10] + -1;
	// andc r10,r7,r10
	state.r[10] = state.r[7] & ~state.r[10];
	// stw r10,108(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 108, Low32(state.r[10]));
	// addi r9,r9,4
	state.r[9] = state.r[9] + 4;
	// stw r9,96(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 96, Low32(state.r[9]));
	// cmplwi cr6,r11,1
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 1, state.xer_so);
	// blt cr6,0x823ace70
	if (state.cr6.lt) goto loc_823ACE70;
	// beq cr6,0x823ace90
	if (state.cr6.eq) goto loc_823ACE90;
	// cmplwi cr6,r11,3
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 3, state.xer_so);
	// blt cr6,0x823aceb0
	if (state.cr6.lt) goto loc_823ACEB0;
	// beq cr6,0x823aced0
	if (state.cr6.eq) goto loc_823ACED0;
	// b 0x823acf68
	goto loc_823ACF68;
loc_823ACE70:
	// cmplwi cr6,r10,0
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), 0, state.xer_so);
	// beq cr6,0x823ace80
	if (state.cr6.eq) goto loc_823ACE80;
	// addi r9,r30,384
	state.r[9] = state.r[30] + 384;
	// b 0x823acedc
	goto loc_823ACEDC;
loc_823ACE80:
	// lwz r10,0(r9)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[9]) + 0);
	// stw r10,108(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 108, Low32(state.r[10]));
	// addi r9,r9,4
	state.r[9] = state.r[9] + 4;
	// stw r9,96(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 96, Low32(state.r[9]));
loc_823ACE90:
	// cmplwi cr6,r10,0
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), 0, state.xer_so);
	// beq cr6,0x823acea0
	if (state.cr6.eq) goto loc_823ACEA0;
	// addi r9,r30,640
	state.r[9] = state.r[30] + 640;
	// b 0x823acedc
	goto loc_823ACEDC;
loc_823ACEA0:
	// lwz r10,0(r9)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[9]) + 0);
	// stw r10,108(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 108, Low32(state.r[10]));
	// addi r9,r9,4
	state.r[9] = state.r[9] + 4;
	// stw r9,96(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 96, Low32(state.r[9]));
loc_823ACEB0:
	// cmplwi cr6,r10,0
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), 0, state.xer_so);
	// beq cr6,0x823acec0
	if (state.cr6.eq) goto loc_823ACEC0;
	// addi r9,r30,896
	state.r[9] = state.r[30] + 896;
	// b 0x823acedc
	goto loc_823ACEDC;
loc_823ACEC0:
	// lwz r10,0(r9)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[9]) + 0);
	// stw r10,108(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 108, Low32(state.r[10]));
	// addi r11,r9,4
	state.r[11] = state.r[9] + 4;
	// stw r11,96(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 96, Low32(state.r[11]));
loc_823ACED0:
	// cmplwi cr6,r10,0
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), 0, state.xer_so);
	// beq cr6,0x823acf68
	if (state.cr6.eq) goto loc_823ACF68;
	// addi r9,r30,1152
	state.r[9] = state.r[30] + 1152;
loc_823ACEDC:
	// addi r11,r10,-1
	state.r[11] = state.r[10] + -1;
	// andc r11,r10,r11
	state.r[11] = state.r[10] & ~state.r[11];
	// cntlzw r11,r11
	state.r[11] = Low32(state.r[11]) == 0 ? 32 : LeadingZeroes(Low32(state.r[11]));
	// subfic r11,r11,31
	state.xer_ca = Low32(state.r[11]) <= 31;
	state.r[11] = 31 - state.r[11];
	// rlwinm r11,r11,3,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r9
	state.r[11] = state.r[11] + state.r[9];
	// lwz r11,4(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r3,r11,-8
	state.r[3] = state.r[11] + -8;
	// stw r3,92(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 92, Low32(state.r[3]));
	// lwz r11,12(r3)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[3]) + 12);
	// lwz r10,8(r3)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[3]) + 8);
	// addi r8,r3,8
	state.r[8] = state.r[3] + 8;
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// lwz r7,4(r10)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// cmplw cr6,r9,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[7]), state.xer_so);
	// bne cr6,0x823ad008
	if (!state.cr6.eq) goto loc_823AD008;
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// bne cr6,0x823ad008
	if (!state.cr6.eq) goto loc_823AD008;
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
	// stw r11,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[11]));
	// bne cr6,0x823ad008
	if (!state.cr6.eq) goto loc_823AD008;
	// lhz r11,0(r3)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[3]) + 0);
	// rlwinm r10,r11,27,5,31
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 27) & 0x7FFFFFF;
	// clrlwi r11,r11,27
	state.r[11] = Low32(state.r[11]) & 0x1F;
	// slw r9,r28,r11
	state.r[9] = Low8(state.r[11]) & 0x20 ? 0 : (Low32(state.r[28]) << (Low8(state.r[11]) & 0x3F));
	// addi r11,r10,88
	state.r[11] = state.r[10] + 88;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r10,r11,r30
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[30]));
	// xor r10,r10,r9
	state.r[10] = state.r[10] ^ state.r[9];
	// stwx r10,r11,r30
	PPC_STORE_U32(Low32(state.r[11]) + Low32(state.r[30]), Low32(state.r[10]));
	// b 0x823ad008
	goto loc_823AD008;
loc_823ACF5C:
	// lwz r11,28(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 28);
	// cmplw cr6,r29,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[29]), Low32(state.r[11]), state.xer_so);
	// bgt cr6,0x823ad404
	if (state.cr6.gt) goto loc_823AD404;
loc_823ACF68:
	// addi r10,r30,384
	state.r[10] = state.r[30] + 384;
	// lwz r11,4(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// stw r11,112(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 112, Low32(state.r[11]));
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// beq cr6,0x823acfc4
	if (state.cr6.eq) goto loc_823ACFC4;
	// addi r11,r11,-8
	state.r[11] = state.r[11] + -8;
	// stw r11,92(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 92, Low32(state.r[11]));
	// lhz r11,0(r11)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// cmplw cr6,r11,r29
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[29]), state.xer_so);
	// blt cr6,0x823acfc4
	if (state.cr6.lt) goto loc_823ACFC4;
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// stw r11,112(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 112, Low32(state.r[11]));
loc_823ACF98:
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// beq cr6,0x823acfc4
	if (state.cr6.eq) goto loc_823ACFC4;
	// addi r3,r11,-8
	state.r[3] = state.r[11] + -8;
	// stw r3,92(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 92, Low32(state.r[3]));
	// lhz r9,0(r3)
	state.r[9] = PPC_LOAD_U16(Low32(state.r[3]) + 0);
	// cmplw cr6,r9,r29
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[29]), state.xer_so);
	// bge cr6,0x823acfd8
	if (!state.cr6.lt) goto loc_823ACFD8;
	// lwz r11,0(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// stw r11,112(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 112, Low32(state.r[11]));
	// mr r8,r8
	state.r[8] = state.r[8];
	// b 0x823acf98
	goto loc_823ACF98;
loc_823ACFC4:
	// mr r3,r27
	state.r[3] = state.r[27];
	// bl 0x827cc428
	state.lr = 0x823ACFCC;
	services.CallDirect(0x827CC428u,memory,state);
	// stw r3,92(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 92, Low32(state.r[3]));
	// cmplwi r3,0
	Compare<uint32_t>(state.cr0, Low32(state.r[3]), 0, state.xer_so);
	// beq 0x823ad4d0
	if (state.cr0.eq) goto loc_823AD4D0;
loc_823ACFD8:
	// lwz r10,8(r3)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[3]) + 8);
	// lwz r11,12(r3)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[3]) + 12);
	// lwz r7,4(r10)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// cmplw cr6,r9,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[7]), state.xer_so);
	// addi r8,r3,8
	state.r[8] = state.r[3] + 8;
	// bne cr6,0x823ad004
	if (!state.cr6.eq) goto loc_823AD004;
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// bne cr6,0x823ad004
	if (!state.cr6.eq) goto loc_823AD004;
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
	// stw r11,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[11]));
loc_823AD004:
	// mr r8,r8
	state.r[8] = state.r[8];
loc_823AD008:
	// clrlwi r11,r29,16
	state.r[11] = Low32(state.r[29]) & 0xFFFF;
	// lbz r10,5(r3)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[3]) + 5);
	// stb r10,80(r31)
	PPC_STORE_U8(Low32(state.r[31]) + 80, Low8(state.r[10]));
	// lhz r9,0(r3)
	state.r[9] = PPC_LOAD_U16(Low32(state.r[3]) + 0);
	// lwz r8,48(r27)
	state.r[8] = PPC_LOAD_U32(Low32(state.r[27]) + 48);
	// subf r9,r9,r8
	state.r[9] = state.r[8] - state.r[9];
	// stw r9,48(r27)
	PPC_STORE_U32(Low32(state.r[27]) + 48, Low32(state.r[9]));
	// mr r26,r3
	state.r[26] = state.r[3];
	// stw r26,124(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 124, Low32(state.r[26]));
	// stb r28,5(r3)
	PPC_STORE_U8(Low32(state.r[3]) + 5, Low8(state.r[28]));
	// lhz r9,0(r3)
	state.r[9] = PPC_LOAD_U16(Low32(state.r[3]) + 0);
	// subf. r6,r29,r9
	state.r[6] = state.r[9] - state.r[29];
	Compare<int32_t>(state.cr0, Signed32(state.r[6]), 0, state.xer_so);
	// sth r11,0(r3)
	PPC_STORE_U16(Low32(state.r[3]) + 0, Low16(state.r[11]));
	// lwz r9,88(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 88);
	// subf r9,r25,r9
	state.r[9] = state.r[9] - state.r[25];
	// stb r9,6(r3)
	PPC_STORE_U8(Low32(state.r[3]) + 6, Low8(state.r[9]));
	// stb r24,7(r3)
	PPC_STORE_U8(Low32(state.r[3]) + 7, Low8(state.r[24]));
	// beq 0x823ad3b4
	if (state.cr0.eq) goto loc_823AD3B4;
	// cmplwi cr6,r6,1
	Compare<uint32_t>(state.cr6, Low32(state.r[6]), 1, state.xer_so);
	// bne cr6,0x823ad074
	if (!state.cr6.eq) goto loc_823AD074;
	// lhz r11,0(r3)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[3]) + 0);
	// addi r11,r11,1
	state.r[11] = state.r[11] + 1;
	// sth r11,0(r3)
	PPC_STORE_U16(Low32(state.r[3]) + 0, Low16(state.r[11]));
	// lbz r11,6(r3)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[3]) + 6);
	// addi r11,r11,16
	state.r[11] = state.r[11] + 16;
	// stb r11,6(r3)
	PPC_STORE_U8(Low32(state.r[3]) + 6, Low8(state.r[11]));
	// b 0x823ad3b4
	goto loc_823AD3B4;
loc_823AD074:
	// rlwinm r9,r29,4,0,27
	state.r[9] = Rotate64(Low32(state.r[29]) | (state.r[29] << 32), 4) & 0xFFFFFFF0;
	// add r30,r9,r3
	state.r[30] = state.r[9] + state.r[3];
	// stb r10,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[10]));
	// sth r11,2(r30)
	PPC_STORE_U16(Low32(state.r[30]) + 2, Low16(state.r[11]));
	// lbz r11,4(r3)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[3]) + 4);
	// stb r11,4(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 4, Low8(state.r[11]));
	// clrlwi r9,r6,16
	state.r[9] = Low32(state.r[6]) & 0xFFFF;
	// sth r9,0(r30)
	PPC_STORE_U16(Low32(state.r[30]) + 0, Low16(state.r[9]));
	// rlwinm. r11,r10,0,27,27
	state.r[11] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x823ad128
	if (state.cr0.eq) goto loc_823AD128;
	// clrlwi r9,r9,16
	state.r[9] = Low32(state.r[9]) & 0xFFFF;
	// cmplwi cr6,r9,128
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), 128, state.xer_so);
	// lbz r11,5(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// bge cr6,0x823ad0f0
	if (!state.cr6.lt) goto loc_823AD0F0;
	// addi r10,r9,48
	state.r[10] = state.r[9] + 48;
	// rlwinm r9,r11,0,27,27
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	// rlwinm r11,r10,3,0,28
	state.r[11] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 3) & 0xFFFFFFF8;
	// stb r9,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[9]));
	// add r10,r11,r27
	state.r[10] = state.r[11] + state.r[27];
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// cmplw cr6,r11,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[10]), state.xer_so);
	// bne cr6,0x823ad194
	if (!state.cr6.eq) goto loc_823AD194;
	// lhz r11,0(r30)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// rlwinm r9,r11,27,5,31
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 27) & 0x7FFFFFF;
	// clrlwi r11,r11,27
	state.r[11] = Low32(state.r[11]) & 0x1F;
	// slw r8,r28,r11
	state.r[8] = Low8(state.r[11]) & 0x20 ? 0 : (Low32(state.r[28]) << (Low8(state.r[11]) & 0x3F));
	// addi r11,r9,88
	state.r[11] = state.r[9] + 88;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r11,r27
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[27]));
	// or r9,r8,r9
	state.r[9] = state.r[8] | state.r[9];
	// b 0x823ad190
	goto loc_823AD190;
loc_823AD0F0:
	// rlwinm r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	// stb r11,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[11]));
	// addi r10,r27,384
	state.r[10] = state.r[27] + 384;
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// stw r11,116(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 116, Low32(state.r[11]));
loc_823AD104:
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// beq cr6,0x823ad1e8
	if (state.cr6.eq) goto loc_823AD1E8;
	// lhz r8,-8(r11)
	state.r[8] = PPC_LOAD_U16(Low32(state.r[11]) + -8);
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// ble cr6,0x823ad1e8
	if (!state.cr6.gt) goto loc_823AD1E8;
	// lwz r11,0(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// stw r11,116(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 116, Low32(state.r[11]));
	// mr r8,r8
	state.r[8] = state.r[8];
	// b 0x823ad104
	goto loc_823AD104;
loc_823AD128:
	// rlwinm r11,r6,4,0,27
	state.r[11] = Rotate64(Low32(state.r[6]) | (state.r[6] << 32), 4) & 0xFFFFFFF0;
	// add r11,r11,r30
	state.r[11] = state.r[11] + state.r[30];
	// lbz r10,5(r11)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[11]) + 5);
	// clrlwi. r8,r10,31
	state.r[8] = Low32(state.r[10]) & 0x1;
	Compare<int32_t>(state.cr0, Signed32(state.r[8]), 0, state.xer_so);
	// beq 0x823ad20c
	if (state.cr0.eq) goto loc_823AD20C;
	// clrlwi r8,r9,16
	state.r[8] = Low32(state.r[9]) & 0xFFFF;
	// sth r9,2(r11)
	PPC_STORE_U16(Low32(state.r[11]) + 2, Low16(state.r[9]));
	// cmplwi cr6,r8,128
	Compare<uint32_t>(state.cr6, Low32(state.r[8]), 128, state.xer_so);
	// lbz r11,5(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// bge cr6,0x823ad1b0
	if (!state.cr6.lt) goto loc_823AD1B0;
	// addi r10,r8,48
	state.r[10] = state.r[8] + 48;
	// rlwinm r9,r11,0,27,27
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	// rlwinm r11,r10,3,0,28
	state.r[11] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 3) & 0xFFFFFFF8;
	// stb r9,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[9]));
	// add r10,r11,r27
	state.r[10] = state.r[11] + state.r[27];
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// cmplw cr6,r11,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[10]), state.xer_so);
	// bne cr6,0x823ad194
	if (!state.cr6.eq) goto loc_823AD194;
	// lhz r11,0(r30)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// rlwinm r9,r11,27,5,31
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 27) & 0x7FFFFFF;
	// clrlwi r11,r11,27
	state.r[11] = Low32(state.r[11]) & 0x1F;
	// slw r8,r28,r11
	state.r[8] = Low8(state.r[11]) & 0x20 ? 0 : (Low32(state.r[28]) << (Low8(state.r[11]) & 0x3F));
	// addi r11,r9,88
	state.r[11] = state.r[9] + 88;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r11,r27
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[27]));
	// or r9,r9,r8
	state.r[9] = state.r[9] | state.r[8];
loc_823AD190:
	// stwx r9,r11,r27
	PPC_STORE_U32(Low32(state.r[11]) + Low32(state.r[27]), Low32(state.r[9]));
loc_823AD194:
	// lwz r11,4(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// addi r9,r30,8
	state.r[9] = state.r[30] + 8;
	// stw r10,8(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 8, Low32(state.r[10]));
	// stw r11,12(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 12, Low32(state.r[11]));
	// stw r9,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[9]));
	// stw r9,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[9]));
	// b 0x823ad200
	goto loc_823AD200;
loc_823AD1B0:
	// rlwinm r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	// stb r11,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[11]));
	// addi r10,r27,384
	state.r[10] = state.r[27] + 384;
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// stw r11,132(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 132, Low32(state.r[11]));
loc_823AD1C4:
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// beq cr6,0x823ad1e8
	if (state.cr6.eq) goto loc_823AD1E8;
	// lhz r9,-8(r11)
	state.r[9] = PPC_LOAD_U16(Low32(state.r[11]) + -8);
	// cmplw cr6,r8,r9
	Compare<uint32_t>(state.cr6, Low32(state.r[8]), Low32(state.r[9]), state.xer_so);
	// ble cr6,0x823ad1e8
	if (!state.cr6.gt) goto loc_823AD1E8;
	// lwz r11,0(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// stw r11,132(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 132, Low32(state.r[11]));
	// mr r8,r8
	state.r[8] = state.r[8];
	// b 0x823ad1c4
	goto loc_823AD1C4;
loc_823AD1E8:
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r9,r30,8
	state.r[9] = state.r[30] + 8;
	// stw r11,8(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 8, Low32(state.r[11]));
	// stw r10,12(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 12, Low32(state.r[10]));
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// stw r9,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[9]));
loc_823AD200:
	// lwz r11,48(r27)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[27]) + 48);
	// add r11,r6,r11
	state.r[11] = state.r[6] + state.r[11];
	// b 0x823ad378
	goto loc_823AD378;
loc_823AD20C:
	// stb r10,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[10]));
	// addi r7,r11,8
	state.r[7] = state.r[11] + 8;
	// lwz r10,12(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 12);
	// lwz r9,8(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 8);
	// lwz r8,0(r10)
	state.r[8] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// lwz r5,4(r9)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[9]) + 4);
	// cmplw cr6,r8,r5
	Compare<uint32_t>(state.cr6, Low32(state.r[8]), Low32(state.r[5]), state.xer_so);
	// bne cr6,0x823ad270
	if (!state.cr6.eq) goto loc_823AD270;
	// cmplw cr6,r8,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[8]), Low32(state.r[7]), state.xer_so);
	// bne cr6,0x823ad270
	if (!state.cr6.eq) goto loc_823AD270;
	// cmplw cr6,r9,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[10]), state.xer_so);
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// stw r10,4(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 4, Low32(state.r[10]));
	// bne cr6,0x823ad270
	if (!state.cr6.eq) goto loc_823AD270;
	// lhz r10,0(r11)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// cmplwi cr6,r10,128
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), 128, state.xer_so);
	// bge cr6,0x823ad270
	if (!state.cr6.lt) goto loc_823AD270;
	// rlwinm r9,r10,27,5,31
	state.r[9] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 27) & 0x7FFFFFF;
	// clrlwi r10,r10,27
	state.r[10] = Low32(state.r[10]) & 0x1F;
	// slw r8,r28,r10
	state.r[8] = Low8(state.r[10]) & 0x20 ? 0 : (Low32(state.r[28]) << (Low8(state.r[10]) & 0x3F));
	// addi r10,r9,88
	state.r[10] = state.r[9] + 88;
	// rlwinm r10,r10,2,0,29
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r10,r27
	state.r[9] = PPC_LOAD_U32(Low32(state.r[10]) + Low32(state.r[27]));
	// xor r9,r8,r9
	state.r[9] = state.r[8] ^ state.r[9];
	// stwx r9,r10,r27
	PPC_STORE_U32(Low32(state.r[10]) + Low32(state.r[27]), Low32(state.r[9]));
loc_823AD270:
	// lhz r10,0(r11)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// lwz r9,48(r27)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[27]) + 48);
	// subf r10,r10,r9
	state.r[10] = state.r[9] - state.r[10];
	// stw r10,48(r27)
	PPC_STORE_U32(Low32(state.r[27]) + 48, Low32(state.r[10]));
	// lhz r11,0(r11)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// add r5,r11,r6
	state.r[5] = state.r[11] + state.r[6];
	// cmplwi cr6,r5,61440
	Compare<uint32_t>(state.cr6, Low32(state.r[5]), 61440, state.xer_so);
	// bgt cr6,0x823ad380
	if (state.cr6.gt) goto loc_823AD380;
	// clrlwi r11,r5,16
	state.r[11] = Low32(state.r[5]) & 0xFFFF;
	// sth r11,0(r30)
	PPC_STORE_U16(Low32(state.r[30]) + 0, Low16(state.r[11]));
	// lbz r10,5(r30)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// rlwinm. r10,r10,0,27,27
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// bne 0x823ad2b0
	if (!state.cr0.eq) goto loc_823AD2B0;
	// rlwinm r10,r5,4,0,27
	state.r[10] = Rotate64(Low32(state.r[5]) | (state.r[5] << 32), 4) & 0xFFFFFFF0;
	// add r10,r10,r30
	state.r[10] = state.r[10] + state.r[30];
	// sth r11,2(r10)
	PPC_STORE_U16(Low32(state.r[10]) + 2, Low16(state.r[11]));
loc_823AD2B0:
	// clrlwi r9,r11,16
	state.r[9] = Low32(state.r[11]) & 0xFFFF;
	// cmplwi cr6,r9,128
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), 128, state.xer_so);
	// lbz r11,5(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// bge cr6,0x823ad320
	if (!state.cr6.lt) goto loc_823AD320;
	// addi r10,r9,48
	state.r[10] = state.r[9] + 48;
	// rlwinm r9,r11,0,27,27
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	// rlwinm r11,r10,3,0,28
	state.r[11] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 3) & 0xFFFFFFF8;
	// stb r9,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[9]));
	// add r10,r11,r27
	state.r[10] = state.r[11] + state.r[27];
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// cmplw cr6,r11,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[10]), state.xer_so);
	// bne cr6,0x823ad304
	if (!state.cr6.eq) goto loc_823AD304;
	// lhz r11,0(r30)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// rlwinm r9,r11,27,5,31
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 27) & 0x7FFFFFF;
	// clrlwi r11,r11,27
	state.r[11] = Low32(state.r[11]) & 0x1F;
	// slw r8,r28,r11
	state.r[8] = Low8(state.r[11]) & 0x20 ? 0 : (Low32(state.r[28]) << (Low8(state.r[11]) & 0x3F));
	// addi r11,r9,88
	state.r[11] = state.r[9] + 88;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r11,r27
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[27]));
	// or r9,r9,r8
	state.r[9] = state.r[9] | state.r[8];
	// stwx r9,r11,r27
	PPC_STORE_U32(Low32(state.r[11]) + Low32(state.r[27]), Low32(state.r[9]));
loc_823AD304:
	// lwz r11,4(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// addi r9,r30,8
	state.r[9] = state.r[30] + 8;
	// stw r10,8(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 8, Low32(state.r[10]));
	// stw r11,12(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 12, Low32(state.r[11]));
	// stw r9,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[9]));
	// stw r9,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[9]));
	// b 0x823ad370
	goto loc_823AD370;
loc_823AD320:
	// rlwinm r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	// stb r11,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[11]));
	// addi r10,r27,384
	state.r[10] = state.r[27] + 384;
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// stw r11,120(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 120, Low32(state.r[11]));
loc_823AD334:
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// beq cr6,0x823ad358
	if (state.cr6.eq) goto loc_823AD358;
	// lhz r8,-8(r11)
	state.r[8] = PPC_LOAD_U16(Low32(state.r[11]) + -8);
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// ble cr6,0x823ad358
	if (!state.cr6.gt) goto loc_823AD358;
	// lwz r11,0(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// stw r11,120(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 120, Low32(state.r[11]));
	// mr r8,r8
	state.r[8] = state.r[8];
	// b 0x823ad334
	goto loc_823AD334;
loc_823AD358:
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r9,r30,8
	state.r[9] = state.r[30] + 8;
	// stw r11,8(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 8, Low32(state.r[11]));
	// stw r10,12(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 12, Low32(state.r[10]));
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// stw r9,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[9]));
loc_823AD370:
	// lwz r11,48(r27)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[27]) + 48);
	// add r11,r5,r11
	state.r[11] = state.r[5] + state.r[11];
loc_823AD378:
	// stw r11,48(r27)
	PPC_STORE_U32(Low32(state.r[27]) + 48, Low32(state.r[11]));
	// b 0x823ad38c
	goto loc_823AD38C;
loc_823AD380:
	// mr r4,r30
	state.r[4] = state.r[30];
	// mr r3,r27
	state.r[3] = state.r[27];
	// bl 0x827cba60
	state.lr = 0x823AD38C;
	services.CallDirect(0x827CBA60u,memory,state);
loc_823AD38C:
	// mr r10,r24
	state.r[10] = state.r[24];
	// stb r10,80(r31)
	PPC_STORE_U8(Low32(state.r[31]) + 80, Low8(state.r[10]));
	// lbz r11,5(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// rlwinm. r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x823ad3b4
	if (state.cr0.eq) goto loc_823AD3B4;
	// lbz r11,4(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 4);
	// addi r11,r11,24
	state.r[11] = state.r[11] + 24;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r27
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[27]));
	// stw r30,64(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 64, Low32(state.r[30]));
loc_823AD3B4:
	// rlwinm. r11,r10,0,27,27
	state.r[11] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x823ad3c8
	if (state.cr0.eq) goto loc_823AD3C8;
	// lbz r11,5(r26)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[26]) + 5);
	// ori r11,r11,16
	state.r[11] = state.r[11] | 16;
	// stb r11,5(r26)
	PPC_STORE_U8(Low32(state.r[26]) + 5, Low8(state.r[11]));
loc_823AD3C8:
	// addi r30,r26,16
	state.r[30] = state.r[26] + 16;
	// stw r30,100(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 100, Low32(state.r[30]));
	// cmplwi cr6,r22,0
	Compare<uint32_t>(state.cr6, Low32(state.r[22]), 0, state.xer_so);
	// beq cr6,0x823ad3e8
	if (state.cr6.eq) goto loc_823AD3E8;
	// lwz r3,1408(r27)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[27]) + 1408);
	// bl 0x830d9c7c
	state.lr = 0x823AD3E0;
	services.CallNative(0x830D9C7Cu,memory,state);
	// mr r22,r24
	state.r[22] = state.r[24];
	// stw r22,104(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 104, Low32(state.r[22]));
loc_823AD3E8:
	// rlwinm. r11,r23,0,28,28
	state.r[11] = Rotate64(Low32(state.r[23]) | (state.r[23] << 32), 0) & 0x8;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x823ad504
	if (state.cr0.eq) goto loc_823AD504;
	// mr r5,r25
	state.r[5] = state.r[25];
	// li r4,0
	state.r[4] = 0;
	// mr r3,r30
	state.r[3] = state.r[30];
	// bl 0x82b7bc40
	state.lr = 0x823AD400;
	services.CallDirect(0x82B7BC40u,memory,state);
	// b 0x823ad504
	goto loc_823AD504;
loc_823AD404:
	// lwz r11,20(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 20);
	// rlwinm. r11,r11,0,30,30
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x2;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x823ad4d4
	if (state.cr0.eq) goto loc_823AD4D4;
	// rlwinm. r11,r23,0,28,28
	state.r[11] = Rotate64(Low32(state.r[23]) | (state.r[23] << 32), 0) & 0x8;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// stw r24,84(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 84, Low32(state.r[24]));
	// addi r10,r4,32
	state.r[10] = state.r[4] + 32;
	// stw r10,88(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 88, Low32(state.r[10]));
	// mr r11,r24
	state.r[11] = state.r[24];
	// bne 0x823ad42c
	if (!state.cr0.eq) goto loc_823AD42C;
	// lis r11,128
	state.r[11] = 8388608;
loc_823AD42C:
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// oris r5,r11,24576
	state.r[5] = state.r[11] | 1610612736;
	// ori r5,r5,4096
	state.r[5] = state.r[5] | 4096;
	// addi r4,r31,88
	state.r[4] = state.r[31] + 88;
	// addi r3,r31,84
	state.r[3] = state.r[31] + 84;
	// bl 0x830d9d1c
	state.lr = 0x823AD448;
	services.CallNative(0x830D9D1Cu,memory,state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// blt 0x823ad4d0
	if (state.cr0.lt) goto loc_823AD4D0;
	// li r5,48
	state.r[5] = 48;
	// li r4,0
	state.r[4] = 0;
	// lwz r3,84(r31)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// bl 0x82b7bc40
	state.lr = 0x823AD460;
	services.CallDirect(0x82B7BC40u,memory,state);
	// addi r11,r30,88
	state.r[11] = state.r[30] + 88;
	// lwz r10,88(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 88);
	// subf r10,r25,r10
	state.r[10] = state.r[10] - state.r[25];
	// li r9,11
	state.r[9] = 11;
	// addis r10,r10,1
	state.r[10] = state.r[10] + 65536;
	// addi r10,r10,-48
	state.r[10] = state.r[10] + -48;
	// lwz r8,84(r31)
	state.r[8] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// sth r10,32(r8)
	PPC_STORE_U16(Low32(state.r[8]) + 32, Low16(state.r[10]));
	// lwz r10,84(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// stb r9,37(r10)
	PPC_STORE_U8(Low32(state.r[10]) + 37, Low8(state.r[9]));
	// lwz r10,88(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 88);
	// lwz r9,84(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// stw r10,24(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 24, Low32(state.r[10]));
	// lwz r10,88(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 88);
	// lwz r9,84(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// stw r10,28(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 28, Low32(state.r[10]));
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// lwz r9,84(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// stw r11,0(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 0, Low32(state.r[11]));
	// lwz r9,84(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// stw r10,4(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 4, Low32(state.r[10]));
	// lwz r9,84(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// lwz r10,84(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// stw r10,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[10]));
	// addi r11,r10,48
	state.r[11] = state.r[10] + 48;
	// stw r11,100(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 100, Low32(state.r[11]));
	// b 0x823ad504
	goto loc_823AD504;
loc_823AD4D0:
	// lwz r4,88(r31)
	state.r[4] = PPC_LOAD_U32(Low32(state.r[31]) + 88);
loc_823AD4D4:
	// rlwinm. r11,r23,0,29,29
	state.r[11] = Rotate64(Low32(state.r[23]) | (state.r[23] << 32), 0) & 0x4;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x823ad500
	if (state.cr0.eq) goto loc_823AD500;
	// lis r11,-16384
	state.r[11] = -1073741824;
	// ori r11,r11,23
	state.r[11] = state.r[11] | 23;
	// stw r11,144(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 144, Low32(state.r[11]));
	// stw r24,152(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 152, Low32(state.r[24]));
	// stw r28,160(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 160, Low32(state.r[28]));
	// addi r3,r31,144
	state.r[3] = state.r[31] + 144;
	// stw r24,148(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 148, Low32(state.r[24]));
	// stw r4,164(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 164, Low32(state.r[4]));
	// bl 0x830da0ac
	state.lr = 0x823AD500;
	services.CallNative(0x830DA0ACu,memory,state);
loc_823AD500:
	// stw r24,100(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 100, Low32(state.r[24]));
loc_823AD504:
	// mr r8,r8
	state.r[8] = state.r[8];
	// addi r12,r31,320
	state.r[12] = state.r[31] + 320;
	// bl 0x823ad544
	state.lr = 0x823AD510;
	Cleanup(memory,services,state);
	// lwz r3,100(r31)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[31]) + 100);
	// addi r1,r31,320
	state.r[1] = state.r[31] + 320;
	// b 0x82b7a720
	Restore22(memory,state);
	return;
}

void Cleanup(GuestMemory& memory,BoundaryServices& services,Registers& state) {
	std::uint64_t temp=0;
	// std r31,-8(r1)
	PPC_STORE_U64(Low32(state.r[1]) + -8, state.r[31]);
	// addi r31,r12,-320
	state.r[31] = state.r[12] + -320;
	// std r27,-16(r1)
	PPC_STORE_U64(Low32(state.r[1]) + -16, state.r[27]);
	// std r22,-24(r1)
	PPC_STORE_U64(Low32(state.r[1]) + -24, state.r[22]);
	// mflr r12
	state.r[12] = state.lr;
	// stw r12,-32(r1)
	PPC_STORE_U32(Low32(state.r[1]) + -32, Low32(state.r[12]));
	// stwu r1,-112(r1)
	temp = state.r[1] + uint64_t(-112);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// cmplwi cr6,r22,0
	Compare<uint32_t>(state.cr6, Low32(state.r[22]), 0, state.xer_so);
	// beq cr6,0x823ad570
	if (state.cr6.eq) goto loc_823AD570;
	// lwz r3,1408(r27)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[27]) + 1408);
	// bl 0x830d9c7c
	state.lr = 0x823AD570;
	services.CallNative(0x830D9C7Cu,memory,state);
loc_823AD570:
	// lwz r1,0(r1)
	state.r[1] = PPC_LOAD_U32(Low32(state.r[1]) + 0);
	// ld r31,-8(r1)
	state.r[31] = PPC_LOAD_U64(Low32(state.r[1]) + -8);
	// ld r27,-16(r1)
	state.r[27] = PPC_LOAD_U64(Low32(state.r[1]) + -16);
	// ld r22,-24(r1)
	state.r[22] = PPC_LOAD_U64(Low32(state.r[1]) + -24);
	// lwz r12,-32(r1)
	state.r[12] = PPC_LOAD_U32(Low32(state.r[1]) + -32);
	// mtlr r12
	state.lr = state.r[12];
	// blr
	return;
}

#undef PPC_LOAD_U8
#undef PPC_LOAD_U16
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U16
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    BoundaryServices& services,Registers& state)
{
    switch(entry)
    {
    case 0x823accb0u: Allocate(memory,services,state);return true;
    case 0x823ad544u: Cleanup(memory,services,state);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::heap_allocation_context

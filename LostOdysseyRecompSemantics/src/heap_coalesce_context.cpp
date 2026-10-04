#include "lo_semantics/heap_coalesce_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_coalesce_context
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

template<typename T>
void Compare(object_child_float::Condition& condition,T left,T right,
    std::uint8_t so)
{
    condition={std::uint8_t(left<right),std::uint8_t(left>right),
        std::uint8_t(left==right),so};
}

void Save25(GuestMemory& memory,Registers& state)
{
    for(unsigned i=25u;i<=31u;++i)
        WriteU64(memory,Address(state.r[1]-8u*(33u-i)),state.r[i]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore25(GuestMemory& memory,Registers& state)
{
    for(unsigned i=25u;i<=31u;++i)
        state.r[i]=ReadU64(memory,Address(state.r[1]-8u*(33u-i)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}

#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a),Low8(v))
#define PPC_STORE_U16(a,v) memory.WriteU16(Address(a),Low16(v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a),Low32(v))

void Coalesce(GuestMemory& memory,BoundaryServices& services,Registers& state)
{

	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6dc
	state.lr = 0x823AE110;
	Save25(memory,state);
	// stwu r1,-144(r1)
	temp = state.r[1] + uint64_t(-144);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r30,r4
	state.r[30] = state.r[4];
	// mr r29,r3
	state.r[29] = state.r[3];
	// mr r27,r5
	state.r[27] = state.r[5];
	// mr r28,r6
	state.r[28] = state.r[6];
	// li r26,1
	state.r[26] = 1;
	// lhz r11,2(r30)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[30]) + 2);
	// rotlwi r11,r11,4
	state.r[11] = std::rotl(Low32(state.r[11]), 4);
	// subf r31,r11,r30
	state.r[31] = state.r[30] - state.r[11];
	// lis r11,-274
	state.r[11] = -17956864;
	// cmplw cr6,r31,r30
	Compare<uint32_t>(state.cr6, Low32(state.r[31]), Low32(state.r[30]), state.xer_so);
	// ori r25,r11,65262
	state.r[25] = state.r[11] | 65262;
	// beq cr6,0x823ae31c
	if (state.cr6.eq) goto loc_823AE31C;
	// lbz r11,5(r31)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[31]) + 5);
	// clrlwi. r11,r11,31
	state.r[11] = Low32(state.r[11]) & 0x1;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// bne 0x823ae31c
	if (!state.cr0.eq) goto loc_823AE31C;
	// lhz r11,0(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// lwz r10,0(r27)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// add r11,r11,r10
	state.r[11] = state.r[11] + state.r[10];
	// cmplwi cr6,r11,61440
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 61440, state.xer_so);
	// bgt cr6,0x823ae31c
	if (state.cr6.gt) goto loc_823AE31C;
	// cmplwi cr6,r28,0
	Compare<uint32_t>(state.cr6, Low32(state.r[28]), 0, state.xer_so);
	// beq cr6,0x823ae218
	if (state.cr6.eq) goto loc_823AE218;
	// lwz r11,12(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 12);
	// addi r8,r30,8
	state.r[8] = state.r[30] + 8;
	// lwz r10,8(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 8);
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// lwz r7,4(r10)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// cmplw cr6,r9,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[7]), state.xer_so);
	// bne cr6,0x823ae1cc
	if (!state.cr6.eq) goto loc_823AE1CC;
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// bne cr6,0x823ae1cc
	if (!state.cr6.eq) goto loc_823AE1CC;
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// stw r11,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[11]));
	// bne cr6,0x823ae1cc
	if (!state.cr6.eq) goto loc_823AE1CC;
	// lhz r11,0(r30)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// cmplwi cr6,r11,128
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 128, state.xer_so);
	// bge cr6,0x823ae1cc
	if (!state.cr6.lt) goto loc_823AE1CC;
	// rlwinm r10,r11,27,5,31
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 27) & 0x7FFFFFF;
	// clrlwi r11,r11,27
	state.r[11] = Low32(state.r[11]) & 0x1F;
	// slw r9,r26,r11
	state.r[9] = Low8(state.r[11]) & 0x20 ? 0 : (Low32(state.r[26]) << (Low8(state.r[11]) & 0x3F));
	// addi r11,r10,88
	state.r[11] = state.r[10] + 88;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r10,r11,r29
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[29]));
	// xor r10,r9,r10
	state.r[10] = state.r[9] ^ state.r[10];
	// stwx r10,r11,r29
	PPC_STORE_U32(Low32(state.r[11]) + Low32(state.r[29]), Low32(state.r[10]));
loc_823AE1CC:
	// lbz r11,5(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// rlwinm. r10,r11,0,29,29
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x4;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// beq 0x823ae204
	if (state.cr0.eq) goto loc_823AE204;
	// lhz r10,0(r30)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// rlwinm. r9,r11,0,30,30
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x2;
	Compare<int32_t>(state.cr0, Signed32(state.r[9]), 0, state.xer_so);
	// rotlwi r11,r10,4
	state.r[11] = std::rotl(Low32(state.r[10]), 4);
	// addi r4,r11,-24
	state.r[4] = state.r[11] + -24;
	// beq 0x823ae1f8
	if (state.cr0.eq) goto loc_823AE1F8;
	// cmplwi cr6,r4,4
	Compare<uint32_t>(state.cr6, Low32(state.r[4]), 4, state.xer_so);
	// ble cr6,0x823ae1f8
	if (!state.cr6.gt) goto loc_823AE1F8;
	// addi r4,r4,-4
	state.r[4] = state.r[4] + -4;
loc_823AE1F8:
	// mr r5,r25
	state.r[5] = state.r[25];
	// addi r3,r30,24
	state.r[3] = state.r[30] + 24;
	// bl 0x830da08c
	state.lr = 0x823AE204;
	services.CallNative(0x830da08cu,memory,state);
loc_823AE204:
	// lhz r11,0(r30)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// li r28,0
	state.r[28] = 0;
	// lwz r10,48(r29)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[29]) + 48);
	// subf r11,r11,r10
	state.r[11] = state.r[10] - state.r[11];
	// stw r11,48(r29)
	PPC_STORE_U32(Low32(state.r[29]) + 48, Low32(state.r[11]));
loc_823AE218:
	// lwz r11,12(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 12);
	// addi r8,r31,8
	state.r[8] = state.r[31] + 8;
	// lwz r10,8(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 8);
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// lwz r7,4(r10)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// cmplw cr6,r9,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[7]), state.xer_so);
	// bne cr6,0x823ae278
	if (!state.cr6.eq) goto loc_823AE278;
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// bne cr6,0x823ae278
	if (!state.cr6.eq) goto loc_823AE278;
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// stw r11,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[11]));
	// bne cr6,0x823ae278
	if (!state.cr6.eq) goto loc_823AE278;
	// lhz r11,0(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// cmplwi cr6,r11,128
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 128, state.xer_so);
	// bge cr6,0x823ae278
	if (!state.cr6.lt) goto loc_823AE278;
	// rlwinm r10,r11,27,5,31
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 27) & 0x7FFFFFF;
	// clrlwi r11,r11,27
	state.r[11] = Low32(state.r[11]) & 0x1F;
	// slw r9,r26,r11
	state.r[9] = Low8(state.r[11]) & 0x20 ? 0 : (Low32(state.r[26]) << (Low8(state.r[11]) & 0x3F));
	// addi r11,r10,88
	state.r[11] = state.r[10] + 88;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r10,r11,r29
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[29]));
	// xor r10,r9,r10
	state.r[10] = state.r[9] ^ state.r[10];
	// stwx r10,r11,r29
	PPC_STORE_U32(Low32(state.r[11]) + Low32(state.r[29]), Low32(state.r[10]));
loc_823AE278:
	// lbz r11,5(r31)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[31]) + 5);
	// rlwinm. r10,r11,0,29,29
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x4;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// beq 0x823ae2b0
	if (state.cr0.eq) goto loc_823AE2B0;
	// lhz r10,0(r31)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// rlwinm. r9,r11,0,30,30
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x2;
	Compare<int32_t>(state.cr0, Signed32(state.r[9]), 0, state.xer_so);
	// rotlwi r11,r10,4
	state.r[11] = std::rotl(Low32(state.r[10]), 4);
	// addi r4,r11,-24
	state.r[4] = state.r[11] + -24;
	// beq 0x823ae2a4
	if (state.cr0.eq) goto loc_823AE2A4;
	// cmplwi cr6,r4,4
	Compare<uint32_t>(state.cr6, Low32(state.r[4]), 4, state.xer_so);
	// ble cr6,0x823ae2a4
	if (!state.cr6.gt) goto loc_823AE2A4;
	// addi r4,r4,-4
	state.r[4] = state.r[4] + -4;
loc_823AE2A4:
	// mr r5,r25
	state.r[5] = state.r[25];
	// addi r3,r31,24
	state.r[3] = state.r[31] + 24;
	// bl 0x830da08c
	state.lr = 0x823AE2B0;
	services.CallNative(0x830da08cu,memory,state);
loc_823AE2B0:
	// lbz r11,5(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// rlwinm. r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// stb r11,5(r31)
	PPC_STORE_U8(Low32(state.r[31]) + 5, Low8(state.r[11]));
	// beq 0x823ae2d4
	if (state.cr0.eq) goto loc_823AE2D4;
	// lbz r11,4(r31)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[31]) + 4);
	// addi r11,r11,24
	state.r[11] = state.r[11] + 24;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r29
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[29]));
	// stw r31,64(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 64, Low32(state.r[31]));
loc_823AE2D4:
	// lwz r10,0(r27)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// mr r30,r31
	state.r[30] = state.r[31];
	// lhz r11,0(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// add r11,r11,r10
	state.r[11] = state.r[11] + state.r[10];
	// stw r11,0(r27)
	PPC_STORE_U32(Low32(state.r[27]) + 0, Low32(state.r[11]));
	// lwz r10,48(r29)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[29]) + 48);
	// lhz r11,0(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// subf r11,r11,r10
	state.r[11] = state.r[10] - state.r[11];
	// stw r11,48(r29)
	PPC_STORE_U32(Low32(state.r[29]) + 48, Low32(state.r[11]));
	// lbz r10,5(r31)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[31]) + 5);
	// lwz r11,0(r27)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// sth r11,0(r31)
	PPC_STORE_U16(Low32(state.r[31]) + 0, Low16(state.r[11]));
	// rlwinm. r10,r10,0,27,27
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// bne 0x823ae31c
	if (!state.cr0.eq) goto loc_823AE31C;
	// lwz r11,0(r27)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// rlwinm r10,r11,4,0,27
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 4) & 0xFFFFFFF0;
	// add r10,r10,r31
	state.r[10] = state.r[10] + state.r[31];
	// sth r11,2(r10)
	PPC_STORE_U16(Low32(state.r[10]) + 2, Low16(state.r[11]));
loc_823AE31C:
	// lbz r11,5(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// rlwinm. r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// bne 0x823ae500
	if (!state.cr0.eq) goto loc_823AE500;
	// lwz r11,0(r27)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// rlwinm r10,r11,4,0,27
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 4) & 0xFFFFFFF0;
	// add r31,r10,r30
	state.r[31] = state.r[10] + state.r[30];
	// lbz r10,5(r31)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[31]) + 5);
	// clrlwi. r10,r10,31
	state.r[10] = Low32(state.r[10]) & 0x1;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// bne 0x823ae500
	if (!state.cr0.eq) goto loc_823AE500;
	// lhz r10,0(r31)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// add r11,r10,r11
	state.r[11] = state.r[10] + state.r[11];
	// cmplwi cr6,r11,61440
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 61440, state.xer_so);
	// bgt cr6,0x823ae500
	if (state.cr6.gt) goto loc_823AE500;
	// cmplwi cr6,r28,0
	Compare<uint32_t>(state.cr6, Low32(state.r[28]), 0, state.xer_so);
	// beq cr6,0x823ae400
	if (state.cr6.eq) goto loc_823AE400;
	// lwz r11,12(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 12);
	// addi r8,r30,8
	state.r[8] = state.r[30] + 8;
	// lwz r10,8(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 8);
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// lwz r7,4(r10)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// cmplw cr6,r9,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[7]), state.xer_so);
	// bne cr6,0x823ae3b8
	if (!state.cr6.eq) goto loc_823AE3B8;
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// bne cr6,0x823ae3b8
	if (!state.cr6.eq) goto loc_823AE3B8;
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// stw r11,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[11]));
	// bne cr6,0x823ae3b8
	if (!state.cr6.eq) goto loc_823AE3B8;
	// lhz r11,0(r30)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// cmplwi cr6,r11,128
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 128, state.xer_so);
	// bge cr6,0x823ae3b8
	if (!state.cr6.lt) goto loc_823AE3B8;
	// rlwinm r10,r11,27,5,31
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 27) & 0x7FFFFFF;
	// clrlwi r11,r11,27
	state.r[11] = Low32(state.r[11]) & 0x1F;
	// slw r9,r26,r11
	state.r[9] = Low8(state.r[11]) & 0x20 ? 0 : (Low32(state.r[26]) << (Low8(state.r[11]) & 0x3F));
	// addi r11,r10,88
	state.r[11] = state.r[10] + 88;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r10,r11,r29
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[29]));
	// xor r10,r9,r10
	state.r[10] = state.r[9] ^ state.r[10];
	// stwx r10,r11,r29
	PPC_STORE_U32(Low32(state.r[11]) + Low32(state.r[29]), Low32(state.r[10]));
loc_823AE3B8:
	// lbz r11,5(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// rlwinm. r10,r11,0,29,29
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x4;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// beq 0x823ae3f0
	if (state.cr0.eq) goto loc_823AE3F0;
	// lhz r10,0(r30)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// rlwinm. r9,r11,0,30,30
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x2;
	Compare<int32_t>(state.cr0, Signed32(state.r[9]), 0, state.xer_so);
	// rotlwi r11,r10,4
	state.r[11] = std::rotl(Low32(state.r[10]), 4);
	// addi r4,r11,-24
	state.r[4] = state.r[11] + -24;
	// beq 0x823ae3e4
	if (state.cr0.eq) goto loc_823AE3E4;
	// cmplwi cr6,r4,4
	Compare<uint32_t>(state.cr6, Low32(state.r[4]), 4, state.xer_so);
	// ble cr6,0x823ae3e4
	if (!state.cr6.gt) goto loc_823AE3E4;
	// addi r4,r4,-4
	state.r[4] = state.r[4] + -4;
loc_823AE3E4:
	// mr r5,r25
	state.r[5] = state.r[25];
	// addi r3,r30,24
	state.r[3] = state.r[30] + 24;
	// bl 0x830da08c
	state.lr = 0x823AE3F0;
	services.CallNative(0x830da08cu,memory,state);
loc_823AE3F0:
	// lhz r11,0(r30)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[30]) + 0);
	// lwz r10,48(r29)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[29]) + 48);
	// subf r11,r11,r10
	state.r[11] = state.r[10] - state.r[11];
	// stw r11,48(r29)
	PPC_STORE_U32(Low32(state.r[29]) + 48, Low32(state.r[11]));
loc_823AE400:
	// lbz r11,5(r31)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[31]) + 5);
	// rlwinm. r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// stb r11,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[11]));
	// beq 0x823ae424
	if (state.cr0.eq) goto loc_823AE424;
	// lbz r11,4(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 4);
	// addi r11,r11,24
	state.r[11] = state.r[11] + 24;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r29
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[29]));
	// stw r30,64(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 64, Low32(state.r[30]));
loc_823AE424:
	// lwz r11,12(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 12);
	// addi r8,r31,8
	state.r[8] = state.r[31] + 8;
	// lwz r10,8(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 8);
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// lwz r7,4(r10)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[10]) + 4);
	// cmplw cr6,r9,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[7]), state.xer_so);
	// bne cr6,0x823ae484
	if (!state.cr6.eq) goto loc_823AE484;
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// bne cr6,0x823ae484
	if (!state.cr6.eq) goto loc_823AE484;
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// stw r11,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[11]));
	// bne cr6,0x823ae484
	if (!state.cr6.eq) goto loc_823AE484;
	// lhz r11,0(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// cmplwi cr6,r11,128
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 128, state.xer_so);
	// bge cr6,0x823ae484
	if (!state.cr6.lt) goto loc_823AE484;
	// rlwinm r10,r11,27,5,31
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 27) & 0x7FFFFFF;
	// clrlwi r11,r11,27
	state.r[11] = Low32(state.r[11]) & 0x1F;
	// slw r9,r26,r11
	state.r[9] = Low8(state.r[11]) & 0x20 ? 0 : (Low32(state.r[26]) << (Low8(state.r[11]) & 0x3F));
	// addi r11,r10,88
	state.r[11] = state.r[10] + 88;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r10,r11,r29
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[29]));
	// xor r10,r9,r10
	state.r[10] = state.r[9] ^ state.r[10];
	// stwx r10,r11,r29
	PPC_STORE_U32(Low32(state.r[11]) + Low32(state.r[29]), Low32(state.r[10]));
loc_823AE484:
	// lbz r11,5(r31)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[31]) + 5);
	// rlwinm. r10,r11,0,29,29
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x4;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// beq 0x823ae4bc
	if (state.cr0.eq) goto loc_823AE4BC;
	// lhz r10,0(r31)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// rlwinm. r9,r11,0,30,30
	state.r[9] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x2;
	Compare<int32_t>(state.cr0, Signed32(state.r[9]), 0, state.xer_so);
	// rotlwi r11,r10,4
	state.r[11] = std::rotl(Low32(state.r[10]), 4);
	// addi r4,r11,-24
	state.r[4] = state.r[11] + -24;
	// beq 0x823ae4b0
	if (state.cr0.eq) goto loc_823AE4B0;
	// cmplwi cr6,r4,4
	Compare<uint32_t>(state.cr6, Low32(state.r[4]), 4, state.xer_so);
	// ble cr6,0x823ae4b0
	if (!state.cr6.gt) goto loc_823AE4B0;
	// addi r4,r4,-4
	state.r[4] = state.r[4] + -4;
loc_823AE4B0:
	// mr r5,r25
	state.r[5] = state.r[25];
	// addi r3,r31,24
	state.r[3] = state.r[31] + 24;
	// bl 0x830da08c
	state.lr = 0x823AE4BC;
	services.CallNative(0x830da08cu,memory,state);
loc_823AE4BC:
	// lwz r10,0(r27)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// lhz r11,0(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// add r11,r11,r10
	state.r[11] = state.r[11] + state.r[10];
	// stw r11,0(r27)
	PPC_STORE_U32(Low32(state.r[27]) + 0, Low32(state.r[11]));
	// lwz r10,48(r29)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[29]) + 48);
	// lhz r11,0(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// subf r11,r11,r10
	state.r[11] = state.r[10] - state.r[11];
	// stw r11,48(r29)
	PPC_STORE_U32(Low32(state.r[29]) + 48, Low32(state.r[11]));
	// lbz r10,5(r30)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[30]) + 5);
	// lwz r11,0(r27)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// sth r11,0(r30)
	PPC_STORE_U16(Low32(state.r[30]) + 0, Low16(state.r[11]));
	// rlwinm. r10,r10,0,27,27
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// bne 0x823ae500
	if (!state.cr0.eq) goto loc_823AE500;
	// lwz r11,0(r27)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// rlwinm r10,r11,4,0,27
	state.r[10] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 4) & 0xFFFFFFF0;
	// add r10,r10,r30
	state.r[10] = state.r[10] + state.r[30];
	// sth r11,2(r10)
	PPC_STORE_U16(Low32(state.r[10]) + 2, Low16(state.r[11]));
loc_823AE500:
	// mr r3,r30
	state.r[3] = state.r[30];
	// addi r1,r1,144
	state.r[1] = state.r[1] + 144;
	// b 0x82b7a72c
	Restore25(memory,state);
	return;
}
class FreeBoundary final : public heap_free_context::BoundaryServices
{
public:
    explicit FreeBoundary(BoundaryServices& lower):lower_(lower){}
    void CallDirect(GuestAddress entry,GuestMemory& memory,
        Registers& state) override
    {
        if(entry==0x823ae108u)
        {
            (void)heap_coalesce_context::Apply(entry,memory,lower_,state);
            return;
        }
        lower_.CallDirect(entry,memory,state);
    }
    void CallNative(GuestAddress entry,GuestMemory& memory,
        Registers& state) override
    {lower_.CallNative(entry,memory,state);}
private:
    BoundaryServices& lower_;
};
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,BoundaryServices& services,
    Registers& state)
{
    if(entry!=0x823ae108u) return false;
    Coalesce(memory,services,state);
    return true;
}
bool ApplyFree(GuestAddress entry,GuestMemory& memory,
    BoundaryServices& services,Registers& state)
{
    if(entry!=0x823ade28u && entry!=0x823ae0bcu) return false;
    FreeBoundary bridge(services);
    return heap_free_context::Apply(entry,memory,bridge,state);
}
} // namespace lo::semantic::gpu::heap_coalesce_context

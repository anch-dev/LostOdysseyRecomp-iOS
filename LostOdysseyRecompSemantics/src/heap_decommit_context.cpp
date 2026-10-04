#include "lo_semantics/heap_decommit_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_decommit_context
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

void Save19(GuestMemory& memory,Registers& state)
{
    for(unsigned index=19u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore19(GuestMemory& memory,Registers& state)
{
    for(unsigned index=19u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}

#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a),Low8(v))
#define PPC_STORE_U16(a,v) memory.WriteU16(Address(a),Low16(v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a),Low32(v))

void Decommit(GuestMemory& memory,BoundaryServices& services,Registers& state)
{
	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6c4
	state.lr = 0x827CC670;
	Save19(memory,state);
	// stwu r1,-208(r1)
	temp = state.r[1] + uint64_t(-208);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r29,r3
	state.r[29] = state.r[3];
	// mr r31,r4
	state.r[31] = state.r[4];
	// mr r24,r5
	state.r[24] = state.r[5];
	// lwz r11,1412(r29)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[29]) + 1412);
	// cmplwi cr6,r11,0
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827cc9b0
	if (!state.cr6.eq) goto loc_827CC9B0;
	// lbz r11,4(r31)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[31]) + 4);
	// addis r10,r31,1
	state.r[10] = state.r[31] + 65536;
	// li r19,0
	state.r[19] = 0;
	// addi r11,r11,24
	state.r[11] = state.r[11] + 24;
	// addi r10,r10,-1
	state.r[10] = state.r[10] + -1;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// rlwinm r8,r10,0,0,15
	state.r[8] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0xFFFF0000;
	// mr r23,r19
	state.r[23] = state.r[19];
	// lis r7,1
	state.r[7] = 65536;
	// lwzx r26,r11,r29
	state.r[26] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[29]));
	// subf r11,r31,r8
	state.r[11] = state.r[8] - state.r[31];
	// stw r8,84(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 84, Low32(state.r[8]));
	// srawi r11,r11,4
	state.xer_ca = (Signed32(state.r[11]) < 0) && ((Low32(state.r[11]) & 0xF) != 0);
	state.r[11] = Signed32(state.r[11]) >> 4;
	// clrlwi r27,r11,16
	state.r[27] = Low32(state.r[11]) & 0xFFFF;
	// cmplwi cr6,r27,1
	Compare<uint32_t>(state.cr6, Low32(state.r[27]), 1, state.xer_so);
	// bne cr6,0x827cc6dc
	if (!state.cr6.eq) goto loc_827CC6DC;
	// add r8,r8,r7
	state.r[8] = state.r[8] + state.r[7];
	// li r27,4097
	state.r[27] = 4097;
	// stw r8,84(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 84, Low32(state.r[8]));
	// b 0x827cc6f8
	goto loc_827CC6F8;
loc_827CC6DC:
	// lhz r11,2(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 2);
	// cmplwi r11,0
	Compare<uint32_t>(state.cr0, Low32(state.r[11]), 0, state.xer_so);
	// beq 0x827cc6f8
	if (state.cr0.eq) goto loc_827CC6F8;
	// cmplw cr6,r8,r31
	Compare<uint32_t>(state.cr6, Low32(state.r[8]), Low32(state.r[31]), state.xer_so);
	// bne cr6,0x827cc6f8
	if (!state.cr6.eq) goto loc_827CC6F8;
	// rlwinm r11,r11,4,0,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 4) & 0xFFFFFFF0;
	// subf r23,r11,r31
	state.r[23] = state.r[31] - state.r[11];
loc_827CC6F8:
	// rlwinm r11,r24,4,0,27
	state.r[11] = Rotate64(Low32(state.r[24]) | (state.r[24] << 32), 4) & 0xFFFFFFF0;
	// mr r20,r19
	state.r[20] = state.r[19];
	// add r10,r11,r31
	state.r[10] = state.r[11] + state.r[31];
	// rlwinm r11,r10,0,0,15
	state.r[11] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0xFFFF0000;
	// subf r9,r11,r10
	state.r[9] = state.r[10] - state.r[11];
	// srawi r9,r9,4
	state.xer_ca = (Signed32(state.r[9]) < 0) && ((Low32(state.r[9]) & 0xF) != 0);
	state.r[9] = Signed32(state.r[9]) >> 4;
	// clrlwi r25,r9,16
	state.r[25] = Low32(state.r[9]) & 0xFFFF;
	// mr r9,r25
	state.r[9] = state.r[25];
	// cmplwi cr6,r9,1
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), 1, state.xer_so);
	// bne cr6,0x827cc72c
	if (!state.cr6.eq) goto loc_827CC72C;
	// li r25,4097
	state.r[25] = 4097;
	// subf r11,r7,r11
	state.r[11] = state.r[11] - state.r[7];
	// b 0x827cc744
	goto loc_827CC744;
loc_827CC72C:
	// cmplwi cr6,r9,0
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), 0, state.xer_so);
	// bne cr6,0x827cc744
	if (!state.cr6.eq) goto loc_827CC744;
	// lbz r9,5(r31)
	state.r[9] = PPC_LOAD_U8(Low32(state.r[31]) + 5);
	// rlwinm. r9,r9,0,27,27
	state.r[9] = Rotate64(Low32(state.r[9]) | (state.r[9] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[9]), 0, state.xer_so);
	// bne 0x827cc744
	if (!state.cr0.eq) goto loc_827CC744;
	// mr r20,r10
	state.r[20] = state.r[10];
loc_827CC744:
	// clrlwi r22,r25,16
	state.r[22] = Low32(state.r[25]) & 0xFFFF;
	// cmplw cr6,r11,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[8]), state.xer_so);
	// rlwinm r21,r22,4,0,27
	state.r[21] = Rotate64(Low32(state.r[22]) | (state.r[22] << 32), 4) & 0xFFFFFFF0;
	// subf r11,r8,r11
	state.r[11] = state.r[11] - state.r[8];
	// subf r28,r21,r10
	state.r[28] = state.r[10] - state.r[21];
	// bgt cr6,0x827cc760
	if (state.cr6.gt) goto loc_827CC760;
	// mr r11,r19
	state.r[11] = state.r[19];
loc_827CC760:
	// stw r11,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[11]));
	// cmplwi cr6,r11,0
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// beq cr6,0x827cc9b0
	if (state.cr6.eq) goto loc_827CC9B0;
	// mr r3,r26
	state.r[3] = state.r[26];
	// bl 0x827cb498
	state.lr = 0x827CC774;
	services.CallDirect(0x827cb498u,memory,state);
	// mr. r30,r3
	state.r[30] = state.r[3];
	Compare<int32_t>(state.cr0, Signed32(state.r[30]), 0, state.xer_so);
	// beq 0x827cc9b0
	if (state.cr0.eq) goto loc_827CC9B0;
	// li r6,0
	state.r[6] = 0;
	// li r5,16384
	state.r[5] = 16384;
	// addi r4,r1,80
	state.r[4] = state.r[1] + 80;
	// addi r3,r1,84
	state.r[3] = state.r[1] + 84;
	// bl 0x830d9d3c
	state.lr = 0x827CC790;
	services.CallNative(0x830d9d3cu,memory,state);
	// lwz r11,24(r26)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[26]) + 24);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// lwz r11,76(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 76);
	// stw r11,0(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 0, Low32(state.r[11]));
	// lwz r11,24(r26)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[26]) + 24);
	// stw r30,76(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 76, Low32(state.r[30]));
	// stw r19,4(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 4, Low32(state.r[19]));
	// stw r19,8(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 8, Low32(state.r[19]));
	// blt 0x827cc9b0
	if (state.cr0.lt) goto loc_827CC9B0;
	// mr r3,r26
	state.r[3] = state.r[26];
	// lwz r5,80(r1)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// lwz r4,84(r1)
	state.r[4] = PPC_LOAD_U32(Low32(state.r[1]) + 84);
	// bl 0x827cb658
	state.lr = 0x827CC7C4;
	services.CallDirect(0x827cb658u,memory,state);
	// lwz r10,80(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// lwz r9,48(r26)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[26]) + 48);
	// li r8,1
	state.r[8] = 1;
	// rlwinm r10,r10,16,16,31
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 16) & 0xFFFF;
	// add r10,r10,r9
	state.r[10] = state.r[10] + state.r[9];
	// stw r10,48(r26)
	PPC_STORE_U32(Low32(state.r[26]) + 48, Low32(state.r[10]));
	// clrlwi. r11,r27,16
	state.r[11] = Low32(state.r[27]) & 0xFFFF;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x827cc898
	if (state.cr0.eq) goto loc_827CC898;
	// li r10,16
	state.r[10] = 16;
	// sth r27,0(r31)
	PPC_STORE_U16(Low32(state.r[31]) + 0, Low16(state.r[27]));
	// cmplwi cr6,r11,128
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 128, state.xer_so);
	// stb r10,5(r31)
	PPC_STORE_U8(Low32(state.r[31]) + 5, Low8(state.r[10]));
	// lwz r10,48(r29)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[29]) + 48);
	// add r10,r11,r10
	state.r[10] = state.r[11] + state.r[10];
	// stw r10,48(r29)
	PPC_STORE_U32(Low32(state.r[29]) + 48, Low32(state.r[10]));
	// stw r31,64(r26)
	PPC_STORE_U32(Low32(state.r[26]) + 64, Low32(state.r[31]));
	// lbz r10,5(r31)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[31]) + 5);
	// rlwinm r10,r10,0,0,28
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0xFFFFFFF8;
	// stb r10,5(r31)
	PPC_STORE_U8(Low32(state.r[31]) + 5, Low8(state.r[10]));
	// bge cr6,0x827cc854
	if (!state.cr6.lt) goto loc_827CC854;
	// addi r11,r11,48
	state.r[11] = state.r[11] + 48;
	// rlwinm r11,r11,3,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r29
	state.r[11] = state.r[11] + state.r[29];
	// lwz r10,0(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// bne cr6,0x827cc87c
	if (!state.cr6.eq) goto loc_827CC87C;
	// lhz r10,0(r31)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// rlwinm r9,r10,27,5,31
	state.r[9] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 27) & 0x7FFFFFF;
	// clrlwi r10,r10,27
	state.r[10] = Low32(state.r[10]) & 0x1F;
	// slw r7,r8,r10
	state.r[7] = Low8(state.r[10]) & 0x20 ? 0 : (Low32(state.r[8]) << (Low8(state.r[10]) & 0x3F));
	// addi r10,r9,88
	state.r[10] = state.r[9] + 88;
	// rlwinm r10,r10,2,0,29
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r10,r29
	state.r[9] = PPC_LOAD_U32(Low32(state.r[10]) + Low32(state.r[29]));
	// or r9,r7,r9
	state.r[9] = state.r[7] | state.r[9];
	// stwx r9,r10,r29
	PPC_STORE_U32(Low32(state.r[10]) + Low32(state.r[29]), Low32(state.r[9]));
	// b 0x827cc87c
	goto loc_827CC87C;
loc_827CC854:
	// addi r9,r29,384
	state.r[9] = state.r[29] + 384;
	// lwz r10,0(r9)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[9]) + 0);
	// b 0x827cc870
	goto loc_827CC870;
loc_827CC860:
	// lhz r7,-8(r10)
	state.r[7] = PPC_LOAD_U16(Low32(state.r[10]) + -8);
	// cmplw cr6,r11,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[7]), state.xer_so);
	// ble cr6,0x827cc878
	if (!state.cr6.gt) goto loc_827CC878;
	// lwz r10,0(r10)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
loc_827CC870:
	// cmplw cr6,r9,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[10]), state.xer_so);
	// bne cr6,0x827cc860
	if (!state.cr6.eq) goto loc_827CC860;
loc_827CC878:
	// mr r11,r10
	state.r[11] = state.r[10];
loc_827CC87C:
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r9,r31,8
	state.r[9] = state.r[31] + 8;
	// stw r11,8(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 8, Low32(state.r[11]));
	// stw r10,12(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 12, Low32(state.r[10]));
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// stw r9,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[9]));
	// b 0x827cc8dc
	goto loc_827CC8DC;
loc_827CC898:
	// cmplwi cr6,r23,0
	Compare<uint32_t>(state.cr6, Low32(state.r[23]), 0, state.xer_so);
	// beq cr6,0x827cc8b4
	if (state.cr6.eq) goto loc_827CC8B4;
	// lbz r11,5(r23)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[23]) + 5);
	// ori r11,r11,16
	state.r[11] = state.r[11] | 16;
	// stb r11,5(r23)
	PPC_STORE_U8(Low32(state.r[23]) + 5, Low8(state.r[11]));
	// stw r23,64(r26)
	PPC_STORE_U32(Low32(state.r[26]) + 64, Low32(state.r[23]));
	// b 0x827cc8dc
	goto loc_827CC8DC;
loc_827CC8B4:
	// lwz r11,64(r26)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[26]) + 64);
	// lwz r10,84(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 84);
	// cmplw cr6,r11,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[10]), state.xer_so);
	// blt cr6,0x827cc8dc
	if (state.cr6.lt) goto loc_827CC8DC;
	// lwz r9,80(r1)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// add r10,r9,r10
	state.r[10] = state.r[9] + state.r[10];
	// cmplw cr6,r11,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[10]), state.xer_so);
	// bge cr6,0x827cc8dc
	if (!state.cr6.lt) goto loc_827CC8DC;
	// lwz r11,40(r26)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[26]) + 40);
	// stw r11,64(r26)
	PPC_STORE_U32(Low32(state.r[26]) + 64, Low32(state.r[11]));
loc_827CC8DC:
	// cmplwi cr6,r22,0
	Compare<uint32_t>(state.cr6, Low32(state.r[22]), 0, state.xer_so);
	// beq cr6,0x827cc9a0
	if (state.cr6.eq) goto loc_827CC9A0;
	// sth r19,2(r28)
	PPC_STORE_U16(Low32(state.r[28]) + 2, Low16(state.r[19]));
	// add r11,r21,r28
	state.r[11] = state.r[21] + state.r[28];
	// lbz r10,4(r26)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[26]) + 4);
	// cmplwi cr6,r22,128
	Compare<uint32_t>(state.cr6, Low32(state.r[22]), 128, state.xer_so);
	// stb r19,5(r28)
	PPC_STORE_U8(Low32(state.r[28]) + 5, Low8(state.r[19]));
	// sth r25,0(r28)
	PPC_STORE_U16(Low32(state.r[28]) + 0, Low16(state.r[25]));
	// stb r10,4(r28)
	PPC_STORE_U8(Low32(state.r[28]) + 4, Low8(state.r[10]));
	// sth r25,2(r11)
	PPC_STORE_U16(Low32(state.r[11]) + 2, Low16(state.r[25]));
	// lbz r11,5(r28)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[28]) + 5);
	// rlwinm r11,r11,0,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFFFFFF8;
	// stb r11,5(r28)
	PPC_STORE_U8(Low32(state.r[28]) + 5, Low8(state.r[11]));
	// bge cr6,0x827cc954
	if (!state.cr6.lt) goto loc_827CC954;
	// addi r11,r22,48
	state.r[11] = state.r[22] + 48;
	// rlwinm r11,r11,3,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r29
	state.r[11] = state.r[11] + state.r[29];
	// lwz r10,0(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// bne cr6,0x827cc978
	if (!state.cr6.eq) goto loc_827CC978;
	// lhz r10,0(r28)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[28]) + 0);
	// rlwinm r9,r10,27,5,31
	state.r[9] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 27) & 0x7FFFFFF;
	// clrlwi r10,r10,27
	state.r[10] = Low32(state.r[10]) & 0x1F;
	// slw r8,r8,r10
	state.r[8] = Low8(state.r[10]) & 0x20 ? 0 : (Low32(state.r[8]) << (Low8(state.r[10]) & 0x3F));
	// addi r10,r9,88
	state.r[10] = state.r[9] + 88;
	// rlwinm r10,r10,2,0,29
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r10,r29
	state.r[9] = PPC_LOAD_U32(Low32(state.r[10]) + Low32(state.r[29]));
	// or r9,r8,r9
	state.r[9] = state.r[8] | state.r[9];
	// stwx r9,r10,r29
	PPC_STORE_U32(Low32(state.r[10]) + Low32(state.r[29]), Low32(state.r[9]));
	// b 0x827cc978
	goto loc_827CC978;
loc_827CC954:
	// addi r10,r29,384
	state.r[10] = state.r[29] + 384;
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// b 0x827cc970
	goto loc_827CC970;
loc_827CC960:
	// lhz r9,-8(r11)
	state.r[9] = PPC_LOAD_U16(Low32(state.r[11]) + -8);
	// cmplw cr6,r22,r9
	Compare<uint32_t>(state.cr6, Low32(state.r[22]), Low32(state.r[9]), state.xer_so);
	// ble cr6,0x827cc978
	if (!state.cr6.gt) goto loc_827CC978;
	// lwz r11,0(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
loc_827CC970:
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// bne cr6,0x827cc960
	if (!state.cr6.eq) goto loc_827CC960;
loc_827CC978:
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r9,r28,8
	state.r[9] = state.r[28] + 8;
	// stw r11,8(r28)
	PPC_STORE_U32(Low32(state.r[28]) + 8, Low32(state.r[11]));
	// stw r10,12(r28)
	PPC_STORE_U32(Low32(state.r[28]) + 12, Low32(state.r[10]));
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// stw r9,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[9]));
	// lwz r11,48(r29)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[29]) + 48);
	// add r11,r22,r11
	state.r[11] = state.r[22] + state.r[11];
	// stw r11,48(r29)
	PPC_STORE_U32(Low32(state.r[29]) + 48, Low32(state.r[11]));
	// b 0x827cc9c0
	goto loc_827CC9C0;
loc_827CC9A0:
	// cmplwi cr6,r20,0
	Compare<uint32_t>(state.cr6, Low32(state.r[20]), 0, state.xer_so);
	// beq cr6,0x827cc9c0
	if (state.cr6.eq) goto loc_827CC9C0;
	// sth r19,2(r20)
	PPC_STORE_U16(Low32(state.r[20]) + 2, Low16(state.r[19]));
	// b 0x827cc9c0
	goto loc_827CC9C0;
loc_827CC9B0:
	// mr r5,r24
	state.r[5] = state.r[24];
	// mr r4,r31
	state.r[4] = state.r[31];
	// mr r3,r29
	state.r[3] = state.r[29];
	// bl 0x827cba60
	state.lr = 0x827CC9C0;
	(void)heap_insert_context::Apply(0x827cba60u,memory,state);
loc_827CC9C0:
	// addi r1,r1,208
	state.r[1] = state.r[1] + 208;
	// b 0x82b7a714
	Restore19(memory,state);
	return;
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    BoundaryServices& services,Registers& state)
{
    if(entry!=0x827cc668u) return false;
    Decommit(memory,services,state);
    return true;
}
} // namespace lo::semantic::gpu::heap_decommit_context

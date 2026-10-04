#include "lo_semantics/heap_growth_lower_context.h"
#include "lo_semantics/heap_insert_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_growth_lower_context
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
std::uint32_t Rotate32(std::uint32_t value,int shift)
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
    for(unsigned index=25u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore25(GuestMemory& memory,Registers& state)
{
    for(unsigned index=25u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}
void Save22(GuestMemory& memory,Registers& state)
{
    for(unsigned index=22u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore22(GuestMemory& memory,Registers& state)
{
    for(unsigned index=22u;index<=31u;++index)
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

void Commit(GuestMemory& memory,BoundaryServices& services,Registers& state)
{
	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6dc
	state.lr = 0x827CB780;
	Save25(memory,state);
	// stwu r1,-160(r1)
	temp = state.r[1] + uint64_t(-160);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r30,r4
	state.r[30] = state.r[4];
	// li r26,0
	state.r[26] = 0;
	// addi r25,r30,56
	state.r[25] = state.r[30] + 56;
	// mr r28,r5
	state.r[28] = state.r[5];
	// mr r27,r25
	state.r[27] = state.r[25];
	// mr r29,r26
	state.r[29] = state.r[26];
	// lwz r31,0(r27)
	state.r[31] = PPC_LOAD_U32(Low32(state.r[27]) + 0);
	// cmplwi r31,0
	Compare<uint32_t>(state.cr0, Low32(state.r[31]), 0, state.xer_so);
	// beq 0x827cb7e0
	if (state.cr0.eq) goto loc_827CB7E0;
	// lwz r11,0(r28)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[28]) + 0);
loc_827CB7AC:
	// lwz r10,8(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 8);
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// blt cr6,0x827cb7cc
	if (state.cr6.lt) goto loc_827CB7CC;
	// cmplwi cr6,r6,0
	Compare<uint32_t>(state.cr6, Low32(state.r[6]), 0, state.xer_so);
	// beq cr6,0x827cb7ec
	if (state.cr6.eq) goto loc_827CB7EC;
	// lwz r10,4(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 4);
	// cmplw cr6,r10,r6
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[6]), state.xer_so);
	// beq cr6,0x827cb7ec
	if (state.cr6.eq) goto loc_827CB7EC;
loc_827CB7CC:
	// mr r29,r31
	state.r[29] = state.r[31];
	// mr r27,r31
	state.r[27] = state.r[31];
	// lwz r31,0(r31)
	state.r[31] = PPC_LOAD_U32(Low32(state.r[31]) + 0);
	// cmplwi r31,0
	Compare<uint32_t>(state.cr0, Low32(state.r[31]), 0, state.xer_so);
	// bne 0x827cb7ac
	if (!state.cr0.eq) goto loc_827CB7AC;
loc_827CB7E0:
	// li r3,0
	state.r[3] = 0;
loc_827CB7E4:
	// addi r1,r1,160
	state.r[1] = state.r[1] + 160;
	// b 0x82b7a72c
	Restore25(memory,state);
	return;
loc_827CB7EC:
	// lwz r10,4(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 4);
	// lwz r11,1412(r3)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[3]) + 1412);
	// cmplwi r11,0
	Compare<uint32_t>(state.cr0, Low32(state.r[11]), 0, state.xer_so);
	// stw r10,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[10]));
	// beq 0x827cb814
	if (state.cr0.eq) goto loc_827CB814;
	// mr r5,r28
	state.r[5] = state.r[28];
	// addi r4,r1,80
	state.r[4] = state.r[1] + 80;
	// mtctr r11
	state.ctr = state.r[11];
	// bctrl
	state.lr = 0x827CB810;
	services.CallIndirect(Address(state.ctr)&~3u,memory,state);
	// b 0x827cb830
	goto loc_827CB830;
loc_827CB814:
	// lis r5,24576
	state.r[5] = 1610612736;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// ori r5,r5,4096
	state.r[5] = state.r[5] | 4096;
	// mr r4,r28
	state.r[4] = state.r[28];
	// addi r3,r1,80
	state.r[3] = state.r[1] + 80;
	// bl 0x830d9d1c
	state.lr = 0x827CB830;
	services.CallNative(0x830d9d1cu,memory,state);
loc_827CB830:
	// cmpwi cr6,r3,0
	Compare<int32_t>(state.cr6, Signed32(state.r[3]), 0, state.xer_so);
	// blt cr6,0x827cb7e0
	if (state.cr6.lt) goto loc_827CB7E0;
	// lwz r11,48(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 48);
	// lhz r9,0(r28)
	state.r[9] = PPC_LOAD_U16(Low32(state.r[28]) + 0);
	// lwz r10,28(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 28);
	// subf r11,r9,r11
	state.r[11] = state.r[11] - state.r[9];
	// stw r11,48(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 48, Low32(state.r[11]));
	// lwz r11,8(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 8);
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// bne cr6,0x827cb85c
	if (!state.cr6.eq) goto loc_827CB85C;
	// stw r26,28(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 28, Low32(state.r[26]));
loc_827CB85C:
	// lwz r11,64(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 64);
	// lwz r7,80(r1)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// mr r3,r7
	state.r[3] = state.r[7];
	// lbz r10,5(r11)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[11]) + 5);
	// rlwinm. r10,r10,0,27,27
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// beq 0x827cb88c
	if (state.cr0.eq) goto loc_827CB88C;
	// lhz r10,0(r11)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// lwz r9,4(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 4);
	// rotlwi r10,r10,4
	state.r[10] = Rotate32(Low32(state.r[10]), 4);
	// add r10,r10,r11
	state.r[10] = state.r[10] + state.r[11];
	// cmplw cr6,r10,r9
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[9]), state.xer_so);
	// beq cr6,0x827cb8f8
	if (state.cr6.eq) goto loc_827CB8F8;
loc_827CB88C:
	// cmplwi cr6,r29,0
	Compare<uint32_t>(state.cr6, Low32(state.r[29]), 0, state.xer_so);
	// bne cr6,0x827cb89c
	if (!state.cr6.eq) goto loc_827CB89C;
	// lwz r11,40(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 40);
	// b 0x827cb8a8
	goto loc_827CB8A8;
loc_827CB89C:
	// lwz r11,8(r29)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[29]) + 8);
	// lwz r10,4(r29)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[29]) + 4);
	// add r11,r11,r10
	state.r[11] = state.r[11] + state.r[10];
loc_827CB8A8:
	// lbz r10,5(r11)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[11]) + 5);
	// rlwinm. r10,r10,0,27,27
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// bne 0x827cb8f8
	if (!state.cr0.eq) goto loc_827CB8F8;
	// lwz r9,44(r30)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[30]) + 44);
loc_827CB8B8:
	// lhz r10,0(r11)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// mr r8,r11
	state.r[8] = state.r[11];
	// rotlwi r10,r10,4
	state.r[10] = Rotate32(Low32(state.r[10]), 4);
	// add r11,r10,r11
	state.r[11] = state.r[10] + state.r[11];
	// cmplw cr6,r11,r9
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[9]), state.xer_so);
	// bge cr6,0x827cb8ec
	if (!state.cr6.lt) goto loc_827CB8EC;
	// lhz r10,0(r11)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// cmplwi r10,0
	Compare<uint32_t>(state.cr0, Low32(state.r[10]), 0, state.xer_so);
	// beq 0x827cb8ec
	if (state.cr0.eq) goto loc_827CB8EC;
	// lbz r10,5(r11)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[11]) + 5);
	// rlwinm. r10,r10,0,27,27
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// beq 0x827cb8b8
	if (state.cr0.eq) goto loc_827CB8B8;
	// b 0x827cb8f8
	goto loc_827CB8F8;
loc_827CB8EC:
	// cmplw cr6,r11,r7
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[7]), state.xer_so);
	// bne cr6,0x827cb7e0
	if (!state.cr6.eq) goto loc_827CB7E0;
	// mr r11,r8
	state.r[11] = state.r[8];
loc_827CB8F8:
	// lbz r10,5(r11)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[11]) + 5);
	// andi. r10,r10,239
	state.r[10] = state.r[10] & 239;
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// stb r10,5(r11)
	PPC_STORE_U8(Low32(state.r[11]) + 5, Low8(state.r[10]));
	// lwz r9,0(r28)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[28]) + 0);
	// lwz r10,4(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 4);
	// lwz r8,8(r31)
	state.r[8] = PPC_LOAD_U32(Low32(state.r[31]) + 8);
	// add r10,r10,r9
	state.r[10] = state.r[10] + state.r[9];
	// stw r10,4(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 4, Low32(state.r[10]));
	// lwz r10,0(r28)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[28]) + 0);
	// subf. r10,r10,r8
	state.r[10] = state.r[8] - state.r[10];
	Compare<int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// stw r10,8(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 8, Low32(state.r[10]));
	// bne 0x827cb988
	if (!state.cr0.eq) goto loc_827CB988;
	// lwz r10,4(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 4);
	// lwz r9,44(r30)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[30]) + 44);
	// cmplw cr6,r10,r9
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[9]), state.xer_so);
	// bne cr6,0x827cb948
	if (!state.cr6.eq) goto loc_827CB948;
	// li r10,16
	state.r[10] = 16;
	// stb r10,5(r3)
	PPC_STORE_U8(Low32(state.r[3]) + 5, Low8(state.r[10]));
	// stw r3,64(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 64, Low32(state.r[3]));
	// b 0x827cb954
	goto loc_827CB954;
loc_827CB948:
	// stb r26,5(r3)
	PPC_STORE_U8(Low32(state.r[3]) + 5, Low8(state.r[26]));
	// lwz r10,40(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 40);
	// stw r10,64(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 64, Low32(state.r[10]));
loc_827CB954:
	// lwz r10,0(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 0);
	// stw r10,0(r27)
	PPC_STORE_U32(Low32(state.r[27]) + 0, Low32(state.r[10]));
	// lwz r10,24(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 24);
	// lwz r10,76(r10)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[10]) + 76);
	// stw r10,0(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 0, Low32(state.r[10]));
	// lwz r10,24(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 24);
	// stw r31,76(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 76, Low32(state.r[31]));
	// stw r26,4(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 4, Low32(state.r[26]));
	// stw r26,8(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 8, Low32(state.r[26]));
	// lwz r10,52(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 52);
	// addi r10,r10,-1
	state.r[10] = state.r[10] + -1;
	// stw r10,52(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 52, Low32(state.r[10]));
	// b 0x827cb994
	goto loc_827CB994;
loc_827CB988:
	// li r10,16
	state.r[10] = 16;
	// stb r10,5(r3)
	PPC_STORE_U8(Low32(state.r[3]) + 5, Low8(state.r[10]));
	// stw r3,64(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 64, Low32(state.r[3]));
loc_827CB994:
	// lbz r10,4(r11)
	state.r[10] = PPC_LOAD_U8(Low32(state.r[11]) + 4);
	// lbz r9,5(r3)
	state.r[9] = PPC_LOAD_U8(Low32(state.r[3]) + 5);
	// stb r10,4(r3)
	PPC_STORE_U8(Low32(state.r[3]) + 4, Low8(state.r[10]));
	// lwz r10,0(r28)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[28]) + 0);
	// rlwinm r10,r10,28,4,31
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 28) & 0xFFFFFFF;
	// sth r10,0(r3)
	PPC_STORE_U16(Low32(state.r[3]) + 0, Low16(state.r[10]));
	// lhz r11,0(r11)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[11]) + 0);
	// rlwinm. r9,r9,0,27,27
	state.r[9] = Rotate64(Low32(state.r[9]) | (state.r[9] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[9]), 0, state.xer_so);
	// sth r11,2(r3)
	PPC_STORE_U16(Low32(state.r[3]) + 2, Low16(state.r[11]));
	// bne 0x827cb9cc
	if (!state.cr0.eq) goto loc_827CB9CC;
	// clrlwi r11,r10,16
	state.r[11] = Low32(state.r[10]) & 0xFFFF;
	// rotlwi r10,r11,4
	state.r[10] = Rotate32(Low32(state.r[11]), 4);
	// add r10,r10,r3
	state.r[10] = state.r[10] + state.r[3];
	// sth r11,2(r10)
	PPC_STORE_U16(Low32(state.r[10]) + 2, Low16(state.r[11]));
loc_827CB9CC:
	// lwz r11,28(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 28);
	// cmplwi cr6,r11,0
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827cb7e4
	if (!state.cr6.eq) goto loc_827CB7E4;
	// lwz r11,0(r25)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[25]) + 0);
	// b 0x827cb9f8
	goto loc_827CB9F8;
loc_827CB9E0:
	// lwz r10,8(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 8);
	// lwz r9,28(r30)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[30]) + 28);
	// cmplw cr6,r10,r9
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[9]), state.xer_so);
	// blt cr6,0x827cb9f4
	if (state.cr6.lt) goto loc_827CB9F4;
	// stw r10,28(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 28, Low32(state.r[10]));
loc_827CB9F4:
	// lwz r11,0(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
loc_827CB9F8:
	// cmplwi r11,0
	Compare<uint32_t>(state.cr0, Low32(state.r[11]), 0, state.xer_so);
	// bne 0x827cb9e0
	if (!state.cr0.eq) goto loc_827CB9E0;
	// b 0x827cb7e4
	goto loc_827CB7E4;
}

void CreateSegment(GuestMemory& memory,BoundaryServices& services,Registers& state)
{
	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6d0
	state.lr = 0x827CC2C8;
	Save22(memory,state);
	// stwu r1,-176(r1)
	temp = state.r[1] + uint64_t(-176);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r29,r7
	state.r[29] = state.r[7];
	// stw r8,236(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 236, Low32(state.r[8]));
	// mr r27,r9
	state.r[27] = state.r[9];
	// mr r31,r4
	state.r[31] = state.r[4];
	// subf r11,r29,r27
	state.r[11] = state.r[27] - state.r[29];
	// mr r28,r3
	state.r[28] = state.r[3];
	// addi r10,r31,87
	state.r[10] = state.r[31] + 87;
	// srawi r11,r11,16
	state.xer_ca = (Signed32(state.r[11]) < 0) && ((Low32(state.r[11]) & 0xFFFF) != 0);
	state.r[11] = Signed32(state.r[11]) >> 16;
	// mr r25,r5
	state.r[25] = state.r[5];
	// mr r23,r6
	state.r[23] = state.r[6];
	// rlwinm r30,r10,0,0,27
	state.r[30] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0xFFFFFFF0;
	// addze r26,r11
	temp = state.r[11] + state.xer_ca;
	state.xer_ca = Low32(temp) < Low32(state.r[11]);
	state.r[26] = temp;
	// cmplw cr6,r28,r29
	Compare<uint32_t>(state.cr6, Low32(state.r[28]), Low32(state.r[29]), state.xer_so);
	// bne cr6,0x827cc30c
	if (!state.cr6.eq) goto loc_827CC30C;
	// lhz r22,0(r28)
	state.r[22] = PPC_LOAD_U16(Low32(state.r[28]) + 0);
	// b 0x827cc310
	goto loc_827CC310;
loc_827CC30C:
	// li r22,0
	state.r[22] = 0;
loc_827CC310:
	// subf r10,r31,r30
	state.r[10] = state.r[30] - state.r[31];
	// addi r11,r30,16
	state.r[11] = state.r[30] + 16;
	// srawi r10,r10,4
	state.xer_ca = (Signed32(state.r[10]) < 0) && ((Low32(state.r[10]) & 0xF) != 0);
	state.r[10] = Signed32(state.r[10]) >> 4;
	// cmplw cr6,r11,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[8]), state.xer_so);
	// clrlwi r24,r10,16
	state.r[24] = Low32(state.r[10]) & 0xFFFF;
	// blt cr6,0x827cc378
	if (state.cr6.lt) goto loc_827CC378;
	// cmplw cr6,r11,r27
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[27]), state.xer_so);
	// blt cr6,0x827cc338
	if (state.cr6.lt) goto loc_827CC338;
loc_827CC330:
	// li r3,0
	state.r[3] = 0;
	// b 0x827cc420
	goto loc_827CC420;
loc_827CC338:
	// subf r11,r8,r30
	state.r[11] = state.r[30] - state.r[8];
	// lis r5,24576
	state.r[5] = 1610612736;
	// addi r11,r11,16
	state.r[11] = state.r[11] + 16;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// ori r5,r5,4096
	state.r[5] = state.r[5] | 4096;
	// addi r4,r1,80
	state.r[4] = state.r[1] + 80;
	// addi r3,r1,236
	state.r[3] = state.r[1] + 236;
	// stw r11,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[11]));
	// bl 0x830d9d1c
	state.lr = 0x827CC360;
	services.CallNative(0x830d9d1cu,memory,state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// blt 0x827cc330
	if (state.cr0.lt) goto loc_827CC330;
	// lwz r10,236(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 236);
	// lwz r11,80(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// add r8,r11,r10
	state.r[8] = state.r[11] + state.r[10];
	// stw r8,236(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 236, Low32(state.r[8]));
loc_827CC378:
	// subf r11,r8,r27
	state.r[11] = state.r[27] - state.r[8];
	// sth r22,2(r31)
	PPC_STORE_U16(Low32(state.r[31]) + 2, Low16(state.r[22]));
	// rlwinm r10,r26,16,0,15
	state.r[10] = Rotate64(Low32(state.r[26]) | (state.r[26] << 32), 16) & 0xFFFF0000;
	// sth r24,0(r31)
	PPC_STORE_U16(Low32(state.r[31]) + 0, Low16(state.r[24]));
	// srawi r11,r11,16
	state.xer_ca = (Signed32(state.r[11]) < 0) && ((Low32(state.r[11]) & 0xFFFF) != 0);
	state.r[11] = Signed32(state.r[11]) >> 16;
	// stb r25,4(r31)
	PPC_STORE_U8(Low32(state.r[31]) + 4, Low8(state.r[25]));
	// lis r7,-18
	state.r[7] = -1179648;
	// stw r23,20(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 20, Low32(state.r[23]));
	// addze. r11,r11
	temp = state.r[11] + state.xer_ca;
	state.xer_ca = Low32(temp) < Low32(state.r[11]);
	state.r[11] = temp;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// stw r28,24(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 24, Low32(state.r[28]));
	// li r9,1
	state.r[9] = 1;
	// stw r29,32(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 32, Low32(state.r[29]));
	// ori r7,r7,65518
	state.r[7] = state.r[7] | 65518;
	// stw r30,40(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 40, Low32(state.r[30]));
	// add r10,r10,r29
	state.r[10] = state.r[10] + state.r[29];
	// stw r26,36(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 36, Low32(state.r[26]));
	// stw r11,48(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 48, Low32(state.r[11]));
	// stb r9,5(r31)
	PPC_STORE_U8(Low32(state.r[31]) + 5, Low8(state.r[9]));
	// stw r7,16(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 16, Low32(state.r[7]));
	// stw r10,44(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 44, Low32(state.r[10]));
	// beq 0x827cc3e0
	if (state.cr0.eq) goto loc_827CC3E0;
	// rlwinm r5,r11,16,0,15
	state.r[5] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 16) & 0xFFFF0000;
	// mr r4,r8
	state.r[4] = state.r[8];
	// mr r3,r31
	state.r[3] = state.r[31];
	// bl 0x827cb658
	state.lr = 0x827CC3DC;
	(void)heap_range_context::Apply(0x827cb658u,memory,services,state);
	// lwz r8,236(r1)
	state.r[8] = PPC_LOAD_U32(Low32(state.r[1]) + 236);
loc_827CC3E0:
	// clrlwi r11,r25,24
	state.r[11] = Low32(state.r[25]) & 0xFF;
	// li r10,16
	state.r[10] = 16;
	// addi r11,r11,24
	state.r[11] = state.r[11] + 24;
	// subf r9,r30,r8
	state.r[9] = state.r[8] - state.r[30];
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// mr r4,r30
	state.r[4] = state.r[30];
	// srawi r5,r9,4
	state.xer_ca = (Signed32(state.r[9]) < 0) && ((Low32(state.r[9]) & 0xF) != 0);
	state.r[5] = Signed32(state.r[9]) >> 4;
	// mr r3,r28
	state.r[3] = state.r[28];
	// stwx r31,r11,r28
	PPC_STORE_U32(Low32(state.r[11]) + Low32(state.r[28]), Low32(state.r[31]));
	// lhz r11,0(r31)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[31]) + 0);
	// stb r10,5(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 5, Low8(state.r[10]));
	// stw r30,64(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 64, Low32(state.r[30]));
	// stb r25,4(r30)
	PPC_STORE_U8(Low32(state.r[30]) + 4, Low8(state.r[25]));
	// sth r11,2(r30)
	PPC_STORE_U16(Low32(state.r[30]) + 2, Low16(state.r[11]));
	// bl 0x827cba60
	state.lr = 0x827CC41C;
	(void)heap_insert_context::Apply(0x827cba60u,memory,state);
	// li r3,1
	state.r[3] = 1;
loc_827CC420:
	// addi r1,r1,176
	state.r[1] = state.r[1] + 176;
	// b 0x82b7a720
	Restore22(memory,state);
	return;
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    BoundaryServices& services,Registers& state)
{
    switch(entry)
    {
    case 0x827cb778u:Commit(memory,services,state);return true;
    case 0x827cc2c0u:CreateSegment(memory,services,state);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::heap_growth_lower_context

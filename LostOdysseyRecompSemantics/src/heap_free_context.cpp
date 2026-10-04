#include "lo_semantics/heap_free_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_free_context
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
#define PPC_LOAD_U64(a) ReadU64(memory,Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a),Low8(v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a),Low32(v))
#define PPC_STORE_U64(a,v) WriteU64(memory,Address(a),(v))

void Cleanup(GuestMemory& memory,BoundaryServices& services,Registers& state);
void Free(GuestMemory& memory,BoundaryServices& services,Registers& state)
{

	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6dc
	state.lr = 0x823ADE30;
	Save25(memory,state);
	// addi r31,r1,-176
	state.r[31] = state.r[1] + -176;
	// stwu r1,-176(r1)
	temp = state.r[1] + uint64_t(-176);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r30,r3
	state.r[30] = state.r[3];
	// mr r28,r4
	state.r[28] = state.r[4];
	// mr r29,r5
	state.r[29] = state.r[5];
	// lwz r11,20(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 20);
	// stw r30,100(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 100, Low32(state.r[30]));
	// li r26,0
	state.r[26] = 0;
	// mr r25,r26
	state.r[25] = state.r[26];
	// stw r25,84(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 84, Low32(state.r[25]));
	// rlwinm. r11,r11,0,13,13
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x40000;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// li r27,1
	state.r[27] = 1;
	// stw r27,88(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 88, Low32(state.r[27]));
	// beq 0x823ade90
	if (state.cr0.eq) goto loc_823ADE90;
	// bl 0x830da07c
	state.lr = 0x823ADE6C;
	services.CallNative(0x830da07cu,memory,state);
	// lbz r11,379(r30)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[30]) + 379);
	// cmpw cr6,r11,r3
	Compare<int32_t>(state.cr6, Signed32(state.r[11]), Signed32(state.r[3]), state.xer_so);
	// beq cr6,0x823ade90
	if (state.cr6.eq) goto loc_823ADE90;
	// mr r7,r29
	state.r[7] = state.r[29];
	// li r6,4390
	state.r[6] = 4390;
	// lwz r5,168(r31)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[31]) + 168);
	// mr r4,r30
	state.r[4] = state.r[30];
	// li r3,244
	state.r[3] = 244;
	// bl 0x830da06c
	state.lr = 0x823ADE90;
	services.CallNative(0x830da06cu,memory,state);
loc_823ADE90:
	// cmplwi cr6,r29,0
	Compare<uint32_t>(state.cr6, Low32(state.r[29]), 0, state.xer_so);
	// bne cr6,0x823adea0
	if (!state.cr6.eq) goto loc_823ADEA0;
	// li r3,1
	state.r[3] = 1;
	// b 0x823ae08c
	goto loc_823AE08C;
loc_823ADEA0:
	// lwz r11,24(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 24);
	// addi r29,r29,-16
	state.r[29] = state.r[29] + -16;
	// or r11,r11,r28
	state.r[11] = state.r[11] | state.r[28];
	// mr r8,r8
	state.r[8] = state.r[8];
	// clrlwi. r11,r11,31
	state.r[11] = Low32(state.r[11]) & 0x1;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// bne 0x823adec8
	if (!state.cr0.eq) goto loc_823ADEC8;
	// lwz r3,1408(r30)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[30]) + 1408);
	// bl 0x830d9c6c
	state.lr = 0x823ADEC0;
	services.CallNative(0x830d9c6cu,memory,state);
	// mr r25,r27
	state.r[25] = state.r[27];
	// stw r25,84(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 84, Low32(state.r[25]));
loc_823ADEC8:
	// lbz r11,5(r29)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[29]) + 5);
	// rlwinm. r11,r11,0,28,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x8;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// bne 0x823ae024
	if (!state.cr0.eq) goto loc_823AE024;
	// lhz r11,0(r29)
	state.r[11] = PPC_LOAD_U16(Low32(state.r[29]) + 0);
	// stw r11,80(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 80, Low32(state.r[11]));
	// li r6,0
	state.r[6] = 0;
	// addi r5,r31,80
	state.r[5] = state.r[31] + 80;
	// mr r4,r29
	state.r[4] = state.r[29];
	// mr r3,r30
	state.r[3] = state.r[30];
	// bl 0x823ae108
	state.lr = 0x823ADEF0;
	services.CallDirect(0x823ae108u,memory,state);
	// mr r4,r3
	state.r[4] = state.r[3];
	// lwz r5,80(r31)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// cmplwi cr6,r5,128
	Compare<uint32_t>(state.cr6, Low32(state.r[5]), 128, state.xer_so);
	// bge cr6,0x823adf74
	if (!state.cr6.lt) goto loc_823ADF74;
	// lbz r11,5(r4)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[4]) + 5);
	// rlwinm r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	// stb r11,5(r4)
	PPC_STORE_U8(Low32(state.r[4]) + 5, Low8(state.r[11]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// clrlwi r11,r11,16
	state.r[11] = Low32(state.r[11]) & 0xFFFF;
	// addi r11,r11,48
	state.r[11] = state.r[11] + 48;
	// rlwinm r11,r11,3,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r30
	state.r[11] = state.r[11] + state.r[30];
	// lwz r10,0(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// bne cr6,0x823adf50
	if (!state.cr6.eq) goto loc_823ADF50;
	// lhz r10,0(r4)
	state.r[10] = PPC_LOAD_U16(Low32(state.r[4]) + 0);
	// rlwinm r9,r10,27,5,31
	state.r[9] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 27) & 0x7FFFFFF;
	// clrlwi r10,r10,27
	state.r[10] = Low32(state.r[10]) & 0x1F;
	// slw r8,r27,r10
	state.r[8] = Low8(state.r[10]) & 0x20 ? 0 : (Low32(state.r[27]) << (Low8(state.r[10]) & 0x3F));
	// addi r10,r9,88
	state.r[10] = state.r[9] + 88;
	// rlwinm r10,r10,2,0,29
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r10,r30
	state.r[9] = PPC_LOAD_U32(Low32(state.r[10]) + Low32(state.r[30]));
	// or r9,r9,r8
	state.r[9] = state.r[9] | state.r[8];
	// stwx r9,r10,r30
	PPC_STORE_U32(Low32(state.r[10]) + Low32(state.r[30]), Low32(state.r[9]));
loc_823ADF50:
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r9,r4,8
	state.r[9] = state.r[4] + 8;
	// stw r11,8(r4)
	PPC_STORE_U32(Low32(state.r[4]) + 8, Low32(state.r[11]));
	// stw r10,12(r4)
	PPC_STORE_U32(Low32(state.r[4]) + 12, Low32(state.r[10]));
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// stw r9,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[9]));
	// lwz r11,48(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 48);
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// b 0x823ae00c
	goto loc_823AE00C;
loc_823ADF74:
	// lwz r11,40(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 40);
	// cmplw cr6,r5,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[5]), Low32(state.r[11]), state.xer_so);
	// blt cr6,0x823adfa0
	if (state.cr6.lt) goto loc_823ADFA0;
	// lwz r11,48(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 48);
	// lwz r10,44(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 44);
	// add r11,r11,r5
	state.r[11] = state.r[11] + state.r[5];
	// cmplw cr6,r11,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[10]), state.xer_so);
	// blt cr6,0x823adfa0
	if (state.cr6.lt) goto loc_823ADFA0;
	// mr r3,r30
	state.r[3] = state.r[30];
	// bl 0x827cc668
	state.lr = 0x823ADF9C;
	services.CallDirect(0x827cc668u,memory,state);
	// b 0x823ae07c
	goto loc_823AE07C;
loc_823ADFA0:
	// cmplwi cr6,r5,61440
	Compare<uint32_t>(state.cr6, Low32(state.r[5]), 61440, state.xer_so);
	// bgt cr6,0x823ae018
	if (state.cr6.gt) goto loc_823AE018;
	// addi r10,r30,384
	state.r[10] = state.r[30] + 384;
	// lbz r11,5(r4)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[4]) + 5);
	// rlwinm r11,r11,0,27,27
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x10;
	// stb r11,5(r4)
	PPC_STORE_U8(Low32(state.r[4]) + 5, Low8(state.r[11]));
	// lwz r11,0(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 0);
	// stw r11,92(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 92, Low32(state.r[11]));
loc_823ADFC0:
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// beq cr6,0x823adfec
	if (state.cr6.eq) goto loc_823ADFEC;
	// lwz r9,80(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// clrlwi r9,r9,16
	state.r[9] = Low32(state.r[9]) & 0xFFFF;
	// lhz r8,-8(r11)
	state.r[8] = PPC_LOAD_U16(Low32(state.r[11]) + -8);
	// cmplw cr6,r9,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[8]), state.xer_so);
	// ble cr6,0x823adfec
	if (!state.cr6.gt) goto loc_823ADFEC;
	// lwz r11,0(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// stw r11,92(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 92, Low32(state.r[11]));
	// mr r8,r8
	state.r[8] = state.r[8];
	// b 0x823adfc0
	goto loc_823ADFC0;
loc_823ADFEC:
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r9,r4,8
	state.r[9] = state.r[4] + 8;
	// stw r11,8(r4)
	PPC_STORE_U32(Low32(state.r[4]) + 8, Low32(state.r[11]));
	// stw r10,12(r4)
	PPC_STORE_U32(Low32(state.r[4]) + 12, Low32(state.r[10]));
	// stw r9,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[9]));
	// stw r9,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[9]));
	// lwz r10,48(r30)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[30]) + 48);
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
loc_823AE00C:
	// add r11,r11,r10
	state.r[11] = state.r[11] + state.r[10];
	// stw r11,48(r30)
	PPC_STORE_U32(Low32(state.r[30]) + 48, Low32(state.r[11]));
	// b 0x823ae07c
	goto loc_823AE07C;
loc_823AE018:
	// mr r3,r30
	state.r[3] = state.r[30];
	// bl 0x827cba60
	state.lr = 0x823AE020;
	services.CallDirect(0x827cba60u,memory,state);
	// b 0x823ae07c
	goto loc_823AE07C;
loc_823AE024:
	// addi r11,r29,-32
	state.r[11] = state.r[29] + -32;
	// stw r11,96(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 96, Low32(state.r[11]));
	// lwz r10,0(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// lwz r11,4(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
	// stw r11,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[11]));
	// cmplwi cr6,r25,0
	Compare<uint32_t>(state.cr6, Low32(state.r[25]), 0, state.xer_so);
	// beq cr6,0x823ae054
	if (state.cr6.eq) goto loc_823AE054;
	// lwz r3,1408(r30)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[30]) + 1408);
	// bl 0x830d9c7c
	state.lr = 0x823AE04C;
	services.CallNative(0x830d9c7cu,memory,state);
	// mr r25,r26
	state.r[25] = state.r[26];
	// stw r25,84(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 84, Low32(state.r[25]));
loc_823AE054:
	// li r6,0
	state.r[6] = 0;
	// lis r5,0
	state.r[5] = 0;
	// ori r5,r5,32768
	state.r[5] = state.r[5] | 32768;
	// addi r4,r31,80
	state.r[4] = state.r[31] + 80;
	// addi r3,r31,96
	state.r[3] = state.r[31] + 96;
	// stw r26,80(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 80, Low32(state.r[26]));
	// bl 0x830d9d3c
	state.lr = 0x823AE070;
	services.CallNative(0x830d9d3cu,memory,state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// bge 0x823ae07c
	if (!state.cr0.lt) goto loc_823AE07C;
	// stw r26,88(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 88, Low32(state.r[26]));
loc_823AE07C:
	// mr r8,r8
	state.r[8] = state.r[8];
	// addi r12,r31,176
	state.r[12] = state.r[31] + 176;
	// bl 0x823ae0bc
	state.lr = 0x823AE088;
	Cleanup(memory,services,state);
	// lwz r3,88(r31)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[31]) + 88);
loc_823AE08C:
	// addi r1,r31,176
	state.r[1] = state.r[31] + 176;
	// b 0x82b7a72c
	Restore25(memory,state);
	return;
}
void Cleanup(GuestMemory& memory,BoundaryServices& services,Registers& state)
{

	std::uint64_t temp=0;
	// std r31,-8(r1)
	PPC_STORE_U64(Low32(state.r[1]) + -8, state.r[31]);
	// addi r31,r12,-176
	state.r[31] = state.r[12] + -176;
	// std r30,-16(r1)
	PPC_STORE_U64(Low32(state.r[1]) + -16, state.r[30]);
	// std r25,-24(r1)
	PPC_STORE_U64(Low32(state.r[1]) + -24, state.r[25]);
	// mflr r12
	state.r[12] = state.lr;
	// stw r12,-32(r1)
	PPC_STORE_U32(Low32(state.r[1]) + -32, Low32(state.r[12]));
	// stwu r1,-112(r1)
	temp = state.r[1] + uint64_t(-112);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// cmplwi cr6,r25,0
	Compare<uint32_t>(state.cr6, Low32(state.r[25]), 0, state.xer_so);
	// beq cr6,0x823ae0e8
	if (state.cr6.eq) goto loc_823AE0E8;
	// lwz r3,1408(r30)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[30]) + 1408);
	// bl 0x830d9c7c
	state.lr = 0x823AE0E8;
	services.CallNative(0x830d9c7cu,memory,state);
loc_823AE0E8:
	// lwz r1,0(r1)
	state.r[1] = PPC_LOAD_U32(Low32(state.r[1]) + 0);
	// ld r31,-8(r1)
	state.r[31] = PPC_LOAD_U64(Low32(state.r[1]) + -8);
	// ld r30,-16(r1)
	state.r[30] = PPC_LOAD_U64(Low32(state.r[1]) + -16);
	// ld r25,-24(r1)
	state.r[25] = PPC_LOAD_U64(Low32(state.r[1]) + -24);
	// lwz r12,-32(r1)
	state.r[12] = PPC_LOAD_U32(Low32(state.r[1]) + -32);
	// mtlr r12
	state.lr = state.r[12];
	// blr
	return;
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,BoundaryServices& services,
    Registers& state)
{
    if(entry==0x823ade28u){Free(memory,services,state);return true;}
    if(entry==0x823ae0bcu){Cleanup(memory,services,state);return true;}
    return false;
}
} // namespace lo::semantic::gpu::heap_free_context

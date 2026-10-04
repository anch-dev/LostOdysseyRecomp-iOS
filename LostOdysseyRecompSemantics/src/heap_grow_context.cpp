#include "lo_semantics/heap_grow_context.h"
#include "lo_semantics/heap_insert_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_grow_context
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

void Save27(GuestMemory& memory,Registers& state)
{
    for(unsigned index=27u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore27(GuestMemory& memory,Registers& state)
{
    for(unsigned index=27u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}

#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a),static_cast<std::uint8_t>(v))
#define PPC_STORE_U16(a,v) memory.WriteU16(Address(a),static_cast<std::uint16_t>(v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a),Low32(v))

void Grow(GuestMemory& memory,BoundaryServices& services,Registers& state)
{
	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6e4
	state.lr = 0x827CC430;
	Save27(memory,state);
	// stwu r1,-144(r1)
	temp = state.r[1] + uint64_t(-144);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r28,r4
	state.r[28] = state.r[4];
	// mr r31,r3
	state.r[31] = state.r[3];
	// addis r10,r28,1
	state.r[10] = state.r[28] + 65536;
	// li r27,64
	state.r[27] = 64;
	// addi r10,r10,-1
	state.r[10] = state.r[10] + -1;
	// li r11,0
	state.r[11] = 0;
	// rlwinm r29,r10,16,16,31
	state.r[29] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 16) & 0xFFFF;
	// li r30,0
	state.r[30] = 0;
	// rlwinm r10,r29,16,0,15
	state.r[10] = Rotate64(Low32(state.r[29]) | (state.r[29] << 32), 16) & 0xFFFF0000;
	// stw r10,88(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 88, Low32(state.r[10]));
loc_827CC45C:
	// addi r10,r30,24
	state.r[10] = state.r[30] + 24;
	// rlwinm r10,r10,2,0,29
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 2) & 0xFFFFFFFC;
	// lwzx r4,r10,r31
	state.r[4] = PPC_LOAD_U32(Low32(state.r[10]) + Low32(state.r[31]));
	// cmplwi r4,0
	Compare<uint32_t>(state.cr0, Low32(state.r[4]), 0, state.xer_so);
	// stw r4,84(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 84, Low32(state.r[4]));
	// beq 0x827cc4e0
	if (state.cr0.eq) goto loc_827CC4E0;
	// lwz r11,48(r4)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[4]) + 48);
	// cmplw cr6,r29,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[29]), Low32(state.r[11]), state.xer_so);
	// bgt cr6,0x827cc4f0
	if (state.cr6.gt) goto loc_827CC4F0;
	// lwz r11,28(r4)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[4]) + 28);
	// lwz r10,88(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 88);
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// bgt cr6,0x827cc4f0
	if (state.cr6.gt) goto loc_827CC4F0;
	// li r6,0
	state.r[6] = 0;
	// addi r5,r1,88
	state.r[5] = state.r[1] + 88;
	// mr r3,r31
	state.r[3] = state.r[31];
	// bl 0x827cb778
	state.lr = 0x827CC4A0;
	services.CallDirect(0x827cb778u,memory,state);
	// mr. r4,r3
	state.r[4] = state.r[3];
	Compare<int32_t>(state.cr0, Signed32(state.r[4]), 0, state.xer_so);
	// beq 0x827cc4f0
	if (state.cr0.eq) goto loc_827CC4F0;
	// lwz r11,88(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 88);
	// li r6,0
	state.r[6] = 0;
	// addi r5,r1,88
	state.r[5] = state.r[1] + 88;
	// rlwinm r11,r11,28,4,31
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 28) & 0xFFFFFFF;
	// mr r3,r31
	state.r[3] = state.r[31];
	// stw r11,88(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 88, Low32(state.r[11]));
	// bl 0x823ae108
	state.lr = 0x827CC4C4;
	(void)heap_coalesce_context::Apply(0x823ae108u,memory,services,state);
	// mr r30,r3
	state.r[30] = state.r[3];
	// lwz r5,88(r1)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[1]) + 88);
	// mr r3,r31
	state.r[3] = state.r[31];
	// mr r4,r30
	state.r[4] = state.r[30];
	// bl 0x827cba60
	state.lr = 0x827CC4D8;
	(void)heap_insert_context::Apply(0x827cba60u,memory,state);
	// mr r3,r30
	state.r[3] = state.r[30];
	// b 0x827cc65c
	goto loc_827CC65C;
loc_827CC4E0:
	// clrlwi r10,r27,24
	state.r[10] = Low32(state.r[27]) & 0xFF;
	// cmplwi cr6,r10,64
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), 64, state.xer_so);
	// bne cr6,0x827cc4f0
	if (!state.cr6.eq) goto loc_827CC4F0;
	// mr r27,r11
	state.r[27] = state.r[11];
loc_827CC4F0:
	// addi r11,r30,1
	state.r[11] = state.r[30] + 1;
	// clrlwi r11,r11,24
	state.r[11] = Low32(state.r[11]) & 0xFF;
	// mr r30,r11
	state.r[30] = state.r[11];
	// cmplwi cr6,r30,64
	Compare<uint32_t>(state.cr6, Low32(state.r[30]), 64, state.xer_so);
	// blt cr6,0x827cc45c
	if (state.cr6.lt) goto loc_827CC45C;
	// clrlwi r11,r27,24
	state.r[11] = Low32(state.r[27]) & 0xFF;
	// cmplwi cr6,r11,64
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), 64, state.xer_so);
	// beq cr6,0x827cc658
	if (state.cr6.eq) goto loc_827CC658;
	// lwz r11,20(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 20);
	// rlwinm. r11,r11,0,30,30
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x2;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x827cc658
	if (state.cr0.eq) goto loc_827CC658;
	// addis r30,r28,1
	state.r[30] = state.r[28] + 65536;
	// lwz r11,32(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 32);
	// li r10,0
	state.r[10] = 0;
	// cmplw cr6,r30,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[30]), Low32(state.r[11]), state.xer_so);
	// stw r30,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[30]));
	// stw r10,84(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 84, Low32(state.r[10]));
	// bgt cr6,0x827cc53c
	if (state.cr6.gt) goto loc_827CC53C;
	// stw r11,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[11]));
loc_827CC53C:
	// lis r5,24576
	state.r[5] = 1610612736;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// ori r5,r5,8192
	state.r[5] = state.r[5] | 8192;
	// addi r4,r1,80
	state.r[4] = state.r[1] + 80;
	// addi r3,r1,84
	state.r[3] = state.r[1] + 84;
	// bl 0x830d9d1c
	state.lr = 0x827CC558;
	services.CallNative(0x830d9d1cu,memory,state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// bge 0x827cc5ac
	if (!state.cr0.lt) goto loc_827CC5AC;
loc_827CC560:
	// lwz r11,80(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// cmplw cr6,r11,r30
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[30]), state.xer_so);
	// beq cr6,0x827cc5a4
	if (state.cr6.eq) goto loc_827CC5A4;
	// rlwinm r11,r11,31,1,31
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 31) & 0x7FFFFFFF;
	// cmplw cr6,r11,r30
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[30]), state.xer_so);
	// stw r11,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[11]));
	// bge cr6,0x827cc580
	if (!state.cr6.lt) goto loc_827CC580;
	// stw r30,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[30]));
loc_827CC580:
	// lis r5,24576
	state.r[5] = 1610612736;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// ori r5,r5,8192
	state.r[5] = state.r[5] | 8192;
	// addi r4,r1,80
	state.r[4] = state.r[1] + 80;
	// addi r3,r1,84
	state.r[3] = state.r[1] + 84;
	// bl 0x830d9d1c
	state.lr = 0x827CC59C;
	services.CallNative(0x830d9d1cu,memory,state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// blt 0x827cc560
	if (state.cr0.lt) goto loc_827CC560;
loc_827CC5A4:
	// cmpwi cr6,r3,0
	Compare<int32_t>(state.cr6, Signed32(state.r[3]), 0, state.xer_so);
	// blt cr6,0x827cc658
	if (state.cr6.lt) goto loc_827CC658;
loc_827CC5AC:
	// lwz r10,32(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 32);
	// lwz r9,80(r1)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// lwz r11,36(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 36);
	// add r10,r10,r9
	state.r[10] = state.r[10] + state.r[9];
	// stw r30,92(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 92, Low32(state.r[30]));
	// cmplw cr6,r30,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[30]), Low32(state.r[11]), state.xer_so);
	// stw r10,32(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 32, Low32(state.r[10]));
	// bgt cr6,0x827cc5d0
	if (state.cr6.gt) goto loc_827CC5D0;
	// stw r11,92(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 92, Low32(state.r[11]));
loc_827CC5D0:
	// lis r5,24576
	state.r[5] = 1610612736;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// ori r5,r5,4096
	state.r[5] = state.r[5] | 4096;
	// addi r4,r1,92
	state.r[4] = state.r[1] + 92;
	// addi r3,r1,84
	state.r[3] = state.r[1] + 84;
	// bl 0x830d9d1c
	state.lr = 0x827CC5EC;
	services.CallNative(0x830d9d1cu,memory,state);
	// mr. r30,r3
	state.r[30] = state.r[3];
	Compare<int32_t>(state.cr0, Signed32(state.r[30]), 0, state.xer_so);
	// blt 0x827cc640
	if (state.cr0.lt) goto loc_827CC640;
	// lwz r7,84(r1)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[1]) + 84);
	// li r6,0
	state.r[6] = 0;
	// lwz r11,80(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// mr r5,r27
	state.r[5] = state.r[27];
	// mr r4,r7
	state.r[4] = state.r[7];
	// add r9,r11,r7
	state.r[9] = state.r[11] + state.r[7];
	// lwz r11,92(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 92);
	// mr r3,r31
	state.r[3] = state.r[31];
	// add r8,r11,r7
	state.r[8] = state.r[11] + state.r[7];
	// bl 0x827cc2c0
	state.lr = 0x827CC61C;
	services.CallDirect(0x827cc2c0u,memory,state);
	// cmplwi r3,0
	Compare<uint32_t>(state.cr0, Low32(state.r[3]), 0, state.xer_so);
	// bne 0x827cc62c
	if (!state.cr0.eq) goto loc_827CC62C;
	// lis r30,-16384
	state.r[30] = -1073741824;
	// ori r30,r30,23
	state.r[30] = state.r[30] | 23;
loc_827CC62C:
	// cmpwi cr6,r30,0
	Compare<int32_t>(state.cr6, Signed32(state.r[30]), 0, state.xer_so);
	// blt cr6,0x827cc640
	if (state.cr6.lt) goto loc_827CC640;
	// lwz r11,84(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 84);
	// lwz r3,40(r11)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[11]) + 40);
	// b 0x827cc65c
	goto loc_827CC65C;
loc_827CC640:
	// lis r5,0
	state.r[5] = 0;
	// li r6,0
	state.r[6] = 0;
	// ori r5,r5,32768
	state.r[5] = state.r[5] | 32768;
	// addi r4,r1,80
	state.r[4] = state.r[1] + 80;
	// addi r3,r1,84
	state.r[3] = state.r[1] + 84;
	// bl 0x830d9d3c
	state.lr = 0x827CC658;
	services.CallNative(0x830d9d3cu,memory,state);
loc_827CC658:
	// li r3,0
	state.r[3] = 0;
loc_827CC65C:
	// addi r1,r1,144
	state.r[1] = state.r[1] + 144;
	// b 0x82b7a734
	Restore27(memory,state);
	return;
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    BoundaryServices& services,Registers& state)
{
    if(entry!=0x827cc428u) return false;
    Grow(memory,services,state);
    return true;
}
} // namespace lo::semantic::gpu::heap_grow_context

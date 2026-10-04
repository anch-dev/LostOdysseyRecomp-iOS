#include "lo_semantics/heap_range_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_range_context
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

template<typename T>
void Compare(object_child_float::Condition& condition,T left,T right,
    std::uint8_t so)
{
    condition={std::uint8_t(left<right),std::uint8_t(left>right),
        std::uint8_t(left==right),so};
}

void Save28(GuestMemory& memory,Registers& state)
{
    for(unsigned index=28u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore28(GuestMemory& memory,Registers& state)
{
    for(unsigned index=28u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}

#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(memory,Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a),static_cast<std::uint8_t>(v))
#define PPC_STORE_U16(a,v) memory.WriteU16(Address(a),static_cast<std::uint16_t>(v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a),Low32(v))
#define PPC_STORE_U64(a,v) WriteU64(memory,Address(a),v)

void Acquire(GuestMemory& memory,BoundaryServices& services,Registers& state)
{
	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(Low32(state.r[1]) + -8, Low32(state.r[12]));
	// std r30,-24(r1)
	PPC_STORE_U64(Low32(state.r[1]) + -24, state.r[30]);
	// std r31,-16(r1)
	PPC_STORE_U64(Low32(state.r[1]) + -16, state.r[31]);
	// stwu r1,-128(r1)
	temp = state.r[1] + uint64_t(-128);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r31,r3
	state.r[31] = state.r[3];
	// lwz r10,24(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 24);
	// addi r11,r10,76
	state.r[11] = state.r[10] + 76;
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// cmplwi cr6,r9,0
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), 0, state.xer_so);
	// bne cr6,0x827cb634
	if (!state.cr6.eq) goto loc_827CB634;
	// lwz r11,72(r10)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[10]) + 72);
	// li r30,0
	state.r[30] = 0;
	// cmplwi r11,0
	Compare<uint32_t>(state.cr0, Low32(state.r[11]), 0, state.xer_so);
	// stw r11,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[11]));
	// beq 0x827cb540
	if (state.cr0.eq) goto loc_827CB540;
	// lwz r10,8(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 8);
	// lwz r9,4(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// cmplw cr6,r10,r9
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[9]), state.xer_so);
	// beq cr6,0x827cb540
	if (state.cr6.eq) goto loc_827CB540;
	// lis r10,1
	state.r[10] = 65536;
	// lis r5,24576
	state.r[5] = 1610612736;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// ori r5,r5,4096
	state.r[5] = state.r[5] | 4096;
	// stw r10,84(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 84, Low32(state.r[10]));
	// addi r4,r1,84
	state.r[4] = state.r[1] + 84;
	// lwz r10,8(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 8);
	// addi r3,r1,92
	state.r[3] = state.r[1] + 92;
	// add r11,r10,r11
	state.r[11] = state.r[10] + state.r[11];
	// stw r11,92(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 92, Low32(state.r[11]));
	// bl 0x830d9d1c
	state.lr = 0x827CB518;
	services.CallNative(0x830d9d1cu,memory,state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// blt 0x827cb570
	if (state.cr0.lt) goto loc_827CB570;
	// lwz r11,80(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// lwz r9,84(r1)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[1]) + 84);
	// lwz r10,8(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 8);
	// add r10,r10,r9
	state.r[10] = state.r[10] + state.r[9];
	// stw r10,8(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 8, Low32(state.r[10]));
	// lwz r10,80(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// lwz r11,92(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 92);
	// b 0x827cb600
	goto loc_827CB600;
loc_827CB540:
	// lis r11,16
	state.r[11] = 1048576;
	// stw r30,80(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 80, Low32(state.r[30]));
	// lis r5,24576
	state.r[5] = 1610612736;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// ori r5,r5,8192
	state.r[5] = state.r[5] | 8192;
	// addi r4,r1,88
	state.r[4] = state.r[1] + 88;
	// stw r11,88(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 88, Low32(state.r[11]));
	// addi r3,r1,80
	state.r[3] = state.r[1] + 80;
	// bl 0x830d9d1c
	state.lr = 0x827CB568;
	services.CallNative(0x830d9d1cu,memory,state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// bge 0x827cb578
	if (!state.cr0.lt) goto loc_827CB578;
loc_827CB570:
	// li r3,0
	state.r[3] = 0;
	// b 0x827cb640
	goto loc_827CB640;
loc_827CB578:
	// lis r11,1
	state.r[11] = 65536;
	// lis r5,24576
	state.r[5] = 1610612736;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// ori r5,r5,4096
	state.r[5] = state.r[5] | 4096;
	// addi r4,r1,84
	state.r[4] = state.r[1] + 84;
	// stw r11,84(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 84, Low32(state.r[11]));
	// addi r3,r1,80
	state.r[3] = state.r[1] + 80;
	// bl 0x830d9d1c
	state.lr = 0x827CB59C;
	services.CallNative(0x830d9d1cu,memory,state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// bge 0x827cb5c0
	if (!state.cr0.lt) goto loc_827CB5C0;
	// lis r5,0
	state.r[5] = 0;
	// li r6,0
	state.r[6] = 0;
	// ori r5,r5,32768
	state.r[5] = state.r[5] | 32768;
	// addi r4,r1,88
	state.r[4] = state.r[1] + 88;
	// addi r3,r1,80
	state.r[3] = state.r[1] + 80;
	// bl 0x830d9d3c
	state.lr = 0x827CB5BC;
	services.CallNative(0x830d9d3cu,memory,state);
	// b 0x827cb570
	goto loc_827CB570;
loc_827CB5C0:
	// lwz r11,24(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 24);
	// lwz r10,80(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// lwz r11,72(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 72);
	// stw r11,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[11]));
	// lwz r11,80(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// lwz r10,24(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 24);
	// stw r11,72(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 72, Low32(state.r[11]));
	// lwz r11,88(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 88);
	// lwz r10,80(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// stw r11,4(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 4, Low32(state.r[11]));
	// lwz r11,84(r1)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[1]) + 84);
	// lwz r10,80(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// stw r11,8(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 8, Low32(state.r[11]));
	// lwz r10,80(r1)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[1]) + 80);
	// addi r11,r10,16
	state.r[11] = state.r[10] + 16;
	// stw r11,92(r1)
	PPC_STORE_U32(Low32(state.r[1]) + 92, Low32(state.r[11]));
loc_827CB600:
	// lwz r9,8(r10)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[10]) + 8);
	// lwz r8,24(r31)
	state.r[8] = PPC_LOAD_U32(Low32(state.r[31]) + 24);
	// add r9,r9,r10
	state.r[9] = state.r[9] + state.r[10];
	// addi r10,r8,76
	state.r[10] = state.r[8] + 76;
	// b 0x827cb620
	goto loc_827CB620;
loc_827CB614:
	// stw r11,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[11]));
	// mr r10,r11
	state.r[10] = state.r[11];
	// addi r11,r11,16
	state.r[11] = state.r[11] + 16;
loc_827CB620:
	// cmplw cr6,r11,r9
	Compare<uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[9]), state.xer_so);
	// blt cr6,0x827cb614
	if (state.cr6.lt) goto loc_827CB614;
	// stw r30,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[30]));
	// lwz r11,24(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 24);
	// addi r11,r11,76
	state.r[11] = state.r[11] + 76;
loc_827CB634:
	// lwz r3,0(r11)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// lwz r10,0(r3)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[3]) + 0);
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
loc_827CB640:
	// addi r1,r1,128
	state.r[1] = state.r[1] + 128;
	// lwz r12,-8(r1)
	state.r[12] = PPC_LOAD_U32(Low32(state.r[1]) + -8);
	// mtlr r12
	state.lr = state.r[12];
	// ld r30,-24(r1)
	state.r[30] = PPC_LOAD_U64(Low32(state.r[1]) + -24);
	// ld r31,-16(r1)
	state.r[31] = PPC_LOAD_U64(Low32(state.r[1]) + -16);
	// blr
	return;
}

void InsertRange(GuestMemory& memory,BoundaryServices& services,Registers& state)
{
	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6e8
	state.lr = 0x827CB660;
	Save28(memory,state);
	// stwu r1,-128(r1)
	temp = state.r[1] + uint64_t(-128);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r31,r3
	state.r[31] = state.r[3];
	// mr r29,r4
	state.r[29] = state.r[4];
	// addi r28,r31,56
	state.r[28] = state.r[31] + 56;
	// mr r30,r5
	state.r[30] = state.r[5];
	// lwz r11,0(r28)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[28]) + 0);
	// cmplwi r11,0
	Compare<uint32_t>(state.cr0, Low32(state.r[11]), 0, state.xer_so);
	// beq 0x827cb6fc
	if (state.cr0.eq) goto loc_827CB6FC;
	// li r8,0
	state.r[8] = 0;
loc_827CB684:
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// cmplw cr6,r10,r29
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[29]), state.xer_so);
	// bgt cr6,0x827cb744
	if (state.cr6.gt) goto loc_827CB744;
	// lwz r9,8(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 8);
	// add r7,r9,r10
	state.r[7] = state.r[9] + state.r[10];
	// cmplw cr6,r7,r29
	Compare<uint32_t>(state.cr6, Low32(state.r[7]), Low32(state.r[29]), state.xer_so);
	// bne cr6,0x827cb6ec
	if (!state.cr6.eq) goto loc_827CB6EC;
	// add r30,r9,r30
	state.r[30] = state.r[9] + state.r[30];
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// mr r29,r10
	state.r[29] = state.r[10];
	// stw r9,0(r28)
	PPC_STORE_U32(Low32(state.r[28]) + 0, Low32(state.r[9]));
	// lwz r10,24(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 24);
	// lwz r10,76(r10)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[10]) + 76);
	// stw r10,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[10]));
	// lwz r10,24(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 24);
	// stw r11,76(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 76, Low32(state.r[11]));
	// stw r8,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[8]));
	// stw r8,8(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 8, Low32(state.r[8]));
	// lwz r11,52(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 52);
	// lwz r10,28(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 28);
	// addi r11,r11,-1
	state.r[11] = state.r[11] + -1;
	// cmplw cr6,r30,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[30]), Low32(state.r[10]), state.xer_so);
	// stw r11,52(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 52, Low32(state.r[11]));
	// ble cr6,0x827cb6f0
	if (!state.cr6.gt) goto loc_827CB6F0;
	// stw r30,28(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 28, Low32(state.r[30]));
	// b 0x827cb6f0
	goto loc_827CB6F0;
loc_827CB6EC:
	// mr r28,r11
	state.r[28] = state.r[11];
loc_827CB6F0:
	// lwz r11,0(r28)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[28]) + 0);
	// cmplwi r11,0
	Compare<uint32_t>(state.cr0, Low32(state.r[11]), 0, state.xer_so);
	// bne 0x827cb684
	if (!state.cr0.eq) goto loc_827CB684;
loc_827CB6FC:
	// mr r3,r31
	state.r[3] = state.r[31];
	// bl 0x827cb498
	state.lr = 0x827CB704;
	Acquire(memory,services,state);
	// cmplwi r3,0
	Compare<uint32_t>(state.cr0, Low32(state.r[3]), 0, state.xer_so);
	// beq 0x827cb73c
	if (state.cr0.eq) goto loc_827CB73C;
	// stw r29,4(r3)
	PPC_STORE_U32(Low32(state.r[3]) + 4, Low32(state.r[29]));
	// stw r30,8(r3)
	PPC_STORE_U32(Low32(state.r[3]) + 8, Low32(state.r[30]));
	// lwz r11,0(r28)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[28]) + 0);
	// stw r11,0(r3)
	PPC_STORE_U32(Low32(state.r[3]) + 0, Low32(state.r[11]));
	// stw r3,0(r28)
	PPC_STORE_U32(Low32(state.r[28]) + 0, Low32(state.r[3]));
	// lwz r11,52(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 52);
	// lwz r10,28(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 28);
	// addi r11,r11,1
	state.r[11] = state.r[11] + 1;
	// cmplw cr6,r30,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[30]), Low32(state.r[10]), state.xer_so);
	// stw r11,52(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 52, Low32(state.r[11]));
	// blt cr6,0x827cb73c
	if (state.cr6.lt) goto loc_827CB73C;
	// stw r30,28(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 28, Low32(state.r[30]));
loc_827CB73C:
	// addi r1,r1,128
	state.r[1] = state.r[1] + 128;
	// b 0x82b7a738
	Restore28(memory,state);
	return;
loc_827CB744:
	// lwz r10,4(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// add r9,r29,r30
	state.r[9] = state.r[29] + state.r[30];
	// cmplw cr6,r9,r10
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[10]), state.xer_so);
	// bne cr6,0x827cb6fc
	if (!state.cr6.eq) goto loc_827CB6FC;
	// lwz r10,8(r11)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[11]) + 8);
	// stw r29,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[29]));
	// add r10,r30,r10
	state.r[10] = state.r[30] + state.r[10];
	// stw r10,8(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 8, Low32(state.r[10]));
	// lwz r11,28(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 28);
	// cmplw cr6,r10,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// ble cr6,0x827cb73c
	if (!state.cr6.gt) goto loc_827CB73C;
	// stw r10,28(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 28, Low32(state.r[10]));
	// b 0x827cb73c
	goto loc_827CB73C;
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    BoundaryServices& services,Registers& state)
{
    switch(entry)
    {
    case 0x827cb498u:Acquire(memory,services,state);return true;
    case 0x827cb658u:InsertRange(memory,services,state);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::heap_range_context

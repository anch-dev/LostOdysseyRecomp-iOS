#include "lo_semantics/heap_insert_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_insert_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using std::int32_t;
using std::uint32_t;

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

void Save28(GuestMemory& memory,Registers& state)
{
    for(unsigned i=28u;i<=31u;++i)
        WriteU64(memory,Address(state.r[1]-8u*(33u-i)),state.r[i]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore28(GuestMemory& memory,Registers& state)
{
    for(unsigned i=28u;i<=31u;++i)
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

void Insert(GuestMemory& memory,Registers& state)
{

	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6e8
	state.lr = 0x827CBA68;
	Save28(memory,state);
	// lwz r11,48(r3)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[3]) + 48);
	// lbz r29,4(r4)
	state.r[29] = PPC_LOAD_U8(Low32(state.r[4]) + 4);
	// add r10,r11,r5
	state.r[10] = state.r[11] + state.r[5];
	// lhz r6,2(r4)
	state.r[6] = PPC_LOAD_U16(Low32(state.r[4]) + 2);
	// mr r11,r29
	state.r[11] = state.r[29];
	// lbz r28,5(r4)
	state.r[28] = PPC_LOAD_U8(Low32(state.r[4]) + 5);
	// addi r11,r11,24
	state.r[11] = state.r[11] + 24;
	// rlwinm r11,r11,2,0,29
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 2) & 0xFFFFFFFC;
	// lwzx r30,r11,r3
	state.r[30] = PPC_LOAD_U32(Low32(state.r[11]) + Low32(state.r[3]));
	// stw r10,48(r3)
	PPC_STORE_U32(Low32(state.r[3]) + 48, Low32(state.r[10]));
	// b 0x827cbb7c
	goto loc_827CBB7C;
loc_827CBA94:
	// cmplwi cr6,r5,61440
	Compare<uint32_t>(state.cr6, Low32(state.r[5]), 61440, state.xer_so);
	// ble cr6,0x827cbab8
	if (!state.cr6.gt) goto loc_827CBAB8;
	// li r31,-4096
	state.r[31] = -4096;
	// cmplwi cr6,r5,61441
	Compare<uint32_t>(state.cr6, Low32(state.r[5]), 61441, state.xer_so);
	// bne cr6,0x827cbaac
	if (!state.cr6.eq) goto loc_827CBAAC;
	// li r31,-4112
	state.r[31] = -4112;
loc_827CBAAC:
	// li r11,0
	state.r[11] = 0;
	// stb r11,5(r4)
	PPC_STORE_U8(Low32(state.r[4]) + 5, Low8(state.r[11]));
	// b 0x827cbac0
	goto loc_827CBAC0;
loc_827CBAB8:
	// clrlwi r31,r5,16
	state.r[31] = Low32(state.r[5]) & 0xFFFF;
	// stb r28,5(r4)
	PPC_STORE_U8(Low32(state.r[4]) + 5, Low8(state.r[28]));
loc_827CBAC0:
	// lbz r11,5(r4)
	state.r[11] = PPC_LOAD_U8(Low32(state.r[4]) + 5);
	// clrlwi r10,r31,16
	state.r[10] = Low32(state.r[31]) & 0xFFFF;
	// sth r6,2(r4)
	PPC_STORE_U16(Low32(state.r[4]) + 2, Low16(state.r[6]));
	// rlwinm r11,r11,0,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFFFFFF8;
	// stb r29,4(r4)
	PPC_STORE_U8(Low32(state.r[4]) + 4, Low8(state.r[29]));
	// sth r31,0(r4)
	PPC_STORE_U16(Low32(state.r[4]) + 0, Low16(state.r[31]));
	// cmplwi cr6,r10,128
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), 128, state.xer_so);
	// stb r11,5(r4)
	PPC_STORE_U8(Low32(state.r[4]) + 5, Low8(state.r[11]));
	// bge cr6,0x827cbb24
	if (!state.cr6.lt) goto loc_827CBB24;
	// addi r11,r10,48
	state.r[11] = state.r[10] + 48;
	// rlwinm r11,r11,3,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r3
	state.r[11] = state.r[11] + state.r[3];
	// lwz r9,0(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
	// cmplw cr6,r9,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[11]), state.xer_so);
	// bne cr6,0x827cbb48
	if (!state.cr6.eq) goto loc_827CBB48;
	// li r7,1
	state.r[7] = 1;
	// clrlwi r8,r10,27
	state.r[8] = Low32(state.r[10]) & 0x1F;
	// rlwinm r9,r10,27,5,31
	state.r[9] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 27) & 0x7FFFFFF;
	// addi r9,r9,88
	state.r[9] = state.r[9] + 88;
	// rlwinm r9,r9,2,0,29
	state.r[9] = Rotate64(Low32(state.r[9]) | (state.r[9] << 32), 2) & 0xFFFFFFFC;
	// slw r8,r7,r8
	state.r[8] = Low8(state.r[8]) & 0x20 ? 0 : (Low32(state.r[7]) << (Low8(state.r[8]) & 0x3F));
	// lwzx r7,r9,r3
	state.r[7] = PPC_LOAD_U32(Low32(state.r[9]) + Low32(state.r[3]));
	// or r8,r8,r7
	state.r[8] = state.r[8] | state.r[7];
	// stwx r8,r9,r3
	PPC_STORE_U32(Low32(state.r[9]) + Low32(state.r[3]), Low32(state.r[8]));
	// b 0x827cbb48
	goto loc_827CBB48;
loc_827CBB24:
	// addi r9,r3,384
	state.r[9] = state.r[3] + 384;
	// lwz r11,0(r9)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[9]) + 0);
	// b 0x827cbb40
	goto loc_827CBB40;
loc_827CBB30:
	// lhz r8,-8(r11)
	state.r[8] = PPC_LOAD_U16(Low32(state.r[11]) + -8);
	// cmplw cr6,r10,r8
	Compare<uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[8]), state.xer_so);
	// ble cr6,0x827cbb48
	if (!state.cr6.gt) goto loc_827CBB48;
	// lwz r11,0(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 0);
loc_827CBB40:
	// cmplw cr6,r9,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[9]), Low32(state.r[11]), state.xer_so);
	// bne cr6,0x827cbb30
	if (!state.cr6.eq) goto loc_827CBB30;
loc_827CBB48:
	// lwz r9,4(r11)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[11]) + 4);
	// addi r8,r4,8
	state.r[8] = state.r[4] + 8;
	// stw r11,8(r4)
	PPC_STORE_U32(Low32(state.r[4]) + 8, Low32(state.r[11]));
	// rlwinm r7,r10,4,0,27
	state.r[7] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 4) & 0xFFFFFFF0;
	// mr r6,r31
	state.r[6] = state.r[31];
	// subf r5,r10,r5
	state.r[5] = state.r[5] - state.r[10];
	// stw r9,12(r4)
	PPC_STORE_U32(Low32(state.r[4]) + 12, Low32(state.r[9]));
	// add r4,r7,r4
	state.r[4] = state.r[7] + state.r[4];
	// stw r8,0(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 0, Low32(state.r[8]));
	// stw r8,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[8]));
	// lwz r11,44(r30)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[30]) + 44);
	// cmplw cr6,r4,r11
	Compare<uint32_t>(state.cr6, Low32(state.r[4]), Low32(state.r[11]), state.xer_so);
	// bge cr6,0x827cbb90
	if (!state.cr6.lt) goto loc_827CBB90;
loc_827CBB7C:
	// cmplwi cr6,r5,0
	Compare<uint32_t>(state.cr6, Low32(state.r[5]), 0, state.xer_so);
	// bne cr6,0x827cba94
	if (!state.cr6.eq) goto loc_827CBA94;
	// rlwinm. r11,r28,0,27,27
	state.r[11] = Rotate64(Low32(state.r[28]) | (state.r[28] << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// bne 0x827cbb90
	if (!state.cr0.eq) goto loc_827CBB90;
	// sth r6,2(r4)
	PPC_STORE_U16(Low32(state.r[4]) + 2, Low16(state.r[6]));
loc_827CBB90:
	// b 0x82b7a738
	Restore28(memory,state);
	return;
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,Registers& state)
{
    if(entry!=0x827cba60u) return false;
    Insert(memory,state);
    return true;
}
} // namespace lo::semantic::gpu::heap_insert_context

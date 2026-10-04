#include "lo_semantics/heap_create_context.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_create_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
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
void Save21(GuestMemory& memory,Registers& state)
{
    for(unsigned index=21u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore21(GuestMemory& memory,Registers& state)
{
    for(unsigned index=21u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}
void FillSelected(GuestMemory& memory,Registers& state)
{
    const auto destination=Low32(state.r[3]);
    const auto bytes=Low32(state.r[5]);
    const auto padding=(0u-destination)&3u;
    const auto prefix=bytes<padding?bytes:padding;
    const auto remaining=bytes-prefix;
    (void)FillGuestMemory(memory,destination,Low32(state.r[4]),bytes);
    state.r[6]=state.r[3]+prefix+(remaining&~3u);
    state.r[5]-=prefix;
    state.r[4]=(state.r[4]&0xffffffff00000000ull)|
        (std::uint32_t{Low8(state.r[4])}*0x01010101u);
    const auto tail=remaining&3u;
    state.r[0]=tail;
    Compare<std::int32_t>(state.cr0,static_cast<std::int32_t>(tail),0,
        state.xer_so);
    state.ctr=tail==3u?1u:0u;
}
#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(memory,Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a),Low8(v))
#define PPC_STORE_U16(a,v) memory.WriteU16(Address(a),Low16(v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a),Low32(v))
#define PPC_STORE_U64(a,v) WriteU64(memory,Address(a),v)
void Create(GuestMemory& memory,BoundaryServices& services,Registers& state)
{
	std::uint64_t temp=0;
	// mflr r12
	state.r[12] = state.lr;
	// bl 0x82b7a6cc
	state.lr = 0x827CC9D8;
	Save21(memory,state);
	// addi r31,r1,-272
	state.r[31] = state.r[1] + -272;
	// stwu r1,-272(r1)
	temp = state.r[1] + std::uint64_t(-272);
	PPC_STORE_U32(Low32(temp), Low32(state.r[1]));
	state.r[1] = temp;
	// mr r24,r3
	state.r[24] = state.r[3];
	// stw r24,292(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 292, Low32(state.r[24]));
	// mr r25,r4
	state.r[25] = state.r[4];
	// stw r25,300(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 300, Low32(state.r[25]));
	// stw r5,308(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 308, Low32(state.r[5]));
	// stw r6,316(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 316, Low32(state.r[6]));
	// mr r21,r7
	state.r[21] = state.r[7];
	// stw r21,324(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 324, Low32(state.r[21]));
	// li r22,0
	state.r[22] = 0;
	// stw r22,80(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 80, Low32(state.r[22]));
	// mr r30,r22
	state.r[30] = state.r[22];
	// addi r10,r31,128
	state.r[10] = state.r[31] + 128;
	// mr r9,r22
	state.r[9] = state.r[22];
	// li r11,6
	state.r[11] = 6;
	// mtctr r11
	state.ctr = state.r[11];
loc_827CCA1C:
	// std r9,0(r10)
	PPC_STORE_U64(Low32(state.r[10]) + 0, state.r[9]);
	// addi r10,r10,8
	state.r[10] = state.r[10] + 8;
	// bdnz 0x827cca1c
	--state.ctr;
	if (Low32(state.ctr) != 0) goto loc_827CCA1C;
	// cmplwi cr6,r8,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[8]), 0, state.xer_so);
	// beq cr6,0x827cca94
	if (state.cr6.eq) goto loc_827CCA94;
	// mr r8,r8
	state.r[8] = state.r[8];
	// mr r8,r8
	state.r[8] = state.r[8];
	// lwz r11,0(r8)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[8]) + 0);
	// cmplwi cr6,r11,48
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 48, state.xer_so);
	// bne cr6,0x827cca58
	if (!state.cr6.eq) goto loc_827CCA58;
	// li r5,48
	state.r[5] = 48;
	// mr r4,r8
	state.r[4] = state.r[8];
	// addi r3,r31,128
	state.r[3] = state.r[31] + 128;
	// bl 0x82b7c470
	state.lr = 0x827CCA54;
	services.CallGuestMove(memory,state);
	// lwz r5,308(r31)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[31]) + 308);
loc_827CCA58:
	// mr r8,r8
	state.r[8] = state.r[8];
	// mr r8,r8
	state.r[8] = state.r[8];
	// mr r8,r8
	state.r[8] = state.r[8];
	// b 0x827cca80
	goto loc_827CCA80;
	// mr r30,r3
	state.r[30] = state.r[3];
	// li r22,0
	state.r[22] = 0;
	// lwz r21,324(r31)
	state.r[21] = PPC_LOAD_U32(Low32(state.r[31]) + 324);
	// lwz r5,308(r31)
	state.r[5] = PPC_LOAD_U32(Low32(state.r[31]) + 308);
	// lwz r25,300(r31)
	state.r[25] = PPC_LOAD_U32(Low32(state.r[31]) + 300);
	// lwz r24,292(r31)
	state.r[24] = PPC_LOAD_U32(Low32(state.r[31]) + 292);
loc_827CCA80:
	// cmpwi cr6,r30,0
	Compare<std::int32_t>(state.cr6, Signed32(state.r[30]), 0, state.xer_so);
	// bge cr6,0x827cca90
	if (!state.cr6.lt) goto loc_827CCA90;
loc_827CCA88:
	// li r3,0
	state.r[3] = 0;
	// b 0x827ccf60
	goto loc_827CCF60;
loc_827CCA90:
	// lwz r6,316(r31)
	state.r[6] = PPC_LOAD_U32(Low32(state.r[31]) + 316);
loc_827CCA94:
	// lis r23,-31945
	state.r[23] = -2093547520;
	// lwz r11,18536(r23)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[23]) + 18536);
	// rlwinm. r11,r11,0,10,10
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0x200000;
	Compare<std::int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// beq 0x827ccaa8
	if (state.cr0.eq) goto loc_827CCAA8;
	// ori r24,r24,128
	state.r[24] = state.r[24] | 128;
loc_827CCAA8:
	// lwz r11,132(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 132);
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827ccac0
	if (!state.cr6.eq) goto loc_827CCAC0;
	// lis r11,-31970
	state.r[11] = -2095185920;
	// lwz r11,32248(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 32248);
	// stw r11,132(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 132, Low32(state.r[11]));
loc_827CCAC0:
	// lwz r11,136(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 136);
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827ccad8
	if (!state.cr6.eq) goto loc_827CCAD8;
	// lis r11,-31970
	state.r[11] = -2095185920;
	// lwz r11,32252(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 32252);
	// stw r11,136(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 136, Low32(state.r[11]));
loc_827CCAD8:
	// lwz r11,140(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 140);
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827ccaf0
	if (!state.cr6.eq) goto loc_827CCAF0;
	// lis r11,-31970
	state.r[11] = -2095185920;
	// lwz r11,32260(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 32260);
	// stw r11,140(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 140, Low32(state.r[11]));
loc_827CCAF0:
	// lwz r11,144(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 144);
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827ccb08
	if (!state.cr6.eq) goto loc_827CCB08;
	// lis r11,-31970
	state.r[11] = -2095185920;
	// lwz r11,32256(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 32256);
	// stw r11,144(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 144, Low32(state.r[11]));
loc_827CCB08:
	// lwz r11,148(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 148);
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827ccb20
	if (!state.cr6.eq) goto loc_827CCB20;
	// lis r11,32764
	state.r[11] = 2147221504;
	// ori r11,r11,65535
	state.r[11] = state.r[11] | 65535;
	// stw r11,148(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 148, Low32(state.r[11]));
loc_827CCB20:
	// lwz r11,152(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 152);
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// beq cr6,0x827ccb38
	if (state.cr6.eq) goto loc_827CCB38;
	// lis r10,15
	state.r[10] = 983040;
	// cmplw cr6,r11,r10
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[10]), state.xer_so);
	// ble cr6,0x827ccb40
	if (!state.cr6.gt) goto loc_827CCB40;
loc_827CCB38:
	// lis r11,15
	state.r[11] = 983040;
	// stw r11,152(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 152, Low32(state.r[11]));
loc_827CCB40:
	// cmplwi cr6,r6,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[6]), 0, state.xer_so);
	// lis r30,1
	state.r[30] = 65536;
	// bne cr6,0x827ccb9c
	if (!state.cr6.eq) goto loc_827CCB9C;
	// stw r30,316(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 316, Low32(state.r[30]));
	// cmplwi cr6,r5,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[5]), 0, state.xer_so);
	// bne cr6,0x827ccb6c
	if (!state.cr6.eq) goto loc_827CCB6C;
	// lis r11,64
	state.r[11] = 4194304;
	// stw r11,308(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 308, Low32(state.r[11]));
	// lis r11,0
	state.r[11] = 0;
	// ori r27,r11,65535
	state.r[27] = state.r[11] | 65535;
	// b 0x827ccb80
	goto loc_827CCB80;
loc_827CCB6C:
	// lis r11,0
	state.r[11] = 0;
	// ori r27,r11,65535
	state.r[27] = state.r[11] | 65535;
loc_827CCB74:
	// add r11,r5,r27
	state.r[11] = state.r[5] + state.r[27];
	// rlwinm r11,r11,0,0,15
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFFF0000;
loc_827CCB7C:
	// stw r11,308(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 308, Low32(state.r[11]));
loc_827CCB80:
	// clrlwi. r11,r24,31
	state.r[11] = Low32(state.r[24]) & 0x1;
	Compare<std::int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// li r29,1424
	state.r[29] = 1424;
	// cmplwi cr6,r21,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[21]), 0, state.xer_so);
	// bne 0x827ccbd4
	if (!state.cr0.eq) goto loc_827CCBD4;
	// beq cr6,0x827ccbc8
	if (state.cr6.eq) goto loc_827CCBC8;
	// oris r24,r24,32768
	state.r[24] = state.r[24] | 2147483648;
	// b 0x827ccbd8
	goto loc_827CCBD8;
loc_827CCB9C:
	// lis r11,0
	state.r[11] = 0;
	// ori r27,r11,65535
	state.r[27] = state.r[11] | 65535;
	// cmplwi cr6,r5,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[5]), 0, state.xer_so);
	// add r11,r6,r27
	state.r[11] = state.r[6] + state.r[27];
	// rlwinm r11,r11,0,0,15
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFFF0000;
	// stw r11,316(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 316, Low32(state.r[11]));
	// bne cr6,0x827ccb74
	if (!state.cr6.eq) goto loc_827CCB74;
	// addis r11,r11,16
	state.r[11] = state.r[11] + 1048576;
	// addi r11,r11,-1
	state.r[11] = state.r[11] + -1;
	// rlwinm r11,r11,0,0,11
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFF00000;
	// b 0x827ccb7c
	goto loc_827CCB7C;
loc_827CCBC8:
	// li r29,1452
	state.r[29] = 1452;
	// li r21,-1
	state.r[21] = -1;
	// b 0x827ccbd8
	goto loc_827CCBD8;
loc_827CCBD4:
	// bne cr6,0x827cca88
	if (!state.cr6.eq) goto loc_827CCA88;
loc_827CCBD8:
	// cmplwi cr6,r25,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[25]), 0, state.xer_so);
	// lwz r11,164(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 164);
	// beq cr6,0x827ccce0
	if (state.cr6.eq) goto loc_827CCCE0;
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// beq cr6,0x827ccc34
	if (state.cr6.eq) goto loc_827CCC34;
	// lwz r10,156(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 156);
	// cmplwi cr6,r10,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[10]), 0, state.xer_so);
	// beq cr6,0x827cca88
	if (state.cr6.eq) goto loc_827CCA88;
	// lwz r11,160(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 160);
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// beq cr6,0x827cca88
	if (state.cr6.eq) goto loc_827CCA88;
	// cmplw cr6,r10,r11
	Compare<std::uint32_t>(state.cr6, Low32(state.r[10]), Low32(state.r[11]), state.xer_so);
	// bgt cr6,0x827cca88
	if (state.cr6.gt) goto loc_827CCA88;
	// rlwinm. r9,r24,0,30,30
	state.r[9] = Rotate64(Low32(state.r[24]) | (state.r[24] << 32), 0) & 0x2;
	Compare<std::int32_t>(state.cr0, Signed32(state.r[9]), 0, state.xer_so);
	// bne 0x827cca88
	if (!state.cr0.eq) goto loc_827CCA88;
	// lis r5,1
	state.r[5] = 65536;
	// li r4,0
	state.r[4] = 0;
	// mr r3,r25
	state.r[3] = state.r[25];
	// stw r25,84(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 84, Low32(state.r[25]));
	// add r28,r10,r25
	state.r[28] = state.r[10] + state.r[25];
	// stw r11,308(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 308, Low32(state.r[11]));
	// bl 0x82b7bc40
	state.lr = 0x827CCC30;
	FillSelected(memory,state);
	// b 0x827cccd0
	goto loc_827CCCD0;
loc_827CCC34:
	// li r5,0
	state.r[5] = 0;
	// addi r4,r31,96
	state.r[4] = state.r[31] + 96;
	// mr r3,r25
	state.r[3] = state.r[25];
	// bl 0x830da09c
	state.lr = 0x827CCC44;
	services.CallNative(0x830da09cu,memory,state);
	// cmpwi r3,0
	Compare<std::int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// blt 0x827cca88
	if (state.cr0.lt) goto loc_827CCA88;
	// lwz r3,96(r31)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[31]) + 96);
	// cmplw cr6,r3,r25
	Compare<std::uint32_t>(state.cr6, Low32(state.r[3]), Low32(state.r[25]), state.xer_so);
	// bne cr6,0x827cca88
	if (!state.cr6.eq) goto loc_827CCA88;
	// lwz r11,112(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 112);
	// cmplw cr6,r11,r30
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[30]), state.xer_so);
	// beq cr6,0x827cca88
	if (state.cr6.eq) goto loc_827CCA88;
	// stw r3,84(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 84, Low32(state.r[3]));
	// cmplwi cr6,r11,4096
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 4096, state.xer_so);
	// bne cr6,0x827cccc8
	if (!state.cr6.eq) goto loc_827CCCC8;
	// lis r5,1
	state.r[5] = 65536;
	// li r4,0
	state.r[4] = 0;
	// bl 0x82b7bc40
	state.lr = 0x827CCC7C;
	FillSelected(memory,state);
	// lwz r11,108(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 108);
	// lwz r10,84(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// add r28,r11,r10
	state.r[28] = state.r[11] + state.r[10];
	// li r5,0
	state.r[5] = 0;
	// addi r4,r31,96
	state.r[4] = state.r[31] + 96;
	// stw r11,316(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 316, Low32(state.r[11]));
	// mr r3,r28
	state.r[3] = state.r[28];
	// bl 0x830da09c
	state.lr = 0x827CCC9C;
	services.CallNative(0x830da09cu,memory,state);
	// lwz r11,316(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 316);
	// stw r11,308(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 308, Low32(state.r[11]));
	// cmpwi r3,0
	Compare<std::int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// blt 0x827cccd0
	if (state.cr0.lt) goto loc_827CCCD0;
	// lwz r10,112(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 112);
	// cmplwi cr6,r10,8192
	Compare<std::uint32_t>(state.cr6, Low32(state.r[10]), 8192, state.xer_so);
	// bne cr6,0x827cccd0
	if (!state.cr6.eq) goto loc_827CCCD0;
	// lwz r10,108(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 108);
	// add r11,r10,r11
	state.r[11] = state.r[10] + state.r[11];
	// stw r11,308(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 308, Low32(state.r[11]));
	// b 0x827cccd0
	goto loc_827CCCD0;
loc_827CCCC8:
	// stw r30,316(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 316, Low32(state.r[30]));
	// mr r28,r3
	state.r[28] = state.r[3];
loc_827CCCD0:
	// li r26,1
	state.r[26] = 1;
	// mr r10,r25
	state.r[10] = state.r[25];
	// stw r10,80(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 80, Low32(state.r[10]));
	// b 0x827ccd2c
	goto loc_827CCD2C;
loc_827CCCE0:
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827cca88
	if (!state.cr6.eq) goto loc_827CCA88;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// lis r5,24576
	state.r[5] = 1610612736;
	// ori r5,r5,8192
	state.r[5] = state.r[5] | 8192;
	// addi r4,r31,308
	state.r[4] = state.r[31] + 308;
	// addi r3,r31,80
	state.r[3] = state.r[31] + 80;
	// bl 0x830d9d1c
	state.lr = 0x827CCD04;
	services.CallNative(0x830d9d1cu,memory,state);
	// cmpwi r3,0
	Compare<std::int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// blt 0x827cca88
	if (state.cr0.lt) goto loc_827CCA88;
	// mr r26,r22
	state.r[26] = state.r[22];
	// lwz r11,316(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 316);
	// cmplwi cr6,r11,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), 0, state.xer_so);
	// bne cr6,0x827ccd20
	if (!state.cr6.eq) goto loc_827CCD20;
	// stw r30,316(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 316, Low32(state.r[30]));
loc_827CCD20:
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r10,84(r31)
	PPC_STORE_U32(Low32(state.r[31]) + 84, Low32(state.r[10]));
	// mr r28,r10
	state.r[28] = state.r[10];
loc_827CCD2C:
	// lwz r11,84(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// cmplw cr6,r11,r28
	Compare<std::uint32_t>(state.cr6, Low32(state.r[11]), Low32(state.r[28]), state.xer_so);
	// bne cr6,0x827ccd8c
	if (!state.cr6.eq) goto loc_827CCD8C;
	// li r7,0
	state.r[7] = 0;
	// li r6,4
	state.r[6] = 4;
	// lis r5,24576
	state.r[5] = 1610612736;
	// ori r5,r5,4096
	state.r[5] = state.r[5] | 4096;
	// addi r4,r31,316
	state.r[4] = state.r[31] + 316;
	// addi r3,r31,84
	state.r[3] = state.r[31] + 84;
	// bl 0x830d9d1c
	state.lr = 0x827CCD54;
	services.CallNative(0x830d9d1cu,memory,state);
	// cmpwi r3,0
	Compare<std::int32_t>(state.cr0, Signed32(state.r[3]), 0, state.xer_so);
	// bge 0x827ccd80
	if (!state.cr0.lt) goto loc_827CCD80;
	// cmplwi cr6,r25,0
	Compare<std::uint32_t>(state.cr6, Low32(state.r[25]), 0, state.xer_so);
	// bne cr6,0x827cca88
	if (!state.cr6.eq) goto loc_827CCA88;
	// li r6,0
	state.r[6] = 0;
	// lis r5,0
	state.r[5] = 0;
	// ori r5,r5,32768
	state.r[5] = state.r[5] | 32768;
	// addi r4,r31,308
	state.r[4] = state.r[31] + 308;
	// addi r3,r31,80
	state.r[3] = state.r[31] + 80;
	// bl 0x830d9d3c
	state.lr = 0x827CCD7C;
	services.CallNative(0x830d9d3cu,memory,state);
	// b 0x827cca88
	goto loc_827CCA88;
loc_827CCD80:
	// lwz r11,316(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 316);
	// add r28,r28,r11
	state.r[28] = state.r[28] + state.r[11];
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
loc_827CCD8C:
	// addi r11,r10,1431
	state.r[11] = state.r[10] + 1431;
	// li r8,8
	state.r[8] = 8;
	// rlwinm r11,r11,0,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFFFFFF8;
	// addi r9,r29,128
	state.r[9] = state.r[29] + 128;
	// addi r10,r10,76
	state.r[10] = state.r[10] + 76;
loc_827CCDA0:
	// addic. r8,r8,-1
	state.xer_ca = Low32(state.r[8]) > 0;
	state.r[8] = state.r[8] + -1;
	Compare<std::int32_t>(state.cr0, Signed32(state.r[8]), 0, state.xer_so);
	// stw r11,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[11]));
	// mr r10,r11
	state.r[10] = state.r[11];
	// addi r11,r11,16
	state.r[11] = state.r[11] + 16;
	// bne 0x827ccda0
	if (!state.cr0.eq) goto loc_827CCDA0;
	// mr r29,r11
	state.r[29] = state.r[11];
	// stw r22,0(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 0, Low32(state.r[22]));
	// lwz r10,18536(r23)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[23]) + 18536);
	// rlwinm. r10,r10,0,20,20
	state.r[10] = Rotate64(Low32(state.r[10]) | (state.r[10] << 32), 0) & 0x800;
	Compare<std::int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// beq 0x827ccde8
	if (state.cr0.eq) goto loc_827CCDE8;
	// addi r11,r11,7
	state.r[11] = state.r[11] + 7;
	// addi r9,r9,1548
	state.r[9] = state.r[9] + 1548;
	// rlwinm r11,r11,0,0,28
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFFFFFF8;
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,380(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 380, Low32(state.r[11]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// lwz r11,380(r11)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[11]) + 380);
	// addi r29,r11,1548
	state.r[29] = state.r[11] + 1548;
loc_827CCDE8:
	// addi r11,r9,15
	state.r[11] = state.r[9] + 15;
	// rlwinm r30,r11,0,0,27
	state.r[30] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 0) & 0xFFFFFFF0;
	// lis r11,-4353
	state.r[11] = -285278208;
	// ori r11,r11,61183
	state.r[11] = state.r[11] | 61183;
	// lis r12,24577
	state.r[12] = 1610678272;
	// ori r12,r12,125
	state.r[12] = state.r[12] | 125;
	// and r10,r24,r12
	state.r[10] = state.r[24] & state.r[12];
	// rlwinm r9,r30,28,4,31
	state.r[9] = Rotate64(Low32(state.r[30]) | (state.r[30] << 32), 28) & 0xFFFFFFF;
	// lwz r8,80(r31)
	state.r[8] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// sth r9,0(r8)
	PPC_STORE_U16(Low32(state.r[8]) + 0, Low16(state.r[9]));
	// lwz r9,80(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// li r8,1
	state.r[8] = 1;
	// stb r8,5(r9)
	PPC_STORE_U8(Low32(state.r[9]) + 5, Low8(state.r[8]));
	// lwz r9,80(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,16(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 16, Low32(state.r[11]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r24,20(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 20, Low32(state.r[24]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r10,24(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 24, Low32(state.r[10]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// sth r27,368(r11)
	PPC_STORE_U16(Low32(state.r[11]) + 368, Low16(state.r[27]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// subf r10,r11,r29
	state.r[10] = state.r[29] - state.r[11];
	// sth r10,58(r11)
	PPC_STORE_U16(Low32(state.r[11]) + 58, Low16(state.r[10]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r22,60(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 60, Low32(state.r[22]));
	// bl 0x830da07c
	state.lr = 0x827CCE54;
	services.CallNative(0x830da07cu,memory,state);
	// li r10,128
	state.r[10] = 128;
	// lwz r9,80(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stb r3,379(r9)
	PPC_STORE_U8(Low32(state.r[9]) + 379, Low8(state.r[3]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// addi r11,r11,384
	state.r[11] = state.r[11] + 384;
loc_827CCE68:
	// addic. r10,r10,-1
	state.xer_ca = Low32(state.r[10]) > 0;
	state.r[10] = state.r[10] + -1;
	Compare<std::int32_t>(state.cr0, Signed32(state.r[10]), 0, state.xer_so);
	// stw r11,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[11]));
	// stw r11,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[11]));
	// addi r11,r11,8
	state.r[11] = state.r[11] + 8;
	// bne 0x827cce68
	if (!state.cr0.eq) goto loc_827CCE68;
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// addi r11,r11,88
	state.r[11] = state.r[11] + 88;
	// cmpwi cr6,r21,-1
	Compare<std::int32_t>(state.cr6, Signed32(state.r[21]), -1, state.xer_so);
	// stw r11,0(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 0, Low32(state.r[11]));
	// stw r11,4(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 4, Low32(state.r[11]));
	// bne cr6,0x827ccea0
	if (!state.cr6.eq) goto loc_827CCEA0;
	// mr r3,r29
	state.r[3] = state.r[29];
	// mr r21,r29
	state.r[21] = state.r[29];
	// bl 0x830d9e9c
	state.lr = 0x827CCEA0;
	services.CallNative(0x830d9e9cu,memory,state);
loc_827CCEA0:
	// mr r8,r28
	state.r[8] = state.r[28];
	// mr r6,r26
	state.r[6] = state.r[26];
	// li r5,0
	state.r[5] = 0;
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r21,1408(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 1408, Low32(state.r[21]));
	// lwz r7,84(r31)
	state.r[7] = PPC_LOAD_U32(Low32(state.r[31]) + 84);
	// lwz r11,308(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 308);
	// add r9,r7,r11
	state.r[9] = state.r[7] + state.r[11];
	// lwz r3,80(r31)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// add r4,r30,r3
	state.r[4] = state.r[30] + state.r[3];
	// bl 0x827cc2c0
	state.lr = 0x827CCECC;
	(void)heap_growth_lower_context::Apply(0x827cc2c0u,memory,services,state);
	// cmplwi r3,0
	Compare<std::uint32_t>(state.cr0, Low32(state.r[3]), 0, state.xer_so);
	// beq 0x827cca88
	if (state.cr0.eq) goto loc_827CCA88;
	// rlwinm. r11,r24,0,15,15
	state.r[11] = Rotate64(Low32(state.r[24]) | (state.r[24] << 32), 0) & 0x10000;
	Compare<std::int32_t>(state.cr0, Signed32(state.r[11]), 0, state.xer_so);
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// sth r22,56(r11)
	PPC_STORE_U16(Low32(state.r[11]) + 56, Low16(state.r[22]));
	// lwz r11,132(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 132);
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,32(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 32, Low32(state.r[11]));
	// lwz r11,136(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 136);
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,36(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 36, Low32(state.r[11]));
	// lwz r11,140(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 140);
	// rlwinm r11,r11,28,4,31
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 28) & 0xFFFFFFF;
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,40(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 40, Low32(state.r[11]));
	// lwz r11,144(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 144);
	// rlwinm r11,r11,28,4,31
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 28) & 0xFFFFFFF;
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,44(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 44, Low32(state.r[11]));
	// lwz r11,148(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 148);
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,52(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 52, Low32(state.r[11]));
	// lwz r11,152(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 152);
	// addi r11,r11,15
	state.r[11] = state.r[11] + 15;
	// rlwinm r11,r11,28,4,31
	state.r[11] = Rotate64(Low32(state.r[11]) | (state.r[11] << 32), 28) & 0xFFFFFFF;
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,28(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 28, Low32(state.r[11]));
	// lwz r11,164(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 164);
	// lwz r10,80(r31)
	state.r[10] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,1412(r10)
	PPC_STORE_U32(Low32(state.r[10]) + 1412, Low32(state.r[11]));
	// li r11,31
	state.r[11] = 31;
	// li r10,-16
	state.r[10] = -16;
	// lwz r9,80(r31)
	state.r[9] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r11,80(r9)
	PPC_STORE_U32(Low32(state.r[9]) + 80, Low32(state.r[11]));
	// lwz r11,80(r31)
	state.r[11] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
	// stw r10,84(r11)
	PPC_STORE_U32(Low32(state.r[11]) + 84, Low32(state.r[10]));
	// lwz r3,80(r31)
	state.r[3] = PPC_LOAD_U32(Low32(state.r[31]) + 80);
loc_827CCF60:
	// addi r1,r31,272
	state.r[1] = state.r[31] + 272;
	// b 0x82b7a71c
	Restore21(memory,state);
	return;
}
} // namespace
bool Apply(GuestAddress entry,GuestMemory& memory,
    BoundaryServices& services,Registers& state)
{
    if(entry!=0x827cc9d0u) return false;
    Create(memory,services,state);
    return true;
}
} // namespace lo::semantic::gpu::heap_create_context

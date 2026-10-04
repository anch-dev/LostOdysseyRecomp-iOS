#include "lo_semantics/crt_stream_bulk_close_routes.h"

#include "lo_semantics/crt_stream_index_unlock.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_bulk_close_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;
std::uint64_t& R(Registers& s, unsigned i)
{ return i == 1u ? s.sp : s.r[i]; }
std::uint32_t W(std::uint64_t x) { return Address(x); }
std::int32_t S(std::uint64_t x)
{ return std::bit_cast<std::int32_t>(W(x)); }
void Compare(Condition& c, Registers& s, std::uint64_t a,
    std::uint64_t b, bool sign)
{
    if (sign)
    {
        const auto x=S(a), y=S(b);
        c={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
    }
    else
    {
        const auto x=W(a), y=W(b);
        c={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
    }
}
void Save28(GuestMemory& m, Registers& s)
{
    for(unsigned i=28u;i<=31u;++i)
        WriteU64(m,Address(s.sp-16u-8u*(31u-i)),R(s,i));
    m.WriteU32(Address(s.sp-8u),W(R(s,12)));
}
void Restore28(GuestMemory& m, Registers& s)
{
    for(unsigned i=28u;i<=31u;++i)
        R(s,i)=ReadU64(m,Address(s.sp-16u-8u*(31u-i)));
    R(s,12)=m.ReadU32(Address(s.sp-8u));s.lr=R(s,12);
}
crt_stream_locks::Registers ToLock(const Registers& s)
{
    crt_stream_locks::Registers x{};
    x.sp=s.sp;x.lr=s.lr;x.ctr=s.ctr;
    x.r3=s.r[3];x.r4=s.r[4];x.r5=s.r[5];x.r6=s.r[6];
    x.r7=s.r[7];x.r8=s.r[8];x.r9=s.r[9];x.r10=s.r[10];
    x.r11=s.r[11];x.r12=s.r[12];x.r13=s.r[13];
    x.r28=s.r[28];x.r29=s.r[29];x.r30=s.r[30];x.r31=s.r[31];
    x.cr0={bool(s.cr0.lt),bool(s.cr0.gt),bool(s.cr0.eq),bool(s.cr0.so)};
    x.cr6={bool(s.cr6.lt),bool(s.cr6.gt),bool(s.cr6.eq),bool(s.cr6.so)};
    x.xer_ca=s.xer_ca;x.xer_so=s.xer_so;
    return x;
}
void FromLock(Registers& s,const crt_stream_locks::Registers& x)
{
    s.sp=x.sp;s.lr=x.lr;s.ctr=x.ctr;
    s.r[3]=x.r3;s.r[4]=x.r4;s.r[5]=x.r5;s.r[6]=x.r6;
    s.r[7]=x.r7;s.r[8]=x.r8;s.r[9]=x.r9;s.r[10]=x.r10;
    s.r[11]=x.r11;s.r[12]=x.r12;s.r[13]=x.r13;
    s.r[28]=x.r28;s.r[29]=x.r29;s.r[30]=x.r30;s.r[31]=x.r31;
    s.cr0={std::uint8_t(x.cr0.lt),std::uint8_t(x.cr0.gt),
        std::uint8_t(x.cr0.eq),std::uint8_t(x.cr0.so)};
    s.cr6={std::uint8_t(x.cr6.lt),std::uint8_t(x.cr6.gt),
        std::uint8_t(x.cr6.eq),std::uint8_t(x.cr6.so)};
    s.xer_ca=x.xer_ca;s.xer_so=x.xer_so;
}
void IndexUnlock(GuestMemory& m, Dependencies d, Registers& s)
{
    crt_stream_index_unlock::Registers x{s.sp,s.lr,R(s,3),R(s,10),
        R(s,11),R(s,12),R(s,29),R(s,31)};
    if(!crt_stream_index_unlock::Apply(0x82b819c8u,m,
            d.pipeline.close.accepted.locks.index_unlock,x))
        throw std::logic_error("missing accepted indexed unlock");
    s.sp=x.sp;s.lr=x.lr;R(s,3)=x.r3;R(s,10)=x.r10;
    R(s,11)=x.r11;R(s,12)=x.r12;R(s,29)=x.r29;R(s,31)=x.r31;
}
void Body_82B85D88(GuestMemory&, Dependencies, Registers&);
void Body_82DF60A8(GuestMemory&, Dependencies, Registers&);
void Body_82B7B708(GuestMemory&, Dependencies, Registers&);
void Body_82B7B7C8(GuestMemory&, Dependencies, Registers&);
void Body_82B85E6C(GuestMemory&, Dependencies, Registers&);
void Body_82DF6188(GuestMemory&, Dependencies, Registers&);
void Body_82DF61D8(GuestMemory&, Dependencies, Registers&);
void Direct(GuestAddress address, GuestMemory& m, Dependencies d, Registers& s)
{
    switch(address)
    {
    case 0x82b85d88u:Body_82B85D88(m,d,s);return;
    case 0x82df60a8u:Body_82DF60A8(m,d,s);return;
    case 0x82b7b708u:Body_82B7B708(m,d,s);return;
    case 0x82b7b7c8u:Body_82B7B7C8(m,d,s);return;
    case 0x82b85e6cu:Body_82B85E6C(m,d,s);return;
    case 0x82df6188u:Body_82DF6188(m,d,s);return;
    case 0x82df61d8u:Body_82DF61D8(m,d,s);return;
    case 0x82b85cc8u:
        if(!crt_stream_close_pipeline::Apply(address,m,d.pipeline,s))
            throw std::logic_error("missing accepted close pipeline");
        return;
    case 0x82b7b778u:case 0x82b7b810u:case 0x82b7b838u:
        if(!crt_format_stream::Apply(address,m,d.pipeline.format,s))
            throw std::logic_error("missing accepted format stream lower");
        return;
    case 0x82b81b28u:
    {
        auto x=ToLock(s);
        if(!crt_stream_locks::Apply(address,m,
                d.pipeline.close.accepted.locks,x))
            throw std::logic_error("missing accepted indexed lock");
        FromLock(s,x);return;
    }
    case 0x82b819c8u:IndexUnlock(m,d,s);return;
    default:break;
    }
    if(!crt_stream_operations::ApplyAcceptedCallee(address,m,
            d.pipeline.close.accepted,s))
        throw std::logic_error("unselected bulk close direct callee");
}

#define PPC_LOAD_U8(a) m.ReadU8(Address(a))
#define PPC_LOAD_U32(a) m.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(m,Address(a))
#define PPC_STORE_U8(a,v) m.WriteU8(Address(a),std::uint8_t(v))
#define PPC_STORE_U32(a,v) m.WriteU32(Address(a),W(v))
#define PPC_STORE_U64(a,v) WriteU64(m,Address(a),(v))

void Body_82B85D88(GuestMemory& m, Dependencies d, Registers& state)
{
	std::uint64_t temp=0;

	// mflr r12
	R(state, 12) = state.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(W(R(state, 1)) + -8, W(R(state, 12)));
	// std r30,-24(r1)
	PPC_STORE_U64(W(R(state, 1)) + -24, R(state, 30));
	// std r31,-16(r1)
	PPC_STORE_U64(W(R(state, 1)) + -16, R(state, 31));
	// addi r31,r1,-112
	R(state, 31) = R(state, 1) + -112;
	// stwu r1,-112(r1)
	temp = R(state, 1) + uint64_t(-112);
	PPC_STORE_U32(W(temp), W(R(state, 1)));
	R(state, 1) = temp;
	// mr r30,r3
	R(state, 30) = R(state, 3);
	// stw r30,132(r31)
	PPC_STORE_U32(W(R(state, 31)) + 132, W(R(state, 30)));
	// cntlzw r11,r30
	R(state, 11) = W(R(state, 30)) == 0 ? 32 : std::countl_zero(W(R(state, 30)));
	// li r10,-1
	R(state, 10) = -1;
	// stw r10,80(r31)
	PPC_STORE_U32(W(R(state, 31)) + 80, W(R(state, 10)));
	// rlwinm r11,r11,27,31,31
	R(state, 11) = std::rotl(W(R(state, 11)) | (R(state, 11) << 32), 27) & 0x1;
	// xori r11,r11,1
	R(state, 11) = R(state, 11) ^ 1;
	// cmpwi r11,0
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// bne 0x82b85df4
	if (!state.cr0.eq) goto loc_82B85DF4;
	// bl 0x82b7fd78
	state.lr = 0x82B85DC8;
	Direct(0x82b7fd78u, m, d, state);
	// mr r11,r3
	R(state, 11) = R(state, 3);
	// li r10,22
	R(state, 10) = 22;
	// li r7,0
	R(state, 7) = 0;
	// li r6,0
	R(state, 6) = 0;
	// li r5,0
	R(state, 5) = 0;
	// li r4,0
	R(state, 4) = 0;
	// li r3,0
	R(state, 3) = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(W(R(state, 11)) + 0, W(R(state, 10)));
	// bl 0x82b7fec0
	state.lr = 0x82B85DEC;
	Direct(0x82b7fec0u, m, d, state);
	// li r3,-1
	R(state, 3) = -1;
	// b 0x82b85e34
	goto loc_82B85E34;
loc_82B85DF4:
	// lwz r11,12(r30)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 30)) + 12);
	// rlwinm. r11,r11,0,25,25
	R(state, 11) = std::rotl(W(R(state, 11)) | (R(state, 11) << 32), 0) & 0x40;
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// beq 0x82b85e0c
	if (state.cr0.eq) goto loc_82B85E0C;
	// li r11,0
	R(state, 11) = 0;
	// stw r11,12(r30)
	PPC_STORE_U32(W(R(state, 30)) + 12, W(R(state, 11)));
	// b 0x82b85e30
	goto loc_82B85E30;
loc_82B85E0C:
	// mr r3,r30
	R(state, 3) = R(state, 30);
	// bl 0x82b7b708
	state.lr = 0x82B85E14;
	Direct(0x82b7b708u, m, d, state);
	// mr r8,r8
	// mr r3,r30
	R(state, 3) = R(state, 30);
	// bl 0x82b85cc8
	state.lr = 0x82B85E20;
	Direct(0x82b85cc8u, m, d, state);
	// stw r3,80(r31)
	PPC_STORE_U32(W(R(state, 31)) + 80, W(R(state, 3)));
	// mr r8,r8
	// addi r12,r31,112
	R(state, 12) = R(state, 31) + 112;
	// bl 0x82b85e6c
	state.lr = 0x82B85E30;
	Direct(0x82b85e6cu, m, d, state);
loc_82B85E30:
	// lwz r3,80(r31)
	R(state, 3) = PPC_LOAD_U32(W(R(state, 31)) + 80);
loc_82B85E34:
	// addi r1,r31,112
	R(state, 1) = R(state, 31) + 112;
	// lwz r12,-8(r1)
	R(state, 12) = PPC_LOAD_U32(W(R(state, 1)) + -8);
	// mtlr r12
	state.lr = R(state, 12);
	// ld r30,-24(r1)
	R(state, 30) = PPC_LOAD_U64(W(R(state, 1)) + -24);
	// ld r31,-16(r1)
	R(state, 31) = PPC_LOAD_U64(W(R(state, 1)) + -16);
	// blr
	return;
}

void Body_82DF60A8(GuestMemory& m, Dependencies d, Registers& state)
{
	std::uint64_t temp=0;

	// mflr r12
	R(state, 12) = state.lr;
	// bl 0x82b7a6e8
	state.lr = 0x82DF60B0;
	Save28(m, state);
	// addi r31,r1,-128
	R(state, 31) = R(state, 1) + -128;
	// stwu r1,-128(r1)
	temp = R(state, 1) + uint64_t(-128);
	PPC_STORE_U32(W(temp), W(R(state, 1)));
	R(state, 1) = temp;
	// li r3,1
	R(state, 3) = 1;
	// li r29,0
	R(state, 29) = 0;
	// stw r29,84(r31)
	PPC_STORE_U32(W(R(state, 31)) + 84, W(R(state, 29)));
	// bl 0x82b81b28
	state.lr = 0x82DF60C8;
	Direct(0x82b81b28u, m, d, state);
	// mr r8,r8
	// stw r29,80(r31)
	PPC_STORE_U32(W(R(state, 31)) + 80, W(R(state, 29)));
	// lis r11,-31944
	R(state, 11) = -2093481984;
	// addi r30,r11,-29040
	R(state, 30) = R(state, 11) + -29040;
	// lis r11,-31944
	R(state, 11) = -2093481984;
	// addi r10,r11,-29036
	R(state, 10) = R(state, 11) + -29036;
	// lwz r11,0(r30)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 30)) + 0);
loc_82DF60E4:
	// lwz r9,0(r10)
	R(state, 9) = PPC_LOAD_U32(W(R(state, 10)) + 0);
	// cmpw cr6,r29,r9
	Compare(state.cr6, state, S(R(state, 29)), S(R(state, 9)), true);
	// bge cr6,0x82df6170
	if (!state.cr6.lt) goto loc_82DF6170;
	// rlwinm r28,r29,2,0,29
	R(state, 28) = std::rotl(W(R(state, 29)) | (R(state, 29) << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r28,r11
	R(state, 9) = PPC_LOAD_U32(W(R(state, 28)) + W(R(state, 11)));
	// cmplwi cr6,r9,0
	Compare(state.cr6, state, W(R(state, 9)), 0, false);
	// beq cr6,0x82df6160
	if (state.cr6.eq) goto loc_82DF6160;
	// rotlwi r4,r9,0
	R(state, 4) = std::rotl(W(R(state, 9)), 0);
	// lwz r9,12(r4)
	R(state, 9) = PPC_LOAD_U32(W(R(state, 4)) + 12);
	// andi. r9,r9,131
	R(state, 9) = R(state, 9) & 131;
	Compare(state.cr0, state, S(R(state, 9)), 0, true);
	// cmpwi r9,0
	Compare(state.cr0, state, S(R(state, 9)), 0, true);
	// beq 0x82df6160
	if (state.cr0.eq) goto loc_82DF6160;
	// mr r3,r29
	R(state, 3) = R(state, 29);
	// bl 0x82b7b778
	state.lr = 0x82DF611C;
	Direct(0x82b7b778u, m, d, state);
	// mr r8,r8
	// lwz r11,0(r30)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 30)) + 0);
	// lwzx r3,r28,r11
	R(state, 3) = PPC_LOAD_U32(W(R(state, 28)) + W(R(state, 11)));
	// lwz r11,12(r3)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 3)) + 12);
	// andi. r11,r11,131
	R(state, 11) = R(state, 11) & 131;
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// cmpwi r11,0
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// beq 0x82df6154
	if (state.cr0.eq) goto loc_82DF6154;
	// lwz r11,28(r3)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 3)) + 28);
	// cmplwi cr6,r11,0
	Compare(state.cr6, state, W(R(state, 11)), 0, false);
	// beq cr6,0x82df6154
	if (state.cr6.eq) goto loc_82DF6154;
	// bl 0x82b85cc8
	state.lr = 0x82DF6148;
	Direct(0x82b85cc8u, m, d, state);
	// lwz r11,84(r31)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 31)) + 84);
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// stw r11,84(r31)
	PPC_STORE_U32(W(R(state, 31)) + 84, W(R(state, 11)));
loc_82DF6154:
	// mr r8,r8
	// addi r12,r31,128
	R(state, 12) = R(state, 31) + 128;
	// bl 0x82df61d8
	state.lr = 0x82DF6160;
	Direct(0x82df61d8u, m, d, state);
loc_82DF6160:
	// mr r8,r8
	// addi r29,r29,1
	R(state, 29) = R(state, 29) + 1;
	// stw r29,80(r31)
	PPC_STORE_U32(W(R(state, 31)) + 80, W(R(state, 29)));
	// b 0x82df60e4
	goto loc_82DF60E4;
loc_82DF6170:
	// mr r8,r8
	// addi r12,r31,128
	R(state, 12) = R(state, 31) + 128;
	// bl 0x82df6188
	state.lr = 0x82DF617C;
	Direct(0x82df6188u, m, d, state);
	// lwz r3,84(r31)
	R(state, 3) = PPC_LOAD_U32(W(R(state, 31)) + 84);
	// addi r1,r31,128
	R(state, 1) = R(state, 31) + 128;
	// b 0x82b7a738
	Restore28(m, state);
	return;
}

void Body_82B7B708(GuestMemory& m, Dependencies d, Registers& state)
{
	std::uint64_t temp=0;

	// mflr r12
	R(state, 12) = state.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(W(R(state, 1)) + -8, W(R(state, 12)));
	// std r31,-16(r1)
	PPC_STORE_U64(W(R(state, 1)) + -16, R(state, 31));
	// stwu r1,-96(r1)
	temp = R(state, 1) + uint64_t(-96);
	PPC_STORE_U32(W(temp), W(R(state, 1)));
	R(state, 1) = temp;
	// lis r11,-31967
	R(state, 11) = -2094989312;
	// mr r31,r3
	R(state, 31) = R(state, 3);
	// addi r11,r11,19184
	R(state, 11) = R(state, 11) + 19184;
	// cmplw cr6,r31,r11
	Compare(state.cr6, state, W(R(state, 31)), W(R(state, 11)), false);
	// blt cr6,0x82b7b758
	if (state.cr6.lt) goto loc_82B7B758;
	// addi r10,r11,608
	R(state, 10) = R(state, 11) + 608;
	// cmplw cr6,r31,r10
	Compare(state.cr6, state, W(R(state, 31)), W(R(state, 10)), false);
	// bgt cr6,0x82b7b758
	if (state.cr6.gt) goto loc_82B7B758;
	// subf r11,r11,r31
	R(state, 11) = R(state, 31) - R(state, 11);
	// srawi r11,r11,5
	state.xer_ca = (S(R(state, 11)) < 0) && ((W(R(state, 11)) & 0x1F) != 0);
	R(state, 11) = S(R(state, 11)) >> 5;
	// addi r3,r11,16
	R(state, 3) = R(state, 11) + 16;
	// bl 0x82b81b28
	state.lr = 0x82B7B748;
	Direct(0x82b81b28u, m, d, state);
	// lwz r11,12(r31)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 31)) + 12);
	// ori r11,r11,32768
	R(state, 11) = R(state, 11) | 32768;
	// stw r11,12(r31)
	PPC_STORE_U32(W(R(state, 31)) + 12, W(R(state, 11)));
	// b 0x82b7b760
	goto loc_82B7B760;
loc_82B7B758:
	// addi r3,r31,32
	R(state, 3) = R(state, 31) + 32;
	// bl 0x830d9c6c
	state.lr = 0x82B7B760;
	d.native.EnterCriticalSection(m, state);
loc_82B7B760:
	// addi r1,r1,96
	R(state, 1) = R(state, 1) + 96;
	// lwz r12,-8(r1)
	R(state, 12) = PPC_LOAD_U32(W(R(state, 1)) + -8);
	// mtlr r12
	state.lr = R(state, 12);
	// ld r31,-16(r1)
	R(state, 31) = PPC_LOAD_U64(W(R(state, 1)) + -16);
	// blr
	return;
}

void Body_82B7B7C8(GuestMemory& m, Dependencies d, Registers& state)
{

	// lis r10,-31967
	R(state, 10) = -2094989312;
	// mr r11,r3
	R(state, 11) = R(state, 3);
	// addi r10,r10,19184
	R(state, 10) = R(state, 10) + 19184;
	// cmplw cr6,r11,r10
	Compare(state.cr6, state, W(R(state, 11)), W(R(state, 10)), false);
	// blt cr6,0x82b7b804
	if (state.cr6.lt) goto loc_82B7B804;
	// addi r9,r10,608
	R(state, 9) = R(state, 10) + 608;
	// cmplw cr6,r11,r9
	Compare(state.cr6, state, W(R(state, 11)), W(R(state, 9)), false);
	// bgt cr6,0x82b7b804
	if (state.cr6.gt) goto loc_82B7B804;
	// lwz r9,12(r11)
	R(state, 9) = PPC_LOAD_U32(W(R(state, 11)) + 12);
	// subf r10,r10,r11
	R(state, 10) = R(state, 11) - R(state, 10);
	// rlwinm r9,r9,0,17,15
	R(state, 9) = std::rotl(W(R(state, 9)) | (R(state, 9) << 32), 0) & 0xFFFFFFFFFFFF7FFF;
	// srawi r10,r10,5
	state.xer_ca = (S(R(state, 10)) < 0) && ((W(R(state, 10)) & 0x1F) != 0);
	R(state, 10) = S(R(state, 10)) >> 5;
	// addi r3,r10,16
	R(state, 3) = R(state, 10) + 16;
	// stw r9,12(r11)
	PPC_STORE_U32(W(R(state, 11)) + 12, W(R(state, 9)));
	// b 0x82b819c8
	Direct(0x82b819c8u, m, d, state);
	return;
loc_82B7B804:
	// addi r3,r11,32
	R(state, 3) = R(state, 11) + 32;
	// b 0x830d9c7c
	d.native.LeaveCriticalSection(m, state);
	return;
}

void Body_82B85E6C(GuestMemory& m, Dependencies d, Registers& state)
{
	std::uint64_t temp=0;

	// std r31,-8(r1)
	PPC_STORE_U64(W(R(state, 1)) + -8, R(state, 31));
	// addi r31,r12,-112
	R(state, 31) = R(state, 12) + -112;
	// std r30,-16(r1)
	PPC_STORE_U64(W(R(state, 1)) + -16, R(state, 30));
	// mflr r12
	R(state, 12) = state.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(W(R(state, 1)) + -24, W(R(state, 12)));
	// stwu r1,-112(r1)
	temp = R(state, 1) + uint64_t(-112);
	PPC_STORE_U32(W(temp), W(R(state, 1)));
	R(state, 1) = temp;
	// mr r3,r30
	R(state, 3) = R(state, 30);
	// bl 0x82b7b7c8
	state.lr = 0x82B85E8C;
	Direct(0x82b7b7c8u, m, d, state);
	// lwz r1,0(r1)
	R(state, 1) = PPC_LOAD_U32(W(R(state, 1)) + 0);
	// ld r31,-8(r1)
	R(state, 31) = PPC_LOAD_U64(W(R(state, 1)) + -8);
	// ld r30,-16(r1)
	R(state, 30) = PPC_LOAD_U64(W(R(state, 1)) + -16);
	// lwz r12,-24(r1)
	R(state, 12) = PPC_LOAD_U32(W(R(state, 1)) + -24);
	// mtlr r12
	state.lr = R(state, 12);
	// blr
	return;
}

void Body_82DF6188(GuestMemory& m, Dependencies d, Registers& state)
{
	std::uint64_t temp=0;

	// mflr r12
	R(state, 12) = state.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(W(R(state, 1)) + -8, W(R(state, 12)));
	// stwu r1,-96(r1)
	temp = R(state, 1) + uint64_t(-96);
	PPC_STORE_U32(W(temp), W(R(state, 1)));
	R(state, 1) = temp;
	// li r3,1
	R(state, 3) = 1;
	// bl 0x82b819c8
	state.lr = 0x82DF619C;
	Direct(0x82b819c8u, m, d, state);
	// lwz r1,0(r1)
	R(state, 1) = PPC_LOAD_U32(W(R(state, 1)) + 0);
	// lwz r12,-8(r1)
	R(state, 12) = PPC_LOAD_U32(W(R(state, 1)) + -8);
	// mtlr r12
	state.lr = R(state, 12);
	// blr
	return;
}

void Body_82DF61D8(GuestMemory& m, Dependencies d, Registers& state)
{
	std::uint64_t temp=0;

	// std r31,-8(r1)
	PPC_STORE_U64(W(R(state, 1)) + -8, R(state, 31));
	// addi r31,r12,-128
	R(state, 31) = R(state, 12) + -128;
	// std r30,-16(r1)
	PPC_STORE_U64(W(R(state, 1)) + -16, R(state, 30));
	// std r29,-24(r1)
	PPC_STORE_U64(W(R(state, 1)) + -24, R(state, 29));
	// mflr r12
	R(state, 12) = state.lr;
	// stw r12,-32(r1)
	PPC_STORE_U32(W(R(state, 1)) + -32, W(R(state, 12)));
	// stwu r1,-112(r1)
	temp = R(state, 1) + uint64_t(-112);
	PPC_STORE_U32(W(temp), W(R(state, 1)));
	R(state, 1) = temp;
	// rlwinm r11,r29,2,0,29
	R(state, 11) = std::rotl(W(R(state, 29)) | (R(state, 29) << 32), 2) & 0xFFFFFFFC;
	// mr r3,r29
	R(state, 3) = R(state, 29);
	// lwz r10,0(r30)
	R(state, 10) = PPC_LOAD_U32(W(R(state, 30)) + 0);
	// lwzx r4,r11,r10
	R(state, 4) = PPC_LOAD_U32(W(R(state, 11)) + W(R(state, 10)));
	// bl 0x82b7b810
	state.lr = 0x82DF6208;
	Direct(0x82b7b810u, m, d, state);
	// lis r11,-31944
	R(state, 11) = -2093481984;
	// addi r30,r11,-29040
	R(state, 30) = R(state, 11) + -29040;
	// lis r11,-31944
	R(state, 11) = -2093481984;
	// addi r10,r11,-29036
	R(state, 10) = R(state, 11) + -29036;
	// lwz r29,80(r31)
	R(state, 29) = PPC_LOAD_U32(W(R(state, 31)) + 80);
	// lwz r11,0(r30)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 30)) + 0);
	// lwz r1,0(r1)
	R(state, 1) = PPC_LOAD_U32(W(R(state, 1)) + 0);
	// ld r31,-8(r1)
	R(state, 31) = PPC_LOAD_U64(W(R(state, 1)) + -8);
	// ld r30,-16(r1)
	R(state, 30) = PPC_LOAD_U64(W(R(state, 1)) + -16);
	// ld r29,-24(r1)
	R(state, 29) = PPC_LOAD_U64(W(R(state, 1)) + -24);
	// lwz r12,-32(r1)
	R(state, 12) = PPC_LOAD_U32(W(R(state, 1)) + -32);
	// mtlr r12
	state.lr = R(state, 12);
	// blr
	return;
}


#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch(entry)
    {
    case 0x82b85d88u:case 0x82df60a8u:case 0x82b7b708u:
    case 0x82b7b7c8u:case 0x82b85e6cu:case 0x82df6188u:
    case 0x82df61d8u:Direct(entry,memory,dependencies,registers);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_bulk_close_routes

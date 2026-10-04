#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace heap_free_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u;
constexpr GuestAddress Block=0x100000u;
constexpr GuestAddress Sentinel=0x320000u;
constexpr GuestAddress Stack=0x3f0000u;
constexpr GuestAddress Payload=Block+16u;

enum class Mode {Null,GuardNull,Small,Large,LargeLocked,
    VirtualLocked,VirtualFailure,CleanupLocked};

struct Inputs
{
    std::uint32_t units=4u;
    bool lock=false,virtual_block=false,guard=false,cleanup=false;
    std::int32_t vm_status=0;
};

inline Inputs Select(Mode mode)
{
    Inputs in{};
    switch(mode)
    {
    case Mode::GuardNull:in.guard=true;break;
    case Mode::Large:in.units=129u;break;
    case Mode::LargeLocked:in.units=129u;in.lock=true;break;
    case Mode::VirtualLocked:in.virtual_block=true;in.lock=true;break;
    case Mode::VirtualFailure:in.virtual_block=true;
        in.vm_status=-1;break;
    case Mode::CleanupLocked:in.cleanup=true;in.lock=true;break;
    default:break;
    }
    return in;
}

inline void Header(GuestMemory& memory,GuestAddress address,
    std::uint16_t size,std::uint16_t previous,std::uint8_t flags)
{
    memory.WriteU16(address,size);
    memory.WriteU16(address+2u,previous);
    memory.WriteU8(address+4u,0u);
    memory.WriteU8(address+5u,flags);
}

inline void Seed(GuestMemory& memory,Mode mode)
{
    const auto in=Select(mode);
    for(unsigned size=0u;size<128u;++size)
    {
        const auto head=Heap+(size+48u)*8u;
        memory.WriteU32(head,head);
        memory.WriteU32(head+4u,head);
    }
    for(unsigned index=0u;index<4u;++index)
        memory.WriteU32(Heap+(88u+index)*4u,0u);
    memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+20u,in.guard?0x40000u:0u);
    memory.WriteU32(Heap+24u,in.lock?0u:1u);
    memory.WriteU32(Heap+40u,0xffffu);
    memory.WriteU32(Heap+44u,0xffffffffu);
    memory.WriteU32(Heap+1408u,Sentinel);
    memory.WriteU8(Heap+379u,1u);
    if(mode==Mode::Null || mode==Mode::GuardNull || in.cleanup) return;
    Header(memory,Block,static_cast<std::uint16_t>(in.units),0u,
        in.virtual_block?8u:0u);
    if(in.virtual_block)
    {
        const auto vm=Block-32u;
        memory.WriteU32(vm,Sentinel);
        memory.WriteU32(vm+4u,Sentinel);
        memory.WriteU32(Sentinel,vm);
        memory.WriteU32(Sentinel+4u,vm);
        return;
    }
    Header(memory,Block+in.units*16u,1u,
        static_cast<std::uint16_t>(in.units),1u);
}
} // namespace heap_free_context_fixture

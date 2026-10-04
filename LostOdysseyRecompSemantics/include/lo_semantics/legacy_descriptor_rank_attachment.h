#pragma once
#include "lo_semantics/legacy_descriptor_clone_chain.h"
namespace lo::semantic::gpu::legacy_descriptor_rank_attachment
{
using Registers = legacy_descriptor_clone_chain::Registers;
using Services = legacy_descriptor_clone_chain::Services;
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Services& services, Registers& state);
}

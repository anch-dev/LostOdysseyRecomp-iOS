#pragma once
#include "lo_semantics/legacy_config_command_name_chain.h"
#include "lo_semantics/legacy_config_line_dispatch.h"
namespace lo::semantic::gpu::legacy_config_command_line_chain
{
using Registers = legacy_config_command_name_chain::Registers;
struct Dependencies
{
    legacy_config_command_name_chain::Dependencies command;
    legacy_config_line_dispatch::Dependencies lines;
};
// Keybinding selection enters the recovered line dispatcher. Name routes and
// parsing lowers are composed; dynamic vtable methods remain explicit.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies, Registers&);
}

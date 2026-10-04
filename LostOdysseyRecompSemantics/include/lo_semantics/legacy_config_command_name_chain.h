#pragma once
#include "lo_semantics/legacy_config_command_dispatch.h"
#include "lo_semantics/legacy_config_name_routes.h"
namespace lo::semantic::gpu::legacy_config_command_name_chain
{
using Registers = legacy_config_command_dispatch::Registers;
struct Dependencies
{
    legacy_config_command_dispatch::Dependencies command;
    legacy_config_name_routes::Dependencies names;
};
// Actual command body composes both recovered name routes at their call sites.
// Other command guests and lower native services retain explicit boundaries.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies, Registers&);
}

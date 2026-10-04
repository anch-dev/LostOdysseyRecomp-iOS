#include "lo_semantics/legacy_config_command_line_chain.h"
#include <stdexcept>
namespace lo::semantic::gpu::legacy_config_command_line_chain
{
namespace
{
class LineBoundary final : public legacy_config_command_dispatch::GuestBoundaries
{
public:
    explicit LineBoundary(Dependencies dependencies) : dependencies_(dependencies) {}
    void Call(GuestAddress target, GuestMemory& memory, Registers& state) override
    {
        if (target == 0x82295da8u)
        {
            if (!legacy_config_line_dispatch::Apply(target, memory,
                dependencies_.lines, state.integer))
                throw std::logic_error("recovered command line dispatcher unavailable");
        }
        else dependencies_.command.command.guests.Call(target, memory, state);
    }
    bool TryApplyLower(GuestAddress entry, GuestMemory& memory,
        Registers& state) override
    {
        return legacy_config_name_routes::ApplyParsingLower(entry, memory,
            dependencies_.command.names, state.integer);
    }
    void SetHostFpControl(std::uint32_t control) override
    { dependencies_.command.command.guests.SetHostFpControl(control); }
private:
    Dependencies dependencies_;
};
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    LineBoundary boundary(dependencies);
    const auto& original = dependencies.command.command;
    legacy_config_command_dispatch::Dependencies command{original.thread,
        original.invalid, original.records, original.arrays, original.numbers,
        boundary};
    legacy_config_command_name_chain::Dependencies names{command,
        dependencies.command.names};
    return legacy_config_command_name_chain::Apply(entry, memory, names, state);
}
}

#include "lo_semantics/legacy_config_command_name_chain.h"
#include <stdexcept>
namespace lo::semantic::gpu::legacy_config_command_name_chain
{
namespace
{
class NameBoundary final : public legacy_config_command_dispatch::GuestBoundaries
{
public:
    explicit NameBoundary(Dependencies dependencies) : dependencies_(dependencies) {}
    void Call(GuestAddress target, GuestMemory& memory, Registers& state) override
    {
        if (target == 0x82713828u || target == 0x82296b00u)
        {
            if (!legacy_config_name_routes::Apply(target, memory,
                dependencies_.names, state.integer))
                throw std::logic_error("recovered command name route unavailable");
        }
        else dependencies_.command.guests.Call(target, memory, state);
    }
    void SetHostFpControl(std::uint32_t control) override
    { dependencies_.command.guests.SetHostFpControl(control); }
    bool TryApplyLower(GuestAddress entry, GuestMemory& memory,
        Registers& state) override
    { return dependencies_.command.guests.TryApplyLower(entry, memory, state); }
private:
    Dependencies dependencies_;
};
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    NameBoundary boundary(dependencies);
    const auto& original = dependencies.command;
    legacy_config_command_dispatch::Dependencies command{original.thread,
        original.invalid, original.records, original.arrays, original.numbers,
        boundary};
    return legacy_config_command_dispatch::Apply(entry, memory, command, state);
}
}

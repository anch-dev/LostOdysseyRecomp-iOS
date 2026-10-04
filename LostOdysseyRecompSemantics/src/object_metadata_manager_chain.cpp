#include "lo_semantics/object_metadata_manager_chain.h"
namespace lo::semantic::gpu::object_metadata_manager_chain
{
namespace
{
class Adapter final : public object_metadata_storage::PpcBoundaryServices
{
public:
    explicit Adapter(Dependencies dependencies) : dependencies_(dependencies) {}
    void InitializeManager827C5F38(GuestMemory& memory,
        object_child_float::Registers& state) override
    { manager_init_context::Apply(memory, dependencies_.manager, state); }
    void CallAllocator(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) override
    { dependencies_.allocator.CallAllocator(target, memory, state); }
private:
    Dependencies dependencies_;
};
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies dependencies,
    object_child_float::Registers& state)
{
    Adapter adapter(dependencies);
    return object_metadata_storage::Apply(entry, memory, adapter, state);
}
}

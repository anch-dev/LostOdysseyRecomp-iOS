#pragma once

#include "lo_semantics/object_float_record.h"
#include "lo_semantics/object_registration_post.h"

namespace lo::semantic::gpu::object_float_record_post_chain
{

class PostObserver
{
public:
    virtual ~PostObserver() = default;
    virtual void Enter(GuestAddress target, std::uint64_t incoming_r3,
        GuestAddress caller_sp, GuestAddress singleton) = 0;
};

struct Dependencies
{
    ManagerFacadeServices& manager;
    registered_constructor_family::RegistrationServices& external_registration;
    registered_callback_family::Services& callback;
    ObjectRegistrationServices& graph;
    object_registration_post::VirtualServices& dynamic;
    PostObserver& observer;
};

// Validation-only composition of 82384C08 -> 8242D038 -> 82627230.
// The accepted constructor API exposes the callback's low guest SP but no
// PPCContext; outer_sp supplies the tested high half for this bounded chain.
[[nodiscard]] bool Apply(GuestMemory& memory, Dependencies dependencies,
    object_float_record::Registers& state);

} // namespace lo::semantic::gpu::object_float_record_post_chain

#pragma once

#include "lo_semantics/crt_thread_data.h"
#include "lo_semantics/invalid_parameter.h"
#include "lo_semantics/metadata_name_record.h"
#include "lo_semantics/registered_metadata_words.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_name_lookup
{

struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r13 = 0;
    std::uint64_t r23 = 0;
    std::uint64_t r24 = 0;
    std::uint64_t r25 = 0;
    std::uint64_t r26 = 0;
    std::uint64_t r27 = 0;
    std::uint64_t r28 = 0;
    std::uint64_t r29 = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
    std::uint64_t r0 = 0;
    std::uint64_t ctr = 0;
    std::uint64_t r8 = 0;
    std::uint64_t r9 = 0;
    std::uint64_t r10 = 0;
};

// 82296D30: resolve a UTF-16 name, optionally parse a numeric suffix,
// create a record, and update the two live name/id indices. Its unready
// path composes the complete 823F4700 registry initializer, not a stand-in.
// r3-r7 and caller_sp are full PPC values. On a known address, result is the
// original live full r3, including the empty and creation-disabled paths.
// This boundary exposes own saved r26-r31/LR, r13 and the hash helper's r0/CTR;
// generic volatile effects inside lower services remain outside its contract.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    metadata_name_record::Services& record_services,
    ArrayResizeServices& resize_services,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::metadata_name_lookup

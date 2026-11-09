#ifndef DOMAIN_CHECKPOINT_H
#define DOMAIN_CHECKPOINT_H

#include "domain/value/transfer_id.h"

#include <cstdint>

namespace domain
{

/// @brief Resume point of a transfer.
///
/// `chunk_offset` is the number of the next chunk the receiver expects: the
/// highest contiguous chunk durably written to disk, plus one. A checkpoint
/// is never written before the corresponding chunk is durable, so
/// `chunk_offset` always points to a chunk boundary that is safe to resume
/// from. See `PROTOCOL.md` §Resumable Transfers.
struct Checkpoint
{
    TransferId transfer_id;
    std::uint64_t chunk_offset = 0;

    bool operator==(const Checkpoint&) const = default;
};

} // namespace domain

#endif

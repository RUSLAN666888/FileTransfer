#ifndef INFRA_KEY_STORE_I_KEY_STORE_H
#define INFRA_KEY_STORE_I_KEY_STORE_H

#include "infra/crypto/keypair.h"
#include "infra/result.h"

namespace infra
{

/// @brief Persistent storage for the peer's Ed25519 identity keypair.
class IKeystore
{
  public:
    virtual ~IKeystore() = default;

    /// @brief Load the stored keypair, or generate and persist a new one.
    ///
    /// Idempotent: the first call generates and stores; subsequent calls
    /// return the same keypair. Returns `Error` if the existing key is
    /// malformed or cannot be read.
    virtual Result<Keypair> load_or_create() = 0;

    /// @brief Replace the stored keypair. Used by "regenerate identity".
    ///
    /// The caller is responsible for the user-visible warning: changing the
    /// keypair changes the PeerId, and the old identity becomes unreachable.
    virtual Result<void> store(const Keypair& kp) = 0;
};

} // namespace infra

#endif

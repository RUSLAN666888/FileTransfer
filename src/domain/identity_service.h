#ifndef DOMAIN_IDENTITY_SERVICE_H
#define DOMAIN_IDENTITY_SERVICE_H

#include "domain/value/peer_id.h"
#include "infra/result.h"

namespace infra
{
class ICrypto;
class IKeystore;
} // namespace infra

namespace domain
{

/// @brief Loads or creates the local Ed25519 identity and derives the PeerId.
///
/// Wraps `IKeystore` (persistence) and `ICrypto` (SHA-256) into a single
/// "keypair -> PeerId" operation. The first call generates and stores a
/// keypair; subsequent calls return the same PeerId.
class IdentityService
{
  public:
    IdentityService(infra::IKeystore& keystore, infra::ICrypto& crypto);

    /// @brief Load the stored keypair or generate and persist a new one,
    ///        then derive the PeerId as `SHA-256(public_key)` truncated to
    ///        128 bits.
    ///
    /// Idempotent: the PeerId is stable across restarts, because the
    /// keypair is stable.
    infra::Result<PeerId> load_or_create();

  private:
    infra::IKeystore& keystore_;
    infra::ICrypto& crypto_;
};

} // namespace domain

#endif

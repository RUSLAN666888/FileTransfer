#include "domain/identity_service.h"

#include "infra/crypto/i_crypto.h"
#include "infra/keystore/i_keystore.h"

namespace domain
{

IdentityService::IdentityService(infra::IKeystore& keystore, infra::ICrypto& crypto) : keystore_(keystore), crypto_(crypto) {}

infra::Result<PeerId> IdentityService::load_or_create()
{
    auto kp = keystore_.load_or_create();
    if (!kp)
        return kp.error();

    std::array<std::uint8_t, infra::ICrypto::kSha256Bytes> hash{};
    crypto_.sha256(hash.data(), kp.value().public_key.data(), kp.value().public_key.size());

    return PeerId::from_sha256_truncated(hash);
}

} // namespace domain

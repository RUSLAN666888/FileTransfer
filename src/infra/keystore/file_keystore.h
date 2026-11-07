#ifndef INFRA_KEY_STORE_FILE_KEYSTORE_H
#define INFRA_KEY_STORE_FILE_KEYSTORE_H

#include "infra/keystore/i_keystore.h"

namespace infra
{

class ICrypto;
class IFileSystem;

class FileKeystore final : public IKeystore
{
  public:
    FileKeystore(IFileSystem& fs, ICrypto& crypto, std::string path);

    Result<Keypair> load_or_create() override;
    Result<void> store(const Keypair& kp) override;

  private:
    IFileSystem& fs_;
    ICrypto& crypto_;
    std::string path_;
};

} // namespace infra

#endif

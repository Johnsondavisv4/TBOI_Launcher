#include "core/file_hash.h"

#include <windows.h>
#include <wincrypt.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace TBOI {

std::string CalculateFileSha256(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return {};

    HCRYPTPROV provider = 0;
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return {};
    }

    HCRYPTHASH hash = 0;
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(provider, 0);
        return {};
    }

    bool valid = true;
    std::array<BYTE, 65536> buffer;
    while (file.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || file.gcount() > 0) {
        if (!CryptHashData(hash, buffer.data(), static_cast<DWORD>(file.gcount()), 0)) {
            valid = false;
            break;
        }
    }

    BYTE digest[32];
    DWORD digestSize = sizeof(digest);
    valid = valid && file.eof() && !file.bad() &&
        CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0);

    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    if (!valid) return {};

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (DWORD i = 0; i < digestSize; ++i) {
        hex << std::setw(2) << static_cast<unsigned int>(digest[i]);
    }
    return hex.str();
}

}

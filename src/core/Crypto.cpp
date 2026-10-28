#include "core/Crypto.h"

#include <windows.h>
#include <bcrypt.h>
#include <dpapi.h>

#include <fstream>

namespace sk::crypto {

namespace {

struct Sha256 {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    Sha256() {
        BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
    }
    ~Sha256() {
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    }
    void Add(const void* p, size_t n) { BCryptHashData(hash, (PUCHAR)p, (ULONG)n, 0); }
    std::string Hex() {
        unsigned char out[32];
        BCryptFinishHash(hash, out, sizeof out, 0);
        static const char* d = "0123456789abcdef";
        std::string s;
        for (unsigned char c : out) {
            s += d[c >> 4];
            s += d[c & 15];
        }
        return s;
    }
};

} 

std::string Sha256Hex(const std::vector<unsigned char>& data) {
    Sha256 h;
    h.Add(data.data(), data.size());
    return h.Hex();
}

std::string Sha256File(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    Sha256 h;
    std::vector<char> buf(1 << 16);
    while (f) {
        f.read(buf.data(), (std::streamsize)buf.size());
        if (f.gcount() > 0) h.Add(buf.data(), (size_t)f.gcount());
    }
    return h.Hex();
}

std::string Protect(const std::string& plain) {
    if (plain.empty()) return {};
    DATA_BLOB in{(DWORD)plain.size(), (BYTE*)plain.data()}, out{};
    if (!CryptProtectData(&in, L"SlayerKit", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
    std::vector<unsigned char> v(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return Base64Encode(v);
}

std::string Unprotect(const std::string& sealed) {
    if (sealed.empty()) return {};
    auto raw = Base64Decode(sealed);
    DATA_BLOB in{(DWORD)raw.size(), raw.data()}, out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
    std::string s((char*)out.pbData, out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return s;
}

} 

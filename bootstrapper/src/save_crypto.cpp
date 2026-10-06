#include "save_crypto.h"
#include "inflate.h"

#include <cstring>

uint32_t SaveCrypto::Rotl(uint32_t value, int bits) {
    return (value << bits) | (value >> (32 - bits));
}

void SaveCrypto::Sha1Init(Sha1& ctx) {
    ctx.h[0] = 0x67452301;
    ctx.h[1] = 0xEFCDAB89;
    ctx.h[2] = 0x98BADCFE;
    ctx.h[3] = 0x10325476;
    ctx.h[4] = 0xC3D2E1F0;
    ctx.length = 0;
    ctx.bufferLen = 0;
}

void SaveCrypto::Sha1Transform(Sha1& ctx, const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) | (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
    for (int i = 16; i < 80; i++) w[i] = Rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = ctx.h[0], b = ctx.h[1], c = ctx.h[2], d = ctx.h[3], e = ctx.h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f = 0, k = 0;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        const uint32_t temp = Rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = Rotl(b, 30);
        b = a;
        a = temp;
    }

    ctx.h[0] += a;
    ctx.h[1] += b;
    ctx.h[2] += c;
    ctx.h[3] += d;
    ctx.h[4] += e;
}

void SaveCrypto::Sha1Update(Sha1& ctx, const uint8_t* data, size_t len) {
    ctx.length += static_cast<uint64_t>(len) * 8;
    while (len > 0) {
        size_t take = 64 - ctx.bufferLen;
        if (take > len) take = len;
        std::memcpy(ctx.buffer + ctx.bufferLen, data, take);
        ctx.bufferLen += take;
        data += take;
        len -= take;
        if (ctx.bufferLen == 64) {
            Sha1Transform(ctx, ctx.buffer);
            ctx.bufferLen = 0;
        }
    }
}

void SaveCrypto::Sha1Final(Sha1& ctx, uint8_t out[20]) {
    const uint64_t bitLength = ctx.length;

    const uint8_t pad = 0x80;
    Sha1Update(ctx, &pad, 1);
    const uint8_t zero = 0;
    while (ctx.bufferLen != 56) Sha1Update(ctx, &zero, 1);

    uint8_t lengthBytes[8];
    for (int i = 0; i < 8; i++) lengthBytes[7 - i] = static_cast<uint8_t>(bitLength >> (i * 8));
    Sha1Update(ctx, lengthBytes, 8);

    for (int i = 0; i < 5; i++) {
        out[i * 4] = static_cast<uint8_t>(ctx.h[i] >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(ctx.h[i] >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(ctx.h[i] >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(ctx.h[i]);
    }
}

void SaveCrypto::HmacSha1(const uint8_t* key, size_t keyLen, const uint8_t* message, size_t messageLen, uint8_t out[20]) {
    uint8_t padded[64];
    std::memset(padded, 0, sizeof(padded));
    if (keyLen > 64) {
        Sha1 hasher;
        Sha1Init(hasher);
        Sha1Update(hasher, key, keyLen);
        Sha1Final(hasher, padded);
    } else {
        std::memcpy(padded, key, keyLen);
    }

    uint8_t inner[64];
    uint8_t outer[64];
    for (int i = 0; i < 64; i++) {
        inner[i] = padded[i] ^ 0x36;
        outer[i] = padded[i] ^ 0x5c;
    }

    Sha1 context;
    Sha1Init(context);
    Sha1Update(context, inner, 64);
    Sha1Update(context, message, messageLen);
    uint8_t innerHash[20];
    Sha1Final(context, innerHash);

    Sha1Init(context);
    Sha1Update(context, outer, 64);
    Sha1Update(context, innerHash, 20);
    Sha1Final(context, out);
}

void SaveCrypto::Pbkdf2Sha1(const std::string& password, const uint8_t* salt, size_t saltLen, uint32_t iterations, uint8_t* out, size_t outLen) {
    const uint8_t* pw = reinterpret_cast<const uint8_t*>(password.data());
    const size_t pwLen = password.size();

    std::vector<uint8_t> block(saltLen + 4);
    std::memcpy(block.data(), salt, saltLen);

    uint32_t counter = 1;
    size_t generated = 0;
    while (generated < outLen) {
        block[saltLen] = static_cast<uint8_t>(counter >> 24);
        block[saltLen + 1] = static_cast<uint8_t>(counter >> 16);
        block[saltLen + 2] = static_cast<uint8_t>(counter >> 8);
        block[saltLen + 3] = static_cast<uint8_t>(counter);

        uint8_t u[20];
        uint8_t t[20];
        HmacSha1(pw, pwLen, block.data(), block.size(), u);
        std::memcpy(t, u, sizeof(u));

        for (uint32_t i = 1; i < iterations; i++) {
            HmacSha1(pw, pwLen, u, sizeof(u), u);
            for (int j = 0; j < 20; j++) t[j] ^= u[j];
        }

        size_t take = outLen - generated;
        if (take > sizeof(t)) take = sizeof(t);
        std::memcpy(out + generated, t, take);
        generated += take;
        counter++;
    }
}

void SaveCrypto::ExpandKey(const uint8_t key[16], uint8_t roundKeys[11][16]) {
    uint8_t expanded[176];
    std::memcpy(expanded, key, 16);

    int generated = 16;
    int rconIndex = 1;
    while (generated < 176) {
        uint8_t temp[4] = {expanded[generated - 4], expanded[generated - 3], expanded[generated - 2], expanded[generated - 1]};
        if (generated % 16 == 0) {
            const uint8_t first = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = first;
            for (int i = 0; i < 4; i++) temp[i] = kSbox[temp[i]];
            temp[0] ^= kRcon[rconIndex++];
        }
        for (int i = 0; i < 4; i++) {
            expanded[generated] = expanded[generated - 16] ^ temp[i];
            generated++;
        }
    }
    std::memcpy(roundKeys, expanded, 176);
}

void SaveCrypto::AddRoundKey(uint8_t state[16], const uint8_t roundKey[16]) {
    for (int i = 0; i < 16; i++) state[i] ^= roundKey[i];
}

void SaveCrypto::SubBytes(uint8_t state[16]) {
    for (int i = 0; i < 16; i++) state[i] = kSbox[state[i]];
}

void SaveCrypto::InvSubBytes(uint8_t state[16]) {
    for (int i = 0; i < 16; i++) state[i] = kInvSbox[state[i]];
}

void SaveCrypto::ShiftRows(uint8_t state[16]) {
    for (int row = 1; row < 4; row++) {
        uint8_t values[4];
        for (int col = 0; col < 4; col++) values[col] = state[row + 4 * col];
        for (int col = 0; col < 4; col++) state[row + 4 * col] = values[(col + row) % 4];
    }
}

void SaveCrypto::InvShiftRows(uint8_t state[16]) {
    for (int row = 1; row < 4; row++) {
        uint8_t values[4];
        for (int col = 0; col < 4; col++) values[col] = state[row + 4 * col];
        for (int col = 0; col < 4; col++) state[row + 4 * col] = values[(col + 4 - row) % 4];
    }
}

uint8_t SaveCrypto::GfMultiply(uint8_t a, uint8_t b) {
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) result ^= a;
        const uint8_t high = a & 0x80;
        a <<= 1;
        if (high) a ^= 0x1B;
        b >>= 1;
    }
    return result;
}

void SaveCrypto::MixColumns(uint8_t state[16]) {
    for (int col = 0; col < 4; col++) {
        const int o = col * 4;
        const uint8_t a0 = state[o], a1 = state[o + 1], a2 = state[o + 2], a3 = state[o + 3];
        state[o] = GfMultiply(a0, 2) ^ GfMultiply(a1, 3) ^ a2 ^ a3;
        state[o + 1] = a0 ^ GfMultiply(a1, 2) ^ GfMultiply(a2, 3) ^ a3;
        state[o + 2] = a0 ^ a1 ^ GfMultiply(a2, 2) ^ GfMultiply(a3, 3);
        state[o + 3] = GfMultiply(a0, 3) ^ a1 ^ a2 ^ GfMultiply(a3, 2);
    }
}

void SaveCrypto::InvMixColumns(uint8_t state[16]) {
    for (int col = 0; col < 4; col++) {
        const int o = col * 4;
        const uint8_t a0 = state[o], a1 = state[o + 1], a2 = state[o + 2], a3 = state[o + 3];
        state[o] = GfMultiply(a0, 14) ^ GfMultiply(a1, 11) ^ GfMultiply(a2, 13) ^ GfMultiply(a3, 9);
        state[o + 1] = GfMultiply(a0, 9) ^ GfMultiply(a1, 14) ^ GfMultiply(a2, 11) ^ GfMultiply(a3, 13);
        state[o + 2] = GfMultiply(a0, 13) ^ GfMultiply(a1, 9) ^ GfMultiply(a2, 14) ^ GfMultiply(a3, 11);
        state[o + 3] = GfMultiply(a0, 11) ^ GfMultiply(a1, 13) ^ GfMultiply(a2, 9) ^ GfMultiply(a3, 14);
    }
}

void SaveCrypto::EncryptBlock(const uint8_t in[16], const uint8_t roundKeys[11][16], uint8_t out[16]) {
    uint8_t state[16];
    std::memcpy(state, in, 16);

    AddRoundKey(state, roundKeys[0]);
    for (int round = 1; round < 10; round++) {
        SubBytes(state);
        ShiftRows(state);
        MixColumns(state);
        AddRoundKey(state, roundKeys[round]);
    }
    SubBytes(state);
    ShiftRows(state);
    AddRoundKey(state, roundKeys[10]);

    std::memcpy(out, state, 16);
}

void SaveCrypto::DecryptBlock(const uint8_t in[16], const uint8_t roundKeys[11][16], uint8_t out[16]) {
    uint8_t state[16];
    std::memcpy(state, in, 16);

    AddRoundKey(state, roundKeys[10]);
    for (int round = 9; round > 0; round--) {
        InvShiftRows(state);
        InvSubBytes(state);
        AddRoundKey(state, roundKeys[round]);
        InvMixColumns(state);
    }
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(state, roundKeys[0]);

    std::memcpy(out, state, 16);
}

std::vector<uint8_t> SaveCrypto::CbcEncrypt(const std::vector<uint8_t>& plaintext, const uint8_t key[16], const uint8_t iv[16]) {
    uint8_t roundKeys[11][16];
    ExpandKey(key, roundKeys);

    std::vector<uint8_t> ciphertext;
    ciphertext.reserve(plaintext.size());

    uint8_t previous[16];
    std::memcpy(previous, iv, 16);

    for (size_t offset = 0; offset < plaintext.size(); offset += 16) {
        uint8_t block[16];
        for (int i = 0; i < 16; i++) block[i] = plaintext[offset + i] ^ previous[i];

        uint8_t encrypted[16];
        EncryptBlock(block, roundKeys, encrypted);

        ciphertext.insert(ciphertext.end(), encrypted, encrypted + 16);
        std::memcpy(previous, encrypted, 16);
    }

    return ciphertext;
}

std::vector<uint8_t> SaveCrypto::CbcDecrypt(const std::vector<uint8_t>& ciphertext, const uint8_t key[16], const uint8_t iv[16]) {
    uint8_t roundKeys[11][16];
    ExpandKey(key, roundKeys);

    std::vector<uint8_t> plaintext;
    plaintext.reserve(ciphertext.size());

    uint8_t previous[16];
    std::memcpy(previous, iv, 16);

    for (size_t offset = 0; offset < ciphertext.size(); offset += 16) {
        uint8_t decrypted[16];
        DecryptBlock(ciphertext.data() + offset, roundKeys, decrypted);

        for (int i = 0; i < 16; i++) plaintext.push_back(decrypted[i] ^ previous[i]);
        std::memcpy(previous, ciphertext.data() + offset, 16);
    }

    return plaintext;
}

bool SaveCrypto::RemovePadding(const std::vector<uint8_t>& data, std::vector<uint8_t>& out) {
    if (data.empty()) return false;

    const uint8_t padding = data.back();
    if (padding < 1 || padding > 16 || padding > data.size()) return false;
    for (size_t i = data.size() - padding; i < data.size(); i++) {
        if (data[i] != padding) return false;
    }

    out.assign(data.begin(), data.end() - padding);
    return true;
}

bool SaveCrypto::Decrypt(const std::vector<uint8_t>& input, const std::string& password, std::vector<uint8_t>& output, bool& wasGzipped, std::wstring& error) {
    if (input.size() < 32) {
        error = L"Input is too short to contain an IV and ciphertext.";
        return false;
    }

    const uint8_t* iv = input.data();
    std::vector<uint8_t> ciphertext(input.begin() + 16, input.end());
    if (ciphertext.size() % 16 != 0) {
        error = L"Encrypted data length must be a nonzero multiple of 16 bytes.";
        return false;
    }

    uint8_t key[16];
    Pbkdf2Sha1(password, iv, 16, 100, key, sizeof(key));

    std::vector<uint8_t> decrypted = CbcDecrypt(ciphertext, key, iv);

    std::vector<uint8_t> plaintext;
    if (!RemovePadding(decrypted, plaintext)) {
        error = L"Invalid PKCS#7 padding. The password or cipher may be wrong.";
        return false;
    }

    wasGzipped = Inflate::LooksGzipped(plaintext);
    if (wasGzipped) {
        std::vector<uint8_t> inflated;
        if (!Inflate::Gzip(plaintext.data(), plaintext.size(), inflated)) {
            error = L"The decrypted data starts with a gzip header but decompression failed.";
            return false;
        }
        output.swap(inflated);
    } else {
        output.swap(plaintext);
    }

    return true;
}

bool SaveCrypto::Encrypt(const std::vector<uint8_t>& input, const std::string& password, bool gzip, std::vector<uint8_t>& output, std::wstring& error) {
    if (gzip) {
        error = L"gzip compression is not supported.";
        return false;
    }

    uint8_t iv[16];
    if (BCryptGenRandom(nullptr, iv, sizeof(iv), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        error = L"Failed to generate a random IV.";
        return false;
    }

    uint8_t key[16];
    Pbkdf2Sha1(password, iv, 16, 100, key, sizeof(key));

    const size_t padding = 16 - (input.size() % 16);
    std::vector<uint8_t> padded = input;
    padded.insert(padded.end(), padding, static_cast<uint8_t>(padding));

    std::vector<uint8_t> ciphertext = CbcEncrypt(padded, key, iv);

    output.clear();
    output.insert(output.end(), iv, iv + 16);
    output.insert(output.end(), ciphertext.begin(), ciphertext.end());
    return true;
}

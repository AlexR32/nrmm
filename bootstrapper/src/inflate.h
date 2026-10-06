#pragma once

#include "framework.h"

class Inflate {
public:
    Inflate() = delete;

    // Decompresses a complete gzip stream (RFC 1952). Returns false when the data is not a valid gzip stream or is corrupt.
    static bool Gzip(const uint8_t* src, size_t size, std::vector<uint8_t>& out);

    // Returns true when `data` begins with the gzip magic bytes.
    static bool LooksGzipped(const std::vector<uint8_t>& data);

private:
    static constexpr int kMaxBits = 15;
    static constexpr int kMaxSymbols = 320;

    struct Huffman {
        short count[kMaxBits + 1];
        short symbol[kMaxSymbols];
    };

    struct BitReader {
        const uint8_t* data = nullptr;
        size_t size = 0;
        size_t pos = 0;
        uint32_t bitbuf = 0;
        int bitcnt = 0;
        bool overflow = false;

        int Bits(int need) {
            while (bitcnt < need) {
                if (pos >= size) {
                    overflow = true;
                    return 0;
                }
                bitbuf |= static_cast<uint32_t>(data[pos++]) << bitcnt;
                bitcnt += 8;
            }
            const int value = static_cast<int>(bitbuf & ((1u << need) - 1));
            bitbuf >>= need;
            bitcnt -= need;
            return value;
        }
    };

    static int Build(Huffman* h, const uint8_t* lengths, int n);
    static int Decode(BitReader* br, const Huffman* h);
    static bool InflateBlock(BitReader* br, std::vector<uint8_t>& out, const Huffman* lencode, const Huffman* distcode);
    static bool StoredBlock(BitReader* br, std::vector<uint8_t>& out);
    static bool FixedBlock(BitReader* br, std::vector<uint8_t>& out);
    static bool DynamicBlock(BitReader* br, std::vector<uint8_t>& out);
    static bool InflateStream(BitReader* br, std::vector<uint8_t>& out);
};

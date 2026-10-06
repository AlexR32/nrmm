#include "inflate.h"

#include <cstring>

// Minimal DEFLATE/gzip decoder. The crypt path only ever needs to *read* gzip (the game writes plain JSON), so there is deliberately no compressor here.
int Inflate::Build(Huffman* h, const uint8_t* lengths, int n) {
    for (int len = 0; len <= kMaxBits; len++) h->count[len] = 0;
    for (int sym = 0; sym < n; sym++) h->count[lengths[sym]]++;
    if (h->count[0] == n) return 0;

    int left = 1;
    for (int len = 1; len <= kMaxBits; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return -1;
    }

    short offsets[kMaxBits + 1];
    offsets[1] = 0;
    for (int len = 1; len < kMaxBits; len++) offsets[len + 1] = offsets[len] + h->count[len];

    for (int sym = 0; sym < n; sym++) {
        if (lengths[sym]) h->symbol[offsets[lengths[sym]]++] = static_cast<short>(sym);
    }
    return left;
}

int Inflate::Decode(BitReader* br, const Huffman* h) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= kMaxBits; len++) {
        code |= br->Bits(1);
        const int count = h->count[len];
        if (code - first < count) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

bool Inflate::InflateBlock(BitReader* br, std::vector<uint8_t>& out, const Huffman* lencode, const Huffman* distcode) {
    static const uint16_t kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    for (;;) {
        int symbol = Decode(br, lencode);
        if (symbol < 0) return false;

        if (symbol < 256) {
            out.push_back(static_cast<uint8_t>(symbol));
        } else if (symbol == 256) {
            return true;
        } else {
            symbol -= 257;
            if (symbol >= 29) return false;
            const int length = kLenBase[symbol] + br->Bits(kLenExtra[symbol]);

            const int distSymbol = Decode(br, distcode);
            if (distSymbol < 0 || distSymbol >= 30) return false;
            const int distance = kDistBase[distSymbol] + br->Bits(kDistExtra[distSymbol]);

            if (distance <= 0 || static_cast<size_t>(distance) > out.size()) return false;
            const size_t start = out.size() - static_cast<size_t>(distance);
            for (int i = 0; i < length; i++) out.push_back(out[start + static_cast<size_t>(i)]);
        }

        if (br->overflow) return false;
    }
}

bool Inflate::StoredBlock(BitReader* br, std::vector<uint8_t>& out) {
    // Discard any remaining bits of the current byte (stored blocks are byte aligned).
    br->bitbuf = 0;
    br->bitcnt = 0;

    const int len = br->Bits(16);
    const int nlen = br->Bits(16);
    if (br->overflow) return false;
    if ((len ^ 0xFFFF) != nlen) return false;
    if (br->pos + static_cast<size_t>(len) > br->size) return false;

    out.insert(out.end(), br->data + br->pos, br->data + br->pos + len);
    br->pos += static_cast<size_t>(len);
    return true;
}

bool Inflate::FixedBlock(BitReader* br, std::vector<uint8_t>& out) {
    uint8_t litLengths[288];
    for (int i = 0; i < 144; i++) litLengths[i] = 8;
    for (int i = 144; i < 256; i++) litLengths[i] = 9;
    for (int i = 256; i < 280; i++) litLengths[i] = 7;
    for (int i = 280; i < 288; i++) litLengths[i] = 8;

    Huffman lencode;
    if (Build(&lencode, litLengths, 288) < 0) return false;

    uint8_t distLengths[30];
    for (int i = 0; i < 30; i++) distLengths[i] = 5;
    Huffman distcode;
    if (Build(&distcode, distLengths, 30) < 0) return false;

    return InflateBlock(br, out, &lencode, &distcode);
}

bool Inflate::DynamicBlock(BitReader* br, std::vector<uint8_t>& out) {
    static const int kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    const int nlen = br->Bits(5) + 257;
    const int ndist = br->Bits(5) + 1;
    const int ncode = br->Bits(4) + 4;
    if (br->overflow) return false;
    if (nlen + ndist > kMaxSymbols) return false;

    uint8_t codeLengths[19];
    std::memset(codeLengths, 0, sizeof(codeLengths));
    for (int i = 0; i < ncode; i++) codeLengths[kOrder[i]] = static_cast<uint8_t>(br->Bits(3));

    Huffman lengthCode;
    if (Build(&lengthCode, codeLengths, 19) < 0) return false;

    uint8_t combined[kMaxSymbols];
    std::memset(combined, 0, sizeof(combined));

    int index = 0;
    while (index < nlen + ndist) {
        const int symbol = Decode(br, &lengthCode);
        if (symbol < 0) return false;
        if (symbol < 16) {
            combined[index++] = static_cast<uint8_t>(symbol);
        } else {
            int value = 0;
            int repeat = 0;
            if (symbol == 16) {
                if (index == 0) return false;
                value = combined[index - 1];
                repeat = 3 + br->Bits(2);
            } else if (symbol == 17) {
                repeat = 3 + br->Bits(3);
            } else {
                repeat = 11 + br->Bits(7);
            }
            if (index + repeat > nlen + ndist) return false;
            while (repeat-- > 0) combined[index++] = static_cast<uint8_t>(value);
        }
        if (br->overflow) return false;
    }

    Huffman litCode;
    Huffman distCode;
    if (Build(&litCode, combined, nlen) < 0) return false;
    if (Build(&distCode, combined + nlen, ndist) < 0) return false;

    return InflateBlock(br, out, &litCode, &distCode);
}

bool Inflate::InflateStream(BitReader* br, std::vector<uint8_t>& out) {
    for (;;) {
        const int last = br->Bits(1);
        const int type = br->Bits(2);
        if (br->overflow) return false;

        bool ok = false;
        switch (type) {
            case 0: ok = StoredBlock(br, out); break;
            case 1: ok = FixedBlock(br, out); break;
            case 2: ok = DynamicBlock(br, out); break;
            default: return false;
        }
        if (!ok) return false;
        if (last) return true;
    }
}

bool Inflate::LooksGzipped(const std::vector<uint8_t>& data) {
    return data.size() >= 2 && data[0] == 0x1f && data[1] == 0x8b;
}

bool Inflate::Gzip(const uint8_t* src, size_t size, std::vector<uint8_t>& out) {
    if (size < 18) return false;
    if (src[0] != 0x1f || src[1] != 0x8b) return false;
    if (src[2] != 0x08) return false;

    const uint8_t flags = src[3];
    size_t pos = 10;

    if (flags & 0x04) {  // FEXTRA
        if (pos + 2 > size) return false;
        const size_t extra = static_cast<size_t>(src[pos]) | (static_cast<size_t>(src[pos + 1]) << 8);
        pos += 2 + extra;
    }
    if (flags & 0x08) {  // FNAME
        while (pos < size && src[pos] != 0) pos++;
        pos++;
    }
    if (flags & 0x10) {  // FCOMMENT
        while (pos < size && src[pos] != 0) pos++;
        pos++;
    }
    if (flags & 0x02) pos += 2;  // FHCRC

    if (pos >= size) return false;

    BitReader br;
    br.data = src;
    br.size = size;
    br.pos = pos;
    return InflateStream(&br, out);
}

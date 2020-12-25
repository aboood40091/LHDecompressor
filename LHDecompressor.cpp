// LH (LZ77+Huffman) decompressor in C++.
// Decompiled from NSMBW and simplified by hand

// Previously influenced by the sead::SZSDecompressor decompilation:
// https://github.com/open-ead/sead/blob/master/modules/src/resource/seadSZSDecompressor.cpp

#include <iostream>
#include <fstream>
#include <cstdint>
using namespace std;

typedef   int8_t s8;
typedef  uint8_t u8;
typedef  int16_t s16;
typedef uint16_t u16;
typedef  int32_t s32;
typedef uint32_t u32;
typedef  int64_t s64;
typedef uint64_t u64;

// Buffer where the length and offset huffman tables will be stored
static u16 WorkBuffer[1024 + 64];

inline u16 Swap16(u16 x)
{
    return (x << 8 | x >> 8) & 0xFFFF;
}

inline u32 Swap32(u32 x)
{
    return x << 24 |
          (x & 0xFF00) << 8 |
           x >> 24 |
           x >> 8 & 0xFF00;
}


class BitReader
{
public:
    inline s32 readS32(u8 nBits)
    {
        while (bitStreamLen < nBits)
        {
            if (srcCount == 0)
                return -1;

            bitStream <<= 8;
            bitStream += srcp[0];
            srcp += 1;
            srcCount -= 1;
            bitStreamLen += 8;
        }

        s32 ret = (s32)(bitStream >> bitStreamLen - nBits & (1 << nBits) - 1);
        bitStreamLen -= nBits;

        return ret;
    }

    inline s64 readS64(u8 nBits)
    {
        u8 overflow = 0;

        while (bitStreamLen < nBits)
        {
            if (srcCount == 0)
                return -1;

            if (bitStreamLen > 24)
                overflow = (u8)(bitStream >> 24);

            bitStream <<= 8;
            bitStream += srcp[0];
            srcp += 1;
            srcCount -= 1;
            bitStreamLen += 8;
        }

        s64 ret = bitStream;
        ret |= (s64)overflow << 32;
        ret = (s64)(ret >> bitStreamLen - nBits & ((s64)1 << nBits) - 1);
        bitStreamLen -= nBits;

        return ret;
    }

    const u8* srcp;
    u32 srcCount;
    u32 bitStream;
    u32 bitStreamLen;
};

class LHDecompressor
{
public:
    static u32 getDecompSize(const void* src);
    static s32 decomp(u8* dst, const void* src, u32 srcSize);
};

u32 LHDecompressor::getDecompSize(const void* src)
{
    // Assumes little-endian host
    u32 size = ((u32*)src)[0] >> 8;
    if (size == 0)
        size = ((u32*)src)[1];

    return size;
}

s32 LHDecompressor::decomp(u8* dst, const void* src, u32 srcSize)
{
    s32 bits32, destCount, huffLen;
    s64 bits64;
    u32 sizeAndMagic, currentIdx, offset;
    u16 *lengthHuffTbl;
    u16 *offsetHuffTbl;
    u16 *currentNode;
    u16 copyLen, n, lzOffset;
    u8 shift;
    s8 nOffsetBits;

    BitReader reader;
    reader.srcp = (const u8*)src;
    reader.srcCount = srcSize;
    reader.bitStream = 0;
    reader.bitStreamLen = 0;

    bits64 = reader.readS64(32);
    if (bits64 < 0)
        return -1;

    sizeAndMagic = Swap32((u32)bits64);
    if ((sizeAndMagic & 0xF0) != 0x40)
        return -1;

    destCount = (s32)(sizeAndMagic >> 8);
    if (destCount == 0)
    {
        bits64 = reader.readS64(32);
        if (bits64 < 0)
            return -1;

        destCount = (s32)Swap32((u32)bits64);
    }

    bits32 = reader.readS32(16);
    if (bits32 < 0)
    {
        if (destCount == 0 and 0x20 < reader.bitStreamLen)
            return -3;

        return destCount;
    }

    lengthHuffTbl = WorkBuffer;
    currentIdx = 1;
    huffLen = (Swap16((u16)bits32) + 1 << 5) - 16;

    while (huffLen >= 9)
    {
        bits32 = reader.readS32(9);
        if (bits32 < 0)
        {
            if (destCount == 0 and 0x20 < reader.bitStreamLen)
                return -3;

            return destCount;
        }

        lengthHuffTbl[currentIdx] = (u16)bits32;
        currentIdx += 1;
        huffLen -= 9;
    }

    if (huffLen > 0)
    {
        bits32 = reader.readS32((u8)huffLen);
        if (bits32 < 0)
        {
            if (destCount == 0 and 0x20 < reader.bitStreamLen)
                return -3;

            return destCount;
        }

        huffLen = 0;
    }

    bits32 = reader.readS32(8);
    if (bits32 < 0)
    {
        if (destCount == 0 and 0x20 < reader.bitStreamLen)
            return -3;

        return destCount;
    }

    offsetHuffTbl = WorkBuffer + 1024;
    currentIdx = 1;
    huffLen = ((u16)bits32 + 1 << 5) - 8;

    while (huffLen >= 5)
    {
        bits32 = reader.readS32(5);
        if (bits32 < 0)
        {
            if (destCount == 0 and 0x20 < reader.bitStreamLen)
                return -3;

            return destCount;
        }

        offsetHuffTbl[currentIdx] = (u16)bits32;
        currentIdx += 1;
        huffLen -= 5;
    }

    if (huffLen > 0)
    {
        bits32 = reader.readS32((u8)huffLen);
        if (bits32 < 0)
        {
            if (destCount == 0 and 0x20 < reader.bitStreamLen)
                return -3;

            return destCount;
        }

        huffLen = 0;
    }

    while (destCount > 0)
    {
        currentNode = lengthHuffTbl + 1;

        while (true)
        {
            bits32 = reader.readS32(1);
            if (bits32 < 0)
            {
                if (destCount == 0 and 0x20 < reader.bitStreamLen)
                    return -3;

                return destCount;
            }

            shift = (u8)(bits32 & 1);
            offset = (((u32)currentNode[0] & 0x7F) + 1 << 1) + shift;

            if (currentNode[0] & 0x100 >> shift)
            {
                copyLen = ((u16*)((uintptr_t)currentNode & ~(uintptr_t)3))[offset];
                currentNode = offsetHuffTbl + 1;
                break;
            }

            else
                currentNode = (u16*)((uintptr_t)currentNode & ~(uintptr_t)3) + offset;
        }

        if (copyLen < 0x100)
        {
            dst[0] = (u8)copyLen;
            dst += 1;
            destCount -= 1;
        }

        else
        {
            n = (u16)((copyLen & 0xFF) + 3);

            while (true)
            {
                bits32 = reader.readS32(1);
                if (bits32 < 0)
                {
                    if (destCount == 0 and 0x20 < reader.bitStreamLen)
                        return -3;

                    return destCount;
                }

                shift = (u8)(bits32 & 1);
                offset = (((u32)currentNode[0] & 7) + 1 << 1) + shift;

                if (currentNode[0] & 0x10 >> shift)
                {
                    currentNode = (u16*)((uintptr_t)currentNode & ~(uintptr_t)3);
                    nOffsetBits = (s8)currentNode[offset];
                    break;
                }

                else
                    currentNode = (u16*)((uintptr_t)currentNode & ~(uintptr_t)3) + offset;
            }

            if (nOffsetBits <= 1)
                bits32 = nOffsetBits;

            else
            {
                bits32 = reader.readS32((u8)(nOffsetBits - 1));
                if (bits32 < 0)
                {
                    if (destCount == 0 and 0x20 < reader.bitStreamLen)
                        return -3;

                    return destCount;
                }
            }

            if (nOffsetBits >= 2)
                bits32 |= 1 << nOffsetBits - 1;

            nOffsetBits = -1;
            lzOffset = (u16)(bits32 + 1);

            if (destCount < n)
                n = (u16)destCount;

            destCount -= n;
            while (n--)
            {
                dst[0] = dst[-lzOffset];
                dst += 1;
            }
        }
    }

    if (0x20 < reader.bitStreamLen)
        return -3;

    return 0;
}

int main(int argc, char **argv)
{
	u8 *inBuf, *outBuf;
	int inLength, outLength;

	cout << "LH Decompressor v2.0" << endl;

	if (argc < 3)
    {
		cout << "No filenames specified.\nTo run: " << argv[0] << " CompFile.bin UncompFile.bin" << endl;
		return 1;
	}

	ifstream loadFile(argv[1], ifstream::in|ifstream::binary);

	loadFile.seekg(0, ios::end);
	inLength = loadFile.tellg();
	loadFile.seekg(0, ios::beg);

	inBuf = new u8[inLength];
	loadFile.read((char*)inBuf, inLength);
	loadFile.close();

	outLength = LHDecompressor::getDecompSize(inBuf);
	outBuf = new u8[outLength];
	s32 res = LHDecompressor::decomp(outBuf, inBuf, inLength);
	if (res != 0)
    {
        cout << "Failed to uncompress entire LH source data! Error code: " << res << endl;
		return 1;
    }

    ofstream outFile(argv[2], ifstream::out|ifstream::binary);
    outFile.write((char*)outBuf, outLength);
    outFile.close();

	delete inBuf;
	delete outBuf;
}


#include <cassert>
#include <utils/HashUtils.h>

// https://github.com/veorq/SipHash/blob/master/siphash.c

/*
   SipHash reference C implementation

   Copyright (c) 2012-2022 Jean-Philippe Aumasson
   <jeanphilippe.aumasson@gmail.com>
   Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>

   To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   You should have received a copy of the CC0 Public Domain Dedication along
   with
   this software. If not, see
   <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#ifndef cROUNDS
#define cROUNDS 2
#endif
#ifndef dROUNDS
#define dROUNDS 4
#endif

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define U32TO8_LE(p, v)                                                        \
    (p)[0] = (uint8_t)((v));                                                   \
    (p)[1] = (uint8_t)((v) >> 8);                                              \
    (p)[2] = (uint8_t)((v) >> 16);                                             \
    (p)[3] = (uint8_t)((v) >> 24);

#define U64TO8_LE(p, v)                                                        \
    U32TO8_LE((p), (uint32_t)((v)));                                           \
    U32TO8_LE((p) + 4, (uint32_t)((v) >> 32));

#define U8TO64_LE(p)                                                           \
    (((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) |                        \
     ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) |                 \
     ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) |                 \
     ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56))

#define SIPROUND                                                               \
    do                                                                         \
    {                                                                          \
        v0 += v1;                                                              \
        v1 = ROTL(v1, 13);                                                     \
        v1 ^= v0;                                                              \
        v0 = ROTL(v0, 32);                                                     \
        v2 += v3;                                                              \
        v3 = ROTL(v3, 16);                                                     \
        v3 ^= v2;                                                              \
        v0 += v3;                                                              \
        v3 = ROTL(v3, 21);                                                     \
        v3 ^= v0;                                                              \
        v2 += v1;                                                              \
        v1 = ROTL(v1, 17);                                                     \
        v1 ^= v2;                                                              \
        v2 = ROTL(v2, 32);                                                     \
    } while (0)

#ifdef DEBUG_SIPHASH
#include <stdio.h>

#define TRACE                                                                  \
    do                                                                         \
    {                                                                          \
        printf("(%3zu) v0 %016" PRIx64 "\n", inlen, v0);                       \
        printf("(%3zu) v1 %016" PRIx64 "\n", inlen, v1);                       \
        printf("(%3zu) v2 %016" PRIx64 "\n", inlen, v2);                       \
        printf("(%3zu) v3 %016" PRIx64 "\n", inlen, v3);                       \
    } while (0)
#else
#define TRACE
#endif

/*
    Computes a SipHash value
    *in: pointer to input data (read-only)
    inlen: input data length in bytes (any size_t value)
    *k: pointer to the key data (read-only), must be 16 bytes
    *out: pointer to output data (write-only), outlen bytes must be allocated
    outlen: length of the output in bytes, must be 8 or 16
*/
uint64_t HashUtils::SipHash(uint64_t in, uint64_t k0, uint64_t k1, uint64_t v0,
                            uint64_t v1, uint64_t v2, uint64_t v3)
{

    uint64_t out_value = 0;
    void *p_out = &out_value;
    uint8_t *out = static_cast<uint8_t *>(p_out);
    int in_len = 8;

    void *p_in = &in;
    const unsigned char *ni = static_cast<const unsigned char *>(p_in);
    // const unsigned char *kk = static_cast<const unsigned char *>(K);

    // assert((OutLen == 8) || (OutLen == 16));
    // uint64_t v0 = UINT64_C(0x736f6d6570736575);
    // uint64_t v1 = UINT64_C(0x646f72616e646f6d);
    // uint64_t v2 = UINT64_C(0x6c7967656e657261);
    // uint64_t v3 = UINT64_C(0x7465646279746573);
    // uint64_t k0 = U8TO64_LE(kk);
    // uint64_t k1 = U8TO64_LE(kk + 8);
    int i;
    const unsigned char *end = ni + in_len - (in_len % sizeof(uint64_t));
    const int left = in_len & 7;
    uint64_t b = static_cast<uint64_t>(in_len) << 56;
    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;

    // if (OutLen == 16)
    //     v1 ^= 0xee;

    for (; ni != end; ni += 8)
    {
        uint64_t m = U8TO64_LE(ni);
        v3 ^= m;

        TRACE;
        for (i = 0; i < cROUNDS; ++i)
            SIPROUND;

        v0 ^= m;
    }

    switch (left)
    {
    case 7:
        b |= static_cast<uint64_t>(ni[6]) << 48;
        /* FALLTHRU */
    case 6:
        b |= static_cast<uint64_t>(ni[5]) << 40;
        /* FALLTHRU */
    case 5:
        b |= static_cast<uint64_t>(ni[4]) << 32;
        /* FALLTHRU */
    case 4:
        b |= static_cast<uint64_t>(ni[3]) << 24;
        /* FALLTHRU */
    case 3:
        b |= static_cast<uint64_t>(ni[2]) << 16;
        /* FALLTHRU */
    case 2:
        b |= static_cast<uint64_t>(ni[1]) << 8;
        /* FALLTHRU */
    case 1:
        b |= static_cast<uint64_t>(ni[0]);
        break;
    case 0:
        break;
    default:
        break;
    }

    v3 ^= b;

    TRACE;
    for (i = 0; i < cROUNDS; ++i)
        SIPROUND;

    v0 ^= b;

    // if (OutLen == 16)
    //     v2 ^= 0xee;
    // else
    v2 ^= 0xff;

    TRACE;
    for (i = 0; i < dROUNDS; ++i)
        SIPROUND;

    b = v0 ^ v1 ^ v2 ^ v3;
    U64TO8_LE(out, b);

    // if (OutLen == 8)
    return out_value;

    // v1 ^= 0xdd;
    //
    // TRACE;
    // for (i = 0; i < dROUNDS; ++i)
    //     SIPROUND;
    //
    // b = v0 ^ v1 ^ v2 ^ v3;
    // U64TO8_LE(Out + 8, b);
}

const char *HashUtils::SipHashLlvmIR()
{
    return R"(
@.str = private unnamed_addr constant [38 x i8] c"mba:1 cff:1,40,70,0,70,0,0 sibr:1,100\00", section "llvm.metadata"
@.str.1 = private unnamed_addr constant [1 x i8] c"\00", section "llvm.metadata"
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @___siphash, ptr @.str, ptr @.str.1, i32 70, ptr null }], section "llvm.metadata"

define dso_local i64 @___siphash(i64 noundef %0, i64 noundef %1, i64 noundef %2, i64 noundef %3, i64 noundef %4, i64 noundef %5, i64 noundef %6) #0 {
  %8 = xor i64 %6, %2
  %9 = xor i64 %5, %1
  %10 = xor i64 %4, %2
  %11 = xor i64 %3, %1
  %12 = xor i64 %8, %0
  br label %13

13:
  %14 = phi i64 [ %11, %7 ], [ %26, %13 ]
  %15 = phi i64 [ %10, %7 ], [ %31, %13 ]
  %16 = phi i64 [ %9, %7 ], [ %32, %13 ]
  %17 = phi i1 [ true, %7 ], [ false, %13 ]
  %18 = phi i64 [ %12, %7 ], [ %28, %13 ]
  %19 = add i64 %14, %15
  %20 = tail call i64 @llvm.fshl.i64(i64 %15, i64 %15, i64 13)
  %21 = xor i64 %19, %20
  %22 = tail call i64 @llvm.fshl.i64(i64 %19, i64 %19, i64 32)
  %23 = add i64 %16, %18
  %24 = tail call i64 @llvm.fshl.i64(i64 %18, i64 %18, i64 16)
  %25 = xor i64 %23, %24
  %26 = add i64 %22, %25
  %27 = tail call i64 @llvm.fshl.i64(i64 %25, i64 %25, i64 21)
  %28 = xor i64 %26, %27
  %29 = add i64 %21, %23
  %30 = tail call i64 @llvm.fshl.i64(i64 %21, i64 %21, i64 17)
  %31 = xor i64 %30, %29
  %32 = tail call i64 @llvm.fshl.i64(i64 %29, i64 %29, i64 32)
  br i1 %17, label %13, label %33

33:
  %34 = xor i64 %26, %0
  %35 = xor i64 %28, 576460752303423488
  br label %36

36:
  %37 = phi i64 [ %34, %33 ], [ %49, %36 ]
  %38 = phi i64 [ %31, %33 ], [ %54, %36 ]
  %39 = phi i64 [ %32, %33 ], [ %55, %36 ]
  %40 = phi i1 [ true, %33 ], [ false, %36 ]
  %41 = phi i64 [ %35, %33 ], [ %51, %36 ]
  %42 = add i64 %37, %38
  %43 = tail call i64 @llvm.fshl.i64(i64 %38, i64 %38, i64 13)
  %44 = xor i64 %42, %43
  %45 = tail call i64 @llvm.fshl.i64(i64 %42, i64 %42, i64 32)
  %46 = add i64 %39, %41
  %47 = tail call i64 @llvm.fshl.i64(i64 %41, i64 %41, i64 16)
  %48 = xor i64 %46, %47
  %49 = add i64 %45, %48
  %50 = tail call i64 @llvm.fshl.i64(i64 %48, i64 %48, i64 21)
  %51 = xor i64 %49, %50
  %52 = add i64 %44, %46
  %53 = tail call i64 @llvm.fshl.i64(i64 %44, i64 %44, i64 17)
  %54 = xor i64 %53, %52
  %55 = tail call i64 @llvm.fshl.i64(i64 %52, i64 %52, i64 32)
  br i1 %40, label %36, label %56

56:
  %57 = xor i64 %49, 576460752303423488
  %58 = xor i64 %55, 255
  br label %59

59:
  %60 = phi i64 [ %57, %56 ], [ %72, %59 ]
  %61 = phi i64 [ %54, %56 ], [ %77, %59 ]
  %62 = phi i64 [ %58, %56 ], [ %78, %59 ]
  %63 = phi i32 [ 0, %56 ], [ %79, %59 ]
  %64 = phi i64 [ %51, %56 ], [ %74, %59 ]
  %65 = add i64 %60, %61
  %66 = tail call i64 @llvm.fshl.i64(i64 %61, i64 %61, i64 13)
  %67 = xor i64 %65, %66
  %68 = tail call i64 @llvm.fshl.i64(i64 %65, i64 %65, i64 32)
  %69 = add i64 %62, %64
  %70 = tail call i64 @llvm.fshl.i64(i64 %64, i64 %64, i64 16)
  %71 = xor i64 %69, %70
  %72 = add i64 %68, %71
  %73 = tail call i64 @llvm.fshl.i64(i64 %71, i64 %71, i64 21)
  %74 = xor i64 %72, %73
  %75 = add i64 %67, %69
  %76 = tail call i64 @llvm.fshl.i64(i64 %67, i64 %67, i64 17)
  %77 = xor i64 %76, %75
  %78 = tail call i64 @llvm.fshl.i64(i64 %75, i64 %75, i64 32)
  %79 = add nuw nsw i32 %63, 1
  %80 = icmp eq i32 %79, 4
  br i1 %80, label %81, label %59

81:
  %82 = xor i64 %78, %74
  %83 = xor i64 %82, %77
  %84 = xor i64 %83, %72
  ret i64 %84
}

declare i64 @llvm.fshl.i64(i64, i64, i64) #1

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
)";
}

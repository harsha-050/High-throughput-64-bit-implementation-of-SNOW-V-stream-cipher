#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <immintrin.h>
#include <chrono>
#include <iomanip>
#include <array>
using namespace std;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
u8 Sigma[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
u64 AesKey1_64[2] = {0, 0};
u64 AesKey2_64[2] = {0, 0};
#define MAKEU64(a, b, c, d) ((((u64)(a) << 48) | ((u64)(b) << 32)) | (((u64)(c) << 16) | ((u64)(d))))
#define MAKEU32(a, b, c, d) ((((u32)(a) << 24) | ((u32)(b) << 16)) | (((u32)(c) << 8) | ((u32)(d))))
#define MAKEU16(a, b) (((u16)(a) << 8) | ((u16)(b)))
struct SnowV64
{
    u32 A_32[8], B_32[8];             // LFSR
    u64 R1_64[2], R2_64[2], R3_64[2]; // FSM
    void aes_enc_round(u64 *result, u64 *state, u64 *roundKey)
    {
        __m128i state_vec = _mm_loadu_si128((__m128i *)state);
        __m128i roundKey_vec = _mm_loadu_si128((__m128i *)roundKey);
        __m128i result_vec = _mm_aesenc_si128(state_vec, roundKey_vec);
        _mm_storeu_si128((__m128i *)result, result_vec);
    }
    static inline u32 mul_x(u32 v, u32 c)
    {
        u32 v1 = v << 1;
        v1 &= ~((u32)1 << 16); // Explicitly clear bit 16

        // If the MSB of either half is set, XOR with the corresponding part of c
        u32 mask = ((v & 0x80000000) ? (c & 0xFFFF0000) : 0) | // Upper 16-bit mask
                   ((v & 0x8000) ? (c & 0xFFFF) : 0);          // Lower 16-bit mask

        return v1 ^ mask;
    }

    static inline u32 mul_x_inv(u32 v, u32 d)
    {
        u32 v1 = v >> 1;
        v1 &= ~((u32)1 << 15);

        u32 mask = ((v & 0x0001) ? (d & 0xFFFF) : 0) |        // Lower 16-bit mask
                   ((v & 0x00010000) ? (d & 0xFFFF0000) : 0); // Upper 16-bit mask

        return v1 ^ mask;
    }
    void permute_sigma(u64 *state)
    {
        u8 tmp[16];
        for (int i = 0; i < 16; i++)
        {
            tmp[i] = (u8)(state[Sigma[i] >> 3] >> ((Sigma[i] & 7) << 3));
        }
        for (int i = 0; i < 2; i++)
        {
            state[i] = MAKEU64(MAKEU16(tmp[8 * i + 7], tmp[8 * i + 6]), MAKEU16(tmp[8 * i + 5], tmp[8 * i + 4]), MAKEU16(tmp[8 * i + 3], tmp[8 * i + 2]), MAKEU16(tmp[8 * i + 1], tmp[8 * i]));
        }
    }
    void fsm_update(void)
    {
        u64 R1_64temp[2];
        R1_64temp[0] = R1_64[0];
        R1_64temp[1] = R1_64[1];

        for (int i = 0; i < 2; i++)
        {
            u64 T21 = ((u64)A_32[2 * i + 1]) << 32;
            u64 T22 = (u64)A_32[2 * i];

            u64 lowerR3 = R3_64[i] & 0xFFFFFFFF;
            u64 lowerR2 = R2_64[i] & 0xFFFFFFFF;
            u64 upperR3 = R3_64[i] & 0xFFFFFFFF00000000;
            u64 upperR2 = R2_64[i] & 0xFFFFFFFF00000000;

            u64 v22 = ((T22 ^ lowerR3) + lowerR2) & 0xFFFFFFFF;
            R1_64[i] = v22 + (T21 ^ upperR3) + upperR2;
        }
        permute_sigma(R1_64);
        aes_enc_round(R3_64, R2_64, AesKey2_64);
        aes_enc_round(R2_64, R1_64temp, AesKey1_64);
    }
    void lfsr_update(void)
    {
        for (int i = 0; i < 4; i++)
        {
            u32 s1 = (A_32[1] << 16) | (A_32[0] >> 16);
            u32 s2 = (B_32[2] << 16) | (B_32[1] >> 16);
            u32 u = mul_x(A_32[0], 0x990f990f) ^ s1 ^ mul_x_inv(A_32[4], 0xcc87cc87) ^ B_32[0];
            u32 v = mul_x(B_32[0], 0xc963c963) ^ s2 ^ mul_x_inv(B_32[4], 0xe4b1e4b1) ^ A_32[0];
            memmove(A_32,A_32 + 1, 7 * sizeof(u32));
            memmove(B_32,B_32 + 1, 7 * sizeof(u32));
            A_32[7] = u;
            B_32[7] = v;
        }
    }
    void keystream(u8 *z)
    {
        for (int i = 0; i < 2; i++)
        {
            // Construct a 64-bit T value directly from B_32
            u64 T = ((u64)B_32[2 * i + 5] << 32) | (u64)B_32[2 * i + 4];

            // Perform 64-bit addition while preventing carry propagation
            u64 lower32 = (T & 0xFFFFFFFF) + (R1_64[i] & 0xFFFFFFFF);
            lower32 &= 0xFFFFFFFF; // Ensure no carry into upper 32 bits

            u64 upper32 = (T >> 32) + (R1_64[i] >> 32);
            upper32 &= 0xFFFFFFFF; // Ensure no overflow beyond 32-bit boundary

            u64 v = ((upper32 << 32) | lower32) ^ R2_64[i];

            memcpy(z + i * 8, &v, 8);// Store the result as bytes
        }

        fsm_update();
        lfsr_update();
    }

    void keyiv_setup(u8 *key, u8 *iv, int is_aead_mode)
    {
        for (int i = 0; i < 4; i++)
        {
            A_32[i] = MAKEU32(iv[4 * i + 3], iv[4 * i + 2], iv[4 * i + 1], iv[4 * i]);
            A_32[i + 4] = MAKEU32(key[4 * i + 3], key[4 * i + 2], key[4 * i + 1], key[4 * i]);
            B_32[i] = 0x00000000;
            B_32[i + 4] = MAKEU32(key[4 * i + 19], key[4 * i + 18], key[4 * i + 17], key[4 * i + 16]);
        }
        if (is_aead_mode == 1)
        {
            B_32[0] = 0x78656C41;
            B_32[1] = 0x20646B45;
            B_32[2] = 0x676E694A;
            B_32[3] = 0x6D6F6854;
        }
        for (int i = 0; i < 2; i++)
        {
            R1_64[i] = R2_64[i] = R3_64[i] = 0x00000000;
        }
        for (int i = 0; i < 16; i++)
        {
            u8 z[16];
            keystream(z);
            for (int j = 0; j < 4; j++)
                A_32[j + 4] ^= MAKEU32(z[4 * j + 3], z[4 * j + 2], z[4 * j + 1], z[4 * j]);
            if (i == 14)
            {
                for (int j = 0; j < 2; j++)
                {
                    R1_64[j] ^= MAKEU64(MAKEU16(key[8 * j + 7], key[8 * j + 6]),
                                        MAKEU16(key[8 * j + 5], key[8 * j + 4]), MAKEU16(key[8 * j + 3], key[8 * j + 2]),
                                        MAKEU16(key[8 * j + 1], key[8 * j + 0]));
                }
            }

            if (i == 15)
            {
                for (int j = 0; j < 2; j++)
                {
                    R1_64[j] ^= MAKEU64(MAKEU16(key[8 * j + 23], key[8 * j + 22]),
                                        MAKEU16(key[8 * j + 21], key[8 * j + 20]), MAKEU16(key[8 * j + 19], key[8 * j + 18]),
                                        MAKEU16(key[8 * j + 17], key[8 * j + 16]));
                }
            }
        }
    }
};

int main()
{
    SnowV64 cipher;
    uint8_t key[32] = {0};
    uint8_t iv[16] = {0};
    long long num_blocks = 1 << 30; // Number of keystream blocks to generate
    uint8_t keystream_block[16];
    // Initialize cipher
    cipher.keyiv_setup(key, iv, 0);

    // Start measuring time for throughput calculation
    auto start_time = chrono::high_resolution_clock::now();

    // Generate keystream blocks
    for (int i = 0; i < num_blocks; i++)
    {
        cipher.keystream(keystream_block);
    }

    // End measuring time for throughput calculation
    auto end_time = chrono::high_resolution_clock::now();

    // Calculate the elapsed time in seconds
    chrono::duration<double> duration = end_time - start_time;
    double elapsed_seconds = duration.count();

    // Calculate the throughput in Mbps (Megabits per second)
    double total_data_bits = num_blocks * 16 * 8;                       // 16 bytes per block, 8 bits per byte
    double throughput_gbps = total_data_bits / (elapsed_seconds * 1e6); // Convert bits to Mbps

    // Output the throughput result
    cout << "Throughput: " << throughput_gbps << " Mbps" << endl;
    cout << fixed << setprecision(8);
    cout << "Elapsed time: " << elapsed_seconds << " seconds" << endl;
    return 0;
}
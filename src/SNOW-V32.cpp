#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <wmmintrin.h>
#include <chrono>
#include <iomanip>
#include <array>
using namespace std;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
u8 Sigma[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
u32 AesKey1[4] = {0, 0, 0, 0};
u32 AesKey2[4] = {0, 0, 0, 0};
#define MAKEU64(a, b) (((u64)(a) << 32) | ((u64)(b)))
#define MAKEU32(a, b) (((u32)(a) << 16) | ((u32)(b)))
#define MAKEU16(a, b) (((u16)(a) << 8) | ((u16)(b)))
struct SnowV32
{
    u16 A[16], B[16];        // LFSR
    u32 R1[4], R2[4], R3[4]; // FSM
    void aes_enc_round(u32 *result, u32 *state, u32 *roundKey)
    {
        __m128i state_vec = _mm_loadu_si128((__m128i *)state);
        __m128i roundKey_vec = _mm_loadu_si128((__m128i *)roundKey);
        __m128i result_vec = _mm_aesenc_si128(state_vec, roundKey_vec);
        _mm_storeu_si128((__m128i *)result, result_vec);
    }
    u16 mul_x(u16 v, u16 c)
    {
        if (v & 0x8000)
            return (v << 1) ^ c;
        else
            return (v << 1);
    }
    u16 mul_x_inv(u16 v, u16 d)
    {
        if (v & 0x0001)
            return (v >> 1) ^ d;
        else
            return (v >> 1);
    }
    void permute_sigma(u32 *state)
    {
        u8 tmp[16];
        for (int i = 0; i < 16; i++)
            tmp[i] = (u8)(state[Sigma[i] >> 2] >> ((Sigma[i] & 3) << 3));
        for (int i = 0; i < 4; i++)
            state[i] = MAKEU32(MAKEU16(tmp[4 * i + 3], tmp[4 * i + 2]),
                               MAKEU16(tmp[4 * i + 1], tmp[4 * i]));
    }
    void fsm_update(void)
    {
        u32 R1temp[4];
        memcpy(R1temp, R1, sizeof(R1));
        for (int i = 0; i < 4; i++)
        {
            u32 T2 = MAKEU32(A[2 * i + 1], A[2 * i]);
            R1[i] = (T2 ^ R3[i]) + R2[i];
        }
        permute_sigma(R1);
        aes_enc_round(R3, R2, AesKey2);
        aes_enc_round(R2, R1temp, AesKey1);
    }
    void lfsr_update(void)
    {
        for (int i = 0; i < 8; i++)
        {
            u16 u = mul_x(A[0], 0x990f) ^ A[1] ^ mul_x_inv(A[8], 0xcc87) ^ B[0];
            u16 v = mul_x(B[0], 0xc963) ^ B[3] ^ mul_x_inv(B[8], 0xe4b1) ^ A[0];
            for (int j = 0; j < 15; j++)
            {
                A[j] = A[j + 1];
                B[j] = B[j + 1];
            }
            A[15] = u;
            B[15] = v;
        }
    }
    void keystream(u8 *z)
    {
        for (int i = 0; i < 4; i++)
        {
            u32 T1 = MAKEU32(B[2 * i + 9], B[2 * i + 8]);
            u32 v = (T1 + R1[i]) ^ R2[i];
            z[i * 4 + 0] = (v >> 0) & 0xff;
            z[i * 4 + 1] = (v >> 8) & 0xff;
            z[i * 4 + 2] = (v >> 16) & 0xff;
            z[i * 4 + 3] = (v >> 24) & 0xff;
        }
        fsm_update();
        lfsr_update();
    }
    void keyiv_setup(u8 *key, u8 *iv, int is_aead_mode)
    {
        for (int i = 0; i < 8; i++)
        {
            A[i] = MAKEU16(iv[2 * i + 1], iv[2 * i]);
            A[i + 8] = MAKEU16(key[2 * i + 1], key[2 * i]);
            B[i] = 0x0000;
            B[i + 8] = MAKEU16(key[2 * i + 17], key[2 * i + 16]);
        }
        if (is_aead_mode == 1)
        {
            B[0] = 0x6C41;
            B[1] = 0x7865;
            B[2] = 0x6B45;
            B[3] = 0x2064;
            B[4] = 0x694A;
            B[5] = 0x676E;
            B[6] = 0x6854;
            B[7] = 0x6D6F;
        }
        for (int i = 0; i < 4; i++)
            R1[i] = R2[i] = R3[i] = 0x00000000;
        for (int i = 0; i < 16; i++)
        {
            u8 z[16];
            keystream(z);
            for (int j = 0; j < 8; j++)
                A[j + 8] ^= MAKEU16(z[2 * j + 1], z[2 * j]);
            if (i == 14)
                for (int j = 0; j < 4; j++)
                    R1[j] ^= MAKEU32(MAKEU16(key[4 * j + 3], key[4 * j + 2]),
                                     MAKEU16(key[4 * j + 1], key[4 * j + 0]));
            if (i == 15)
                for (int j = 0; j < 4; j++)
                    R1[j] ^= MAKEU32(MAKEU16(key[4 * j + 19], key[4 * j + 18]),
                                     MAKEU16(key[4 * j + 17], key[4 * j + 16]));
        }
    }
};


int main()
{
    SnowV32 cipher;
    uint8_t key[32] = {0};
    uint8_t iv[16] = {0};
    long long num_blocks = 1 << 15; // Number of keystream blocks to generate
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

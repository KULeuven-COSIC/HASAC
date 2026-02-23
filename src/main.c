/* HASAC */
/* Sayon Duttagupta */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <psa/crypto.h>
#include "lz4.h"

/* --- CONFIGURATION --- */
#define PAYLOAD_SIZE       1024
#define BENCH_ITERS        100
#define ACTIVE_POWER_MW    30.0
#define CPU_CLOCK_HZ       64000000.0

#define CCM_TAG_LEN        16
#define CCM_ALG_TAGGED     PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, CCM_TAG_LEN)

/* --- DATA BUFFERS --- */
static uint8_t key[32];
static uint8_t nonce[12];
static uint8_t input_buf[PAYLOAD_SIZE];
static uint8_t output_buf[PAYLOAD_SIZE + 64];
static uint8_t tag[16];

/* Dummy compressed data */
static const uint8_t lz4_dummy_data[] = {
    0x1F, 0x00, 0x01, 0x00, 0xF0, 0x00, 0x50, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/* --- SIMPLE DETERMINISTIC FILL --- */
static uint32_t xorshift32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static void fill_bytes(uint8_t *dst, size_t len, uint32_t seed)
{
    uint32_t s = seed;
    for (size_t i = 0; i < len; i++) {
        uint32_t r = xorshift32(&s);
        dst[i] = (uint8_t)r;
    }
}

/* --- PRINT HELPER --- */
static void print_bench_row(const char *name,
                            double time_ms,
                            double energy_uJ,
                            uint32_t cpu_cycles,
                            double speed_kB)
{
    int t_int = (int)time_ms;
    int t_dec = (int)((time_ms - t_int) * 1000.0);

    int e_int = (int)energy_uJ;
    int e_dec = (int)((energy_uJ - e_int) * 100.0);

    int s_int = (int)speed_kB;
    int s_dec = (int)((speed_kB - s_int) * 100.0);

    printk("| %-18s | %7d.%03d | %9d.%02d | %9u | %9d.%02d |\n",
           name, t_int, t_dec, e_int, e_dec, cpu_cycles, s_int, s_dec);
}

/* --- ENDIAN HELPERS --- */
static uint32_t load32_le(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void store32_le(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void store64_le(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }

/*  Software AES */
static const uint8_t sbox[256] = {
  0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
  0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
  0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
  0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
  0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
  0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
  0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
  0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
  0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
  0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
  0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
  0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
  0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
  0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
  0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
  0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static uint8_t xtime(uint8_t x) { return (x << 1) ^ ((x & 0x80) ? 0x1b : 0x00); }
static void aes_subbytes(uint8_t s[16]) { for (int i = 0; i < 16; i++) s[i] = sbox[s[i]]; }

static void aes_shiftrows(uint8_t s[16])
{
    uint8_t t;
    t=s[1];  s[1]=s[5];  s[5]=s[9];  s[9]=s[13]; s[13]=t;
    t=s[2];  s[2]=s[10]; s[10]=t;    t=s[6];     s[6]=s[14]; s[14]=t;
    t=s[15]; s[15]=s[11]; s[11]=s[7]; s[7]=s[3]; s[3]=t;
}

static void aes_mixcolumns(uint8_t s[16])
{
    for (int c = 0; c < 4; c++) {
        uint8_t *p = &s[c * 4];
        uint8_t t = p[0] ^ p[1] ^ p[2] ^ p[3];
        uint8_t u = p[0];
        p[0] ^= t ^ xtime(p[0] ^ p[1]);
        p[1] ^= t ^ xtime(p[1] ^ p[2]);
        p[2] ^= t ^ xtime(p[2] ^ p[3]);
        p[3] ^= t ^ xtime(p[3] ^ u);
    }
}

static void aes_addroundkey(uint8_t s[16], const uint8_t rk[16]) { for (int i = 0; i < 16; i++) s[i] ^= rk[i]; }
static const uint8_t rcon10[10] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};

static void aes128_key_expand(const uint8_t key16[16], uint8_t rk[176])
{
    memcpy(rk, key16, 16);
    uint8_t t[4];
    int bytes = 16;
    int rcon_i = 0;

    while (bytes < 176) {
        memcpy(t, &rk[bytes - 4], 4);
        if ((bytes % 16) == 0) {
            uint8_t u = t[0];
            t[0] = sbox[t[1]] ^ rcon10[rcon_i++];
            t[1] = sbox[t[2]];
            t[2] = sbox[t[3]];
            t[3] = sbox[u];
        }
        for (int i = 0; i < 4; i++) rk[bytes + i] = rk[bytes - 16 + i] ^ t[i];
        bytes += 4;
    }
}

static void aes128_encrypt_block(const uint8_t rk[176], const uint8_t in[16], uint8_t out[16])
{
    uint8_t s[16];
    memcpy(s, in, 16);

    aes_addroundkey(s, &rk[0]);
    for (int r = 1; r <= 9; r++) {
        aes_subbytes(s);
        aes_shiftrows(s);
        aes_mixcolumns(s);
        aes_addroundkey(s, &rk[r * 16]);
    }
    aes_subbytes(s);
    aes_shiftrows(s);
    aes_addroundkey(s, &rk[10 * 16]);

    memcpy(out, s, 16);
}

static void ccm_fmt_b0(uint8_t b0[16], const uint8_t n[12], size_t len)
{
    b0[0] = 0x3A;
    memcpy(&b0[1], n, 12);
    b0[13] = (len >> 16) & 0xFF;
    b0[14] = (len >> 8) & 0xFF;
    b0[15] = len & 0xFF;
}

static void ccm_fmt_ctr(uint8_t ctr[16], const uint8_t n[12], uint32_t c)
{
    ctr[0] = 2;
    memcpy(&ctr[1], n, 12);
    ctr[13] = (c >> 16) & 0xFF;
    ctr[14] = (c >> 8) & 0xFF;
    ctr[15] = c & 0xFF;
}

static void aes_ccm_sw(const uint8_t key16[16],
                       const uint8_t nonce12[12],
                       const uint8_t *in,
                       uint8_t *out,
                       size_t len,
                       uint8_t tag16[16])
{
    uint8_t rk[176];
    aes128_key_expand(key16, rk);

    uint8_t x[16] = {0};
    uint8_t b[16];
    uint8_t tmp[16];

    ccm_fmt_b0(b, nonce12, len);
    for (int i = 0; i < 16; i++) x[i] ^= b[i];
    aes128_encrypt_block(rk, x, x);

    size_t off = 0;
    while (off < len) {
        size_t chk = (len - off > 16) ? 16 : (len - off);
        memset(tmp, 0, 16);
        memcpy(tmp, in + off, chk);
        for (int i = 0; i < 16; i++) x[i] ^= tmp[i];
        aes128_encrypt_block(rk, x, x);
        off += chk;
    }

    uint8_t s0[16];
    uint8_t ctr[16];
    ccm_fmt_ctr(ctr, nonce12, 0);
    aes128_encrypt_block(rk, ctr, s0);
    for (int i = 0; i < 16; i++) tag16[i] = x[i] ^ s0[i];

    off = 0;
    uint32_t c = 1;
    while (off < len) {
        uint8_t si[16];
        ccm_fmt_ctr(ctr, nonce12, c++);
        aes128_encrypt_block(rk, ctr, si);
        size_t chk = (len - off > 16) ? 16 : (len - off);
        for (size_t i = 0; i < chk; i++) out[off + i] = in[off + i] ^ si[i];
        off += chk;
    }
}

/* Software ChaCha20-Poly1305 */
#define ROTL32(v,n) (((v)<<(n)) | ((v)>>(32-(n))))

static void qr(uint32_t *x, int a, int b, int c, int d)
{
    x[a]+=x[b]; x[d]^=x[a]; x[d]=ROTL32(x[d],16);
    x[c]+=x[d]; x[b]^=x[c]; x[b]=ROTL32(x[b],12);
    x[a]+=x[b]; x[d]^=x[a]; x[d]=ROTL32(x[d], 8);
    x[c]+=x[d]; x[b]^=x[c]; x[b]=ROTL32(x[b], 7);
}

static void chacha_blk(uint8_t out[64], const uint8_t k[32], const uint8_t n[12], uint32_t c)
{
    uint32_t s[16] = {0x61707865,0x3320646e,0x79622d32,0x6b206574};
    for (int i = 0; i < 8; i++) s[4 + i] = load32_le(k + i * 4);
    s[12] = c;
    for (int i = 0; i < 3; i++) s[13 + i] = load32_le(n + i * 4);

    uint32_t w[16];
    memcpy(w, s, 64);

    for (int i = 0; i < 10; i++) {
        qr(w,0,4,8,12); qr(w,1,5,9,13); qr(w,2,6,10,14); qr(w,3,7,11,15);
        qr(w,0,5,10,15); qr(w,1,6,11,12); qr(w,2,7,8,13); qr(w,3,4,9,14);
    }

    for (int i = 0; i < 16; i++) store32_le(out + i * 4, w[i] + s[i]);
}

typedef struct { uint32_t r[5], h[5], pad[4]; } poly_ctx;

static void poly_init(poly_ctx *ctx, const uint8_t k[32])
{
    uint32_t t[4];
    for (int i = 0; i < 4; i++) t[i] = load32_le(k + i * 4);

    ctx->r[0] = t[0] & 0x3ffffff;
    ctx->r[1] = (t[0] >> 26 | t[1] << 6) & 0x3ffff03;
    ctx->r[2] = (t[1] >> 20 | t[2] << 12) & 0x3ffc0ff;
    ctx->r[3] = (t[2] >> 14 | t[3] << 18) & 0x3f03fff;
    ctx->r[4] = (t[3] >> 8) & 0xfffff;

    memset(ctx->h, 0, sizeof(ctx->h));
    for (int i = 0; i < 4; i++) ctx->pad[i] = load32_le(k + 16 + i * 4);
}

static void poly_blocks(poly_ctx *ctx, const uint8_t *m, size_t bytes)
{
    uint32_t h0=ctx->h[0], h1=ctx->h[1], h2=ctx->h[2], h3=ctx->h[3], h4=ctx->h[4];
    uint32_t r0=ctx->r[0], r1=ctx->r[1], r2=ctx->r[2], r3=ctx->r[3], r4=ctx->r[4];
    uint32_t s1=r1*5, s2=r2*5, s3=r3*5, s4=r4*5;

    while (bytes >= 16) {
        uint32_t t[4];
        for (int i = 0; i < 4; i++) t[i] = load32_le(m + i * 4);

        h0 += t[0] & 0x3ffffff;
        h1 += (t[0] >> 26 | t[1] << 6) & 0x3ffffff;
        h2 += (t[1] >> 20 | t[2] << 12) & 0x3ffffff;
        h3 += (t[2] >> 14 | t[3] << 18) & 0x3ffffff;
        h4 += (t[3] >> 8) | (1 << 24);

        uint64_t d0=(uint64_t)h0*r0 + (uint64_t)h1*s4 + (uint64_t)h2*s3 + (uint64_t)h3*s2 + (uint64_t)h4*s1;
        uint64_t d1=(uint64_t)h0*r1 + (uint64_t)h1*r0 + (uint64_t)h2*s4 + (uint64_t)h3*s3 + (uint64_t)h4*s2;
        uint64_t d2=(uint64_t)h0*r2 + (uint64_t)h1*r1 + (uint64_t)h2*r0 + (uint64_t)h3*s4 + (uint64_t)h4*s3;
        uint64_t d3=(uint64_t)h0*r3 + (uint64_t)h1*r2 + (uint64_t)h2*r1 + (uint64_t)h3*r0 + (uint64_t)h4*s4;
        uint64_t d4=(uint64_t)h0*r4 + (uint64_t)h1*r3 + (uint64_t)h2*r2 + (uint64_t)h3*r1 + (uint64_t)h4*r0;

        uint32_t c;
        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff; d1 += c;
        c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff; d2 += c;
        c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff; d3 += c;
        c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff; d4 += c;
        c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff; h0 += c * 5;
        c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;

        m += 16;
        bytes -= 16;
    }

    ctx->h[0]=h0; ctx->h[1]=h1; ctx->h[2]=h2; ctx->h[3]=h3; ctx->h[4]=h4;
}

static void poly_finish(poly_ctx *ctx, uint8_t mac[16])
{
    uint32_t h0=ctx->h[0], h1=ctx->h[1], h2=ctx->h[2], h3=ctx->h[3], h4=ctx->h[4];
    uint32_t c;

    c=h1>>26; h1&=0x3ffffff; h2+=c;
    c=h2>>26; h2&=0x3ffffff; h3+=c;
    c=h3>>26; h3&=0x3ffffff; h4+=c;
    c=h4>>26; h4&=0x3ffffff; h0+=c*5;
    c=h0>>26; h0&=0x3ffffff; h1+=c;

    uint32_t g0=h0+5; c=g0>>26; g0&=0x3ffffff;
    uint32_t g1=h1+c; c=g1>>26; g1&=0x3ffffff;
    uint32_t g2=h2+c; c=g2>>26; g2&=0x3ffffff;
    uint32_t g3=h3+c; c=g3>>26; g3&=0x3ffffff;
    uint32_t g4=h4+c-(1<<26);

    uint32_t mask = (uint32_t)-(int32_t)(g4 >> 31);
    h0 = (h0 & ~mask) | (g0 & mask);
    h1 = (h1 & ~mask) | (g1 & mask);
    h2 = (h2 & ~mask) | (g2 & mask);
    h3 = (h3 & ~mask) | (g3 & mask);
    h4 = (h4 & ~mask) | (g4 & mask);

    uint64_t f0=(uint64_t)h0 | ((uint64_t)h1<<26);
    uint64_t f1=(uint64_t)(h1>>6) | ((uint64_t)h2<<20);
    uint64_t f2=(uint64_t)(h2>>12) | ((uint64_t)h3<<14);
    uint64_t f3=(uint64_t)(h3>>18) | ((uint64_t)h4<<8);

    f0 += ctx->pad[0];
    f1 += ctx->pad[1] + (f0 >> 32);
    f2 += ctx->pad[2] + (f1 >> 32);
    f3 += ctx->pad[3] + (f2 >> 32);

    store32_le(mac + 0, (uint32_t)f0);
    store32_le(mac + 4, (uint32_t)f1);
    store32_le(mac + 8, (uint32_t)f2);
    store32_le(mac + 12,(uint32_t)f3);
}

static void chacha_poly_sw(const uint8_t key32[32],
                           const uint8_t nonce12[12],
                           const uint8_t *in,
                           uint8_t *out,
                           size_t len,
                           uint8_t tag16[16])
{
    uint8_t otk[64];
    chacha_blk(otk, key32, nonce12, 0);

    uint8_t subkey[32];
    memcpy(subkey, otk, 32);

    size_t off = 0;
    uint32_t cnt = 1;
    while (off < len) {
        uint8_t block[64];
        chacha_blk(block, key32, nonce12, cnt++);
        size_t chk = (len - off > 64) ? 64 : (len - off);
        for (size_t i = 0; i < chk; i++) out[off + i] = in[off + i] ^ block[i];
        off += chk;
    }

    poly_ctx ctx;
    poly_init(&ctx, subkey);

    const uint8_t *c = out;
    size_t l = len;

    while (l >= 16) {
        poly_blocks(&ctx, c, 16);
        c += 16;
        l -= 16;
    }

    if (l) {
        uint8_t t[16] = {0};
        memcpy(t, c, l);
        t[l] = 1;
        poly_blocks(&ctx, t, 16);
    }

    uint8_t lens[16] = {0};
    store64_le(lens + 8, (uint64_t)len);
    poly_blocks(&ctx, lens, 16);

    poly_finish(&ctx, tag16);
}

/* Hardware AES-CCM */
static psa_key_id_t hw_aes_key = 0;
static bool hw_ready = false;

static psa_status_t hw_init_status = PSA_ERROR_BAD_STATE;
static psa_status_t hw_import_status = PSA_ERROR_BAD_STATE;
static psa_status_t hw_last_status = PSA_ERROR_BAD_STATE;
static size_t hw_last_out_len = 0;

static void hw_init_key_once(void)
{
    hw_init_status = psa_crypto_init();
    if (hw_init_status != PSA_SUCCESS) {
        hw_ready = false;
        return;
    }

    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, 128);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, CCM_ALG_TAGGED);

    hw_import_status = psa_import_key(&attr, key, 16, &hw_aes_key);
    if (hw_import_status != PSA_SUCCESS) {
        hw_ready = false;
        return;
    }

    hw_ready = true;
}

static void hw_ccm_encrypt_once(bool verbose)
{
    if (!hw_ready) {
        hw_last_status = PSA_ERROR_BAD_STATE;
        hw_last_out_len = 0;
        if (verbose) {
            printk("HW: not ready (init=%d import=%d)\n", (int)hw_init_status, (int)hw_import_status);
        }
        return;
    }

    hw_last_out_len = 0;
    hw_last_status = psa_aead_encrypt(
        hw_aes_key,
        CCM_ALG_TAGGED,
        nonce, sizeof(nonce),
        NULL, 0,
        input_buf, PAYLOAD_SIZE,
        output_buf, sizeof(output_buf),
        &hw_last_out_len
    );
}

/* Benchmarkings */
static void run_bench(const char *name, void (*func)(void))
{
    func(); 

    uint32_t start = k_cycle_get_32();
    for (int i = 0; i < BENCH_ITERS; i++) {
        func();
    }
    uint32_t end = k_cycle_get_32();

    uint32_t cycles = end - start;
    uint32_t hz = sys_clock_hw_cycles_per_sec();

    double total_s = (double)cycles / (double)hz;
    double avg_s   = total_s / (double)BENCH_ITERS;
    double avg_ms  = avg_s * 1000.0;

    double energy_uJ = ACTIVE_POWER_MW * avg_s * 1000.0;
    uint32_t cpu_cycles = (uint32_t)(avg_s * CPU_CLOCK_HZ);
    double throughput = (PAYLOAD_SIZE / 1024.0) / avg_s;

    if (strstr(name, "HW AES-128-CCM") != NULL) {
        if (!hw_ready || hw_last_status != PSA_SUCCESS) {
            printk("| %-18s |   *FAIL* |   *FAIL* |   *FAIL* |   *FAIL* |\n", name);
            printk("HW failure detail: ready=%d init=%d import=%d last=%d out_len=%u\n",
                   (int)hw_ready,
                   (int)hw_init_status,
                   (int)hw_import_status,
                   (int)hw_last_status,
                   (unsigned)hw_last_out_len);
            return;
        }
    }

    print_bench_row(name, avg_ms, energy_uJ, cpu_cycles, throughput);
}

/* test wrappers */
static void test_sw_aes(void)    { aes_ccm_sw(key, nonce, input_buf, output_buf, PAYLOAD_SIZE, tag); }
static void test_sw_chacha(void) { chacha_poly_sw(key, nonce, input_buf, output_buf, PAYLOAD_SIZE, tag); }
static void test_sw_lz4(void)    { LZ4_decompress_safe((const char*)lz4_dummy_data, (char*)output_buf, sizeof(lz4_dummy_data), PAYLOAD_SIZE); }
static void test_hw_ccm_quiet(void) { hw_ccm_encrypt_once(false); }


void main(void)
{
    k_sleep(K_SECONDS(2));

    fill_bytes(key, sizeof(key),   0xC0FFEE01u);
    fill_bytes(nonce, sizeof(nonce), 0xC0FFEE02u);
    fill_bytes(input_buf, sizeof(input_buf), 0xC0FFEE03u);

    hw_init_key_once();

    printk("\n\n");
    printk("==============================================================================\n");
    printk("      HASAC BENCHMARK SUITE (nRF5340 / Cortex M33)\n");
    printk("      Payload: %d bytes | Iters: %d | Power: %d mW | Clock: %u Hz\n",
           PAYLOAD_SIZE, BENCH_ITERS, (int)ACTIVE_POWER_MW, sys_clock_hw_cycles_per_sec());
    printk("==============================================================================\n");
    printk("| Algorithm          |  Time (ms)  |  Energy(uJ)  | CPU Cycles|  Speed(kB/s) |\n");
    printk("|--------------------|-------------|--------------|-----------|--------------|\n");

    run_bench("SW AES-128-CCM", test_sw_aes);
    run_bench("SW ChaCha Poly", test_sw_chacha);

    /* Make sure we have a fresh hw_last_status for the HW row */
    hw_ccm_encrypt_once(false);
    run_bench("HW AES-128-CCM", test_hw_ccm_quiet);

    /* Print one clean line with HW status after timing is done */
    hw_ccm_encrypt_once(true);

    run_bench("SW LZ4 Decompress", test_sw_lz4);

    if (hw_ready) {
        psa_destroy_key(hw_aes_key);
    }

    printk("==============================================================================\n");
}
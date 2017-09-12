//
//  latm2adts.c
//  LATM2ADTS
//
//  Created by mini on 2017/5/19.
//  Copyright © 2017年 mini. All rights reserved.
//

#include "latm2adts.h"
#include <stddef.h>
#include <assert.h>
#include <limits.h>

#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define L2A_MIN(a,b) ((a) > (b) ? (b) : (a))
#define L2A_LOAS_SYNC_WORD   0x2b7       ///< 11 bits LOAS sync word
#define L2A_FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define L2A_MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))
#define L2A_MAX_ELEM_ID 16
#define L2A_FF_COMPLIANCE_STRICT        1 ///< Strictly conform to all the things in the spec no matter
#define l2a_overread_err "Input buffer exhausted before END element found\n"
static const int8_t l2a_tags_per_config[16] = { 0, 1, 1, 2, 3, 3, 4, 5, 0, 0, 0, 4, 5, 0, 5, 0 };




#define AV_CH_FRONT_LEFT             0x00000001
#define AV_CH_FRONT_RIGHT            0x00000002
#define AV_CH_FRONT_CENTER           0x00000004
#define AV_CH_LOW_FREQUENCY          0x00000008
#define AV_CH_BACK_LEFT              0x00000010
#define AV_CH_BACK_RIGHT             0x00000020
#define AV_CH_FRONT_LEFT_OF_CENTER   0x00000040
#define AV_CH_FRONT_RIGHT_OF_CENTER  0x00000080
#define AV_CH_BACK_CENTER            0x00000100
#define AV_CH_SIDE_LEFT              0x00000200
#define AV_CH_SIDE_RIGHT             0x00000400
#define AV_CH_TOP_CENTER             0x00000800
#define AV_CH_TOP_FRONT_LEFT         0x00001000
#define AV_CH_TOP_FRONT_CENTER       0x00002000
#define AV_CH_TOP_FRONT_RIGHT        0x00004000
#define AV_CH_TOP_BACK_LEFT          0x00008000
#define AV_CH_TOP_BACK_CENTER        0x00010000
#define AV_CH_TOP_BACK_RIGHT         0x00020000
#define AV_CH_STEREO_LEFT            0x20000000  ///< Stereo downmix.
#define AV_CH_STEREO_RIGHT           0x40000000  ///< See AV_CH_STEREO_LEFT.
#define AV_CH_WIDE_LEFT              0x0000000080000000ULL
#define AV_CH_WIDE_RIGHT             0x0000000100000000ULL
#define AV_CH_SURROUND_DIRECT_LEFT   0x0000000200000000ULL
#define AV_CH_SURROUND_DIRECT_RIGHT  0x0000000400000000ULL
#define AV_CH_LOW_FREQUENCY_2        0x0000000800000000ULL

/** Channel mask value used for AVCodecContext.request_channel_layout
 to indicate that the user requests the channel order of the decoder output
 to be the native codec channel order. */
#define AV_CH_LAYOUT_NATIVE          0x8000000000000000ULL

/**
 * @}
 * @defgroup channel_mask_c Audio channel layouts
 * @{
 * */
#define AV_CH_LAYOUT_MONO              (AV_CH_FRONT_CENTER)
#define AV_CH_LAYOUT_STEREO            (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT)
#define AV_CH_LAYOUT_2POINT1           (AV_CH_LAYOUT_STEREO|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_2_1               (AV_CH_LAYOUT_STEREO|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_SURROUND          (AV_CH_LAYOUT_STEREO|AV_CH_FRONT_CENTER)
#define AV_CH_LAYOUT_3POINT1           (AV_CH_LAYOUT_SURROUND|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_4POINT0           (AV_CH_LAYOUT_SURROUND|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_4POINT1           (AV_CH_LAYOUT_4POINT0|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_2_2               (AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
#define AV_CH_LAYOUT_QUAD              (AV_CH_LAYOUT_STEREO|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT0           (AV_CH_LAYOUT_SURROUND|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
#define AV_CH_LAYOUT_5POINT1           (AV_CH_LAYOUT_5POINT0|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_5POINT0_BACK      (AV_CH_LAYOUT_SURROUND|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT1_BACK      (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_6POINT0           (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT0_FRONT     (AV_CH_LAYOUT_2_2|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_HEXAGONAL         (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1           (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1_BACK      (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1_FRONT     (AV_CH_LAYOUT_6POINT0_FRONT|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_7POINT0           (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT0_FRONT     (AV_CH_LAYOUT_5POINT0|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1           (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT1_WIDE      (AV_CH_LAYOUT_5POINT1|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1_WIDE_BACK (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_OCTAGONAL         (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_LEFT|AV_CH_BACK_CENTER|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_HEXADECAGONAL     (AV_CH_LAYOUT_OCTAGONAL|AV_CH_WIDE_LEFT|AV_CH_WIDE_RIGHT|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT|AV_CH_TOP_BACK_CENTER|AV_CH_TOP_FRONT_CENTER|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_STEREO_DOWNMIX    (AV_CH_STEREO_LEFT|AV_CH_STEREO_RIGHT)


static const uint64_t l2a_aac_channel_layout[16] = {
    AV_CH_LAYOUT_MONO,
    AV_CH_LAYOUT_STEREO,
    AV_CH_LAYOUT_SURROUND,
    AV_CH_LAYOUT_4POINT0,
    AV_CH_LAYOUT_5POINT0_BACK,
    AV_CH_LAYOUT_5POINT1_BACK,
    AV_CH_LAYOUT_7POINT1_WIDE_BACK,
    0,
    0,
    0,
    AV_CH_LAYOUT_6POINT1,
    AV_CH_LAYOUT_7POINT1,
    0,
    /* AV_CH_LAYOUT_7POINT1_TOP, */
};




enum AudioObjectType {
    AOT_NULL,
    // Support?                Name
    AOT_AAC_MAIN,              ///< Y                       Main
    AOT_AAC_LC,                ///< Y                       Low Complexity
    AOT_AAC_SSR,               ///< N (code in SoC repo)    Scalable Sample Rate
    AOT_AAC_LTP,               ///< Y                       Long Term Prediction
    AOT_SBR,                   ///< Y                       Spectral Band Replication
    AOT_AAC_SCALABLE,          ///< N                       Scalable
    AOT_TWINVQ,                ///< N                       Twin Vector Quantizer
    AOT_CELP,                  ///< N                       Code Excited Linear Prediction
    AOT_HVXC,                  ///< N                       Harmonic Vector eXcitation Coding
    AOT_TTSI             = 12, ///< N                       Text-To-Speech Interface
    AOT_MAINSYNTH,             ///< N                       Main Synthesis
    AOT_WAVESYNTH,             ///< N                       Wavetable Synthesis
    AOT_MIDI,                  ///< N                       General MIDI
    AOT_SAFX,                  ///< N                       Algorithmic Synthesis and Audio Effects
    AOT_ER_AAC_LC,             ///< N                       Error Resilient Low Complexity
    AOT_ER_AAC_LTP       = 19, ///< N                       Error Resilient Long Term Prediction
    AOT_ER_AAC_SCALABLE,       ///< N                       Error Resilient Scalable
    AOT_ER_TWINVQ,             ///< N                       Error Resilient Twin Vector Quantizer
    AOT_ER_BSAC,               ///< N                       Error Resilient Bit-Sliced Arithmetic Coding
    AOT_ER_AAC_LD,             ///< N                       Error Resilient Low Delay
    AOT_ER_CELP,               ///< N                       Error Resilient Code Excited Linear Prediction
    AOT_ER_HVXC,               ///< N                       Error Resilient Harmonic Vector eXcitation Coding
    AOT_ER_HILN,               ///< N                       Error Resilient Harmonic and Individual Lines plus Noise
    AOT_ER_PARAM,              ///< N                       Error Resilient Parametric
    AOT_SSC,                   ///< N                       SinuSoidal Coding
    AOT_PS,                    ///< N                       Parametric Stereo
    AOT_SURROUND,              ///< N                       MPEG Surround
    AOT_ESCAPE,                ///< Y                       Escape Value
    AOT_L1,                    ///< Y                       Layer 1
    AOT_L2,                    ///< Y                       Layer 2
    AOT_L3,                    ///< Y                       Layer 3
    AOT_DST,                   ///< N                       Direct Stream Transfer
    AOT_ALS,                   ///< Y                       Audio LosslesS
    AOT_SLS,                   ///< N                       Scalable LosslesS
    AOT_SLS_NON_CORE,          ///< N                       Scalable LosslesS (non core)
    AOT_ER_AAC_ELD,            ///< N                       Error Resilient Enhanced Low Delay
    AOT_SMR_SIMPLE,            ///< N                       Symbolic Music Representation Simple
    AOT_SMR_MAIN,              ///< N                       Symbolic Music Representation Main
    AOT_USAC_NOSBR,            ///< N                       Unified Speech and Audio Coding (no SBR)
    AOT_SAOC,                  ///< N                       Spatial Audio Object Coding
    AOT_LD_SURROUND,           ///< N                       Low Delay MPEG Surround
    AOT_USAC,                  ///< N                       Unified Speech and Audio Coding
};

enum ChannelPosition {
    AAC_CHANNEL_OFF   = 0,
    AAC_CHANNEL_FRONT = 1,
    AAC_CHANNEL_SIDE  = 2,
    AAC_CHANNEL_BACK  = 3,
    AAC_CHANNEL_LFE   = 4,
    AAC_CHANNEL_CC    = 5,
};

enum OCStatus {
    OC_NONE,        ///< Output unconfigured
    OC_TRIAL_PCE,   ///< Output configuration under trial specified by an inband PCE
    OC_TRIAL_FRAME, ///< Output configuration under trial specified by a frame header
    OC_GLOBAL_HDR,  ///< Output configuration set in a global header but not yet locked
    OC_LOCKED,      ///< Output configuration locked in place
};

enum RawDataBlockType {
    TYPE_SCE,
    TYPE_CPE,
    TYPE_CCE,
    TYPE_LFE,
    TYPE_DSE,
    TYPE_PCE,
    TYPE_FIL,
    TYPE_END,
};





#pragma mark ---- GetBitContext
/*
 * Safe bitstream reading:
 * optionally, the get_bits API can check to ensure that we
 * don't read past input buffer boundaries. This is protected
 * with CONFIG_SAFE_BITSTREAM_READER at the global level, and
 * then below that with UNCHECKED_BITSTREAM_READER at the per-
 * decoder level. This means that decoders that check internally
 * can "#define UNCHECKED_BITSTREAM_READER 1" to disable
 * overread checks.
 * Boundary checking causes a minor performance penalty so for
 * applications that won't want/need this, it can be disabled
 * globally using "#define CONFIG_SAFE_BITSTREAM_READER 0".
 */
#ifndef UNCHECKED_BITSTREAM_READER
#define UNCHECKED_BITSTREAM_READER !CONFIG_SAFE_BITSTREAM_READER
#endif

typedef struct GetBitContext {
    const uint8_t *buffer, *buffer_end;
    int index;
    int size_in_bits;
    int size_in_bits_plus8;
} GetBitContext;

/* Bitstream reader API docs:
 * name
 *   arbitrary name which is used as prefix for the internal variables
 *
 * gb
 *   getbitcontext
 *
 * OPEN_READER(name, gb)
 *   load gb into local variables
 *
 * CLOSE_READER(name, gb)
 *   store local vars in gb
 *
 * UPDATE_CACHE(name, gb)
 *   Refill the internal cache from the bitstream.
 *   After this call at least MIN_CACHE_BITS will be available.
 *
 * GET_CACHE(name, gb)
 *   Will output the contents of the internal cache,
 *   next bit is MSB of 32 or 64 bits (FIXME 64 bits).
 *
 * SHOW_UBITS(name, gb, num)
 *   Will return the next num bits.
 *
 * SHOW_SBITS(name, gb, num)
 *   Will return the next num bits and do sign extension.
 *
 * SKIP_BITS(name, gb, num)
 *   Will skip over the next num bits.
 *   Note, this is equivalent to SKIP_CACHE; SKIP_COUNTER.
 *
 * SKIP_CACHE(name, gb, num)
 *   Will remove the next num bits from the cache (note SKIP_COUNTER
 *   MUST be called before UPDATE_CACHE / CLOSE_READER).
 *
 * SKIP_COUNTER(name, gb, num)
 *   Will increment the internal bit counter (see SKIP_CACHE & SKIP_BITS).
 *
 * LAST_SKIP_BITS(name, gb, num)
 *   Like SKIP_BITS, to be used if next call is UPDATE_CACHE or CLOSE_READER.
 *
 * BITS_LEFT(name, gb)
 *   Return the number of bits left
 *
 * For examples see get_bits, show_bits, skip_bits, get_vlc.
 */

#if defined(__GNUC__)
#    define av_unused __attribute__((unused))
#else
#    define av_unused
#endif

#ifdef LONG_BITSTREAM_READER
#   define MIN_CACHE_BITS 32
#else
#   define MIN_CACHE_BITS 25
#endif

#define NEG_USR32(a,s) (((uint32_t)(a))>>(32-(s)))

#define NEG_SSR32 NEG_SSR32
static inline  int32_t NEG_SSR32( int32_t a, int8_t s){
    __asm__ ("sarl %1, %0\n\t"
             : "+r" (a)
             : "ic" ((uint8_t)(-s))
             );
    return a;
}

#   define AV_RB32(x)                                \
(((uint32_t)((const uint8_t*)(x))[0] << 24) |    \
(((const uint8_t*)(x))[1] << 16) |    \
(((const uint8_t*)(x))[2] <<  8) |    \
((const uint8_t*)(x))[3])

#   define AV_RL32(x)                                \
(((uint32_t)((const uint8_t*)(x))[3] << 24) |    \
(((const uint8_t*)(x))[2] << 16) |    \
(((const uint8_t*)(x))[1] <<  8) |    \
((const uint8_t*)(x))[0])

# define AV_WB32(p, darg) do {                \
unsigned d = (darg);                    \
((uint8_t*)(p))[3] = (d);               \
((uint8_t*)(p))[2] = (d)>>8;            \
((uint8_t*)(p))[1] = (d)>>16;           \
((uint8_t*)(p))[0] = (d)>>24;           \
} while(0)

static inline const unsigned zero_extend(unsigned val, unsigned bits)
{
    return (val << ((8 * sizeof(int)) - bits)) >> ((8 * sizeof(int)) - bits);
}

#define OPEN_READER_NOSIZE(name, gb)            \
unsigned int name ## _index = (gb)->index;  \
unsigned int av_unused name ## _cache

#if UNCHECKED_BITSTREAM_READER
#define OPEN_READER(name, gb) OPEN_READER_NOSIZE(name, gb)

#define BITS_AVAILABLE(name, gb) 1
#else
#define OPEN_READER(name, gb)                   \
OPEN_READER_NOSIZE(name, gb);               \
unsigned int name ## _size_plus8 = (gb)->size_in_bits_plus8

#define BITS_AVAILABLE(name, gb) name ## _index < name ## _size_plus8
#endif

#define CLOSE_READER(name, gb) (gb)->index = name ## _index

# ifdef LONG_BITSTREAM_READER

# define UPDATE_CACHE_LE(name, gb) name ## _cache = \
AV_RL64((gb)->buffer + (name ## _index >> 3)) >> (name ## _index & 7)

# define UPDATE_CACHE_BE(name, gb) name ## _cache = \
AV_RB64((gb)->buffer + (name ## _index >> 3)) >> (32 - (name ## _index & 7))

#else

# define UPDATE_CACHE_LE(name, gb) name ## _cache = \
AV_RL32((gb)->buffer + (name ## _index >> 3)) >> (name ## _index & 7)

# define UPDATE_CACHE_BE(name, gb) name ## _cache = \
AV_RB32((gb)->buffer + (name ## _index >> 3)) << (name ## _index & 7)

#endif



# define UPDATE_CACHE(name, gb) UPDATE_CACHE_BE(name, gb)

# define SKIP_CACHE(name, gb, num) name ## _cache <<= (num)


#if UNCHECKED_BITSTREAM_READER
#   define SKIP_COUNTER(name, gb, num) name ## _index += (num)
#else
#   define SKIP_COUNTER(name, gb, num) \
name ## _index = FFMIN(name ## _size_plus8, name ## _index + (num))
#endif

#define BITS_LEFT(name, gb) ((int)((gb)->size_in_bits - name ## _index))

#define SKIP_BITS(name, gb, num)                \
do {                                        \
SKIP_CACHE(name, gb, num);              \
SKIP_COUNTER(name, gb, num);            \
} while (0)

#define LAST_SKIP_BITS(name, gb, num) SKIP_COUNTER(name, gb, num)

#define SHOW_UBITS_LE(name, gb, num) zero_extend(name ## _cache, num)
#define SHOW_SBITS_LE(name, gb, num) sign_extend(name ## _cache, num)

#define SHOW_UBITS_BE(name, gb, num) NEG_USR32(name ## _cache, num)
#define SHOW_SBITS_BE(name, gb, num) NEG_SSR32(name ## _cache, num)

#   define SHOW_UBITS(name, gb, num) SHOW_UBITS_BE(name, gb, num)
#   define SHOW_SBITS(name, gb, num) SHOW_SBITS_BE(name, gb, num)

#define GET_CACHE(name, gb) ((uint32_t) name ## _cache)

static inline int get_bits_count(const GetBitContext *s)
{
    return s->index;
}

static inline void skip_bits_long(GetBitContext *s, int n)
{
    s->index += n;
}



/**
 * Read 1-25 bits.
 */
static inline unsigned int get_bits(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER(re, s);
    assert(n>0 && n<=25);
    UPDATE_CACHE(re, s);
    tmp = SHOW_UBITS(re, s, n);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    return tmp;
}

static inline unsigned int get_bits_le(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER(re, s);
    assert(n>0 && n<=25);
    UPDATE_CACHE_LE(re, s);
    tmp = SHOW_UBITS_LE(re, s, n);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    return tmp;
}

static inline unsigned int get_bits1(GetBitContext *s)
{
    unsigned int index = s->index;
    uint8_t result     = s->buffer[index >> 3];
#ifdef BITSTREAM_READER_LE
    result >>= index & 7;
    result  &= 1;
#else
    result <<= index & 7;
    result >>= 8 - 1;
#endif
#if !UNCHECKED_BITSTREAM_READER
    if (s->index < s->size_in_bits_plus8)
#endif
        index++;
    s->index = index;
    
    return result;
}


/**
 * Show 1-25 bits.
 */
static inline unsigned int show_bits(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER_NOSIZE(re, s);
    assert(n>0 && n<=25);
    UPDATE_CACHE(re, s);
    tmp = SHOW_UBITS(re, s, n);
    return tmp;
}



static inline void skip_bits(GetBitContext *s, int n)
{
    OPEN_READER(re, s);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
}

static inline void skip_bits1(GetBitContext *s)
{
    skip_bits(s, 1);
}

static inline const uint8_t *align_get_bits(GetBitContext *s)
{
    int n = -get_bits_count(s) & 7;
    if (n)
        skip_bits(s, n);
    return s->buffer + (s->index >> 3);
}

/**
 * Read 0-32 bits.
 */
static inline unsigned int get_bits_long(GetBitContext *s, int n)
{
    if (!n) {
        return 0;
    } else if (n <= MIN_CACHE_BITS) {
        return get_bits(s, n);
    } else {
        unsigned ret = get_bits(s, 16) << (n - 16);
        return ret | get_bits(s, n - 16);
    }
}

/**
 * Show 0-32 bits.
 */
static inline unsigned int show_bits_long(GetBitContext *s, int n)
{
    if (n <= MIN_CACHE_BITS) {
        return show_bits(s, n);
    } else {
        GetBitContext gb = *s;
        return get_bits_long(&gb, n);
    }
}


/**
 * Initialize GetBitContext.
 * @param buffer bitstream buffer, must be AV_INPUT_BUFFER_PADDING_SIZE bytes
 *        larger than the actual read bits because some optimized bitstream
 *        readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 * @return 0 on success, AVERROR_INVALIDDATA if the buffer_size would overflow.
 */
static inline int init_get_bits(GetBitContext *s, const uint8_t *buffer,
                                int bit_size)
{
    int buffer_size;
    int ret = 0;
    
    if (bit_size >= INT_MAX - 7 || bit_size < 0 || !buffer) {
        bit_size    = 0;
        buffer      = NULL;
        ret         = L2AERROR_INVAILD_PARA;
    }
    
    buffer_size = (bit_size + 7) >> 3;
    
    s->buffer             = buffer;
    s->size_in_bits       = bit_size;
    s->size_in_bits_plus8 = bit_size + 8;
    s->buffer_end         = buffer + buffer_size;
    s->index              = 0;
    
    return ret;
}

/**
 * Initialize GetBitContext.
 * @param buffer bitstream buffer, must be AV_INPUT_BUFFER_PADDING_SIZE bytes
 *        larger than the actual read bits because some optimized bitstream
 *        readers read 32 or 64 bit at once and could read over the end
 * @param byte_size the size of the buffer in bytes
 * @return 0 on success, AVERROR_INVALIDDATA if the buffer_size would overflow.
 */
static inline int init_get_bits8(GetBitContext *s, const uint8_t *buffer,
                                 int byte_size)
{
    if (byte_size > INT_MAX / 8 || byte_size < 0)
        byte_size = -1;
    return init_get_bits(s, buffer, byte_size * 8);
}


static inline int get_bits_left(GetBitContext *gb)
{
    return gb->size_in_bits - get_bits_count(gb);
}

#pragma mark ---- PutBitContext

typedef struct PutBitContext {
    uint32_t bit_buf;
    int bit_left;
    uint8_t *buf, *buf_ptr, *buf_end;
    int size_in_bits;
} PutBitContext;

/**
 * Initialize the PutBitContext s.
 *
 * @param buffer the buffer where to put bits
 * @param buffer_size the size in bytes of buffer
 */
static inline void init_put_bits(PutBitContext *s, uint8_t *buffer,
                                 int buffer_size)
{
    if (buffer_size < 0) {
        buffer_size = 0;
        buffer      = NULL;
    }
    
    s->size_in_bits = 8 * buffer_size;
    s->buf          = buffer;
    s->buf_end      = s->buf + buffer_size;
    s->buf_ptr      = s->buf;
    s->bit_left     = 32;
    s->bit_buf      = 0;
    
}

/**
 * Pad the end of the output stream with zeros.
 */
static inline void flush_put_bits(PutBitContext *s)
{
    if (s->bit_left < 32)
        s->bit_buf <<= s->bit_left;
    while (s->bit_left < 32) {
        assert(s->buf_ptr < s->buf_end);
        *s->buf_ptr++ = s->bit_buf >> 24;
        s->bit_buf  <<= 8;
        s->bit_left  += 8;
    }
    s->bit_left = 32;
    s->bit_buf  = 0;
}



static inline void put_bits(PutBitContext *s, int n, unsigned int value)
{
    unsigned int bit_buf;
    int bit_left;
    
    assert(n <= 31 && value < (1U << n));
    
    bit_buf  = s->bit_buf;
    bit_left = s->bit_left;
    
    if (n < bit_left) {
        bit_buf     = (bit_buf << n) | value;
        bit_left   -= n;
    } else {
        bit_buf   <<= bit_left;
        bit_buf    |= value >> (n - bit_left);
        if (3 < s->buf_end - s->buf_ptr) {
            AV_WB32(s->buf_ptr, bit_buf);
            s->buf_ptr += 4;
        } else {
            printf("Internal error, put_bits buffer too small\n");
            assert(0);
        }
        bit_left   += 32 - n;
        bit_buf     = value;
    }
    
    s->bit_buf  = bit_buf;
    s->bit_left = bit_left;
    
}

#pragma mark --- LATM parse

typedef struct MPEG4AudioConfig {
    int object_type;
    int sampling_index;
    int sample_rate;
    int chan_config;
    int sbr; ///< -1 implicit, 1 presence
    int ext_object_type;
    int ext_sampling_index;
    int ext_sample_rate;
    int ext_chan_config;
    int channels;
    int ps;  ///< -1 implicit, 1 presence
    int frame_length_short;
} MPEG4AudioConfig;



typedef struct OriginalFrame {
    int frame_size;
    uint8_t *frame_data;
}OriginalFrame;

struct LATMContext {
    int initialized;        ///< initialized after a valid extradata was seen
    // parser data
    int audio_mux_version_A; ///< LATM syntax version
    int frame_length_type;   ///< 0/1 variable/fixed frame length
    int frame_length;        ///< frame length for fixed frame length
    int use_same_stream_mux;
    MPEG4AudioConfig m4ac;
    OriginalFrame  of;
};



static int parse_config_ALS(GetBitContext *gb, MPEG4AudioConfig *c)
{
    if (get_bits_left(gb) < 112)
        return -1;
    
    if (get_bits_long(gb, 32) != L2A_MKBETAG('A','L','S','\0'))
        return -1;
    
    // override AudioSpecificConfig channel configuration and sample rate
    // which are buggy in old ALS conformance files
    c->sample_rate = get_bits_long(gb, 32);
    
    // skip number of samples
    skip_bits_long(gb, 32);
    
    // read number of channels
    c->chan_config = 0;
    c->channels    = get_bits(gb, 16) + 1;
    
    return 0;
}

static inline uint32_t latm_get_value(GetBitContext *b)
{
    int length = get_bits(b, 2);
    
    return get_bits_long(b, (length+1)*8);
}

const int l2a_avpriv_mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};


const uint8_t l2a_ff_mpeg4audio_channels[8] = {
    0, 1, 2, 3, 4, 5, 6, 8
};

static inline int get_object_type(GetBitContext *gb)
{
    int object_type = get_bits(gb, 5);
    if (object_type == AOT_ESCAPE)
        object_type = 32 + get_bits(gb, 6);
    return object_type;
}

static inline int get_sample_rate(GetBitContext *gb, int *index)
{
    *index = get_bits(gb, 4);
    return *index == 0x0f ? get_bits(gb, 24) :
    l2a_avpriv_mpeg4audio_sample_rates[*index];
}

static int count_channels(uint8_t (*layout)[3], int tags)
{
    int i, sum = 0;
    for (i = 0; i < tags; i++) {
        int syn_ele = layout[i][0];
        int pos     = layout[i][2];
        sum += (1 + (syn_ele == TYPE_CPE)) *
        (pos != AAC_CHANNEL_OFF && pos != AAC_CHANNEL_CC);
    }
    return sum;
}

int l2a_avpriv_mpeg4audio_get_config(MPEG4AudioConfig *c, const uint8_t *buf,
                                 int bit_size, int sync_extension)
{
    GetBitContext gb;
    int specific_config_bitindex, ret;
    
    if (bit_size <= 0)
        return L2AERROR_INVAILD_DATA;
    
    ret = init_get_bits(&gb, buf, bit_size);
    if (ret < 0)
        return ret;
    
    c->object_type = get_object_type(&gb);
    c->sample_rate = get_sample_rate(&gb, &c->sampling_index);
    c->chan_config = get_bits(&gb, 4);
    if (c->chan_config < L2A_FF_ARRAY_ELEMS(l2a_ff_mpeg4audio_channels))
        c->channels = l2a_ff_mpeg4audio_channels[c->chan_config];
    c->sbr = -1;
    c->ps  = -1;
    if (c->object_type == AOT_SBR || (c->object_type == AOT_PS &&
                                      // check for W6132 Annex YYYY draft MP3onMP4
                                      !(show_bits(&gb, 3) & 0x03 && !(show_bits(&gb, 9) & 0x3F)))) {
        if (c->object_type == AOT_PS)
            c->ps = 1;
        c->ext_object_type = AOT_SBR;
        c->sbr = 1;
        c->ext_sample_rate = get_sample_rate(&gb, &c->ext_sampling_index);
        c->object_type = get_object_type(&gb);
        if (c->object_type == AOT_ER_BSAC)
            c->ext_chan_config = get_bits(&gb, 4);
    } else {
        c->ext_object_type = AOT_NULL;
        c->ext_sample_rate = 0;
    }
    specific_config_bitindex = get_bits_count(&gb);
    
    if (c->object_type == AOT_ALS) {
        skip_bits(&gb, 5);
        if (show_bits_long(&gb, 24) != L2A_MKBETAG('\0','A','L','S'))
            skip_bits_long(&gb, 24);
        
        specific_config_bitindex = get_bits_count(&gb);
        
        if (parse_config_ALS(&gb, c))
            return -1;
    }
    
    if (c->ext_object_type != AOT_SBR && sync_extension) {
        while (get_bits_left(&gb) > 15) {
            if (show_bits(&gb, 11) == 0x2b7) { // sync extension
                get_bits(&gb, 11);
                c->ext_object_type = get_object_type(&gb);
                if (c->ext_object_type == AOT_SBR && (c->sbr = get_bits1(&gb)) == 1) {
                    c->ext_sample_rate = get_sample_rate(&gb, &c->ext_sampling_index);
                    if (c->ext_sample_rate == c->sample_rate)
                        c->sbr = -1;
                }
                if (get_bits_left(&gb) > 11 && get_bits(&gb, 11) == 0x548)
                    c->ps = get_bits1(&gb);
                break;
            } else
                get_bits1(&gb); // skip 1 bit
        }
    }
    
    //PS requires SBR
    if (!c->sbr)
        c->ps = 0;
    //Limit implicit PS to the HE-AACv2 Profile
    if ((c->ps == -1 && c->object_type != AOT_AAC_LC) || c->channels & ~0x01)
        c->ps = 0;
    
    return specific_config_bitindex;
}

static void decode_channel_map(uint8_t layout_map[][3],
                               enum ChannelPosition type,
                               GetBitContext *gb, int n)
{
    while (n--) {
        enum RawDataBlockType syn_ele;
        switch (type) {
            case AAC_CHANNEL_FRONT:
            case AAC_CHANNEL_BACK:
            case AAC_CHANNEL_SIDE:
                syn_ele = get_bits1(gb);
                break;
            case AAC_CHANNEL_CC:
                skip_bits1(gb);
                syn_ele = TYPE_CCE;
                break;
            case AAC_CHANNEL_LFE:
                syn_ele = TYPE_LFE;
                break;
            default:
                // AAC_CHANNEL_OFF has no channel map
                assert(0);
        }
        layout_map[0][0] = syn_ele;
        layout_map[0][1] = get_bits(gb, 4);
        layout_map[0][2] = type;
        layout_map++;
    }
}

static int decode_pce(MPEG4AudioConfig *m4ac,
                      uint8_t (*layout_map)[3],
                      GetBitContext *gb)
{
    int num_front, num_side, num_back, num_lfe, num_assoc_data, num_cc;
    int sampling_index;
    int comment_len;
    int tags;
    
    skip_bits(gb, 2);  // object_type
    
    sampling_index = get_bits(gb, 4);
    if (m4ac->sampling_index != sampling_index)
        printf("Sample rate index in program config element does not "
               "match the sample rate index configured by the container.\n");
    
    num_front       = get_bits(gb, 4);
    num_side        = get_bits(gb, 4);
    num_back        = get_bits(gb, 4);
    num_lfe         = get_bits(gb, 2);
    num_assoc_data  = get_bits(gb, 3);
    num_cc          = get_bits(gb, 4);
    
    if (get_bits1(gb))
        skip_bits(gb, 4); // mono_mixdown_tag
    if (get_bits1(gb))
        skip_bits(gb, 4); // stereo_mixdown_tag
    
    if (get_bits1(gb))
        skip_bits(gb, 3); // mixdown_coeff_index and pseudo_surround
    
    if (get_bits_left(gb) < 4 * (num_front + num_side + num_back + num_lfe + num_assoc_data + num_cc)) {
        printf("decode_pce: %s",l2a_overread_err);
        return -1;
    }
    decode_channel_map(layout_map       , AAC_CHANNEL_FRONT, gb, num_front);
    tags = num_front;
    decode_channel_map(layout_map + tags, AAC_CHANNEL_SIDE,  gb, num_side);
    tags += num_side;
    decode_channel_map(layout_map + tags, AAC_CHANNEL_BACK,  gb, num_back);
    tags += num_back;
    decode_channel_map(layout_map + tags, AAC_CHANNEL_LFE,   gb, num_lfe);
    tags += num_lfe;
    
    skip_bits_long(gb, 4 * num_assoc_data);
    
    decode_channel_map(layout_map + tags, AAC_CHANNEL_CC,    gb, num_cc);
    tags += num_cc;
    
    align_get_bits(gb);
    
    /* comment field, first byte is length */
    comment_len = get_bits(gb, 8) * 8;
    if (get_bits_left(gb) < comment_len) {
        printf("decode_pce: %s",l2a_overread_err);
        return L2AERROR_INVAILD_DATA;
    }
    skip_bits_long(gb, comment_len);
    return tags;
}

/**
 * Set up channel positions based on a default channel configuration
 * as specified in table 1.17.
 *
 * @return  Returns error status. 0 - OK, !0 - error
 */
static int set_default_channel_config(uint8_t (*layout_map)[3],
                                      int *tags,
                                      int channel_config)
{
    if (channel_config < 1 || (channel_config > 7 && channel_config < 11) ||
        channel_config > 12) {
        printf("invalid default channel configuration (%d)\n",
               channel_config);
        return L2AERROR_INVAILD_DATA;
    }
    *tags = l2a_tags_per_config[channel_config];
    memcpy(layout_map, &l2a_aac_channel_layout[channel_config - 1],
           *tags * sizeof(*layout_map));
    
    /*
     * AAC specification has 7.1(wide) as a default layout for 8-channel streams.
     * However, at least Nero AAC encoder encodes 7.1 streams using the default
     * channel config 7, mapping the side channels of the original audio stream
     * to the second AAC_CHANNEL_FRONT pair in the AAC stream. Similarly, e.g. FAAD
     * decodes the second AAC_CHANNEL_FRONT pair as side channels, therefore decoding
     * the incorrect streams as if they were correct (and as the encoder intended).
     *
     * As actual intended 7.1(wide) streams are very rare, default to assuming a
     * 7.1 layout was intended.
     */
//    if (channel_config == 7 && avctx->strict_std_compliance < L2A_FF_COMPLIANCE_STRICT) {
//        printf("Assuming an incorrectly encoded 7.1 channel layout"
//               " instead of a spec-compliant 7.1(wide) layout, use -strict %d to decode"
//               " according to the specification instead.\n", L2A_FF_COMPLIANCE_STRICT);
//        layout_map[2][2] = AAC_CHANNEL_SIDE;
//    }
    
    return 0;
}


static int decode_ga_specific_config(GetBitContext *gb,
                                     MPEG4AudioConfig *m4ac,
                                     int channel_config)
{
    int extension_flag, ret, ep_config, res_flags;
    uint8_t layout_map[L2A_MAX_ELEM_ID*4][3];
    int tags = 0;
    
    if (get_bits1(gb)) { // frameLengthFlag
        printf("960/120 MDCT window\n");
        return L2AERROR_PATCHWELCOME;
    }
    m4ac->frame_length_short = 0;
    
    if (get_bits1(gb))       // dependsOnCoreCoder
        skip_bits(gb, 14);   // coreCoderDelay
    extension_flag = get_bits1(gb);
    
    if (m4ac->object_type == AOT_AAC_SCALABLE ||
        m4ac->object_type == AOT_ER_AAC_SCALABLE)
        skip_bits(gb, 3);     // layerNr
    
    if (channel_config == 0) {
        skip_bits(gb, 4);  // element_instance_tag
        tags = decode_pce(m4ac, layout_map, gb);
        if (tags < 0)
            return tags;
    } else {
        if ((ret = set_default_channel_config(layout_map,
                                              &tags, channel_config)))
            return ret;
    }
    
    if (count_channels(layout_map, tags) > 1) {
        m4ac->ps = 0;
    } else if (m4ac->sbr == 1 && m4ac->ps == -1)
        m4ac->ps = 1;
    
//    if (ac && (ret = output_configure(ac, layout_map, tags, OC_GLOBAL_HDR, 0)))
//        return ret;
    
    if (extension_flag) {
        switch (m4ac->object_type) {
            case AOT_ER_BSAC:
                skip_bits(gb, 5);    // numOfSubFrame
                skip_bits(gb, 11);   // layer_length
                break;
            case AOT_ER_AAC_LC:
            case AOT_ER_AAC_LTP:
            case AOT_ER_AAC_SCALABLE:
            case AOT_ER_AAC_LD:
                res_flags = get_bits(gb, 3);
                if (res_flags) {
                    printf("AAC data resilience (flags %x)\n",
                           res_flags);
                    return L2AERROR_PATCHWELCOME;
                }
                break;
        }
        skip_bits1(gb);    // extensionFlag3 (TBD in version 3)
    }
    switch (m4ac->object_type) {
        case AOT_ER_AAC_LC:
        case AOT_ER_AAC_LTP:
        case AOT_ER_AAC_SCALABLE:
        case AOT_ER_AAC_LD:
            ep_config = get_bits(gb, 2);
            if (ep_config) {
                printf("epConfig %d\n", ep_config);
                return L2AERROR_PATCHWELCOME;
            }
    }
    return 0;
}

static int decode_eld_specific_config(GetBitContext *gb,
                                      MPEG4AudioConfig *m4ac,
                                      int channel_config)
{
    int ret, ep_config, res_flags;
    uint8_t layout_map[L2A_MAX_ELEM_ID*4][3];
    int tags = 0;
    const int ELDEXT_TERM = 0;
    
    m4ac->ps  = 0;
    m4ac->sbr = 0;
    m4ac->frame_length_short = get_bits1(gb);
    res_flags = get_bits(gb, 3);
    if (res_flags) {
        printf("AAC data resilience (flags %x)\n",
               res_flags);
        return L2AERROR_PATCHWELCOME;
    }
    
    if (get_bits1(gb)) { // ldSbrPresentFlag
        printf("Low Delay SBR\n");
        return L2AERROR_PATCHWELCOME;
    }
    
    while (get_bits(gb, 4) != ELDEXT_TERM) {
        int len = get_bits(gb, 4);
        if (len == 15)
            len += get_bits(gb, 8);
        if (len == 15 + 255)
            len += get_bits(gb, 16);
        if (get_bits_left(gb) < len * 8 + 4) {
            //av_log(avctx, AV_LOG_ERROR, overread_err);
            printf("decode_eld_specific_config error\n");
            return L2AERROR_INVAILD_DATA;
        }
        skip_bits_long(gb, 8 * len);
    }
    
    if ((ret = set_default_channel_config(layout_map,
                                          &tags, channel_config)))
        return ret;
    
//    if (ac && (ret = output_configure(ac, layout_map, tags, OC_GLOBAL_HDR, 0)))
//        return ret;
    
    ep_config = get_bits(gb, 2);
    if (ep_config) {
        printf("epConfig %d\n", ep_config);
        return L2AERROR_PATCHWELCOME;
    }
    return 0;
}



static int decode_audio_specific_config(MPEG4AudioConfig *m4ac,
                                        const uint8_t *data, int bit_size,
                                        int sync_extension)
{
    GetBitContext gb;
    int i, ret;
    
    if (bit_size < 0 || bit_size > INT_MAX) {
        printf("Audio specific config size is invalid\n");
        return L2AERROR_INVAILD_DATA;
    }
    
    //printf("audio specific config size(bit) %d\n", (int)bit_size >> 3);
    
    if ((ret = init_get_bits(&gb, data, bit_size)) < 0)
        return ret;
    
    if ((i = l2a_avpriv_mpeg4audio_get_config(m4ac, data, bit_size,
                                          sync_extension)) < 0)
        return L2AERROR_INVAILD_DATA;
    if (m4ac->sampling_index > 12) {
        printf("invalid sampling rate index %d\n",
               m4ac->sampling_index);
        return L2AERROR_INVAILD_DATA;
    }
    if (m4ac->object_type == AOT_ER_AAC_LD &&
        (m4ac->sampling_index < 3 || m4ac->sampling_index > 7)) {
        printf("invalid low delay sampling rate index %d\n",
               m4ac->sampling_index);
        return L2AERROR_INVAILD_DATA;
    }
    
    skip_bits_long(&gb, i);
    
    switch (m4ac->object_type) {
        case AOT_AAC_MAIN:
        case AOT_AAC_LC:
        case AOT_AAC_LTP:
        case AOT_ER_AAC_LC:
        case AOT_ER_AAC_LD:
            if ((ret = decode_ga_specific_config(&gb,
                                                 m4ac, m4ac->chan_config)) < 0)
                return ret;
            break;
        case AOT_ER_AAC_ELD:
            if ((ret = decode_eld_specific_config(&gb,
                                                  m4ac, m4ac->chan_config)) < 0)
                return ret;
            break;
        default:
            printf("Audio object type %s%d\n",
                   m4ac->sbr == 1 ? "SBR+" : "",
                   m4ac->object_type);
            return L2AERROR_ENOSYS;
    }
    
//    printf("AOT %d chan config %d sampling index %d (%d) SBR %d PS %d\n",
//            m4ac->object_type, m4ac->chan_config, m4ac->sampling_index,
//            m4ac->sample_rate, m4ac->sbr,
//            m4ac->ps);
//    printf("AOT %d chan config %d sampling index %d (%d)\n",
//           m4ac->object_type, m4ac->chan_config, m4ac->sampling_index,
//           m4ac->sample_rate);
    
    return get_bits_count(&gb);
}


static int latm_decode_audio_specific_config(struct LATMContext *latmctx,
                                             GetBitContext *gb, int asclen)
{

    int config_start_bit  = get_bits_count(gb);
    int sync_extension    = 0;
    int bits_consumed;// esize;
    
    if (asclen) {
        sync_extension = 1;
        asclen         = L2A_MIN(asclen, get_bits_left(gb));
    } else
        asclen         = get_bits_left(gb);
    
    if (config_start_bit % 8) {
        printf("Non-byte-aligned audio-specific config");
        return L2AERROR_PATCHWELCOME;
    }
    if (asclen <= 0)
        return L2AERROR_INVAILD_DATA;
    
    
    
    
    
    bits_consumed = decode_audio_specific_config(&latmctx->m4ac,
                                                 gb->buffer + (config_start_bit / 8),
                                                 asclen, sync_extension);

    if (bits_consumed < 0)
        return L2AERROR_INVAILD_DATA;
//
//    if (!latmctx->initialized ||
//        ac->oc[1].m4ac.sample_rate != m4ac.sample_rate ||
//        ac->oc[1].m4ac.chan_config != m4ac.chan_config) {
//        
//        if(latmctx->initialized) {
//            av_log(avctx, AV_LOG_INFO, "audio config changed\n");
//        } else {
//            av_log(avctx, AV_LOG_DEBUG, "initializing latmctx\n");
//        }
//        latmctx->initialized = 0;
//        
//        esize = (bits_consumed+7) / 8;
//        
//        if (avctx->extradata_size < esize) {
//            av_free(avctx->extradata);
//            avctx->extradata = av_malloc(esize + AV_INPUT_BUFFER_PADDING_SIZE);
//            if (!avctx->extradata)
//                return AVERROR(ENOMEM);
//        }
//        
//        avctx->extradata_size = esize;
//        memcpy(avctx->extradata, gb->buffer + (config_start_bit/8), esize);
//        memset(avctx->extradata+esize, 0, AV_INPUT_BUFFER_PADDING_SIZE);
//    }
    skip_bits_long(gb, bits_consumed);
    
    return bits_consumed;
}

static int read_stream_mux_config(struct LATMContext *latmctx,
                                  GetBitContext *gb)
{
    int ret, audio_mux_version = get_bits(gb, 1);
    
    latmctx->audio_mux_version_A = 0;
    if (audio_mux_version)
        latmctx->audio_mux_version_A = get_bits(gb, 1);
    
    if (!latmctx->audio_mux_version_A) {
        
        if (audio_mux_version)
            latm_get_value(gb);                 // taraFullness
        
        skip_bits(gb, 1);                       // allStreamSameTimeFraming
        skip_bits(gb, 6);                       // numSubFrames
        //int numSubFrames = get_bits(gb, 6);
        // numPrograms
        int numPrograms = get_bits(gb, 4);
        if (numPrograms) {                  // numPrograms
            printf("Multiple programs");
            return L2AERROR_PATCHWELCOME;
        }
        
        // for each program (which there is only one in DVB)
        
        // for each layer (which there is only one in DVB)
        if (get_bits(gb, 3)) {                   // numLayer
            printf("Multiple layers");
            return L2AERROR_PATCHWELCOME;
        }
        
        // for all but first stream: use_same_config = get_bits(gb, 1);
        if (!audio_mux_version) {
            if ((ret = latm_decode_audio_specific_config(latmctx, gb, 0)) < 0)
                return ret;
        } else {
            int ascLen = latm_get_value(gb);
            if ((ret = latm_decode_audio_specific_config(latmctx, gb, ascLen)) < 0)
                return ret;
            ascLen -= ret;
            skip_bits_long(gb, ascLen);
        }
        
        latmctx->frame_length_type = get_bits(gb, 3);
        switch (latmctx->frame_length_type) {
            case 0:
                skip_bits(gb, 8);       // latmBufferFullness
                break;
            case 1:
                latmctx->frame_length = get_bits(gb, 9);
                break;
            case 3:
            case 4:
            case 5:
                skip_bits(gb, 6);       // CELP frame length table index
                break;
            case 6:
            case 7:
                skip_bits(gb, 1);       // HVXC frame length table index
                break;
        }
        
        if (get_bits(gb, 1)) {                  // other data
            if (audio_mux_version) {
                latm_get_value(gb);             // other_data_bits
            } else {
                int esc;
                do {
                    esc = get_bits(gb, 1);
                    skip_bits(gb, 8);
                } while (esc);
            }
        }
        
        if (get_bits(gb, 1))                     // crc present
            skip_bits(gb, 8);                    // config_crc
    }
    
    return 0;
}

static int read_payload_length_info(struct LATMContext *ctx, GetBitContext *gb)
{
    uint8_t tmp;
    
    if (ctx->frame_length_type == 0) {
        int mux_slot_length = 0;
        do {
            tmp = get_bits(gb, 8);
            mux_slot_length += tmp;
        } while (tmp == 255);
        return mux_slot_length;
    } else if (ctx->frame_length_type == 1) {
        return ctx->frame_length;
    } else if (ctx->frame_length_type == 3 ||
               ctx->frame_length_type == 5 ||
               ctx->frame_length_type == 7) {
        skip_bits(gb, 2);          // mux_slot_length_coded
    }
    return 0;
}

static int read_audio_mux_element(struct LATMContext *latmctx,
                                  GetBitContext *gb)
{
    int err;
    uint8_t use_same_mux = get_bits(gb, 1);
    latmctx->use_same_stream_mux = use_same_mux;
    if (!use_same_mux) {
        if ((err = read_stream_mux_config(latmctx, gb)) < 0)
            return err;
    }
//    else if (!latmctx->aac_ctx.avctx->extradata) {
//        av_log(latmctx->aac_ctx.avctx, AV_LOG_DEBUG,
//               "no decoder config found\n");
//        return AVERROR(EAGAIN);
//    }
    if (latmctx->audio_mux_version_A == 0) {
        int mux_slot_length_bytes = read_payload_length_info(latmctx, gb);
        if (mux_slot_length_bytes * 8 > get_bits_left(gb)) {
            printf("incomplete frame %d << %d\n",mux_slot_length_bytes * 8, get_bits_left(gb));
            return L2AERROR_INVAILD_DATA;
        } else if (mux_slot_length_bytes * 8 + 256 < get_bits_left(gb)) {
            printf("frame length mismatch %d << %d\n",
                   mux_slot_length_bytes * 8, get_bits_left(gb));
            return L2AERROR_INVAILD_DATA;
        }
        
        latmctx->of.frame_size = mux_slot_length_bytes;
        latmctx->of.frame_data = malloc(latmctx->of.frame_size);
        if(!latmctx->of.frame_data){
            return L2AERROR_ENOMEM;
        }
        uint8_t temp = 0;
        int index = get_bits_count(gb)%8;
        if(index != 0){
            for(int i = 0;i < mux_slot_length_bytes;++i){
                uint8_t *begin = (uint8_t *)(gb->buffer + get_bits_count(gb)/8);
                temp = (begin[i] << index) | (begin[i+1] >> (8 - index));
                memcpy(&latmctx->of.frame_data[i], &temp, 1);
            }
        }else{
            memcpy(latmctx->of.frame_data, gb->buffer + get_bits_count(gb)/8, mux_slot_length_bytes);
        }
    }
    return 0;
}


static int latm_decode_frame(struct LATMContext *latmctx,uint8_t *latm_frame_data, int len)
{
    int                 muxlength, err;
    GetBitContext       gb;
    
    if ((err = init_get_bits8(&gb, latm_frame_data, len)) < 0)
        return err;
    
    // check for LOAS sync word
    if (get_bits(&gb, 11) != L2A_LOAS_SYNC_WORD)
        return L2AERROR_INVAILD_DATA;
    
    muxlength = get_bits(&gb, 13) + 3;
    // not enough data, the parser should have sorted this out
    if (muxlength > len)
        return L2AERROR_INVAILD_DATA;
    
    if ((err = read_audio_mux_element(latmctx, &gb)) < 0)
        return err;
//
//    if (!latmctx->initialized) {
//        if (!avctx->extradata) {
//            *got_frame_ptr = 0;
//            return avpkt->size;
//        } else {
//            push_output_configuration(&latmctx->aac_ctx);
//            if ((err = decode_audio_specific_config(
//                                                    &latmctx->aac_ctx, avctx, &latmctx->aac_ctx.oc[1].m4ac,
//                                                    avctx->extradata, avctx->extradata_size*8LL, 1)) < 0) {
//                pop_output_configuration(&latmctx->aac_ctx);
//                return err;
//            }
//            latmctx->initialized = 1;
//        }
//    }
//    
    if (show_bits(&gb, 12) == 0xfff) {
        printf("ADTS header detected, probably as result of configuration "
               "misparsing\n");
        return L2AERROR_INVAILD_DATA;
    }
    
    switch (latmctx->m4ac.object_type) {
        case AOT_ER_AAC_LC:
        case AOT_ER_AAC_LTP:
        case AOT_ER_AAC_LD:
        case AOT_ER_AAC_ELD:
            //err = aac_decode_er_frame(avctx, out, got_frame_ptr, &gb);
            break;
        default:
            //err = aac_decode_frame_int(avctx, out, got_frame_ptr, &gb, avpkt);
            break;
    }
//    if (err < 0)
//        return err;
    
    return muxlength;
}


#pragma mark --- adts

#define MAX_PCE_SIZE 320 ///<Maximum size of a PCE including the 3-bit ID_PCE
///<marker and the comment
#define ADTS_HEADER_SIZE 7

typedef struct ADTSContext {
    int write_adts;
    int objecttype;
    int sample_rate_index;
    int channel_conf;
    int pce_size;
    int apetag;
    int id3v2tag;
    uint8_t pce_data[MAX_PCE_SIZE];
} ADTSContext;

#define ADTS_MAX_FRAME_BYTES ((1 << 13) - 1)

static int adts_get_profile_index_by_audio_object_type(enum AudioObjectType type){
    
    int index = 0;
    switch (type) {
        case AOT_AAC_MAIN:
            index = 0;
            break;
        case AOT_AAC_LC:
            index = 1;
            break;
        case AOT_AAC_SSR:
            index = 2;
            break;
        default:
            index = 3;
            break;
    }
    return index;
    
}

static int adts_write_frame_header(ADTSContext *ctx,
                                   uint8_t *buf, int pce_size,uint8_t **adts_frame_data,int *adts_frame_size)
{
    PutBitContext pb;
    
    unsigned full_frame_size = (unsigned)ADTS_HEADER_SIZE + pce_size;
    if (full_frame_size > ADTS_MAX_FRAME_BYTES) {
        printf("ADTS frame size too large: %u (max %d)\n",
               full_frame_size, ADTS_MAX_FRAME_BYTES);
        return L2AERROR_INVAILD_DATA;
    }
    *adts_frame_data = (uint8_t *)malloc(full_frame_size);
    if(!*adts_frame_data){
        return L2AERROR_ENOMEM;
    }
    *adts_frame_size = full_frame_size;
    
    
    uint8_t header[ADTS_HEADER_SIZE];
    init_put_bits(&pb, header, ADTS_HEADER_SIZE);
    
    int objecttype = ctx->objecttype;
    int sample_rate_index = ctx->sample_rate_index;
    int channel_conf = ctx->channel_conf;
    
    /* adts_fixed_header */
    put_bits(&pb, 12, 0xfff);   /* syncword */
    put_bits(&pb, 1, 0);        /* ID */
    put_bits(&pb, 2, 0);        /* layer */
    put_bits(&pb, 1, 1);        /* protection_absent */
    put_bits(&pb, 2, objecttype); /* profile_objecttype */
    put_bits(&pb, 4, sample_rate_index);
    put_bits(&pb, 1, 0);        /* private_bit */
    put_bits(&pb, 3, channel_conf); /* channel_configuration */
    put_bits(&pb, 1, 0);        /* original_copy */
    put_bits(&pb, 1, 0);        /* home */
    
    /* adts_variable_header */
    put_bits(&pb, 1, 0);        /* copyright_identification_bit */
    put_bits(&pb, 1, 0);        /* copyright_identification_start */
    put_bits(&pb, 13, full_frame_size); /* aac_frame_length */
    put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
    put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */
    
    flush_put_bits(&pb);
    

    memcpy(*adts_frame_data, header, ADTS_HEADER_SIZE);
    memcpy(*adts_frame_data + ADTS_HEADER_SIZE, buf, pce_size);
    
    return 0;
}

#pragma mark ---- latm adts

int func_latm2adts(uint8_t *latm_frame_data,int latm_frame_size,uint8_t **adts_frame_data,int *adts_frame_size,AudioSpecificConfig *asc)
{
    if(!latm_frame_data || latm_frame_size <= 0 || !adts_frame_data || !adts_frame_size){
        return L2AERROR_INVAILD_PARA;
    }
    *adts_frame_data = NULL;
    *adts_frame_size = 0;

    struct LATMContext latmctx1 = {0};
    struct LATMContext *latmctx = &latmctx1;
    ADTSContext ctx1 = {0};
    ADTSContext *ctx = &ctx1;
    
    int ret =  latm_decode_frame(latmctx,latm_frame_data, latm_frame_size);
    
    while(ret > 0){
        
        if(!latmctx->use_same_stream_mux){
            ctx->objecttype = adts_get_profile_index_by_audio_object_type(latmctx->m4ac.object_type);
            ctx->sample_rate_index = latmctx->m4ac.sampling_index;
            ctx->channel_conf = latmctx->m4ac.chan_config;
        }else{
            if(!asc){
                ret = L2AERROR_INVAILD_PARA;
                break;
            }
            if(asc->sampling_frequency < 0 || asc->sampling_frequency > 15){
                printf("asc->sampling_frequency invaild %d\n",asc->sampling_frequency);
                ret = L2AERROR_INVAILD_PARA;
                break;
            }
            if(asc->channel_configuration < 0 || asc->channel_configuration > 8){
                printf("asc->channel_configuration invaild %d\n",asc->channel_configuration);
                ret = L2AERROR_INVAILD_PARA;
                break;
            }
            if(asc->audio_object_type < 0 || asc->audio_object_type > 3){
                printf("asc->audio_object_type invaild %d\n",asc->audio_object_type);
                ret = L2AERROR_INVAILD_PARA;
                break;
            }
            
            ctx->objecttype = asc->audio_object_type;
            ctx->sample_rate_index = asc->sampling_frequency;
            ctx->channel_conf = asc->channel_configuration;
            
        }
        if(!latmctx->of.frame_data || latmctx->of.frame_size <= 0){
            ret = L2AERROR_INVAILD_DATA;
            break;
        }
        
      ret = adts_write_frame_header(ctx, latmctx->of.frame_data, latmctx->of.frame_size,adts_frame_data,adts_frame_size);
        break;
    }
    
    if(latmctx->of.frame_data){
        free(latmctx->of.frame_data);
    }
    return ret >= 0 ? L2A_SUCCESS : ret;
}

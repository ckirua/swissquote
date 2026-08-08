/* Generated SBE (Simple Binary Encoding) message codec */

#ifndef _SWISSQUOTE_BBO_BBOSNAPSHOT_H_
#define _SWISSQUOTE_BBO_BBOSNAPSHOT_H_

#include <errno.h>
#if !defined(__STDC_LIMIT_MACROS)
#define __STDC_LIMIT_MACROS 1
#endif
#include <limits.h>
#define SBE_FLOAT_NAN NAN
#define SBE_DOUBLE_NAN NAN
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "messageHeader.h"
#include "groupSizeEncoding.h"
#include "varAsciiEncoding.h"

#ifdef __cplusplus
#define SBE_ONE_DEF inline
#else
#define SBE_ONE_DEF static inline
#endif

/*
 * Define some byte ordering macros
 */
#if defined(WIN32) || defined(_WIN32)
    #define SBE_BIG_ENDIAN_ENCODE_16(v) _byteswap_ushort(v)
    #define SBE_BIG_ENDIAN_ENCODE_32(v) _byteswap_ulong(v)
    #define SBE_BIG_ENDIAN_ENCODE_64(v) _byteswap_uint64(v)
    #define SBE_LITTLE_ENDIAN_ENCODE_16(v) (v)
    #define SBE_LITTLE_ENDIAN_ENCODE_32(v) (v)
    #define SBE_LITTLE_ENDIAN_ENCODE_64(v) (v)
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SBE_BIG_ENDIAN_ENCODE_16(v) __builtin_bswap16(v)
    #define SBE_BIG_ENDIAN_ENCODE_32(v) __builtin_bswap32(v)
    #define SBE_BIG_ENDIAN_ENCODE_64(v) __builtin_bswap64(v)
    #define SBE_LITTLE_ENDIAN_ENCODE_16(v) (v)
    #define SBE_BIG_ENDIAN_ENCODE_32(v) __builtin_bswap32(v)
    #define SBE_BIG_ENDIAN_ENCODE_64(v) __builtin_bswap64(v)
    #define SBE_LITTLE_ENDIAN_ENCODE_16(v) (v)
    #define SBE_LITTLE_ENDIAN_ENCODE_32(v) (v)
    #define SBE_LITTLE_ENDIAN_ENCODE_64(v) (v)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define SBE_LITTLE_ENDIAN_ENCODE_16(v) __builtin_bswap16(v)
    #define SBE_LITTLE_ENDIAN_ENCODE_32(v) __builtin_bswap32(v)
    #define SBE_LITTLE_ENDIAN_ENCODE_64(v) __builtin_bswap64(v)
    #define SBE_BIG_ENDIAN_ENCODE_16(v) (v)
    #define SBE_BIG_ENDIAN_ENCODE_32(v) (v)
    #define SBE_BIG_ENDIAN_ENCODE_64(v) (v)
#else
    #error "Byte Ordering of platform not determined. Set __BYTE_ORDER__ manually before including this file."
#endif

#if !defined(SBE_BOUNDS_CHECK_EXPECT)
#  if defined(SBE_NO_BOUNDS_CHECK)
#    define SBE_BOUNDS_CHECK_EXPECT(exp, c) (false)
#  elif defined(_MSC_VER)
#    define SBE_BOUNDS_CHECK_EXPECT(exp, c) (exp)
#  else 
#    define SBE_BOUNDS_CHECK_EXPECT(exp, c) (__builtin_expect(exp, c))
#  endif

#endif

#define SBE_NULLVALUE_INT8 INT8_MIN
#define SBE_NULLVALUE_INT16 INT16_MIN
#define SBE_NULLVALUE_INT32 INT32_MIN
#define SBE_NULLVALUE_INT64 INT64_MIN
#define SBE_NULLVALUE_UINT8 UINT8_MAX
#define SBE_NULLVALUE_UINT16 UINT16_MAX
#define SBE_NULLVALUE_UINT32 UINT32_MAX
#define SBE_NULLVALUE_UINT64 UINT64_MAX

#define E100 -50100 // E_BUF_SHORT
#define E103 -50103 // VAL_UNKNOWN_ENUM
#define E104 -50104 // I_OUT_RANGE_NUM
#define E105 -50105 // I_OUT_RANGE_NUM
#define E106 -50106 // I_OUT_RANGE_NUM
#define E107 -50107 // BUF_SHORT_FLYWEIGHT
#define E108 -50108 // BUF_SHORT_NXT_GRP_IND
#define E109 -50109 // STR_TOO_LONG_FOR_LEN_TYP
#define E110 -50110 // CNT_OUT_RANGE

#ifndef SBE_STRERROR_DEFINED
#define SBE_STRERROR_DEFINED
SBE_ONE_DEF const char *sbe_strerror(const int errnum)
{
    switch (errnum)
    {
        case E100:
            return "buffer too short";
        case E103:
            return "unknown value for enum";
        case E104:
            return "index out of range";
        case E105:
            return "index out of range";
        case E106:
            return "length too large";
        case E107:
            return "buffer too short for flyweight";
        case E108:
            return "buffer too short to support next group index";
        case E109:
            return "std::string too long for length type";
        case E110:
            return "count outside of allowed range";
        default:
            return "unknown error";
    }
}
#endif

struct swissquote_bbo_bboSnapshot
{
    char *buffer;
    uint64_t buffer_length;
    uint64_t offset;
    uint64_t position;
    uint64_t acting_block_length;
    uint64_t acting_version;
};

enum swissquote_bbo_bboSnapshot_meta_attribute
{
    swissquote_bbo_bboSnapshot_meta_attribute_EPOCH,
    swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT,
    swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE,
    swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE
};

union swissquote_bbo_bboSnapshot_float_as_uint
{
    float fp_value;
    uint32_t uint_value;
};

union swissquote_bbo_bboSnapshot_double_as_uint
{
    double fp_value;
    uint64_t uint_value;
};

struct swissquote_bbo_bboSnapshot_string_view
{
    const char* data;
    size_t length;
};

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_sbe_position(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
    return codec->position;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_set_sbe_position(
    struct swissquote_bbo_bboSnapshot *const codec,
    const uint64_t position)
{
    if (SBE_BOUNDS_CHECK_EXPECT((position > codec->buffer_length), false))
    {
        errno = E100;
        return false;
    }
    codec->position = position;

    return true;
}

SBE_ONE_DEF uint64_t *swissquote_bbo_bboSnapshot_sbe_position_ptr(
    struct swissquote_bbo_bboSnapshot *const codec)
{
    return &codec->position;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot *swissquote_bbo_bboSnapshot_reset(
    struct swissquote_bbo_bboSnapshot *const codec,
    char *buffer,
    const uint64_t offset,
    const uint64_t buffer_length,
    const uint64_t acting_block_length,
    const uint64_t acting_version)
{
    codec->buffer = buffer;
    codec->offset = offset;
    codec->buffer_length = buffer_length;
    codec->acting_block_length = acting_block_length;
    codec->acting_version = acting_version;
    if (!swissquote_bbo_bboSnapshot_set_sbe_position(codec, offset + acting_block_length))
    {
        return NULL;
    }

    return codec;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot *swissquote_bbo_bboSnapshot_copy(
    struct swissquote_bbo_bboSnapshot *const codec,
    const struct swissquote_bbo_bboSnapshot *const other)
{
     codec->buffer = other->buffer;
     codec->offset = other->offset;
     codec->buffer_length = other->buffer_length;
     codec->acting_block_length = other->acting_block_length;
     codec->acting_version = other->acting_version;
     codec->position = other->position;

     return codec;
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_sbe_block_length(void)
{
    return (uint16_t)12;
}

#define SWISSQUOTE_BBO_BBO_SNAPSHOT_SBE_TEMPLATE_ID (uint16_t)1

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_sbe_template_id(void)
{
    return (uint16_t)1;
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_sbe_schema_id(void)
{
    return (uint16_t)1;
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_sbe_schema_version(void)
{
    return (uint16_t)1;
}

SBE_ONE_DEF const char* swissquote_bbo_bboSnapshot_sbe_semantic_version(void)
{
    return "1.0.0";
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_sbe_semantic_type(void)
{
    return "";
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_offset(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
    return codec->offset;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot *swissquote_bbo_bboSnapshot_wrap_and_apply_header(
    struct swissquote_bbo_bboSnapshot *const codec,
    char *buffer,
    const uint64_t offset,
    const uint64_t buffer_length,
    struct swissquote_bbo_messageHeader *const hdr)
{
    swissquote_bbo_messageHeader_wrap(
        hdr, buffer + offset, 0, swissquote_bbo_messageHeader_sbe_schema_version(), buffer_length);

    swissquote_bbo_messageHeader_set_blockLength(hdr, swissquote_bbo_bboSnapshot_sbe_block_length());
    swissquote_bbo_messageHeader_set_templateId(hdr, swissquote_bbo_bboSnapshot_sbe_template_id());
    swissquote_bbo_messageHeader_set_schemaId(hdr, swissquote_bbo_bboSnapshot_sbe_schema_id());
    swissquote_bbo_messageHeader_set_version(hdr, swissquote_bbo_bboSnapshot_sbe_schema_version());

    swissquote_bbo_bboSnapshot_reset(
        codec,
        buffer + offset + swissquote_bbo_messageHeader_encoded_length(),
        0,
        buffer_length - swissquote_bbo_messageHeader_encoded_length(),
        swissquote_bbo_bboSnapshot_sbe_block_length(),
        swissquote_bbo_bboSnapshot_sbe_schema_version());

    return codec;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot *swissquote_bbo_bboSnapshot_wrap_for_encode(
    struct swissquote_bbo_bboSnapshot *const codec,
    char *buffer,
    const uint64_t offset,
    const uint64_t buffer_length)
{
    return swissquote_bbo_bboSnapshot_reset(
        codec,
        buffer,
        offset,
        buffer_length,
        swissquote_bbo_bboSnapshot_sbe_block_length(),
        swissquote_bbo_bboSnapshot_sbe_schema_version());
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot *swissquote_bbo_bboSnapshot_wrap_for_decode(
    struct swissquote_bbo_bboSnapshot *const codec,
    char *buffer,
    const uint64_t offset,
    const uint64_t acting_block_length,
    const uint64_t acting_version,
    const uint64_t buffer_length)
{
    return swissquote_bbo_bboSnapshot_reset(
        codec,
        buffer,
        offset,
        buffer_length,
        acting_block_length,
        acting_version);
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot *swissquote_bbo_bboSnapshot_sbe_rewind(
    struct swissquote_bbo_bboSnapshot *const codec)
{
    return swissquote_bbo_bboSnapshot_wrap_for_decode(
        codec,
        codec->buffer,
        codec->offset,
        codec->acting_block_length,
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_encoded_length(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
    return swissquote_bbo_bboSnapshot_sbe_position(codec) - codec->offset;
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_buffer(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
    return codec->buffer;
}

SBE_ONE_DEF char *swissquote_bbo_bboSnapshot_mut_buffer(
    struct swissquote_bbo_bboSnapshot *const codec)
{
    return codec->buffer;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_buffer_length(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
    return codec->buffer_length;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_acting_version(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
    return codec->acting_version;
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_symbol_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_symbol_id(void)
{
    return 1;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_symbol_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_symbol_in_acting_version(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_symbol_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_symbol_encoding_offset(void)
{
    return 0;
}

SBE_ONE_DEF struct swissquote_bbo_varAsciiEncoding *swissquote_bbo_bboSnapshot_symbol(
    struct swissquote_bbo_bboSnapshot *const codec,
    struct swissquote_bbo_varAsciiEncoding *const composite)
{
    return swissquote_bbo_varAsciiEncoding_wrap(
        composite,
        codec->buffer,
        codec->offset + 0,
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_fetchTimeMs_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_fetchTimeMs_id(void)
{
    return 2;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_fetchTimeMs_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_fetchTimeMs_in_acting_version(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_fetchTimeMs_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_fetchTimeMs_encoding_offset(void)
{
    return 4;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_fetchTimeMs_null_value(void)
{
    return SBE_NULLVALUE_UINT64;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_fetchTimeMs_min_value(void)
{
    return UINT64_C(0x0);
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_fetchTimeMs_max_value(void)
{
    return UINT64_C(0xfffffffffffffffe);
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_fetchTimeMs_encoding_length(void)
{
    return 8;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_fetchTimeMs(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
    uint64_t val;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    memcpy(&val, codec->buffer + codec->offset + 4, sizeof(uint64_t));
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    return SBE_LITTLE_ENDIAN_ENCODE_64(val);
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot *swissquote_bbo_bboSnapshot_set_fetchTimeMs(
    struct swissquote_bbo_bboSnapshot *const codec,
    const uint64_t value)
{
    uint64_t val = SBE_LITTLE_ENDIAN_ENCODE_64(value);
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    memcpy(codec->buffer + codec->offset + 4, &val, sizeof(uint64_t));
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    return codec;
}

struct swissquote_bbo_bboSnapshot_venues
{
    char *buffer;
    uint64_t buffer_length;
    uint64_t *position_ptr;
    uint64_t block_length;
    uint64_t count;
    uint64_t index;
    uint64_t offset;
    uint64_t acting_version;
};

SBE_ONE_DEF uint64_t *swissquote_bbo_bboSnapshot_venues_sbe_position_ptr(
    struct swissquote_bbo_bboSnapshot_venues *const codec)
{
    return codec->position_ptr;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues *swissquote_bbo_bboSnapshot_venues_wrap_for_decode(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    char *const buffer,
    uint64_t *const pos,
    const uint64_t acting_version,
    const uint64_t buffer_length)
{
    codec->buffer = buffer;
    codec->buffer_length = buffer_length;
    struct swissquote_bbo_groupSizeEncoding dimensions;
    if (!swissquote_bbo_groupSizeEncoding_wrap(&dimensions, codec->buffer, *pos, acting_version, buffer_length))
    {
        return NULL;
    }

    codec->block_length = swissquote_bbo_groupSizeEncoding_blockLength(&dimensions);
    codec->count = swissquote_bbo_groupSizeEncoding_numInGroup(&dimensions);
    codec->index = -1;
    codec->acting_version = acting_version;
    codec->position_ptr = pos;
    *codec->position_ptr = *codec->position_ptr + 4;

    return codec;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues *swissquote_bbo_bboSnapshot_venues_wrap_for_encode(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    char *const buffer,
    const uint16_t count,
    uint64_t *const pos,
    const uint64_t acting_version,
    const uint64_t buffer_length)
{
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
    if (count > 65534)
    {
        errno = E110;
        return NULL;
    }
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    codec->buffer = buffer;
    codec->buffer_length = buffer_length;
    struct swissquote_bbo_groupSizeEncoding dimensions;
    if (!swissquote_bbo_groupSizeEncoding_wrap(&dimensions, codec->buffer, *pos, acting_version, buffer_length))
    {
        return NULL;
    }

    swissquote_bbo_groupSizeEncoding_set_blockLength(&dimensions, (uint16_t)16);
    swissquote_bbo_groupSizeEncoding_set_numInGroup(&dimensions, (uint16_t)count);
    codec->index = -1;
    codec->count = count;
    codec->block_length = 16;
    codec->acting_version = acting_version;
    codec->position_ptr = pos;
    *codec->position_ptr = *codec->position_ptr + 4;

    return codec;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_sbe_header_size(void)
{
    return 4;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_sbe_block_length(void)
{
    return 16;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_sbe_position(
    const struct swissquote_bbo_bboSnapshot_venues *const codec)
{
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    return *codec->position_ptr;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_set_sbe_position(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    const uint64_t position)
{
    if (SBE_BOUNDS_CHECK_EXPECT((position > codec->buffer_length), false))
    {
       errno = E100;
       return false;
    }
    *codec->position_ptr = position;

    return true;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_count(
    const struct swissquote_bbo_bboSnapshot_venues *const codec)
{
    return codec->count;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_has_next(
    const struct swissquote_bbo_bboSnapshot_venues *const codec)
{
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    return codec->index + 1 < codec->count;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues *swissquote_bbo_bboSnapshot_venues_next(
    struct swissquote_bbo_bboSnapshot_venues *const codec)
{
    codec->offset = *codec->position_ptr;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    if (SBE_BOUNDS_CHECK_EXPECT(((codec->offset + codec->block_length) > codec->buffer_length), false))
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    {
        errno = E108;
        return NULL;
    }
    *codec->position_ptr = codec->offset + codec->block_length;
    ++codec->index;

    return codec;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues *swissquote_bbo_bboSnapshot_venues_for_each(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    void (*func)(struct swissquote_bbo_bboSnapshot_venues *, void *),
    void *const context)
{
    while (swissquote_bbo_bboSnapshot_venues_has_next(codec))
    {
        if (!swissquote_bbo_bboSnapshot_venues_next(codec))
        {
            return NULL;
        }
        func(codec, context);
    }

    return codec;
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_venues_platform_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_platform_id(void)
{
    return 4;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_platform_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_platform_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_platform_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_platform_encoding_offset(void)
{
    return 0;
}

SBE_ONE_DEF struct swissquote_bbo_varAsciiEncoding *swissquote_bbo_bboSnapshot_venues_platform(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    struct swissquote_bbo_varAsciiEncoding *const composite)
{
    return swissquote_bbo_varAsciiEncoding_wrap(
        composite,
        codec->buffer,
        codec->offset + 0,
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_venues_server_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_server_id(void)
{
    return 5;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_server_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_server_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_server_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_server_encoding_offset(void)
{
    return 4;
}

SBE_ONE_DEF struct swissquote_bbo_varAsciiEncoding *swissquote_bbo_bboSnapshot_venues_server(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    struct swissquote_bbo_varAsciiEncoding *const composite)
{
    return swissquote_bbo_varAsciiEncoding_wrap(
        composite,
        codec->buffer,
        codec->offset + 4,
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_venues_venueTsMs_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_venueTsMs_id(void)
{
    return 6;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_venueTsMs_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_venueTsMs_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_venueTsMs_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_venueTsMs_encoding_offset(void)
{
    return 8;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_venueTsMs_null_value(void)
{
    return SBE_NULLVALUE_UINT64;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_venueTsMs_min_value(void)
{
    return UINT64_C(0x0);
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_venueTsMs_max_value(void)
{
    return UINT64_C(0xfffffffffffffffe);
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_venueTsMs_encoding_length(void)
{
    return 8;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_venueTsMs(
    const struct swissquote_bbo_bboSnapshot_venues *const codec)
{
    uint64_t val;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    memcpy(&val, codec->buffer + codec->offset + 8, sizeof(uint64_t));
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    return SBE_LITTLE_ENDIAN_ENCODE_64(val);
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues *swissquote_bbo_bboSnapshot_venues_set_venueTsMs(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    const uint64_t value)
{
    uint64_t val = SBE_LITTLE_ENDIAN_ENCODE_64(value);
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    memcpy(codec->buffer + codec->offset + 8, &val, sizeof(uint64_t));
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    return codec;
}

struct swissquote_bbo_bboSnapshot_venues_profiles
{
    char *buffer;
    uint64_t buffer_length;
    uint64_t *position_ptr;
    uint64_t block_length;
    uint64_t count;
    uint64_t index;
    uint64_t offset;
    uint64_t acting_version;
};

SBE_ONE_DEF uint64_t *swissquote_bbo_bboSnapshot_venues_profiles_sbe_position_ptr(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
    return codec->position_ptr;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_wrap_for_decode(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    char *const buffer,
    uint64_t *const pos,
    const uint64_t acting_version,
    const uint64_t buffer_length)
{
    codec->buffer = buffer;
    codec->buffer_length = buffer_length;
    struct swissquote_bbo_groupSizeEncoding dimensions;
    if (!swissquote_bbo_groupSizeEncoding_wrap(&dimensions, codec->buffer, *pos, acting_version, buffer_length))
    {
        return NULL;
    }

    codec->block_length = swissquote_bbo_groupSizeEncoding_blockLength(&dimensions);
    codec->count = swissquote_bbo_groupSizeEncoding_numInGroup(&dimensions);
    codec->index = -1;
    codec->acting_version = acting_version;
    codec->position_ptr = pos;
    *codec->position_ptr = *codec->position_ptr + 4;

    return codec;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_wrap_for_encode(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    char *const buffer,
    const uint16_t count,
    uint64_t *const pos,
    const uint64_t acting_version,
    const uint64_t buffer_length)
{
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
    if (count > 65534)
    {
        errno = E110;
        return NULL;
    }
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    codec->buffer = buffer;
    codec->buffer_length = buffer_length;
    struct swissquote_bbo_groupSizeEncoding dimensions;
    if (!swissquote_bbo_groupSizeEncoding_wrap(&dimensions, codec->buffer, *pos, acting_version, buffer_length))
    {
        return NULL;
    }

    swissquote_bbo_groupSizeEncoding_set_blockLength(&dimensions, (uint16_t)28);
    swissquote_bbo_groupSizeEncoding_set_numInGroup(&dimensions, (uint16_t)count);
    codec->index = -1;
    codec->count = count;
    codec->block_length = 28;
    codec->acting_version = acting_version;
    codec->position_ptr = pos;
    *codec->position_ptr = *codec->position_ptr + 4;

    return codec;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_sbe_header_size(void)
{
    return 4;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_sbe_block_length(void)
{
    return 28;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_sbe_position(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    return *codec->position_ptr;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_profiles_set_sbe_position(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    const uint64_t position)
{
    if (SBE_BOUNDS_CHECK_EXPECT((position > codec->buffer_length), false))
    {
       errno = E100;
       return false;
    }
    *codec->position_ptr = position;

    return true;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_count(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
    return codec->count;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_profiles_has_next(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    return codec->index + 1 < codec->count;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_next(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
    codec->offset = *codec->position_ptr;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    if (SBE_BOUNDS_CHECK_EXPECT(((codec->offset + codec->block_length) > codec->buffer_length), false))
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    {
        errno = E108;
        return NULL;
    }
    *codec->position_ptr = codec->offset + codec->block_length;
    ++codec->index;

    return codec;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_for_each(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    void (*func)(struct swissquote_bbo_bboSnapshot_venues_profiles *, void *),
    void *const context)
{
    while (swissquote_bbo_bboSnapshot_venues_profiles_has_next(codec))
    {
        if (!swissquote_bbo_bboSnapshot_venues_profiles_next(codec))
        {
            return NULL;
        }
        func(codec, context);
    }

    return codec;
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_venues_profiles_spreadProfile_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_profiles_spreadProfile_id(void)
{
    return 8;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_spreadProfile_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_profiles_spreadProfile_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_profiles_spreadProfile_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_spreadProfile_encoding_offset(void)
{
    return 0;
}

SBE_ONE_DEF struct swissquote_bbo_varAsciiEncoding *swissquote_bbo_bboSnapshot_venues_profiles_spreadProfile(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    struct swissquote_bbo_varAsciiEncoding *const composite)
{
    return swissquote_bbo_varAsciiEncoding_wrap(
        composite,
        codec->buffer,
        codec->offset + 0,
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_venues_profiles_bid_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_profiles_bid_id(void)
{
    return 9;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_bid_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_profiles_bid_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_profiles_bid_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_bid_encoding_offset(void)
{
    return 4;
}

SBE_ONE_DEF double swissquote_bbo_bboSnapshot_venues_profiles_bid_null_value(void)
{
    return SBE_DOUBLE_NAN;
}

SBE_ONE_DEF double swissquote_bbo_bboSnapshot_venues_profiles_bid_min_value(void)
{
    return -1.7976931348623157E308;
}

SBE_ONE_DEF double swissquote_bbo_bboSnapshot_venues_profiles_bid_max_value(void)
{
    return 1.7976931348623157E308;
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_bid_encoding_length(void)
{
    return 8;
}

SBE_ONE_DEF double swissquote_bbo_bboSnapshot_venues_profiles_bid(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
    union swissquote_bbo_bboSnapshot_double_as_uint val;
    memcpy(&val, codec->buffer + codec->offset + 4, sizeof(double));
    val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_64(val.uint_value);
    return val.fp_value;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_set_bid(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    const double value)
{
    union swissquote_bbo_bboSnapshot_double_as_uint val;
    val.fp_value = value;
    val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_64(val.uint_value);
    memcpy(codec->buffer + codec->offset + 4, &val, sizeof(double));
    return codec;
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_venues_profiles_ask_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_profiles_ask_id(void)
{
    return 10;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_ask_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_profiles_ask_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_profiles_ask_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_ask_encoding_offset(void)
{
    return 12;
}

SBE_ONE_DEF double swissquote_bbo_bboSnapshot_venues_profiles_ask_null_value(void)
{
    return SBE_DOUBLE_NAN;
}

SBE_ONE_DEF double swissquote_bbo_bboSnapshot_venues_profiles_ask_min_value(void)
{
    return -1.7976931348623157E308;
}

SBE_ONE_DEF double swissquote_bbo_bboSnapshot_venues_profiles_ask_max_value(void)
{
    return 1.7976931348623157E308;
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_ask_encoding_length(void)
{
    return 8;
}

SBE_ONE_DEF double swissquote_bbo_bboSnapshot_venues_profiles_ask(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
    union swissquote_bbo_bboSnapshot_double_as_uint val;
    memcpy(&val, codec->buffer + codec->offset + 12, sizeof(double));
    val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_64(val.uint_value);
    return val.fp_value;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_set_ask(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    const double value)
{
    union swissquote_bbo_bboSnapshot_double_as_uint val;
    val.fp_value = value;
    val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_64(val.uint_value);
    memcpy(codec->buffer + codec->offset + 12, &val, sizeof(double));
    return codec;
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_id(void)
{
    return 11;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_encoding_offset(void)
{
    return 20;
}

SBE_ONE_DEF float swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_null_value(void)
{
    return SBE_FLOAT_NAN;
}

SBE_ONE_DEF float swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_min_value(void)
{
    return -3.4028234663852886E38f;
}

SBE_ONE_DEF float swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_max_value(void)
{
    return 3.4028234663852886E38f;
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_bidSpread_encoding_length(void)
{
    return 4;
}

SBE_ONE_DEF float swissquote_bbo_bboSnapshot_venues_profiles_bidSpread(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
    union swissquote_bbo_bboSnapshot_float_as_uint val;
    memcpy(&val, codec->buffer + codec->offset + 20, sizeof(float));
    val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_32(val.uint_value);
    return val.fp_value;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_set_bidSpread(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    const float value)
{
    union swissquote_bbo_bboSnapshot_float_as_uint val;
    val.fp_value = value;
    val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_32(val.uint_value);
    memcpy(codec->buffer + codec->offset + 20, &val, sizeof(float));
    return codec;
}

SBE_ONE_DEF const char *swissquote_bbo_bboSnapshot_venues_profiles_askSpread_meta_attribute(
    const enum swissquote_bbo_bboSnapshot_meta_attribute attribute)
{
    switch (attribute)
    {
        case swissquote_bbo_bboSnapshot_meta_attribute_EPOCH: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_TIME_UNIT: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_SEMANTIC_TYPE: return "";
        case swissquote_bbo_bboSnapshot_meta_attribute_PRESENCE: return "required";
    }

    return "";
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_profiles_askSpread_id(void)
{
    return 12;
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_askSpread_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_profiles_askSpread_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_profiles_askSpread_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_askSpread_encoding_offset(void)
{
    return 24;
}

SBE_ONE_DEF float swissquote_bbo_bboSnapshot_venues_profiles_askSpread_null_value(void)
{
    return SBE_FLOAT_NAN;
}

SBE_ONE_DEF float swissquote_bbo_bboSnapshot_venues_profiles_askSpread_min_value(void)
{
    return -3.4028234663852886E38f;
}

SBE_ONE_DEF float swissquote_bbo_bboSnapshot_venues_profiles_askSpread_max_value(void)
{
    return 3.4028234663852886E38f;
}

SBE_ONE_DEF size_t swissquote_bbo_bboSnapshot_venues_profiles_askSpread_encoding_length(void)
{
    return 4;
}

SBE_ONE_DEF float swissquote_bbo_bboSnapshot_venues_profiles_askSpread(
    const struct swissquote_bbo_bboSnapshot_venues_profiles *const codec)
{
    union swissquote_bbo_bboSnapshot_float_as_uint val;
    memcpy(&val, codec->buffer + codec->offset + 24, sizeof(float));
    val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_32(val.uint_value);
    return val.fp_value;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_set_askSpread(
    struct swissquote_bbo_bboSnapshot_venues_profiles *const codec,
    const float value)
{
    union swissquote_bbo_bboSnapshot_float_as_uint val;
    val.fp_value = value;
    val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_32(val.uint_value);
    memcpy(codec->buffer + codec->offset + 24, &val, sizeof(float));
    return codec;
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_profiles_id(void)
{
    return 7;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_get_profiles(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    struct swissquote_bbo_bboSnapshot_venues_profiles *const property)
{
    return swissquote_bbo_bboSnapshot_venues_profiles_wrap_for_decode(
        property,
        codec->buffer,
        swissquote_bbo_bboSnapshot_venues_sbe_position_ptr(codec),
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues_profiles *swissquote_bbo_bboSnapshot_venues_profiles_set_count(
    struct swissquote_bbo_bboSnapshot_venues *const codec,
    struct swissquote_bbo_bboSnapshot_venues_profiles *const property,
    const uint16_t count)
{
    return swissquote_bbo_bboSnapshot_venues_profiles_wrap_for_encode(
        property,
        codec->buffer,
        count,
        swissquote_bbo_bboSnapshot_venues_sbe_position_ptr(codec),
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_profiles_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_profiles_in_acting_version(
    const struct swissquote_bbo_bboSnapshot_venues *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_profiles_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

SBE_ONE_DEF uint16_t swissquote_bbo_bboSnapshot_venues_id(void)
{
    return 3;
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues *swissquote_bbo_bboSnapshot_get_venues(
    struct swissquote_bbo_bboSnapshot *const codec,
    struct swissquote_bbo_bboSnapshot_venues *const property)
{
    return swissquote_bbo_bboSnapshot_venues_wrap_for_decode(
        property,
        codec->buffer,
        swissquote_bbo_bboSnapshot_sbe_position_ptr(codec),
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF struct swissquote_bbo_bboSnapshot_venues *swissquote_bbo_bboSnapshot_venues_set_count(
    struct swissquote_bbo_bboSnapshot *const codec,
    struct swissquote_bbo_bboSnapshot_venues *const property,
    const uint16_t count)
{
    return swissquote_bbo_bboSnapshot_venues_wrap_for_encode(
        property,
        codec->buffer,
        count,
        swissquote_bbo_bboSnapshot_sbe_position_ptr(codec),
        codec->acting_version,
        codec->buffer_length);
}

SBE_ONE_DEF uint64_t swissquote_bbo_bboSnapshot_venues_since_version(void)
{
    return 0;
}

SBE_ONE_DEF bool swissquote_bbo_bboSnapshot_venues_in_acting_version(
    const struct swissquote_bbo_bboSnapshot *const codec)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
    return codec->acting_version >= swissquote_bbo_bboSnapshot_venues_since_version();
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

#endif

/* Generated SBE (Simple Binary Encoding) message codec */
#ifndef _SWISSQUOTE_BBO_BBOSNAPSHOT_CXX_H_
#define _SWISSQUOTE_BBO_BBOSNAPSHOT_CXX_H_

#if __cplusplus >= 201103L
#  define SBE_CONSTEXPR constexpr
#  define SBE_NOEXCEPT noexcept
#else
#  define SBE_CONSTEXPR
#  define SBE_NOEXCEPT
#endif

#if __cplusplus >= 201703L
#  include <string_view>
#  define SBE_NODISCARD [[nodiscard]]
#else
#  define SBE_NODISCARD
#endif

#if !defined(__STDC_LIMIT_MACROS)
#  define __STDC_LIMIT_MACROS 1
#endif

#include <cstdint>
#include <limits>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>

#if defined(WIN32) || defined(_WIN32)
#  define SBE_BIG_ENDIAN_ENCODE_16(v) _byteswap_ushort(v)
#  define SBE_BIG_ENDIAN_ENCODE_32(v) _byteswap_ulong(v)
#  define SBE_BIG_ENDIAN_ENCODE_64(v) _byteswap_uint64(v)
#  define SBE_LITTLE_ENDIAN_ENCODE_16(v) (v)
#  define SBE_LITTLE_ENDIAN_ENCODE_32(v) (v)
#  define SBE_LITTLE_ENDIAN_ENCODE_64(v) (v)
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define SBE_BIG_ENDIAN_ENCODE_16(v) __builtin_bswap16(v)
#  define SBE_BIG_ENDIAN_ENCODE_32(v) __builtin_bswap32(v)
#  define SBE_BIG_ENDIAN_ENCODE_64(v) __builtin_bswap64(v)
#  define SBE_LITTLE_ENDIAN_ENCODE_16(v) (v)
#  define SBE_LITTLE_ENDIAN_ENCODE_32(v) (v)
#  define SBE_LITTLE_ENDIAN_ENCODE_64(v) (v)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define SBE_LITTLE_ENDIAN_ENCODE_16(v) __builtin_bswap16(v)
#  define SBE_LITTLE_ENDIAN_ENCODE_32(v) __builtin_bswap32(v)
#  define SBE_LITTLE_ENDIAN_ENCODE_64(v) __builtin_bswap64(v)
#  define SBE_BIG_ENDIAN_ENCODE_16(v) (v)
#  define SBE_BIG_ENDIAN_ENCODE_32(v) (v)
#  define SBE_BIG_ENDIAN_ENCODE_64(v) (v)
#else
#  error "Byte Ordering of platform not determined. Set __BYTE_ORDER__ manually before including this file."
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

#define SBE_FLOAT_NAN std::numeric_limits<float>::quiet_NaN()
#define SBE_DOUBLE_NAN std::numeric_limits<double>::quiet_NaN()
#define SBE_NULLVALUE_INT8 (std::numeric_limits<std::int8_t>::min)()
#define SBE_NULLVALUE_INT16 (std::numeric_limits<std::int16_t>::min)()
#define SBE_NULLVALUE_INT32 (std::numeric_limits<std::int32_t>::min)()
#define SBE_NULLVALUE_INT64 (std::numeric_limits<std::int64_t>::min)()
#define SBE_NULLVALUE_UINT8 (std::numeric_limits<std::uint8_t>::max)()
#define SBE_NULLVALUE_UINT16 (std::numeric_limits<std::uint16_t>::max)()
#define SBE_NULLVALUE_UINT32 (std::numeric_limits<std::uint32_t>::max)()
#define SBE_NULLVALUE_UINT64 (std::numeric_limits<std::uint64_t>::max)()


#include "MessageHeader.h"
#include "GroupSizeEncoding.h"
#include "VarAsciiEncoding.h"

namespace swissquote {
namespace bbo {

class BboSnapshot
{
private:
    char *m_buffer = nullptr;
    std::uint64_t m_bufferLength = 0;
    std::uint64_t m_offset = 0;
    std::uint64_t m_position = 0;
    std::uint64_t m_actingBlockLength = 0;
    std::uint64_t m_actingVersion = 0;

    inline std::uint64_t *sbePositionPtr() SBE_NOEXCEPT
    {
        return &m_position;
    }

public:
    static constexpr std::uint16_t SBE_BLOCK_LENGTH = static_cast<std::uint16_t>(12);
    static constexpr std::uint16_t SBE_TEMPLATE_ID = static_cast<std::uint16_t>(1);
    static constexpr std::uint16_t SBE_SCHEMA_ID = static_cast<std::uint16_t>(1);
    static constexpr std::uint16_t SBE_SCHEMA_VERSION = static_cast<std::uint16_t>(1);
    static constexpr const char* SBE_SEMANTIC_VERSION = "1.0.0";

    enum MetaAttribute
    {
        EPOCH, TIME_UNIT, SEMANTIC_TYPE, PRESENCE
    };

    union sbe_float_as_uint_u
    {
        float fp_value;
        std::uint32_t uint_value;
    };

    union sbe_double_as_uint_u
    {
        double fp_value;
        std::uint64_t uint_value;
    };

    using messageHeader = MessageHeader;

    BboSnapshot() = default;

    BboSnapshot(
        char *buffer,
        const std::uint64_t offset,
        const std::uint64_t bufferLength,
        const std::uint64_t actingBlockLength,
        const std::uint64_t actingVersion) :
        m_buffer(buffer),
        m_bufferLength(bufferLength),
        m_offset(offset),
        m_position(sbeCheckPosition(offset + actingBlockLength)),
        m_actingBlockLength(actingBlockLength),
        m_actingVersion(actingVersion)
    {
    }

    BboSnapshot(char *buffer, const std::uint64_t bufferLength) :
        BboSnapshot(buffer, 0, bufferLength, sbeBlockLength(), sbeSchemaVersion())
    {
    }

    BboSnapshot(
        char *buffer,
        const std::uint64_t bufferLength,
        const std::uint64_t actingBlockLength,
        const std::uint64_t actingVersion) :
        BboSnapshot(buffer, 0, bufferLength, actingBlockLength, actingVersion)
    {
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::uint16_t sbeBlockLength() SBE_NOEXCEPT
    {
        return static_cast<std::uint16_t>(12);
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t sbeBlockAndHeaderLength() SBE_NOEXCEPT
    {
        return messageHeader::encodedLength() + sbeBlockLength();
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::uint16_t sbeTemplateId() SBE_NOEXCEPT
    {
        return static_cast<std::uint16_t>(1);
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::uint16_t sbeSchemaId() SBE_NOEXCEPT
    {
        return static_cast<std::uint16_t>(1);
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::uint16_t sbeSchemaVersion() SBE_NOEXCEPT
    {
        return static_cast<std::uint16_t>(1);
    }

    SBE_NODISCARD static const char *sbeSemanticVersion() SBE_NOEXCEPT
    {
        return "1.0.0";
    }

    SBE_NODISCARD static SBE_CONSTEXPR const char *sbeSemanticType() SBE_NOEXCEPT
    {
        return "";
    }

    SBE_NODISCARD std::uint64_t offset() const SBE_NOEXCEPT
    {
        return m_offset;
    }

    BboSnapshot &wrapForEncode(char *buffer, const std::uint64_t offset, const std::uint64_t bufferLength)
    {
        m_buffer = buffer;
        m_bufferLength = bufferLength;
        m_offset = offset;
        m_actingBlockLength = sbeBlockLength();
        m_actingVersion = sbeSchemaVersion();
        m_position = sbeCheckPosition(m_offset + m_actingBlockLength);
        return *this;
    }

    BboSnapshot &wrapAndApplyHeader(char *buffer, const std::uint64_t offset, const std::uint64_t bufferLength)
    {
        messageHeader hdr(buffer, offset, bufferLength, sbeSchemaVersion());

        hdr
            .blockLength(sbeBlockLength())
            .templateId(sbeTemplateId())
            .schemaId(sbeSchemaId())
            .version(sbeSchemaVersion());

        m_buffer = buffer;
        m_bufferLength = bufferLength;
        m_offset = offset + messageHeader::encodedLength();
        m_actingBlockLength = sbeBlockLength();
        m_actingVersion = sbeSchemaVersion();
        m_position = sbeCheckPosition(m_offset + m_actingBlockLength);
        return *this;
    }

    BboSnapshot &wrapForDecode(
        char *buffer,
        const std::uint64_t offset,
        const std::uint64_t actingBlockLength,
        const std::uint64_t actingVersion,
        const std::uint64_t bufferLength)
    {
        m_buffer = buffer;
        m_bufferLength = bufferLength;
        m_offset = offset;
        m_actingBlockLength = actingBlockLength;
        m_actingVersion = actingVersion;
        m_position = sbeCheckPosition(m_offset + m_actingBlockLength);
        return *this;
    }

    BboSnapshot &sbeRewind()
    {
        return wrapForDecode(m_buffer, m_offset, m_actingBlockLength, m_actingVersion, m_bufferLength);
    }

    SBE_NODISCARD std::uint64_t sbePosition() const SBE_NOEXCEPT
    {
        return m_position;
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    std::uint64_t sbeCheckPosition(const std::uint64_t position)
    {
        if (SBE_BOUNDS_CHECK_EXPECT((position > m_bufferLength), false))
        {
            throw std::runtime_error("buffer too short [E100]");
        }
        return position;
    }

    void sbePosition(const std::uint64_t position)
    {
        m_position = sbeCheckPosition(position);
    }

    SBE_NODISCARD std::uint64_t encodedLength() const SBE_NOEXCEPT
    {
        return sbePosition() - m_offset;
    }

    SBE_NODISCARD std::uint64_t decodeLength() const
    {
        BboSnapshot skipper(m_buffer, m_offset, m_bufferLength, sbeBlockLength(), m_actingVersion);
        skipper.skip();
        return skipper.encodedLength();
    }

    SBE_NODISCARD const char *buffer() const SBE_NOEXCEPT
    {
        return m_buffer;
    }

    SBE_NODISCARD char *buffer() SBE_NOEXCEPT
    {
        return m_buffer;
    }

    SBE_NODISCARD std::uint64_t bufferLength() const SBE_NOEXCEPT
    {
        return m_bufferLength;
    }

    SBE_NODISCARD std::uint64_t actingVersion() const SBE_NOEXCEPT
    {
        return m_actingVersion;
    }

    SBE_NODISCARD static const char *symbolMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
    {
        switch (metaAttribute)
        {
            case MetaAttribute::PRESENCE: return "required";
            default: return "";
        }
    }

    static SBE_CONSTEXPR std::uint16_t symbolId() SBE_NOEXCEPT
    {
        return 1;
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t symbolSinceVersion() SBE_NOEXCEPT
    {
        return 0;
    }

    SBE_NODISCARD bool symbolInActingVersion() SBE_NOEXCEPT
    {
        return true;
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::size_t symbolEncodingOffset() SBE_NOEXCEPT
    {
        return 0;
    }

private:
    VarAsciiEncoding m_symbol;

public:
    SBE_NODISCARD VarAsciiEncoding &symbol()
    {
        m_symbol.wrap(m_buffer, m_offset + 0, m_actingVersion, m_bufferLength);
        return m_symbol;
    }

    SBE_NODISCARD static const char *fetchTimeMsMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
    {
        switch (metaAttribute)
        {
            case MetaAttribute::PRESENCE: return "required";
            default: return "";
        }
    }

    static SBE_CONSTEXPR std::uint16_t fetchTimeMsId() SBE_NOEXCEPT
    {
        return 2;
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t fetchTimeMsSinceVersion() SBE_NOEXCEPT
    {
        return 0;
    }

    SBE_NODISCARD bool fetchTimeMsInActingVersion() SBE_NOEXCEPT
    {
        return true;
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::size_t fetchTimeMsEncodingOffset() SBE_NOEXCEPT
    {
        return 4;
    }

    static SBE_CONSTEXPR std::uint64_t fetchTimeMsNullValue() SBE_NOEXCEPT
    {
        return SBE_NULLVALUE_UINT64;
    }

    static SBE_CONSTEXPR std::uint64_t fetchTimeMsMinValue() SBE_NOEXCEPT
    {
        return UINT64_C(0x0);
    }

    static SBE_CONSTEXPR std::uint64_t fetchTimeMsMaxValue() SBE_NOEXCEPT
    {
        return UINT64_C(0xfffffffffffffffe);
    }

    static SBE_CONSTEXPR std::size_t fetchTimeMsEncodingLength() SBE_NOEXCEPT
    {
        return 8;
    }

    SBE_NODISCARD std::uint64_t fetchTimeMs() const SBE_NOEXCEPT
    {
        std::uint64_t val;
        std::memcpy(&val, m_buffer + m_offset + 4, sizeof(std::uint64_t));
        return SBE_LITTLE_ENDIAN_ENCODE_64(val);
    }

    BboSnapshot &fetchTimeMs(const std::uint64_t value) SBE_NOEXCEPT
    {
        std::uint64_t val = SBE_LITTLE_ENDIAN_ENCODE_64(value);
        std::memcpy(m_buffer + m_offset + 4, &val, sizeof(std::uint64_t));
        return *this;
    }

    class Venues
    {
    private:
        char *m_buffer = nullptr;
        std::uint64_t m_bufferLength = 0;
        std::uint64_t m_initialPosition = 0;
        std::uint64_t *m_positionPtr = nullptr;
        std::uint64_t m_blockLength = 0;
        std::uint64_t m_count = 0;
        std::uint64_t m_index = 0;
        std::uint64_t m_offset = 0;
        std::uint64_t m_actingVersion = 0;

        SBE_NODISCARD std::uint64_t *sbePositionPtr() SBE_NOEXCEPT
        {
            return m_positionPtr;
        }

    public:
        Venues() = default;

        inline void wrapForDecode(
            char *buffer,
            std::uint64_t *pos,
            const std::uint64_t actingVersion,
            const std::uint64_t bufferLength)
        {
            GroupSizeEncoding dimensions(buffer, *pos, bufferLength, actingVersion);
            m_buffer = buffer;
            m_bufferLength = bufferLength;
            m_blockLength = dimensions.blockLength();
            m_count = dimensions.numInGroup();
            m_index = 0;
            m_actingVersion = actingVersion;
            m_initialPosition = *pos;
            m_positionPtr = pos;
            *m_positionPtr = *m_positionPtr + 4;
        }

        inline void wrapForEncode(
            char *buffer,
            const std::uint16_t count,
            std::uint64_t *pos,
            const std::uint64_t actingVersion,
            const std::uint64_t bufferLength)
        {
    #if defined(__GNUG__) && !defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wtype-limits"
    #endif
            if (count > 65534)
            {
                throw std::runtime_error("count outside of allowed range [E110]");
            }
    #if defined(__GNUG__) && !defined(__clang__)
    #pragma GCC diagnostic pop
    #endif
            m_buffer = buffer;
            m_bufferLength = bufferLength;
            GroupSizeEncoding dimensions(buffer, *pos, bufferLength, actingVersion);
            dimensions.blockLength(static_cast<std::uint16_t>(16));
            dimensions.numInGroup(static_cast<std::uint16_t>(count));
            m_index = 0;
            m_count = count;
            m_blockLength = 16;
            m_actingVersion = actingVersion;
            m_initialPosition = *pos;
            m_positionPtr = pos;
            *m_positionPtr = *m_positionPtr + 4;
        }

        static SBE_CONSTEXPR std::uint64_t sbeHeaderSize() SBE_NOEXCEPT
        {
            return 4;
        }

        static SBE_CONSTEXPR std::uint64_t sbeBlockLength() SBE_NOEXCEPT
        {
            return 16;
        }

        SBE_NODISCARD std::uint64_t sbeActingBlockLength() SBE_NOEXCEPT
        {
            return m_blockLength;
        }

        SBE_NODISCARD std::uint64_t sbePosition() const SBE_NOEXCEPT
        {
            return *m_positionPtr;
        }

        // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
        std::uint64_t sbeCheckPosition(const std::uint64_t position)
        {
            if (SBE_BOUNDS_CHECK_EXPECT((position > m_bufferLength), false))
            {
                throw std::runtime_error("buffer too short [E100]");
            }
            return position;
        }

        void sbePosition(const std::uint64_t position)
        {
            *m_positionPtr = sbeCheckPosition(position);
        }

        SBE_NODISCARD inline std::uint64_t count() const SBE_NOEXCEPT
        {
            return m_count;
        }

        SBE_NODISCARD inline bool hasNext() const SBE_NOEXCEPT
        {
            return m_index < m_count;
        }

        inline Venues &next()
        {
            if (m_index >= m_count)
            {
                throw std::runtime_error("index >= count [E108]");
            }
            m_offset = *m_positionPtr;
            if (SBE_BOUNDS_CHECK_EXPECT(((m_offset + m_blockLength) > m_bufferLength), false))
            {
                throw std::runtime_error("buffer too short for next group index [E108]");
            }
            *m_positionPtr = m_offset + m_blockLength;
            ++m_index;

            return *this;
        }

        inline std::uint64_t resetCountToIndex()
        {
            m_count = m_index;
            GroupSizeEncoding dimensions(m_buffer, m_initialPosition, m_bufferLength, m_actingVersion);
            dimensions.numInGroup(static_cast<std::uint16_t>(m_count));
            return m_count;
        }

        template<class Func> inline void forEach(Func &&func)
        {
            while (hasNext())
            {
                next();
                func(*this);
            }
        }


        SBE_NODISCARD static const char *platformMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
        {
            switch (metaAttribute)
            {
                case MetaAttribute::PRESENCE: return "required";
                default: return "";
            }
        }

        static SBE_CONSTEXPR std::uint16_t platformId() SBE_NOEXCEPT
        {
            return 4;
        }

        SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t platformSinceVersion() SBE_NOEXCEPT
        {
            return 0;
        }

        SBE_NODISCARD bool platformInActingVersion() SBE_NOEXCEPT
        {
            return true;
        }

        SBE_NODISCARD static SBE_CONSTEXPR std::size_t platformEncodingOffset() SBE_NOEXCEPT
        {
            return 0;
        }

private:
        VarAsciiEncoding m_platform;

public:
        SBE_NODISCARD VarAsciiEncoding &platform()
        {
            m_platform.wrap(m_buffer, m_offset + 0, m_actingVersion, m_bufferLength);
            return m_platform;
        }

        SBE_NODISCARD static const char *serverMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
        {
            switch (metaAttribute)
            {
                case MetaAttribute::PRESENCE: return "required";
                default: return "";
            }
        }

        static SBE_CONSTEXPR std::uint16_t serverId() SBE_NOEXCEPT
        {
            return 5;
        }

        SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t serverSinceVersion() SBE_NOEXCEPT
        {
            return 0;
        }

        SBE_NODISCARD bool serverInActingVersion() SBE_NOEXCEPT
        {
            return true;
        }

        SBE_NODISCARD static SBE_CONSTEXPR std::size_t serverEncodingOffset() SBE_NOEXCEPT
        {
            return 4;
        }

private:
        VarAsciiEncoding m_server;

public:
        SBE_NODISCARD VarAsciiEncoding &server()
        {
            m_server.wrap(m_buffer, m_offset + 4, m_actingVersion, m_bufferLength);
            return m_server;
        }

        SBE_NODISCARD static const char *venueTsMsMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
        {
            switch (metaAttribute)
            {
                case MetaAttribute::PRESENCE: return "required";
                default: return "";
            }
        }

        static SBE_CONSTEXPR std::uint16_t venueTsMsId() SBE_NOEXCEPT
        {
            return 6;
        }

        SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t venueTsMsSinceVersion() SBE_NOEXCEPT
        {
            return 0;
        }

        SBE_NODISCARD bool venueTsMsInActingVersion() SBE_NOEXCEPT
        {
            return true;
        }

        SBE_NODISCARD static SBE_CONSTEXPR std::size_t venueTsMsEncodingOffset() SBE_NOEXCEPT
        {
            return 8;
        }

        static SBE_CONSTEXPR std::uint64_t venueTsMsNullValue() SBE_NOEXCEPT
        {
            return SBE_NULLVALUE_UINT64;
        }

        static SBE_CONSTEXPR std::uint64_t venueTsMsMinValue() SBE_NOEXCEPT
        {
            return UINT64_C(0x0);
        }

        static SBE_CONSTEXPR std::uint64_t venueTsMsMaxValue() SBE_NOEXCEPT
        {
            return UINT64_C(0xfffffffffffffffe);
        }

        static SBE_CONSTEXPR std::size_t venueTsMsEncodingLength() SBE_NOEXCEPT
        {
            return 8;
        }

        SBE_NODISCARD std::uint64_t venueTsMs() const SBE_NOEXCEPT
        {
            std::uint64_t val;
            std::memcpy(&val, m_buffer + m_offset + 8, sizeof(std::uint64_t));
            return SBE_LITTLE_ENDIAN_ENCODE_64(val);
        }

        Venues &venueTsMs(const std::uint64_t value) SBE_NOEXCEPT
        {
            std::uint64_t val = SBE_LITTLE_ENDIAN_ENCODE_64(value);
            std::memcpy(m_buffer + m_offset + 8, &val, sizeof(std::uint64_t));
            return *this;
        }

        class Profiles
        {
        private:
            char *m_buffer = nullptr;
            std::uint64_t m_bufferLength = 0;
            std::uint64_t m_initialPosition = 0;
            std::uint64_t *m_positionPtr = nullptr;
            std::uint64_t m_blockLength = 0;
            std::uint64_t m_count = 0;
            std::uint64_t m_index = 0;
            std::uint64_t m_offset = 0;
            std::uint64_t m_actingVersion = 0;

            SBE_NODISCARD std::uint64_t *sbePositionPtr() SBE_NOEXCEPT
            {
                return m_positionPtr;
            }

        public:
            Profiles() = default;

            inline void wrapForDecode(
                char *buffer,
                std::uint64_t *pos,
                const std::uint64_t actingVersion,
                const std::uint64_t bufferLength)
            {
                GroupSizeEncoding dimensions(buffer, *pos, bufferLength, actingVersion);
                m_buffer = buffer;
                m_bufferLength = bufferLength;
                m_blockLength = dimensions.blockLength();
                m_count = dimensions.numInGroup();
                m_index = 0;
                m_actingVersion = actingVersion;
                m_initialPosition = *pos;
                m_positionPtr = pos;
                *m_positionPtr = *m_positionPtr + 4;
            }

            inline void wrapForEncode(
                char *buffer,
                const std::uint16_t count,
                std::uint64_t *pos,
                const std::uint64_t actingVersion,
                const std::uint64_t bufferLength)
            {
        #if defined(__GNUG__) && !defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wtype-limits"
        #endif
                if (count > 65534)
                {
                    throw std::runtime_error("count outside of allowed range [E110]");
                }
        #if defined(__GNUG__) && !defined(__clang__)
        #pragma GCC diagnostic pop
        #endif
                m_buffer = buffer;
                m_bufferLength = bufferLength;
                GroupSizeEncoding dimensions(buffer, *pos, bufferLength, actingVersion);
                dimensions.blockLength(static_cast<std::uint16_t>(28));
                dimensions.numInGroup(static_cast<std::uint16_t>(count));
                m_index = 0;
                m_count = count;
                m_blockLength = 28;
                m_actingVersion = actingVersion;
                m_initialPosition = *pos;
                m_positionPtr = pos;
                *m_positionPtr = *m_positionPtr + 4;
            }

            static SBE_CONSTEXPR std::uint64_t sbeHeaderSize() SBE_NOEXCEPT
            {
                return 4;
            }

            static SBE_CONSTEXPR std::uint64_t sbeBlockLength() SBE_NOEXCEPT
            {
                return 28;
            }

            SBE_NODISCARD std::uint64_t sbeActingBlockLength() SBE_NOEXCEPT
            {
                return m_blockLength;
            }

            SBE_NODISCARD std::uint64_t sbePosition() const SBE_NOEXCEPT
            {
                return *m_positionPtr;
            }

            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            std::uint64_t sbeCheckPosition(const std::uint64_t position)
            {
                if (SBE_BOUNDS_CHECK_EXPECT((position > m_bufferLength), false))
                {
                    throw std::runtime_error("buffer too short [E100]");
                }
                return position;
            }

            void sbePosition(const std::uint64_t position)
            {
                *m_positionPtr = sbeCheckPosition(position);
            }

            SBE_NODISCARD inline std::uint64_t count() const SBE_NOEXCEPT
            {
                return m_count;
            }

            SBE_NODISCARD inline bool hasNext() const SBE_NOEXCEPT
            {
                return m_index < m_count;
            }

            inline Profiles &next()
            {
                if (m_index >= m_count)
                {
                    throw std::runtime_error("index >= count [E108]");
                }
                m_offset = *m_positionPtr;
                if (SBE_BOUNDS_CHECK_EXPECT(((m_offset + m_blockLength) > m_bufferLength), false))
                {
                    throw std::runtime_error("buffer too short for next group index [E108]");
                }
                *m_positionPtr = m_offset + m_blockLength;
                ++m_index;

                return *this;
            }

            inline std::uint64_t resetCountToIndex()
            {
                m_count = m_index;
                GroupSizeEncoding dimensions(m_buffer, m_initialPosition, m_bufferLength, m_actingVersion);
                dimensions.numInGroup(static_cast<std::uint16_t>(m_count));
                return m_count;
            }

            template<class Func> inline void forEach(Func &&func)
            {
                while (hasNext())
                {
                    next();
                    func(*this);
                }
            }


            SBE_NODISCARD static const char *spreadProfileMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
            {
                switch (metaAttribute)
                {
                    case MetaAttribute::PRESENCE: return "required";
                    default: return "";
                }
            }

            static SBE_CONSTEXPR std::uint16_t spreadProfileId() SBE_NOEXCEPT
            {
                return 8;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t spreadProfileSinceVersion() SBE_NOEXCEPT
            {
                return 0;
            }

            SBE_NODISCARD bool spreadProfileInActingVersion() SBE_NOEXCEPT
            {
                return true;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::size_t spreadProfileEncodingOffset() SBE_NOEXCEPT
            {
                return 0;
            }

private:
            VarAsciiEncoding m_spreadProfile;

public:
            SBE_NODISCARD VarAsciiEncoding &spreadProfile()
            {
                m_spreadProfile.wrap(m_buffer, m_offset + 0, m_actingVersion, m_bufferLength);
                return m_spreadProfile;
            }

            SBE_NODISCARD static const char *bidMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
            {
                switch (metaAttribute)
                {
                    case MetaAttribute::PRESENCE: return "required";
                    default: return "";
                }
            }

            static SBE_CONSTEXPR std::uint16_t bidId() SBE_NOEXCEPT
            {
                return 9;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t bidSinceVersion() SBE_NOEXCEPT
            {
                return 0;
            }

            SBE_NODISCARD bool bidInActingVersion() SBE_NOEXCEPT
            {
                return true;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::size_t bidEncodingOffset() SBE_NOEXCEPT
            {
                return 4;
            }

            static SBE_CONSTEXPR double bidNullValue() SBE_NOEXCEPT
            {
                return SBE_DOUBLE_NAN;
            }

            static SBE_CONSTEXPR double bidMinValue() SBE_NOEXCEPT
            {
                return -1.7976931348623157E308;
            }

            static SBE_CONSTEXPR double bidMaxValue() SBE_NOEXCEPT
            {
                return 1.7976931348623157E308;
            }

            static SBE_CONSTEXPR std::size_t bidEncodingLength() SBE_NOEXCEPT
            {
                return 8;
            }

            SBE_NODISCARD double bid() const SBE_NOEXCEPT
            {
                union sbe_double_as_uint_u val;
                std::memcpy(&val, m_buffer + m_offset + 4, sizeof(double));
                val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_64(val.uint_value);
                return val.fp_value;
            }

            Profiles &bid(const double value) SBE_NOEXCEPT
            {
                union sbe_double_as_uint_u val;
                val.fp_value = value;
                val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_64(val.uint_value);
                std::memcpy(m_buffer + m_offset + 4, &val, sizeof(double));
                return *this;
            }

            SBE_NODISCARD static const char *askMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
            {
                switch (metaAttribute)
                {
                    case MetaAttribute::PRESENCE: return "required";
                    default: return "";
                }
            }

            static SBE_CONSTEXPR std::uint16_t askId() SBE_NOEXCEPT
            {
                return 10;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t askSinceVersion() SBE_NOEXCEPT
            {
                return 0;
            }

            SBE_NODISCARD bool askInActingVersion() SBE_NOEXCEPT
            {
                return true;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::size_t askEncodingOffset() SBE_NOEXCEPT
            {
                return 12;
            }

            static SBE_CONSTEXPR double askNullValue() SBE_NOEXCEPT
            {
                return SBE_DOUBLE_NAN;
            }

            static SBE_CONSTEXPR double askMinValue() SBE_NOEXCEPT
            {
                return -1.7976931348623157E308;
            }

            static SBE_CONSTEXPR double askMaxValue() SBE_NOEXCEPT
            {
                return 1.7976931348623157E308;
            }

            static SBE_CONSTEXPR std::size_t askEncodingLength() SBE_NOEXCEPT
            {
                return 8;
            }

            SBE_NODISCARD double ask() const SBE_NOEXCEPT
            {
                union sbe_double_as_uint_u val;
                std::memcpy(&val, m_buffer + m_offset + 12, sizeof(double));
                val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_64(val.uint_value);
                return val.fp_value;
            }

            Profiles &ask(const double value) SBE_NOEXCEPT
            {
                union sbe_double_as_uint_u val;
                val.fp_value = value;
                val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_64(val.uint_value);
                std::memcpy(m_buffer + m_offset + 12, &val, sizeof(double));
                return *this;
            }

            SBE_NODISCARD static const char *bidSpreadMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
            {
                switch (metaAttribute)
                {
                    case MetaAttribute::PRESENCE: return "required";
                    default: return "";
                }
            }

            static SBE_CONSTEXPR std::uint16_t bidSpreadId() SBE_NOEXCEPT
            {
                return 11;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t bidSpreadSinceVersion() SBE_NOEXCEPT
            {
                return 0;
            }

            SBE_NODISCARD bool bidSpreadInActingVersion() SBE_NOEXCEPT
            {
                return true;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::size_t bidSpreadEncodingOffset() SBE_NOEXCEPT
            {
                return 20;
            }

            static SBE_CONSTEXPR float bidSpreadNullValue() SBE_NOEXCEPT
            {
                return SBE_FLOAT_NAN;
            }

            static SBE_CONSTEXPR float bidSpreadMinValue() SBE_NOEXCEPT
            {
                return -3.4028234663852886E38f;
            }

            static SBE_CONSTEXPR float bidSpreadMaxValue() SBE_NOEXCEPT
            {
                return 3.4028234663852886E38f;
            }

            static SBE_CONSTEXPR std::size_t bidSpreadEncodingLength() SBE_NOEXCEPT
            {
                return 4;
            }

            SBE_NODISCARD float bidSpread() const SBE_NOEXCEPT
            {
                union sbe_float_as_uint_u val;
                std::memcpy(&val, m_buffer + m_offset + 20, sizeof(float));
                val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_32(val.uint_value);
                return val.fp_value;
            }

            Profiles &bidSpread(const float value) SBE_NOEXCEPT
            {
                union sbe_float_as_uint_u val;
                val.fp_value = value;
                val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_32(val.uint_value);
                std::memcpy(m_buffer + m_offset + 20, &val, sizeof(float));
                return *this;
            }

            SBE_NODISCARD static const char *askSpreadMetaAttribute(const MetaAttribute metaAttribute) SBE_NOEXCEPT
            {
                switch (metaAttribute)
                {
                    case MetaAttribute::PRESENCE: return "required";
                    default: return "";
                }
            }

            static SBE_CONSTEXPR std::uint16_t askSpreadId() SBE_NOEXCEPT
            {
                return 12;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t askSpreadSinceVersion() SBE_NOEXCEPT
            {
                return 0;
            }

            SBE_NODISCARD bool askSpreadInActingVersion() SBE_NOEXCEPT
            {
                return true;
            }

            SBE_NODISCARD static SBE_CONSTEXPR std::size_t askSpreadEncodingOffset() SBE_NOEXCEPT
            {
                return 24;
            }

            static SBE_CONSTEXPR float askSpreadNullValue() SBE_NOEXCEPT
            {
                return SBE_FLOAT_NAN;
            }

            static SBE_CONSTEXPR float askSpreadMinValue() SBE_NOEXCEPT
            {
                return -3.4028234663852886E38f;
            }

            static SBE_CONSTEXPR float askSpreadMaxValue() SBE_NOEXCEPT
            {
                return 3.4028234663852886E38f;
            }

            static SBE_CONSTEXPR std::size_t askSpreadEncodingLength() SBE_NOEXCEPT
            {
                return 4;
            }

            SBE_NODISCARD float askSpread() const SBE_NOEXCEPT
            {
                union sbe_float_as_uint_u val;
                std::memcpy(&val, m_buffer + m_offset + 24, sizeof(float));
                val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_32(val.uint_value);
                return val.fp_value;
            }

            Profiles &askSpread(const float value) SBE_NOEXCEPT
            {
                union sbe_float_as_uint_u val;
                val.fp_value = value;
                val.uint_value = SBE_LITTLE_ENDIAN_ENCODE_32(val.uint_value);
                std::memcpy(m_buffer + m_offset + 24, &val, sizeof(float));
                return *this;
            }

            template<typename CharT, typename Traits>
            friend std::basic_ostream<CharT, Traits> & operator << (
                std::basic_ostream<CharT, Traits> &builder, Profiles &writer)
            {
                builder << '{';
                builder << R"("spreadProfile": )";
                builder << writer.spreadProfile();

                builder << ", ";
                builder << R"("bid": )";
                builder << +writer.bid();

                builder << ", ";
                builder << R"("ask": )";
                builder << +writer.ask();

                builder << ", ";
                builder << R"("bidSpread": )";
                builder << +writer.bidSpread();

                builder << ", ";
                builder << R"("askSpread": )";
                builder << +writer.askSpread();

                builder << '}';

                return builder;
            }

            void skip()
            {
            }

            SBE_NODISCARD static SBE_CONSTEXPR bool isConstLength() SBE_NOEXCEPT
            {
                return true;
            }

            SBE_NODISCARD static std::size_t computeLength()
            {
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
                std::size_t length = sbeBlockLength();

                return length;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
            }
        };

private:
        Profiles m_profiles;

public:
        SBE_NODISCARD static SBE_CONSTEXPR std::uint16_t profilesId() SBE_NOEXCEPT
        {
            return 7;
        }

        SBE_NODISCARD inline Profiles &profiles()
        {
            m_profiles.wrapForDecode(m_buffer, sbePositionPtr(), m_actingVersion, m_bufferLength);
            return m_profiles;
        }

        Profiles &profilesCount(const std::uint16_t count)
        {
            m_profiles.wrapForEncode(m_buffer, count, sbePositionPtr(), m_actingVersion, m_bufferLength);
            return m_profiles;
        }

        SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t profilesSinceVersion() SBE_NOEXCEPT
        {
            return 0;
        }

        SBE_NODISCARD bool profilesInActingVersion() const SBE_NOEXCEPT
        {
            return true;
        }

        template<typename CharT, typename Traits>
        friend std::basic_ostream<CharT, Traits> & operator << (
            std::basic_ostream<CharT, Traits> &builder, Venues &writer)
        {
            builder << '{';
            builder << R"("platform": )";
            builder << writer.platform();

            builder << ", ";
            builder << R"("server": )";
            builder << writer.server();

            builder << ", ";
            builder << R"("venueTsMs": )";
            builder << +writer.venueTsMs();

            builder << ", ";
            {
                bool atLeastOne = false;
                builder << R"("profiles": [)";
                writer.profiles().forEach(
                    [&](Profiles &profiles)
                    {
                        if (atLeastOne)
                        {
                            builder << ", ";
                        }
                        atLeastOne = true;
                        builder << profiles;
                    });
                builder << ']';
            }

            builder << '}';

            return builder;
        }

        void skip()
        {
            auto &profilesGroup { profiles() };
            while (profilesGroup.hasNext())
            {
                profilesGroup.next().skip();
            }
        }

        SBE_NODISCARD static SBE_CONSTEXPR bool isConstLength() SBE_NOEXCEPT
        {
            return false;
        }

        SBE_NODISCARD static std::size_t computeLength(std::size_t profilesLength = 0)
        {
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
            std::size_t length = sbeBlockLength();

            length += Profiles::sbeHeaderSize();
            if (profilesLength > 65534LL)
            {
                throw std::runtime_error("profilesLength outside of allowed range [E110]");
            }
            length += profilesLength *Profiles::sbeBlockLength();

            return length;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
        }
    };

private:
    Venues m_venues;

public:
    SBE_NODISCARD static SBE_CONSTEXPR std::uint16_t venuesId() SBE_NOEXCEPT
    {
        return 3;
    }

    SBE_NODISCARD inline Venues &venues()
    {
        m_venues.wrapForDecode(m_buffer, sbePositionPtr(), m_actingVersion, m_bufferLength);
        return m_venues;
    }

    Venues &venuesCount(const std::uint16_t count)
    {
        m_venues.wrapForEncode(m_buffer, count, sbePositionPtr(), m_actingVersion, m_bufferLength);
        return m_venues;
    }

    SBE_NODISCARD static SBE_CONSTEXPR std::uint64_t venuesSinceVersion() SBE_NOEXCEPT
    {
        return 0;
    }

    SBE_NODISCARD bool venuesInActingVersion() const SBE_NOEXCEPT
    {
        return true;
    }

template<typename CharT, typename Traits>
friend std::basic_ostream<CharT, Traits> & operator << (
    std::basic_ostream<CharT, Traits> &builder, const BboSnapshot &_writer)
{
    BboSnapshot writer(
        _writer.m_buffer,
        _writer.m_offset,
        _writer.m_bufferLength,
        _writer.m_actingBlockLength,
        _writer.m_actingVersion);

    builder << '{';
    builder << R"("Name": "BboSnapshot", )";
    builder << R"("sbeTemplateId": )";
    builder << writer.sbeTemplateId();
    builder << ", ";

    builder << R"("symbol": )";
    builder << writer.symbol();

    builder << ", ";
    builder << R"("fetchTimeMs": )";
    builder << +writer.fetchTimeMs();

    builder << ", ";
    {
        bool atLeastOne = false;
        builder << R"("venues": [)";
        writer.venues().forEach(
            [&](Venues &venues)
            {
                if (atLeastOne)
                {
                    builder << ", ";
                }
                atLeastOne = true;
                builder << venues;
            });
        builder << ']';
    }

    builder << '}';

    return builder;
}

void skip()
{
    auto &venuesGroup { venues() };
    while (venuesGroup.hasNext())
    {
        venuesGroup.next().skip();
    }
}

SBE_NODISCARD static SBE_CONSTEXPR bool isConstLength() SBE_NOEXCEPT
{
    return false;
}

SBE_NODISCARD static std::size_t computeLength(const std::vector<std::tuple<std::size_t>> &venuesItemLengths = {})
{
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
    std::size_t length = sbeBlockLength();

    length += Venues::sbeHeaderSize();
    if (venuesItemLengths.size() > 65534LL)
    {
        throw std::runtime_error("venuesItemLengths.size() outside of allowed range [E110]");
    }

    for (const auto &e: venuesItemLengths)
    {
        #if __cplusplus >= 201703L
        length += std::apply(Venues::computeLength, e);
        #else
        length += Venues::computeLength(std::get<0>(e));
        #endif
    }

    return length;
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}
};
}
}
#endif

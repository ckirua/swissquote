# cython: language_level=3, boundscheck=False, wraparound=False

cimport cython
from cpython.unicode cimport PyUnicode_DecodeASCII
from datetime import datetime, timezone, timedelta
from libc.stdint cimport int64_t, uint16_t, uint32_t, uint64_t
from libc.string cimport memcpy
from picop.cybytes cimport bas_string, blen
from picop.cylist cimport lappend

include "bbo_layout.pxi"

cdef object _UTC = timezone.utc


cdef inline int64_t _floor_div(int64_t value, int64_t divisor) noexcept:
    cdef int64_t quotient = <int64_t>(value / divisor)
    if value % divisor < 0:
        quotient -= 1
    return quotient


cdef inline object _ms_to_datetime(int64_t ms_timestamp):
    """Unix epoch milliseconds → aware UTC datetime."""
    cdef int64_t seconds = _floor_div(ms_timestamp, 1000)
    cdef int64_t remainder = ms_timestamp - seconds * 1000
    return (
        datetime(1970, 1, 1, tzinfo=_UTC)
        + timedelta(seconds=seconds, microseconds=<int>(remainder * 1000))
    )


cdef inline uint16_t _read_u16_le(const char* p) noexcept nogil:
    return (
        <uint16_t>(<unsigned char>p[0])
        | (<uint16_t>(<unsigned char>p[1]) << 8)
    )


cdef inline uint32_t _read_u32_le(const char* p) noexcept nogil:
    return (
        <uint32_t>(<unsigned char>p[0])
        | (<uint32_t>(<unsigned char>p[1]) << 8)
        | (<uint32_t>(<unsigned char>p[2]) << 16)
        | (<uint32_t>(<unsigned char>p[3]) << 24)
    )


cdef inline uint64_t _read_u64_le(const char* p) noexcept nogil:
    cdef uint64_t v = _read_u32_le(p)
    return v | (<uint64_t>_read_u32_le(p + 4) << 32)


cdef inline float _read_f32_le(const char* p) noexcept nogil:
    cdef uint32_t bits = _read_u32_le(p)
    cdef float out
    memcpy(&out, &bits, sizeof(float))
    return out


cdef inline double _read_f64_le(const char* p) noexcept nogil:
    cdef uint64_t bits = _read_u64_le(p)
    cdef double out
    memcpy(&out, &bits, sizeof(double))
    return out


cdef inline object _decode_ascii(const char* p, Py_ssize_t n) except *:
    cdef object result = PyUnicode_DecodeASCII(p, n, NULL)
    if result is None:
        raise ValueError("invalid ascii in frame")
    return result


cdef void _bounds_check(Py_ssize_t offset, Py_ssize_t need, Py_ssize_t frame_len) except *:
    if offset < 0 or offset + need > frame_len:
        raise ValueError("frame too short")


cdef void _validate_header(const char* buf, Py_ssize_t frame_len, Py_ssize_t* offset) except *:
    _bounds_check(0, SBE_HEADER_SIZE + ROOT_BLOCK_LENGTH, frame_len)

    cdef uint16_t block_length = _read_u16_le(buf)
    cdef uint16_t template_id = _read_u16_le(buf + 2)
    cdef uint16_t schema_id = _read_u16_le(buf + 4)
    cdef uint16_t version = _read_u16_le(buf + 6)

    if template_id != BBO_TEMPLATE_ID:
        raise ValueError(f"unexpected templateId {template_id}")
    if schema_id != BBO_SCHEMA_ID:
        raise ValueError(f"unexpected schemaId {schema_id}")
    if version != BBO_SCHEMA_VERSION:
        raise ValueError(f"unexpected schema version {version}")
    if block_length != ROOT_BLOCK_LENGTH:
        raise ValueError(f"unexpected blockLength {block_length}")

    offset[0] = SBE_HEADER_SIZE


cdef int _flatten_into(const char* buf, Py_ssize_t frame_len, list buffer) except -1:
    cdef Py_ssize_t offset
    cdef Py_ssize_t sym_len
    cdef Py_ssize_t plat_len
    cdef Py_ssize_t srv_len
    cdef Py_ssize_t prof_len
    cdef int venue_count
    cdef int prof_count
    cdef int row_count = 0
    cdef uint16_t venue_block_len
    cdef uint16_t prof_block_len
    cdef uint64_t venue_ts_ms
    cdef double bid
    cdef double ask
    cdef float bid_spread
    cdef float ask_spread
    cdef object symbol
    cdef object platform
    cdef object server
    cdef object spread_profile
    cdef object ts
    cdef object row

    _validate_header(buf, frame_len, &offset)

    sym_len = <Py_ssize_t>_read_u32_le(buf + offset)
    offset += 4
    offset += 8  # fetch_time_ms — not used in flattened rows
    _bounds_check(offset, sym_len, frame_len)
    symbol = _decode_ascii(buf + offset, sym_len)
    offset += sym_len

    venue_block_len = _read_u16_le(buf + offset)
    venue_count = <int>_read_u16_le(buf + offset + 2)
    offset += 4
    if venue_block_len != VENUE_BLOCK_LENGTH:
        raise ValueError(f"unexpected venue blockLength {venue_block_len}")

    for _ in range(venue_count):
        _bounds_check(offset, VENUE_BLOCK_LENGTH, frame_len)
        plat_len = <Py_ssize_t>_read_u32_le(buf + offset)
        srv_len = <Py_ssize_t>_read_u32_le(buf + offset + 4)
        venue_ts_ms = _read_u64_le(buf + offset + 8)
        offset += VENUE_BLOCK_LENGTH

        _bounds_check(offset, plat_len + srv_len, frame_len)
        platform = _decode_ascii(buf + offset, plat_len)
        offset += plat_len
        server = _decode_ascii(buf + offset, srv_len)
        offset += srv_len
        ts = _ms_to_datetime(<int64_t>venue_ts_ms)

        _bounds_check(offset, 4, frame_len)
        prof_block_len = _read_u16_le(buf + offset)
        prof_count = <int>_read_u16_le(buf + offset + 2)
        offset += 4
        if prof_block_len != PROFILE_BLOCK_LENGTH:
            raise ValueError(f"unexpected profile blockLength {prof_block_len}")

        for _ in range(prof_count):
            _bounds_check(offset, PROFILE_BLOCK_LENGTH, frame_len)
            prof_len = <Py_ssize_t>_read_u32_le(buf + offset)
            bid = _read_f64_le(buf + offset + 4)
            ask = _read_f64_le(buf + offset + 12)
            bid_spread = _read_f32_le(buf + offset + 20)
            ask_spread = _read_f32_le(buf + offset + 24)
            offset += PROFILE_BLOCK_LENGTH
            _bounds_check(offset, prof_len, frame_len)
            spread_profile = _decode_ascii(buf + offset, prof_len)
            offset += prof_len
            row = (
                ts,
                symbol,
                platform,
                server,
                spread_profile,
                bid,
                ask,
                bid_spread,
                ask_spread,
            )
            lappend(buffer, row)
            row_count += 1

    return row_count


@cython.final
cpdef list flatten_bbo_frame(bytes frame):
    """Decode SBE frame directly into ClickHouse row tuples (no snapshot tree)."""
    cdef list rows = []
    _flatten_into(bas_string(frame), blen(frame), rows)
    return rows


@cython.final
cpdef int flatten_bbo_frame_into(bytes frame, list buffer) except -1:
    """Append decoded rows to *buffer*; return number of rows appended."""
    return _flatten_into(bas_string(frame), blen(frame), buffer)

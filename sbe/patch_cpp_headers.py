#!/usr/bin/env python3
"""Apply project-owned C++ conveniences to Real Logic SBE output.

The upstream C++ generator exposes variable data as a length-prefixed flyweight,
without put/get methods. Keep that small policy extension reproducible here;
never edit generated headers by hand.
"""

from __future__ import annotations

import argparse
from pathlib import Path

_MARKER = "template<typename CharT, typename Traits>"
_EXTENSION = r'''    VarAsciiEncoding &put(const std::string_view value, std::uint64_t &position)
    {
        if (value.size() > lengthMaxValue()
            || position > m_bufferLength
            || value.size() > m_bufferLength - position)
        {
            throw std::runtime_error("variable data exceeds buffer [E100]");
        }
        for (const unsigned char character : value)
        {
            if (character > 0x7f)
            {
                throw std::runtime_error("invalid ASCII variable data [E111]");
            }
        }
        length(static_cast<std::uint32_t>(value.size()));
        std::memcpy(m_buffer + position, value.data(), value.size());
        position += value.size();
        return *this;
    }

    SBE_NODISCARD std::string_view get(std::uint64_t &position) const
    {
        const auto valueLength = length();
        if (position > m_bufferLength || valueLength > m_bufferLength - position)
        {
            throw std::runtime_error("variable data exceeds buffer [E100]");
        }
        const std::string_view value(m_buffer + position, valueLength);
        for (const unsigned char character : value)
        {
            if (character > 0x7f)
            {
                throw std::runtime_error("invalid ASCII variable data [E111]");
            }
        }
        position += valueLength;
        return value;
    }

'''


def patch_var_ascii(path: Path) -> None:
    text = path.read_text()
    if "VarAsciiEncoding &put(" in text:
        return
    if text.count(_MARKER) != 1:
        raise RuntimeError(f"unexpected generated header shape: {path}")
    path.write_text(text.replace(_MARKER, _EXTENSION + _MARKER, 1))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    patch_var_ascii(args.output / "swissquote_bbo" / "VarAsciiEncoding.h")


if __name__ == "__main__":
    main()

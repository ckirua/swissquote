#pragma once

namespace swissquote::codec {

enum class EncodeError {
    InvalidJson = 1,
    NotArray = 2,
    VenueNotObject = 3,
    VenueMissingTopo = 4,
    ProfilesNotArray = 5,
    ProfileNotObject = 6,
};

enum class DecodeError {
    FrameTooShort = 1,
    InvalidHeader = 2,
    InvalidAscii = 3,
};

} // namespace swissquote::codec

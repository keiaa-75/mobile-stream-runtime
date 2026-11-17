#pragma once

namespace msr {

enum class Status {
    Ok,
    IoError,
    Eof,
    InvalidArgument,
    ParseError,
    Unsupported,
    Cancelled,
    IntegrityMismatch,
};

inline const char* status_name(Status s) {
    switch (s) {
        case Status::Ok:                 return "OK";
        case Status::IoError:            return "IO_ERROR";
        case Status::Eof:                return "EOF";
        case Status::InvalidArgument:    return "INVALID_ARGUMENT";
        case Status::ParseError:         return "PARSE_ERROR";
        case Status::Unsupported:        return "UNSUPPORTED";
        case Status::Cancelled:          return "CANCELLED";
        case Status::IntegrityMismatch:  return "INTEGRITY_MISMATCH";
    }
    return "UNKNOWN";
}

}  // namespace msr

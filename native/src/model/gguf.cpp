#include <msr/model/gguf.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <tuple>

namespace msr::model {
namespace {

constexpr std::uint64_t kMaxString = 1ULL << 30;
constexpr std::uint64_t kMaxArray = 1ULL << 28;

struct TypeInfo {
    const char* name;
    std::uint32_t block;
    std::uint32_t type_bytes;
};

TypeInfo type_info(std::uint32_t t) {
    switch (t) {
        case 0:  return {"F32", 1, 4};
        case 1:  return {"F16", 1, 2};
        case 2:  return {"Q4_0", 32, 18};
        case 3:  return {"Q4_1", 32, 20};
        case 6:  return {"Q5_0", 32, 22};
        case 7:  return {"Q5_1", 32, 24};
        case 8:  return {"Q8_0", 32, 34};
        case 9:  return {"Q8_1", 32, 40};
        case 10: return {"Q2_K", 256, 84};
        case 11: return {"Q3_K", 256, 110};
        case 12: return {"Q4_K", 256, 144};
        case 13: return {"Q5_K", 256, 176};
        case 14: return {"Q6_K", 256, 210};
        case 15: return {"Q8_K", 256, 292};
        case 24: return {"I8", 1, 1};
        case 25: return {"I16", 1, 2};
        case 26: return {"I32", 1, 4};
        case 27: return {"I64", 1, 8};
        case 28: return {"F64", 1, 8};
        case 30: return {"BF16", 1, 2};
        default: return {"UNKNOWN", 0, 0};
    }
}

const char* value_type_name(std::uint32_t t) {
    switch (t) {
        case 0:  return "u8";
        case 1:  return "i8";
        case 2:  return "u16";
        case 3:  return "i16";
        case 4:  return "u32";
        case 5:  return "i32";
        case 6:  return "f32";
        case 7:  return "bool";
        case 8:  return "str";
        case 9:  return "arr";
        case 10: return "u64";
        case 11: return "i64";
        case 12: return "f64";
        default: return "?";
    }
}

class Cursor {
public:
    explicit Cursor(storage::ModelFile& file)
        : file_(file), buf_(64 * 1024) {}

    std::uint64_t pos() const { return fetched_ - (bl_ - bp_); }

    Status read(void* dst, std::size_t n, std::string* err) {
        if (n == 0) return Status::Ok;
        auto* p = static_cast<char*>(dst);
        std::size_t done = 0;
        while (done < n) {
            if (bp_ == bl_) {
                std::uint64_t want = std::min<std::uint64_t>(buf_.size(),
                                                             file_.size() - fetched_);
                if (want == 0) {
                    if (err) *err = "unexpected EOF in GGUF header";
                    return Status::Eof;
                }
                Status st = file_.pread_exact(fetched_, buf_.data(),
                                              static_cast<std::size_t>(want), err);
                if (st != Status::Ok) return st;
                fetched_ += want;
                bp_ = 0;
                bl_ = static_cast<std::size_t>(want);
            }
            std::size_t take = std::min(n - done, bl_ - bp_);
            std::memcpy(p + done, buf_.data() + bp_, take);
            bp_ += take;
            done += take;
        }
        return Status::Ok;
    }

    Status u32(std::uint32_t& v, std::string* err) {
        std::array<unsigned char, 4> b{};
        Status st = read(b.data(), b.size(), err);
        if (st != Status::Ok) return st;
        v = static_cast<std::uint32_t>(b[0]) |
            (static_cast<std::uint32_t>(b[1]) << 8) |
            (static_cast<std::uint32_t>(b[2]) << 16) |
            (static_cast<std::uint32_t>(b[3]) << 24);
        return Status::Ok;
    }

    Status u64(std::uint64_t& v, std::string* err) {
        std::array<unsigned char, 8> b{};
        Status st = read(b.data(), b.size(), err);
        if (st != Status::Ok) return st;
        v = 0;
        for (int i = 7; i >= 0; --i) v = (v << 8) | b[i];
        return Status::Ok;
    }

    Status str(std::string& out, std::string* err) {
        std::uint64_t n = 0;
        Status st = u64(n, err);
        if (st != Status::Ok) return st;
        if (n > kMaxString) {
            if (err) *err = "implausible GGUF string length";
            return Status::ParseError;
        }
        out.resize(static_cast<std::size_t>(n));
        if (n == 0) return Status::Ok;
        return read(&out[0], static_cast<std::size_t>(n), err);
    }

private:
    storage::ModelFile& file_;
    std::vector<unsigned char> buf_;
    std::size_t bp_ = 0;
    std::size_t bl_ = 0;
    std::uint64_t fetched_ = 0;
};

Status consume_value(Cursor& c, std::uint32_t type, int depth, std::string* err) {
    if (depth > 4) {
        if (err) *err = "metadata nesting too deep";
        return Status::ParseError;
    }
    std::array<unsigned char, 8> b{};
    std::size_t n = 0;
    switch (type) {
        case 0: case 1: case 7: n = 1; break;
        case 2: case 3: n = 2; break;
        case 4: case 5: case 6: n = 4; break;
        case 10: case 11: case 12: n = 8; break;
        case 8: {
            std::string s;
            return c.str(s, err);
        }
        case 9: {
            std::uint32_t et = 0;
            std::uint64_t count = 0;
            Status st = c.u32(et, err);
            if (st != Status::Ok) return st;
            st = c.u64(count, err);
            if (st != Status::Ok) return st;
            if (count > kMaxArray) {
                if (err) *err = "implausible GGUF array length";
                return Status::ParseError;
            }
            for (std::uint64_t i = 0; i < count; ++i) {
                st = consume_value(c, et, depth + 1, err);
                if (st != Status::Ok) return st;
            }
            return Status::Ok;
        }
        default:
            if (err) *err = std::string("unknown metadata value type ") + std::to_string(type);
            return Status::ParseError;
    }
    return c.read(b.data(), n, err);
}

Status read_value(Cursor& c, std::uint32_t type, MetaValue& out,
                  int depth, std::string* err) {
    out.type = type;
    if (type == 9) {
        std::uint32_t et = 0;
        std::uint64_t count = 0;
        Status st = c.u32(et, err);
        if (st != Status::Ok) return st;
        st = c.u64(count, err);
        if (st != Status::Ok) return st;
        if (count > kMaxArray) {
            if (err) *err = "implausible GGUF array length";
            return Status::ParseError;
        }
        for (std::uint64_t i = 0; i < count; ++i) {
            st = consume_value(c, et, depth + 1, err);
            if (st != Status::Ok) return st;
        }
        out.text = "<array " + std::to_string(count) + " x " + value_type_name(et) + ">";
        return Status::Ok;
    }
    if (type == 8) return c.str(out.text, err);

    std::array<unsigned char, 8> b{};
    std::size_t n = 0;
    switch (type) {
        case 0: case 1: case 7: n = 1; break;
        case 2: case 3: n = 2; break;
        case 4: case 5: case 6: n = 4; break;
        case 10: case 11: case 12: n = 8; break;
        default:
            if (err) *err = std::string("unknown metadata value type ") + std::to_string(type);
            return Status::ParseError;
    }
    Status st = c.read(b.data(), n, err);
    if (st != Status::Ok) return st;

    std::uint64_t u = 0;
    for (std::size_t i = 0; i < n; ++i) u |= static_cast<std::uint64_t>(b[i]) << (8 * i);
    switch (type) {
        case 1:  out.text = std::to_string(static_cast<int>(static_cast<signed char>(b[0]))); break;
        case 3:  out.text = std::to_string(static_cast<int>(static_cast<std::int16_t>(u))); break;
        case 5:  out.text = std::to_string(static_cast<int>(static_cast<std::int32_t>(u))); break;
        case 7:  out.text = b[0] ? "true" : "false"; break;
        case 11: out.text = std::to_string(static_cast<std::int64_t>(u)); break;
        case 6: {
            float f = 0;
            std::memcpy(&f, b.data(), 4);
            out.text = std::to_string(f);
            break;
        }
        case 12: {
            double d = 0;
            std::memcpy(&d, b.data(), 8);
            out.text = std::to_string(d);
            break;
        }
        default: out.text = std::to_string(u); break;
    }
    return Status::Ok;
}

std::uint64_t align_up(std::uint64_t v, std::uint64_t a) {
    return ((v + a - 1) / a) * a;
}

}  // namespace

const TensorInfo* GgufHeader::find(const std::string& name) const {
    for (const auto& t : tensors) if (t.name == name) return &t;
    return nullptr;
}

bool meta_as_u64(const Metadata& meta, const std::string& key, std::uint64_t& out) {
    auto it = meta.find(key);
    if (it == meta.end()) return false;
    try {
        out = std::stoull(it->second.text);
        return true;
    } catch (...) {
        return false;
    }
}

bool meta_as_string(const Metadata& meta, const std::string& key, std::string& out) {
    auto it = meta.find(key);
    if (it == meta.end()) return false;
    out = it->second.text;
    return true;
}

const char* quant_type_name(std::uint32_t type) {
    return type_info(type).name;
}

Status parse_gguf(storage::ModelFile& file, GgufHeader& out,
                  const ValidationOptions& options, std::string* err) {
    out = GgufHeader{};
    Cursor c(file);

    std::array<unsigned char, 4> magic{};
    Status st = c.read(magic.data(), magic.size(), err);
    if (st != Status::Ok) return st;
    const bool le_magic = magic[0] == 'G' && magic[1] == 'G' &&
                          magic[2] == 'U' && magic[3] == 'F';
    if (!le_magic) {
        const bool be_magic = magic[0] == 'F' && magic[1] == 'U' &&
                              magic[2] == 'G' && magic[3] == 'G';
        if (err) *err = be_magic ? "big-endian GGUF is not supported"
                                 : "bad GGUF magic number";
        return be_magic ? Status::Unsupported : Status::ParseError;
    }

    st = c.u32(out.version, err);
    if (st != Status::Ok) return st;
    if (out.version == 0 || out.version > 3) {
        if (err) *err = "unsupported GGUF version " + std::to_string(out.version);
        return Status::Unsupported;
    }

    st = c.u64(out.tensor_count, err);
    if (st != Status::Ok) return st;
    st = c.u64(out.kv_count, err);
    if (st != Status::Ok) return st;
    if (out.tensor_count > (1ULL << 24) || out.kv_count > (1ULL << 24)) {
        if (err) *err = "implausible GGUF counts";
        return Status::ParseError;
    }

    for (std::uint64_t i = 0; i < out.kv_count; ++i) {
        std::string key;
        st = c.str(key, err);
        if (st != Status::Ok) return st;
        std::uint32_t type = 0;
        st = c.u32(type, err);
        if (st != Status::Ok) return st;
        MetaValue value;
        st = read_value(c, type, value, 0, err);
        if (st != Status::Ok) return st;
        out.metadata[key] = std::move(value);
    }

    std::uint64_t alignment = 32;
    if (meta_as_u64(out.metadata, "general.alignment", alignment)) {
        if (alignment == 0 || alignment > (1ULL << 20) ||
            (alignment & (alignment - 1)) != 0) {
            if (err) *err = "invalid general.alignment";
            return Status::ParseError;
        }
    }
    out.alignment = static_cast<std::uint32_t>(alignment);

    out.tensors.reserve(static_cast<std::size_t>(out.tensor_count));
    for (std::uint64_t i = 0; i < out.tensor_count; ++i) {
        TensorInfo t;
        st = c.str(t.name, err);
        if (st != Status::Ok) return st;
        st = c.u32(t.n_dims, err);
        if (st != Status::Ok) return st;
        if (t.n_dims == 0 || t.n_dims > 16) {
            if (err) *err = "invalid tensor rank for " + t.name;
            return Status::ParseError;
        }
        t.dims.resize(t.n_dims);
        for (std::uint32_t d = 0; d < t.n_dims; ++d) {
            st = c.u64(t.dims[d], err);
            if (st != Status::Ok) return st;
        }
        st = c.u32(t.type, err);
        if (st != Status::Ok) return st;
        st = c.u64(t.offset, err);
        if (st != Status::Ok) return st;

        TypeInfo ti = type_info(t.type);
        if (ti.block == 0 || ti.type_bytes == 0) {
            if (err) *err = "unsupported tensor type " + std::to_string(t.type) +
                            " for " + t.name;
            return Status::Unsupported;
        }
        t.block_size = ti.block;
        t.elements = 1;
        for (std::uint64_t d : t.dims) {
            if (d != 0 && t.elements > std::numeric_limits<std::uint64_t>::max() / d) {
                if (err) *err = "tensor dimension overflow in " + t.name;
                return Status::ParseError;
            }
            t.elements *= d;
        }
        if (t.elements == 0 && !options.allow_zero_length_tensor) {
            if (err) *err = "zero-length tensor " + t.name;
            return Status::ParseError;
        }
        if (options.check_block_size && t.elements % ti.block != 0) {
            if (err) *err = "tensor " + t.name + " has " + std::to_string(t.elements) +
                            " elements, not a multiple of block " + std::to_string(ti.block);
            return Status::ParseError;
        }
        std::uint64_t blocks = t.elements / ti.block;
        if (blocks > std::numeric_limits<std::uint64_t>::max() / ti.type_bytes) {
            if (err) *err = "tensor byte-size overflow in " + t.name;
            return Status::ParseError;
        }
        t.bytes = blocks * ti.type_bytes;
        out.tensors.push_back(std::move(t));
    }

    out.header_bytes = c.pos();
    out.data_start = align_up(out.header_bytes, out.alignment);
    if (out.data_start > file.size()) {
        if (err) *err = "GGUF header extends past end of file";
        return Status::ParseError;
    }
    std::uint64_t capacity = file.size() - out.data_start;

    for (const auto& t : out.tensors) {
        out.payload_bytes += t.bytes;
        if (options.check_alignment && t.offset % out.alignment != 0) {
            if (err) *err = "tensor " + t.name + " offset " + std::to_string(t.offset) +
                            " is not aligned to " + std::to_string(out.alignment);
            return Status::ParseError;
        }
        if (t.bytes > capacity || t.offset > capacity - t.bytes) {
            if (err) *err = "tensor " + t.name + " extends past end of file";
            return Status::ParseError;
        }
    }

    if (options.check_overlap) {
        std::vector<std::tuple<std::uint64_t, std::uint64_t, std::string>> spans;
        for (const auto& t : out.tensors) {
            if (t.bytes == 0) continue;
            spans.emplace_back(t.offset, t.offset + t.bytes, t.name);
        }
        std::sort(spans.begin(), spans.end());
        for (std::size_t i = 1; i < spans.size(); ++i) {
            if (std::get<0>(spans[i]) < std::get<1>(spans[i - 1])) {
                if (err) *err = "tensor overlap: " + std::get<2>(spans[i - 1]) +
                                " and " + std::get<2>(spans[i]);
                return Status::ParseError;
            }
        }
    }

    return Status::Ok;
}

Status read_tensor_range(storage::ModelFile& file, const GgufHeader& header,
                         const TensorInfo& tensor, std::uint64_t slice_offset,
                         std::size_t len, void* dst, bool use_direct_io,
                         storage::AlignedBuffer& scratch, std::string* err) {
    if (!dst) {
        if (err) *err = "null tensor destination";
        return Status::InvalidArgument;
    }
    if (slice_offset > tensor.bytes || len > tensor.bytes - slice_offset) {
        if (err) *err = "tensor slice out of bounds";
        return Status::InvalidArgument;
    }
    std::uint64_t file_offset = header.data_start + tensor.offset + slice_offset;
    if (use_direct_io) return file.read_aligned(file_offset, dst, len, scratch, err);
    return file.pread_exact(file_offset, dst, len, err);
}

}  // namespace msr::model

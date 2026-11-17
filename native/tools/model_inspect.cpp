#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <msr/model/gguf.hpp>
#include <msr/model/integrity.hpp>
#include <msr/status.hpp>
#include <msr/storage/model_file.hpp>

using namespace msr;

namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char* h = "0123456789abcdef";
                    out += "\\u00";
                    out += h[c >> 4];
                    out += h[c & 0x0f];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

void write_json(const std::string& path, const std::string& model_path,
                const storage::ModelFile& file, const model::GgufHeader& h) {
    std::ofstream os(path);
    if (!os) return;
    os << "{\n"
       << "  \"file\": \"" << json_escape(model_path) << "\",\n"
       << "  \"file_bytes\": " << file.size() << ",\n"
       << "  \"version\": " << h.version << ",\n"
       << "  \"tensor_count\": " << h.tensor_count << ",\n"
       << "  \"kv_count\": " << h.kv_count << ",\n"
       << "  \"alignment\": " << h.alignment << ",\n"
       << "  \"header_bytes\": " << h.header_bytes << ",\n"
       << "  \"data_start\": " << h.data_start << ",\n"
       << "  \"payload_bytes\": " << h.payload_bytes << ",\n"
       << "  \"tensors\": [\n";
    for (std::size_t i = 0; i < h.tensors.size(); ++i) {
        const auto& t = h.tensors[i];
        os << "    {\"name\": \"" << json_escape(t.name)
           << "\", \"type\": \"" << model::quant_type_name(t.type)
           << "\", \"dims\": [";
        for (std::size_t d = 0; d < t.dims.size(); ++d) {
            if (d) os << ", ";
            os << t.dims[d];
        }
        os << "], \"offset\": " << t.offset << ", \"bytes\": " << t.bytes << "}";
        if (i + 1 < h.tensors.size()) os << ",";
        os << "\n";
    }
    os << "  ]\n}\n";
}

void print_summary(const std::string& path, const storage::ModelFile& file,
                   const model::GgufHeader& h) {
    std::string arch;
    model::meta_as_string(h.metadata, "general.architecture", arch);
    std::cout << "MODEL " << path << "\n"
              << "FILE_BYTES " << file.size() << "\n"
              << "DIRECT_IO " << (file.direct_io() ? "yes" : "no") << "\n"
              << "VERSION " << h.version << "\n"
              << "ARCH " << (arch.empty() ? "-" : arch) << "\n"
              << "TENSORS " << h.tensors.size()
              << " KV " << h.kv_count
              << " ALIGNMENT " << h.alignment << "\n"
              << "HEADER_BYTES " << h.header_bytes
              << " DATA_START " << h.data_start
              << " PAYLOAD_BYTES " << h.payload_bytes << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: model-inspect MODEL [--tensor NAME] [--read-bytes N] "
                     "[--hash-tensor NAME] [--verify SHA256] [--json OUT] "
                     "[--all-tensors] [--direct]\n";
        return 2;
    }

    std::string model_path = argv[1];
    std::string tensor_name, hash_tensor, verify_hex, json_path;
    std::uint64_t read_bytes = 0;
    bool all_tensors = false;
    bool direct = false;

    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* opt, std::string& dst) -> bool {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << opt << "\n";
                return false;
            }
            dst = argv[++i];
            return true;
        };
        if (a == "--tensor") { if (!need(a.c_str(), tensor_name)) return 2; }
        else if (a == "--hash-tensor") { if (!need(a.c_str(), hash_tensor)) return 2; }
        else if (a == "--verify") { if (!need(a.c_str(), verify_hex)) return 2; }
        else if (a == "--json") { if (!need(a.c_str(), json_path)) return 2; }
        else if (a == "--read-bytes") {
            std::string v;
            if (!need(a.c_str(), v)) return 2;
            read_bytes = std::strtoull(v.c_str(), nullptr, 10);
        }
        else if (a == "--all-tensors") all_tensors = true;
        else if (a == "--direct") direct = true;
        else {
            std::cerr << "unknown option " << a << "\n";
            return 2;
        }
    }

    std::unique_ptr<storage::ModelFile> file;
    std::string err;
    Status st = storage::ModelFile::open(model_path, direct, file, &err);
    if (st != Status::Ok) {
        std::cerr << "open: " << status_name(st) << ": " << err << "\n";
        return 1;
    }

    model::GgufHeader header;
    st = parse_gguf(*file, header, model::ValidationOptions{}, &err);
    if (st != Status::Ok) {
        std::cerr << "parse: " << status_name(st) << ": " << err << "\n";
        return 1;
    }
    print_summary(model_path, *file, header);

    if (!json_path.empty()) write_json(json_path, model_path, *file, header);

    if (all_tensors) {
        for (const auto& t : header.tensors) {
            std::cout << "TENSOR " << t.name << " type=" << model::quant_type_name(t.type)
                      << " dims=" << t.dims.size() << " elements=" << t.elements
                      << " offset=" << t.offset << " bytes=" << t.bytes << "\n";
        }
    }

    auto do_read = [&](const std::string& name, bool hash_only) -> int {
        const model::TensorInfo* t = header.find(name);
        if (!t) {
            std::cerr << "tensor not found: " << name << "\n";
            return 1;
        }
        std::uint64_t len = hash_only ? t->bytes
                                      : (read_bytes ? std::min(read_bytes, t->bytes) : t->bytes);
        if (len == 0 || len > (1ULL << 30)) {
            std::cerr << "unsupported tensor slice length " << len << "\n";
            return 1;
        }
        std::vector<unsigned char> dst(static_cast<std::size_t>(len));
        storage::AlignedBuffer scratch;
        if (direct && !scratch.allocate(static_cast<std::size_t>(len + 8192))) {
            std::cerr << "scratch allocation failed\n";
            return 1;
        }
        err.clear();
        st = read_tensor_range(*file, header, *t, 0, static_cast<std::size_t>(len),
                               dst.data(), direct, scratch, &err);
        if (st != Status::Ok) {
            std::cerr << "read: " << status_name(st) << ": " << err << "\n";
            return 1;
        }
        model::Digest d{};
        st = model::sha256_bytes(dst.data(), dst.size(), d, &err);
        if (st != Status::Ok) {
            std::cerr << "hash: " << status_name(st) << ": " << err << "\n";
            return 1;
        }
        if (hash_only) {
            std::cout << "TENSOR_SHA name=" << name << " file_offset="
                      << (header.data_start + t->offset) << " bytes=" << len
                      << " sha256=" << model::digest_hex(d) << "\n";
        } else {
            std::string head;
            for (std::size_t i = 0; i < std::min<std::size_t>(32, dst.size()); ++i) {
                static const char* hx = "0123456789abcdef";
                head += hx[dst[i] >> 4];
                head += hx[dst[i] & 0x0f];
            }
            std::cout << "READ_TENSOR name=" << name << " file_offset="
                      << (header.data_start + t->offset) << " bytes=" << len
                      << " sha256=" << model::digest_hex(d)
                      << " first_hex=" << head << "\n";
        }
        return 0;
    };

    if (!tensor_name.empty()) {
        int rc = do_read(tensor_name, false);
        if (rc != 0) return rc;
    }
    if (!hash_tensor.empty()) {
        int rc = do_read(hash_tensor, true);
        if (rc != 0) return rc;
    }
    if (!verify_hex.empty()) {
        model::Digest actual{};
        err.clear();
        st = model::verify_file_sha256(model_path, verify_hex, actual, &err);
        std::cout << "VERIFY result=" << status_name(st)
                  << " sha256=" << model::digest_hex(actual) << "\n";
        if (st != Status::Ok) {
            std::cerr << "verify: " << err << "\n";
            return 1;
        }
    }
    return 0;
}

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <msr/model/gguf.hpp>
#include <msr/model/integrity.hpp>
#include <msr/status.hpp>
#include <msr/storage/model_file.hpp>

using namespace msr;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    } else {
        std::cout << "ok   " << what << "\n";
    }
}

std::string read_text(const std::string& path) {
    std::ifstream is(path);
    std::string line;
    std::getline(is, line);
    return line;
}

void expect_status(const std::string& dir, const std::string& name, Status expected) {
    std::unique_ptr<storage::ModelFile> f;
    std::string err;
    Status st = storage::ModelFile::open(dir + "/" + name, false, f, &err);
    if (st != Status::Ok) {
        check(false, name + " open: " + err);
        return;
    }
    model::GgufHeader h;
    st = parse_gguf(*f, h, model::ValidationOptions{}, &err);
    check(st == expected, name + " -> " + status_name(expected) +
                          " (got " + status_name(st) + ": " + err + ")");
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = (argc > 1) ? argv[1] : ".";
    if (!dir.empty() && dir.back() == '/') dir.pop_back();

    // Happy path: valid fixture parses, validates and reads byte-exactly.
    std::unique_ptr<storage::ModelFile> f;
    std::string err;
    Status st = storage::ModelFile::open(dir + "/valid.gguf", false, f, &err);
    check(st == Status::Ok, "valid.gguf opens");
    if (st != Status::Ok) return 1;

    model::GgufHeader h;
    st = parse_gguf(*f, h, model::ValidationOptions{}, &err);
    check(st == Status::Ok, "valid.gguf parses: " + err);
    check(h.version == 3, "version == 3");
    check(h.tensor_count == 3, "tensor_count == 3");
    check(h.alignment == 32, "alignment == 32");
    check(h.tensors.size() == 3, "three tensor infos");
    check(h.payload_bytes == 224, "tensor payload_bytes == 224 (excluding alignment gaps)");
    check(h.data_start % 32 == 0 && h.data_start >= h.header_bytes,
          "data_start is aligned past header");

    const auto* a = h.find("a");
    check(a != nullptr, "tensor a found");
    if (a) {
        check(a->bytes == 64, "tensor a is 64 bytes");
        std::vector<unsigned char> buf(static_cast<std::size_t>(a->bytes));
        storage::AlignedBuffer scratch;
        st = read_tensor_range(*f, h, *a, 0, buf.size(), buf.data(), false, scratch, &err);
        check(st == Status::Ok, "tensor a read into bounded buffer: " + err);
        model::Digest d{};
        st = model::sha256_bytes(buf.data(), buf.size(), d, &err);
        check(st == Status::Ok, "tensor a hash computed");
        check(model::digest_hex(d) == read_text(dir + "/expected_tensor_a.sha"),
              "tensor a bytes match independent SHA-256");
        bool pattern = true;
        for (std::size_t i = 0; i < buf.size(); ++i)
            if (buf[i] != static_cast<unsigned char>((i * 7) % 256)) pattern = false;
        check(pattern, "tensor a payload byte pattern");
    }

    model::Digest file_digest{};
    st = model::sha256_file(dir + "/valid.gguf", nullptr, file_digest, &err);
    check(st == Status::Ok, "whole-file SHA-256 computed");
    check(model::digest_hex(file_digest) == read_text(dir + "/expected.sha"),
          "whole-file SHA-256 matches expected");

    st = model::verify_file_sha256(dir + "/valid.gguf",
                                   read_text(dir + "/expected.sha"), file_digest, &err);
    check(st == Status::Ok, "verify accepts correct hash");
    st = model::verify_file_sha256(dir + "/valid.gguf", std::string(64, '0'),
                                   file_digest, &err);
    check(st == Status::IntegrityMismatch, "verify rejects wrong hash");

    // Malformed inputs.
    expect_status(dir, "truncated.gguf", Status::ParseError);
    expect_status(dir, "wrong_endian.gguf", Status::Unsupported);
    expect_status(dir, "bad_version.gguf", Status::Unsupported);
    expect_status(dir, "zero_tensor.gguf", Status::ParseError);
    expect_status(dir, "overlap.gguf", Status::ParseError);
    expect_status(dir, "unaligned.gguf", Status::ParseError);
    expect_status(dir, "bad_block.gguf", Status::ParseError);
    expect_status(dir, "out_of_bounds.gguf", Status::ParseError);

    std::cout << (failures ? "FAILED " : "PASSED ") << failures << " failure(s)\n";
    return failures ? 1 : 0;
}

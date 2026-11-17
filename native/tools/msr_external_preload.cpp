// LD_PRELOAD provider for the MSR bounded-arena loader path.
//
// In `MSR_NO_ALLOC` mode llama.cpp creates the weight tensors without backend
// storage. This provider binds every tensor directly to one arena:
//   - resident units keep permanent slices of the arena,
//   - the remaining transformer layers share one buffer that is refilled right
//     before each streamed layer executes.
//
// The refill marker is the `-<layer>` suffix that llama.cpp's graph builder
// puts on every layer node ("norm-33", "Qcur-7", ...). A registered tensor
// pointer is not reliable because fused ops absorb the tensor the graph
// constructor hands us.
//
// No tensor is copied into a backend allocation, so the loader cannot keep a
// second resident copy of the weights.
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <msr/memory/memory_manager.hpp>
#include <msr/model/gguf.hpp>
#include <msr/scheduler/async_reader.hpp>
#include <msr/storage/model_file.hpp>
#include <msr/streaming/layer_plan.hpp>

namespace {

constexpr std::uint64_t kTensorAlign = 64;
constexpr std::uint64_t kUnitAlign = 4096;
constexpr std::uint64_t kDefaultBudget = 2ull << 30;  // 2 GiB
constexpr std::size_t kReadQueueDepth = 32;

std::uint64_t align_up(std::uint64_t v, std::uint64_t a) {
    return ((v + a - 1) / a) * a;
}

int layer_of(const std::string& unit_name) {
    if (unit_name.rfind("blk.", 0) != 0) return -1;
    return std::atoi(unit_name.c_str() + 4);
}

struct Binding {
    const msr::model::TensorInfo* info = nullptr;
    int layer = -1;
    bool streamed = false;
    void* ptr = nullptr;
};

struct UnitLayout {
    const msr::streaming::LayerUnit* unit = nullptr;
    int layer = -1;
    std::uint64_t bytes = 0;
    std::vector<std::pair<const msr::model::TensorInfo*, std::uint64_t>> tensors;
};

std::mutex g_mutex;
bool g_initialized = false;
bool g_ready = false;
std::unique_ptr<msr::storage::ModelFile> g_file;
msr::model::GgufHeader g_header;
std::unique_ptr<msr::memory::MemoryManager> g_mem;
std::vector<msr::memory::Buffer> g_unit_buffers;
std::vector<msr::memory::Buffer> g_stream_buffers;
std::vector<UnitLayout> g_layouts;
std::vector<std::size_t> g_streamed_layouts;             // ascending layer order
std::unordered_map<int, std::size_t> g_stream_index;      // layer -> index above
std::unordered_map<std::string, Binding> g_bindings;
std::unique_ptr<msr::scheduler::AsyncReader> g_reader;
std::unique_ptr<msr::scheduler::LayerPrefetcher> g_prefetcher;
bool g_prefetch = false;
std::size_t g_stream_slots = 1;
std::size_t g_read_workers = 1;
std::set<int> g_streamed_layers;
std::set<int> g_logged_refills;
std::atomic<int> g_current_stream_layer{-1};
std::string g_model_path;
std::uint64_t g_budget = kDefaultBudget;
std::uint64_t g_resident_bytes = 0;
std::uint64_t g_stream_bytes = 0;
std::uint64_t g_peak_weight_bytes = 0;

extern "C" void msr_external_summary();

msr::Status read_tensor(const msr::model::TensorInfo& t, void* dst, std::string* err) {
    return g_file->pread_exact(g_header.data_start + t.offset, dst, t.bytes, err);
}

bool initialize() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) return g_ready;
    g_initialized = true;

    const char* model = std::getenv("MSR_MODEL_PATH");
    if (!model) {
        std::cerr << "msr_external: MSR_MODEL_PATH is required\n";
        return false;
    }
    g_model_path = model;

    if (const char* budget = std::getenv("MSR_BUDGET_BYTES")) {
        g_budget = std::strtoull(budget, nullptr, 10);
    }
    if (g_budget == 0) {
        std::cerr << "msr_external: invalid MSR_BUDGET_BYTES\n";
        return false;
    }

    std::string err;
    if (msr::storage::ModelFile::open(g_model_path, false, g_file, &err) != msr::Status::Ok) {
        std::cerr << "msr_external: open failed: " << err << "\n";
        return false;
    }
    if (msr::model::parse_gguf(*g_file, g_header, msr::model::ValidationOptions{}, &err) !=
        msr::Status::Ok) {
        std::cerr << "msr_external: parse failed: " << err << "\n";
        return false;
    }

    std::vector<msr::streaming::LayerUnit> units;
    if (msr::streaming::build_layer_plan(g_header, units, &err) != msr::Status::Ok) {
        std::cerr << "msr_external: layer plan failed: " << err << "\n";
        return false;
    }

    g_layouts.reserve(units.size());
    std::uint64_t total_bytes = 0;
    for (const auto& u : units) {
        UnitLayout layout;
        layout.unit = &u;
        layout.layer = layer_of(u.name);
        std::uint64_t cursor = 0;
        for (const auto& name : u.tensor_names) {
            const auto* t = g_header.find(name);
            if (!t) {
                std::cerr << "msr_external: tensor vanished: " << name << "\n";
                return false;
            }
            cursor = align_up(cursor, kTensorAlign);
            layout.tensors.emplace_back(t, cursor);
            cursor += t->bytes;
        }
        layout.bytes = align_up(cursor, kUnitAlign);
        total_bytes += layout.bytes;
        g_layouts.push_back(std::move(layout));
    }

    if (const char* pf = std::getenv("MSR_PREFETCH")) {
        g_prefetch = std::strtol(pf, nullptr, 10) != 0;
    }
    g_stream_slots = g_prefetch ? 2 : 1;
    if (const char* workers = std::getenv("MSR_READ_THREADS")) {
        const long v = std::strtol(workers, nullptr, 10);
        if (v > 0 && v <= 16) g_read_workers = static_cast<std::size_t>(v);
    }

    // Keep the first units resident; stream transformer layers from the end
    // until resident bytes plus one buffer per slot fit the hard budget.
    std::vector<bool> streamed(g_layouts.size(), false);
    std::uint64_t resident_bytes = total_bytes;
    std::uint64_t stream_bytes = 0;
    if (resident_bytes + g_stream_slots * stream_bytes > g_budget) {
        for (std::size_t i = g_layouts.size(); i-- > 0;) {
            if (g_layouts[i].layer < 0) continue;
            streamed[i] = true;
            resident_bytes -= g_layouts[i].bytes;
            stream_bytes = std::max(stream_bytes, g_layouts[i].bytes);
            if (resident_bytes + g_stream_slots * stream_bytes <= g_budget) break;
        }
    }
    if (resident_bytes + g_stream_slots * stream_bytes > g_budget) {
        std::cerr << "msr_external: budget too small: resident=" << resident_bytes
                  << " stream=" << stream_bytes << " slots=" << g_stream_slots
                  << " budget=" << g_budget << "\n";
        return false;
    }
    if (resident_bytes == 0) {
        std::cerr << "msr_external: no resident units fit\n";
        return false;
    }

    for (std::size_t i = 0; i < g_layouts.size(); ++i) {
        if (!streamed[i]) continue;
        g_stream_index[g_layouts[i].layer] = g_streamed_layouts.size();
        g_streamed_layouts.push_back(i);
    }

    g_mem = std::make_unique<msr::memory::MemoryManager>(g_budget);
    if (g_mem->capacity() == 0) {
        std::cerr << "msr_external: arena allocation failed\n";
        return false;
    }

    g_unit_buffers.resize(g_layouts.size());
    for (std::size_t i = 0; i < g_layouts.size(); ++i) {
        if (streamed[i]) continue;
        msr::memory::Buffer buf;
        if (g_mem->acquire(g_layouts[i].bytes, buf, &err) != msr::Status::Ok) {
            std::cerr << "msr_external: resident acquire failed: " << err << "\n";
            return false;
        }
        g_unit_buffers[i] = buf;
        g_resident_bytes += g_layouts[i].bytes;
        for (const auto& [t, off] : g_layouts[i].tensors) {
            void* ptr = static_cast<char*>(buf.data) + off;
            if (read_tensor(*t, ptr, &err) != msr::Status::Ok) {
                std::cerr << "msr_external: read " << t->name << " failed: " << err << "\n";
                return false;
            }
            g_bindings[t->name] = Binding{t, g_layouts[i].layer, false, ptr};
        }
    }

    if (stream_bytes > 0) {
        g_stream_buffers.resize(g_stream_slots);
        for (auto& buf : g_stream_buffers) {
            if (g_mem->acquire(stream_bytes, buf, &err) != msr::Status::Ok) {
                std::cerr << "msr_external: stream acquire failed: " << err << "\n";
                return false;
            }
        }
        g_stream_bytes = stream_bytes * g_stream_slots;

        for (std::size_t k = 0; k < g_streamed_layouts.size(); ++k) {
            const auto& layout = g_layouts[g_streamed_layouts[k]];
            g_streamed_layers.insert(layout.layer);
            char* base = static_cast<char*>(
                g_stream_buffers[k % g_stream_slots].data);
            for (const auto& [t, off] : layout.tensors) {
                g_bindings[t->name] = Binding{t, layout.layer, true, base + off};
            }
        }

        if (msr::scheduler::AsyncReader::create(g_file.get(), kReadQueueDepth,
                                                g_reader, &err,
                                                g_read_workers) != msr::Status::Ok) {
            std::cerr << "msr_external: async reader failed: " << err << "\n";
            return false;
        }
        g_prefetcher = std::make_unique<msr::scheduler::LayerPrefetcher>(
            g_reader.get(), g_stream_slots);
    }

    g_peak_weight_bytes = g_resident_bytes + g_stream_bytes;
    g_ready = true;
    std::atexit(msr_external_summary);

    std::cerr << "msr_external: model=" << g_model_path << "\n"
              << "msr_external: budget_bytes=" << g_budget << "\n"
              << "msr_external: payload_bytes=" << g_header.payload_bytes << "\n"
              << "msr_external: resident_weight_bytes=" << g_resident_bytes << "\n"
              << "msr_external: stream_buffer_bytes=" << g_stream_bytes << "\n"
              << "msr_external: stream_slots=" << g_stream_slots << "\n"
              << "msr_external: prefetch=" << (g_prefetch ? "on" : "off") << "\n"
              << "msr_external: read_threads=" << g_read_workers << "\n"
              << "msr_external: peak_weight_bytes=" << g_peak_weight_bytes << "\n"
              << "msr_external: streamed_layers=" << g_streamed_layers.size() << "\n"
              << "msr_external: bound_tensors=" << g_bindings.size() << "\n";
    return true;
}

bool schedule_stream_layer_locked(std::size_t k, bool synchronous) {
    if (!g_prefetcher || k >= g_streamed_layouts.size()) return false;
    const auto& layout = g_layouts[g_streamed_layouts[k]];
    char* base = static_cast<char*>(g_stream_buffers[k % g_stream_slots].data);
    std::string err;
    for (const auto& [t, off] : layout.tensors) {
        const std::uint64_t file_offset = g_header.data_start + t->offset;
        if (g_prefetcher->schedule(layout.layer, file_offset, base + off, t->bytes,
                                   synchronous, &err) != msr::Status::Ok) {
            std::cerr << "msr_external: schedule " << t->name << " failed: " << err << "\n";
            return false;
        }
    }
    return true;
}

/// Makes `layer` readable in its slot, then (prefetch mode) starts the read for
/// the next streamed layer. Caller holds g_mutex.
bool begin_stream_layer_locked(int layer) {
    auto it = g_stream_index.find(layer);
    if (it == g_stream_index.end()) return true;  // resident unit

    const std::size_t k = it->second;
    if (g_prefetcher->scheduled(layer)) {
        std::string err;
        if (g_prefetcher->wait_layer(layer, &err) != msr::Status::Ok) {
            std::cerr << "msr_external: prefetch wait for layer " << layer
                      << " failed: " << err << "\n";
            return false;
        }
    } else if (!schedule_stream_layer_locked(k, /*synchronous=*/true)) {
        return false;
    }

    if (g_prefetch && k + 1 < g_streamed_layouts.size()) {
        schedule_stream_layer_locked(k + 1, /*synchronous=*/false);
    }

    g_current_stream_layer.store(layer);
    return true;
}

}  // namespace

extern "C" void msr_external_summary() {
    if (!g_ready) return;
    std::cerr << "msr_external: arena_peak_bytes=" << g_mem->stats().peak_bytes
              << " arena_current_bytes=" << g_mem->stats().current_bytes << "\n";
    if (g_reader) {
        const auto st = g_reader->stats();
        std::cerr << "msr_external: reads_submitted=" << st.submitted
                  << " reads_completed=" << st.completed
                  << " reads_failed=" << st.failed
                  << " reads_dropped=" << st.dropped
                  << " read_bytes=" << st.bytes_read
                  << " read_io_ns=" << st.io_ns
                  << " read_wait_ns=" << st.wait_ns
                  << " read_submit_block_ns=" << st.submit_block_ns
                  << " read_peak_depth=" << st.peak_queue_depth << "\n";
    }
}

extern "C" bool msr_streaming_arena(void** base, std::size_t* size) {
    if (!initialize() || !base || !size || !g_mem) return false;
    *base = g_mem->base();
    *size = static_cast<std::size_t>(g_mem->capacity());
    return *base != nullptr && *size > 0;
}

extern "C" bool msr_streaming_bind_tensor(const char* name, std::size_t size, void** data) {
    if (!initialize() || !name || !data) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_bindings.find(name);
    if (it == g_bindings.end() || it->second.info->bytes != size) return false;
    *data = it->second.ptr;
    return true;
}

// Kept for the earlier non-no_alloc proof path.
extern "C" bool msr_streaming_supply_tensor(const char* name, void** data, std::size_t size) {
    return msr_streaming_bind_tensor(name, size, data);
}

extern "C" void msr_streaming_prepare_layer(int layer) {
    // Refills happen at the first compute node; a new graph must refill its
    // first streamed layer even if it matches the previous graph's.
    if (layer != 0) return;
    g_current_stream_layer.store(-1);
    if (!g_prefetch || !g_ready) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_streamed_layouts.empty() || !g_prefetcher) return;
    const int first = g_layouts[g_streamed_layouts.front()].layer;
    if (!g_prefetcher->scheduled(first)) {
        // Overlap the first layer's I/O with graph construction and the
        // resident layers that run before it.
        schedule_stream_layer_locked(0, /*synchronous=*/false);
    }
}

extern "C" void msr_streaming_layer_first_node(int layer, void* tensor, const char* name) {
    // Kept as a hook point for build-time scheduling; the compute-time
    // refill uses the node-name suffix instead (see node_start).
    (void) layer;
    (void) tensor;
    (void) name;
}

extern "C" void msr_streaming_layer_boundary(int layer, void* tensor) {
    (void) layer;
    (void) tensor;
}

extern "C" void msr_streaming_node_start(void* node, const char* name) {
    (void) node;
    if (!g_ready || !name) return;

    // Graph node names carry a "-<layer>" suffix (e.g. "norm-33", "Qcur-7").
    // Fused ops keep the suffix, whereas the raw tensor registered by
    // msr_streaming_layer_first_node can be absorbed into a fused node, so the
    // suffix is the reliable per-layer marker.
    int layer = -1;
    const char* dash = std::strrchr(name, '-');
    if (dash && dash[1] != '\0') {
        char* end = nullptr;
        long v = std::strtol(dash + 1, &end, 10);
        if (end && *end == '\0' && v >= 0 && v < 1000) layer = static_cast<int>(v);
    }
    if (layer < 0) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_streamed_layers.count(layer)) return;
    if (layer == g_current_stream_layer.load()) return;
    if (!begin_stream_layer_locked(layer)) return;

    if (g_logged_refills.insert(layer).second) {
        std::cerr << "msr_external: streamed layer " << layer << " ready\n";
    }
}

/// Drops queued prefetch work and marks the streamed layer state stale.
/// Exported for the Phase 4 JNI layer; nothing calls it during a normal run.
extern "C" void msr_streaming_cancel() {
    if (!g_ready) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_prefetcher) g_prefetcher->cancel_pending();
    g_current_stream_layer.store(-1);
}

extern "C" void msr_streaming_node_done(void* node) {
    (void) node;  // no per-node revocation in the arena path
}

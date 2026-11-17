// C engine API implementation. The only stable surface for host programs;
// llama.cpp/ggml and the provider stay behind it. The vendored llama.cpp is
// linked statically into the SDK library so its weak hooks resolve to the
// provider's strong definitions without any runtime injection.
#include <msr/engine/engine.h>

#include <llama.h>

#include <msr/msr_external_abi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" void msr_streaming_cancel();

namespace {

using Clock = std::chrono::steady_clock;

long long ms_since(Clock::time_point t0) {
    return static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count());
}

struct Engine {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    std::atomic<bool> cancel{false};
    std::mutex generate_mutex;
    std::string last_error;

    msr_engine_stats stats{};

    bool aborted() const { return cancel.load(); }
};

bool abort_cb(void* data) {
    return static_cast<Engine*>(data)->aborted();
}

msr_result fail(Engine* eng, msr_result code, const char* message) {
    if (eng) eng->last_error = message ? message : "unknown error";
    return code;
}

long long read_kib(const char* path, const char* key) {
    (void) path;
    if (FILE* fh = std::fopen("/proc/self/status", "r")) {
        char line[256];
        while (std::fgets(line, sizeof(line), fh)) {
            if (std::strncmp(line, key, std::strlen(key)) == 0) {
                const long long v = std::atoll(line + std::strlen(key));
                std::fclose(fh);
                return v;
            }
        }
        std::fclose(fh);
    }
    return -1;
}

int32_t read_thermal_c() {
    int hottest_mc = -1;
    for (int i = 0; i < 32; ++i) {
        char path[128];
        std::snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", i);
        if (FILE* fh = std::fopen(path, "r")) {
            int mc = -1;
            if (std::fscanf(fh, "%d", &mc) == 1 && mc > hottest_mc) hottest_mc = mc;
            std::fclose(fh);
        }
    }
    return hottest_mc < 0 ? -1 : hottest_mc / 1000;
}

std::string format_prompt(const Engine* eng, const char* text) {
    if (!text) return {};
    const char* tmpl = llama_model_chat_template(eng->model, nullptr);
    if (!tmpl || !*tmpl) return text;

    llama_chat_message msg{};
    msg.role = "user";
    msg.content = text;

    const int need = llama_chat_apply_template(tmpl, &msg, 1, true, nullptr, 0);
    if (need <= 0) return text;
    std::vector<char> buf(static_cast<std::size_t>(need) + 1);
    const int written = llama_chat_apply_template(tmpl, &msg, 1, true, buf.data(),
                                                  static_cast<int32_t>(buf.size()));
    if (written <= 0) return text;
    return std::string(buf.data(), static_cast<std::size_t>(written));
}

std::vector<llama_token> tokenize(const Engine* eng, const std::string& text) {
    std::vector<llama_token> tokens(text.size() + 16);
    int n = llama_tokenize(eng->vocab, text.data(), static_cast<int32_t>(text.size()),
                           tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    if (n < 0) {
        tokens.resize(static_cast<std::size_t>(-n));
        n = llama_tokenize(eng->vocab, text.data(), static_cast<int32_t>(text.size()),
                           tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    }
    if (n < 0) throw std::runtime_error("prompt tokenization failed");
    tokens.resize(static_cast<std::size_t>(n));
    return tokens;
}

}  // namespace

extern "C" {

MSR_API msr_result msr_engine_create(msr_engine** out) {
    if (!out) return MSR_INVALID_ARGUMENT;
    *out = reinterpret_cast<msr_engine*>(new Engine());
    return MSR_OK;
}

MSR_API void msr_engine_destroy(msr_engine* engine) {
    delete reinterpret_cast<Engine*>(engine);
}

MSR_API msr_result msr_engine_load(msr_engine* engine, const msr_engine_config* config) {
    Engine* eng = reinterpret_cast<Engine*>(engine);
    if (!eng || !config || !config->model_path || !*config->model_path) {
        return fail(eng, MSR_INVALID_ARGUMENT, "engine and model_path are required");
    }
    try {
        if (eng->model) throw std::runtime_error("model already loaded");

        // The provider reads these at first model load; see LIMITATION.md B2.
        msr_external_configure(config->model_path, config->budget_bytes,
                               config->prefetch ? 1 : 0,
                               static_cast<int>(config->read_threads));
        setenv("MSR_NO_ALLOC", "1", 1);

        static std::once_flag backend_once;
        std::call_once(backend_once, [] { llama_backend_init(); });

        llama_model_params mparams = llama_model_default_params();
        mparams.n_gpu_layers = 0;
        mparams.load_mode = LLAMA_LOAD_MODE_NONE;  // == --load-mode none: no whole-file mmap
        mparams.lazy_mode = LLAMA_LAZY_MODE_OFF;
        mparams.use_extra_bufts = false;           // == --no-repack
        mparams.vocab_only = false;

        eng->model = llama_model_load_from_file(config->model_path, mparams);
        if (!eng->model) throw std::runtime_error("llama_model_load_from_file failed");
        eng->vocab = llama_model_get_vocab(eng->model);

        llama_context_params cparams = llama_context_default_params();
        cparams.n_ctx = config->context_tokens ? config->context_tokens : 2048;
        cparams.n_batch = 512;
        cparams.n_threads = config->threads ? config->threads : 4;
        cparams.n_threads_batch = cparams.n_threads;
        cparams.abort_callback = abort_cb;
        cparams.abort_callback_data = eng;

        eng->ctx = llama_init_from_model(eng->model, cparams);
        if (!eng->ctx) throw std::runtime_error("llama_init_from_model failed");
        eng->last_error.clear();
        return MSR_OK;
    } catch (const std::exception& e) {
        return fail(eng, MSR_MODEL_ERROR, e.what());
    }
}

MSR_API msr_result msr_engine_generate(msr_engine* engine, const char* prompt,
                                       uint32_t max_tokens, float temperature, int32_t seed,
                                       msr_token_fn on_token, void* user_data) {
    Engine* eng = reinterpret_cast<Engine*>(engine);
    if (!eng || !prompt || !max_tokens) {
        return fail(eng, MSR_INVALID_ARGUMENT, "engine, prompt and max_tokens are required");
    }
    if (!eng->model || !eng->ctx) {
        return fail(eng, MSR_NOT_LOADED, "load a model before generating");
    }

    std::lock_guard<std::mutex> lock(eng->generate_mutex);
    eng->cancel.store(false);
    eng->stats.prompt_tokens = eng->stats.generated_tokens = 0;
    eng->stats.ttft_ms = eng->stats.prompt_ms = eng->stats.generate_ms = 0;
    eng->last_error.clear();

    llama_sampler* smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    if (!smpl) return fail(eng, MSR_INTERNAL_ERROR, "sampler init failed");
    if (temperature <= 0.0f) {
        llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
    } else {
        llama_sampler_chain_add(smpl, llama_sampler_init_temp(temperature));
        llama_sampler_chain_add(smpl, llama_sampler_init_dist(static_cast<uint32_t>(seed)));
    }

    msr_result result = MSR_OK;
    try {
        const std::string text = format_prompt(eng, prompt);
        const std::vector<llama_token> tokens = tokenize(eng, text);
        const int32_t n_ctx = static_cast<int32_t>(llama_n_ctx(eng->ctx));
        if (static_cast<int32_t>(tokens.size()) + static_cast<int32_t>(max_tokens) > n_ctx) {
            throw std::runtime_error("prompt + max_tokens exceeds context");
        }

        llama_memory_clear(llama_get_memory(eng->ctx), true);
        const auto prompt_start = Clock::now();
        const int32_t batch_size = std::min<int32_t>(512, n_ctx);
        for (size_t off = 0; off < tokens.size(); off += static_cast<size_t>(batch_size)) {
            if (eng->aborted()) break;
            const int32_t chunk = static_cast<int32_t>(
                std::min<size_t>(batch_size, tokens.size() - off));
            llama_batch batch = llama_batch_get_one(
                const_cast<llama_token*>(tokens.data()) + off, chunk);
            if (llama_decode(eng->ctx, batch) != 0) {
                if (eng->aborted()) break;
                throw std::runtime_error("prompt decode failed");
            }
        }
        eng->stats.prompt_tokens = tokens.size();
        eng->stats.prompt_ms = static_cast<uint64_t>(ms_since(prompt_start));

        const auto gen_start = Clock::now();
        for (uint32_t i = 0; i < max_tokens; ++i) {
            if (eng->aborted()) break;
            llama_token token = llama_sampler_sample(smpl, eng->ctx, -1);
            llama_sampler_accept(smpl, token);
            if (llama_vocab_is_eog(eng->vocab, token)) break;

            if (eng->stats.generated_tokens == 0) {
                eng->stats.ttft_ms = static_cast<uint64_t>(ms_since(prompt_start));
            }

            char buf[256];
            int32_t n = llama_token_to_piece(eng->vocab, token, buf,
                                             static_cast<int32_t>(sizeof(buf)), 0, false);
            std::string piece;
            if (n >= 0) {
                piece.assign(buf, static_cast<size_t>(n));
            } else {
                std::vector<char> big(static_cast<size_t>(-n));
                n = llama_token_to_piece(eng->vocab, token, big.data(),
                                         static_cast<int32_t>(big.size()), 0, false);
                if (n > 0) piece.assign(big.data(), static_cast<size_t>(n));
            }
            ++eng->stats.generated_tokens;

            if (on_token && on_token(piece.data(), piece.size(), user_data) != 0) {
                eng->cancel.store(true);
            }
            if (eng->aborted()) break;

            if (i + 1 < max_tokens) {
                llama_batch batch = llama_batch_get_one(&token, 1);
                if (llama_decode(eng->ctx, batch) != 0) {
                    if (eng->aborted()) break;
                    throw std::runtime_error("decode failed");
                }
            }
        }
        eng->stats.generate_ms = static_cast<uint64_t>(ms_since(gen_start));
        if (eng->aborted()) result = MSR_ABORTED;
    } catch (const std::exception& e) {
        result = fail(eng, MSR_INTERNAL_ERROR, e.what());
    }
    llama_sampler_free(smpl);
    return result;
}

MSR_API msr_result msr_engine_cancel(msr_engine* engine) {
    Engine* eng = reinterpret_cast<Engine*>(engine);
    if (!eng) return MSR_INVALID_ARGUMENT;
    eng->cancel.store(true);
    msr_streaming_cancel();  // drops queued prefetches; in-flight reads finish
    return MSR_OK;
}

MSR_API msr_result msr_engine_get_stats(const msr_engine* engine, msr_engine_stats* out) {
    const Engine* eng = reinterpret_cast<const Engine*>(engine);
    if (!eng || !out) return MSR_INVALID_ARGUMENT;

    msr_external_stats weights{};
    msr_external_get_stats(&weights);

    *out = eng->stats;
    out->budget_bytes = weights.budget_bytes;
    out->resident_weight_bytes = weights.resident_weight_bytes;
    out->peak_weight_bytes = weights.peak_weight_bytes;
    out->read_bytes = weights.read_bytes;
    out->streamed_layers = static_cast<uint32_t>(weights.streamed_layers);
    out->stream_slots = static_cast<uint32_t>(weights.stream_slots);
    out->prefetch = static_cast<uint32_t>(weights.prefetch);
    out->read_threads = static_cast<uint32_t>(weights.read_threads);
    out->peak_vm_hwm_kb = static_cast<uint64_t>(read_kib(nullptr, "VmHWM:"));
    out->peak_vm_rss_kb = static_cast<uint64_t>(read_kib(nullptr, "VmRSS:"));
    out->thermal_c = read_thermal_c();
    return MSR_OK;
}

MSR_API const char* msr_engine_last_error(const msr_engine* engine) {
    const Engine* eng = reinterpret_cast<const Engine*>(engine);
    return eng ? eng->last_error.c_str() : nullptr;
}

MSR_API const char* msr_engine_version(void) {
    return "0.1.0-framework";
}

}  // extern "C"

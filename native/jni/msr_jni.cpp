// JNI bridge for the streaming runtime. Stable Java/Kotlin surface; no llama,
// ggml or provider types cross the boundary.
//
// The provider is linked STATICALLY into this library: the weak hooks in the
// vendored llama.cpp resolve to the strong definitions here, so an app does
// not need LD_PRELOAD (which is unavailable on Android).

#include <jni.h>

#include <llama.h>

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

#include <msr/msr_external_abi.h>

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
    std::string last_error;
    std::mutex generate_mutex;

    long long prompt_tokens = 0;
    long long gen_tokens = 0;
    long long ttft_ms = 0;
    long long prompt_ms = 0;
    long long gen_ms = 0;

    bool abort_now() const { return cancel.load(); }
};

bool abort_cb(void* data) {
    return static_cast<Engine*>(data)->abort_now();
}

Engine* handle_of(JNIEnv* env, jlong handle) {
    if (handle == 0) {
        jclass c = env->FindClass("java/lang/IllegalStateException");
        if (c) env->ThrowNew(c, "engine is closed");
        return nullptr;
    }
    return reinterpret_cast<Engine*>(handle);
}

void throw_java(JNIEnv* env, Engine* eng, const char* what) {
    if (eng) eng->last_error = what ? what : "unknown error";
    jclass c = env->FindClass("java/lang/IllegalStateException");
    if (c) {
        env->ThrowNew(c, eng ? eng->last_error.c_str() : (what ? what : "unknown error"));
    }
}

std::string jstring_to_utf8(JNIEnv* env, jstring s) {
    if (!s) return {};
    const jsize n = env->GetStringUTFLength(s);
    std::string out(static_cast<std::size_t>(n), '\0');
    env->GetStringUTFRegion(s, 0, n, &out[0]);
    return out;
}

struct ProcessMem {
    long long vmhwm_kb = -1;
    long long vmrss_kb = -1;
};

ProcessMem read_process_mem() {
    ProcessMem out;
    if (FILE* fh = std::fopen("/proc/self/status", "r")) {
        char line[256];
        while (std::fgets(line, sizeof(line), fh)) {
            if (std::strncmp(line, "VmHWM:", 6) == 0) {
                out.vmhwm_kb = std::atoll(line + 6);
            } else if (std::strncmp(line, "VmRSS:", 6) == 0) {
                out.vmrss_kb = std::atoll(line + 6);
            }
        }
        std::fclose(fh);
    }
    return out;
}

int read_thermal_c() {
    // Optional: many devices deny thermal-zone access from app context.
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

std::string format_prompt(const Engine* eng, const std::string& text) {
    const char* tmpl = llama_model_chat_template(eng->model, nullptr);
    if (!tmpl || !*tmpl) return text;

    llama_chat_message msg{};
    msg.role = "user";
    msg.content = text.c_str();

    int need = llama_chat_apply_template(tmpl, &msg, 1, true, nullptr, 0);
    if (need <= 0) return text;
    std::vector<char> buf(static_cast<std::size_t>(need) + 1);
    int written = llama_chat_apply_template(tmpl, &msg, 1, true, buf.data(),
                                            static_cast<int32_t>(buf.size()));
    if (written <= 0) return text;
    return std::string(buf.data(), static_cast<std::size_t>(written));
}

std::vector<llama_token> tokenize_prompt(const Engine* eng, const std::string& text) {
    const llama_vocab* vocab = eng->vocab;
    std::vector<llama_token> tokens(text.size() + 16);
    int n = llama_tokenize(vocab, text.data(), static_cast<int32_t>(text.size()),
                           tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    if (n < 0) {
        tokens.resize(static_cast<std::size_t>(-n));
        n = llama_tokenize(vocab, text.data(), static_cast<int32_t>(text.size()),
                           tokens.data(), static_cast<int32_t>(tokens.size()), true, true);
    }
    if (n < 0) throw std::runtime_error("prompt tokenization failed");
    tokens.resize(static_cast<std::size_t>(n));
    return tokens;
}

void generate_impl(JNIEnv* env, Engine* eng, jstring jprompt, jint max_tokens,
                   jfloat temperature, jint seed, jobject sink) {
    if (!eng->model || !eng->ctx) throw std::runtime_error("model is not loaded");
    if (max_tokens <= 0) throw std::runtime_error("max_tokens must be positive");
    if (!sink) throw std::runtime_error("token sink is required");

    jclass sink_class = env->GetObjectClass(sink);
    jmethodID on_token = env->GetMethodID(sink_class, "onToken", "(Ljava/lang/String;)Z");
    if (!on_token) throw std::runtime_error("sink has no onToken(String) method");

    eng->cancel.store(false);
    eng->prompt_tokens = eng->gen_tokens = 0;
    eng->ttft_ms = eng->prompt_ms = eng->gen_ms = 0;

    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    llama_sampler* smpl = llama_sampler_chain_init(sparams);
    if (!smpl) throw std::runtime_error("sampler init failed");
    if (temperature <= 0.0f) {
        llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
    } else {
        llama_sampler_chain_add(smpl, llama_sampler_init_temp(temperature));
        llama_sampler_chain_add(smpl, llama_sampler_init_dist(static_cast<uint32_t>(seed)));
    }

    try {
        const std::string prompt = format_prompt(eng, jstring_to_utf8(env, jprompt));
        std::vector<llama_token> tokens = tokenize_prompt(eng, prompt);
        const int32_t n_ctx = static_cast<int32_t>(llama_n_ctx(eng->ctx));
        if (static_cast<int32_t>(tokens.size()) + max_tokens > n_ctx) {
            throw std::runtime_error("prompt + max_tokens exceeds context size");
        }

        llama_memory_clear(llama_get_memory(eng->ctx), true);
        const auto prompt_start = Clock::now();
        const int32_t batch_size = std::min<int32_t>(512, n_ctx);
        for (size_t off = 0; off < tokens.size(); off += static_cast<size_t>(batch_size)) {
            if (eng->abort_now()) break;
            const int32_t chunk = static_cast<int32_t>(
                std::min<size_t>(batch_size, tokens.size() - off));
            llama_batch batch = llama_batch_get_one(tokens.data() + off, chunk);
            if (llama_decode(eng->ctx, batch) != 0) {
                if (eng->abort_now()) break;  // graceful cancellation
                throw std::runtime_error("prompt decode failed (out of context or compute error)");
            }
        }
        eng->prompt_tokens = static_cast<long long>(tokens.size());
        eng->prompt_ms = ms_since(prompt_start);

        const auto gen_start = Clock::now();
        for (jint i = 0; i < max_tokens; ++i) {
            if (eng->abort_now()) break;
            llama_token token = llama_sampler_sample(smpl, eng->ctx, -1);
            llama_sampler_accept(smpl, token);
            if (llama_vocab_is_eog(eng->vocab, token)) break;

            if (eng->gen_tokens == 0) eng->ttft_ms = ms_since(prompt_start);

            char piece_buf[256];
            int32_t n = llama_token_to_piece(eng->vocab, token, piece_buf,
                                             static_cast<int32_t>(sizeof(piece_buf)), 0, false);
            std::string piece;
            if (n >= 0) {
                piece.assign(piece_buf, static_cast<size_t>(n));
            } else {
                std::vector<char> big(static_cast<size_t>(-n));
                n = llama_token_to_piece(eng->vocab, token, big.data(),
                                         static_cast<int32_t>(big.size()), 0, false);
                if (n > 0) piece.assign(big.data(), static_cast<size_t>(n));
            }
            ++eng->gen_tokens;

            jstring jpiece = env->NewStringUTF(piece.c_str());
            const jboolean keep_going = env->CallBooleanMethod(sink, on_token, jpiece);
            env->DeleteLocalRef(jpiece);
            if (env->ExceptionCheck()) throw std::runtime_error("token sink threw");
            if (!keep_going || eng->abort_now()) {
                eng->cancel.store(true);
                break;
            }

            if (i + 1 < max_tokens) {
                llama_batch batch = llama_batch_get_one(&token, 1);
                if (llama_decode(eng->ctx, batch) != 0) {
                    if (eng->abort_now()) break;  // graceful cancellation
                    throw std::runtime_error("decode failed (out of context or compute error)");
                }
            }
        }
        eng->gen_ms = ms_since(gen_start);
    } catch (...) {
        llama_sampler_free(smpl);
        throw;
    }
    llama_sampler_free(smpl);
}

}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL Java_dev_msr_engine_MsrEngine_nativeCreate(JNIEnv*, jobject) {
    return reinterpret_cast<jlong>(new Engine());
}

JNIEXPORT void JNICALL Java_dev_msr_engine_MsrEngine_nativeDestroy(JNIEnv*, jobject, jlong h) {
    delete reinterpret_cast<Engine*>(h);
}

JNIEXPORT void JNICALL Java_dev_msr_engine_MsrEngine_nativeLoad(
        JNIEnv* env, jobject, jlong h, jstring jpath, jlong budget_bytes, jint ctx_tokens,
        jint threads, jboolean prefetch, jint read_threads) {
    Engine* eng = handle_of(env, h);
    if (!eng) return;
    try {
        if (eng->model) throw std::runtime_error("model already loaded");
        const std::string path = jstring_to_utf8(env, jpath);
        if (path.empty()) throw std::runtime_error("empty model path");

        // The provider reads these at first model load. There is no LD_PRELOAD
        // in an app: the hooks are linked into this shared object.
        msr_external_configure(path.c_str(), static_cast<unsigned long long>(budget_bytes),
                               prefetch ? 1 : 0, read_threads);
        setenv("MSR_NO_ALLOC", "1", 1);

        static std::once_flag backend_once;
        std::call_once(backend_once, [] { llama_backend_init(); });

        llama_model_params mparams = llama_model_default_params();
        mparams.n_gpu_layers = 0;
        mparams.use_extra_bufts = false;  // == CLI --no-repack
        mparams.vocab_only = false;

        eng->model = llama_model_load_from_file(path.c_str(), mparams);
        if (!eng->model) throw std::runtime_error("llama_model_load_from_file failed");
        eng->vocab = llama_model_get_vocab(eng->model);

        llama_context_params cparams = llama_context_default_params();
        cparams.n_ctx = static_cast<uint32_t>(ctx_tokens > 0 ? ctx_tokens : 2048);
        cparams.n_batch = 512;
        cparams.n_threads = threads > 0 ? threads : 4;
        cparams.n_threads_batch = cparams.n_threads;
        cparams.abort_callback = abort_cb;
        cparams.abort_callback_data = eng;

        eng->ctx = llama_init_from_model(eng->model, cparams);
        if (!eng->ctx) throw std::runtime_error("llama_init_from_model failed");
    } catch (const std::exception& e) {
        throw_java(env, eng, e.what());
    }
}

JNIEXPORT void JNICALL Java_dev_msr_engine_MsrEngine_nativeGenerate(
        JNIEnv* env, jobject, jlong h, jstring prompt, jint max_tokens, jfloat temperature,
        jint seed, jobject sink) {
    Engine* eng = handle_of(env, h);
    if (!eng) return;
    std::lock_guard<std::mutex> lock(eng->generate_mutex);
    try {
        generate_impl(env, eng, prompt, max_tokens, temperature, seed, sink);
    } catch (const std::exception& e) {
        throw_java(env, eng, e.what());
    }
}

JNIEXPORT void JNICALL Java_dev_msr_engine_MsrEngine_nativeCancel(JNIEnv* env, jobject, jlong h) {
    Engine* eng = handle_of(env, h);
    if (!eng) return;
    eng->cancel.store(true);
    msr_streaming_cancel();  // drop queued prefetches; in-flight reads finish
}

JNIEXPORT jlongArray JNICALL Java_dev_msr_engine_MsrEngine_nativeStats(JNIEnv* env, jobject, jlong h) {
    Engine* eng = handle_of(env, h);
    if (!eng) return nullptr;
    msr_external_stats w{};
    msr_external_get_stats(&w);
    const ProcessMem mem = read_process_mem();
    const jlong values[] = {
        eng->prompt_tokens,
        eng->gen_tokens,
        eng->ttft_ms,
        eng->prompt_ms,
        eng->gen_ms,
        static_cast<jlong>(w.budget_bytes),
        static_cast<jlong>(w.resident_weight_bytes),
        static_cast<jlong>(w.peak_weight_bytes),
        static_cast<jlong>(w.read_bytes),
        w.streamed_layers,
        w.stream_slots,
        w.prefetch,
        w.read_threads,
        mem.vmhwm_kb,
        mem.vmrss_kb,
        read_thermal_c(),
    };
    jlongArray out = env->NewLongArray(static_cast<jsize>(sizeof(values) / sizeof(values[0])));
    if (out) {
        env->SetLongArrayRegion(out, 0,
                                static_cast<jsize>(sizeof(values) / sizeof(values[0])), values);
    }
    return out;
}

JNIEXPORT jstring JNICALL Java_dev_msr_engine_MsrEngine_nativeLastError(JNIEnv* env, jobject, jlong h) {
    Engine* eng = handle_of(env, h);
    if (!eng) return nullptr;
    return env->NewStringUTF(eng->last_error.c_str());
}

}  // extern "C"

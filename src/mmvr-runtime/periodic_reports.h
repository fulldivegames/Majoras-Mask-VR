#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace mmvr {
// Diagnostic data only: no renderer, game, settings or runtime pointers may be
// captured. A stalled disk/debugger must never make frame submission wait.
template<class Report, size_t Capacity = 4> class AsyncReportQueue {
    static_assert(Capacity > 0);
    std::function<void(const Report&)> consume;
    std::array<std::optional<Report>, Capacity> pending;
    std::mutex mutex;
    std::condition_variable ready;
    size_t first = 0, count = 0;
    bool stopping = false;
    std::atomic<size_t> dropped{0}, failed{0};
    std::thread worker;

    void Run() noexcept {
        for (;;) {
            std::optional<Report> report;
            {
                std::unique_lock lock(mutex);
                ready.wait(lock, [&] { return stopping || count; });
                if (!count) return;
                report = std::move(pending[first]);
                pending[first].reset();
                first = (first + 1) % Capacity;
                --count;
            }
            try { consume(*report); }
            catch (...) { failed.fetch_add(1, std::memory_order_relaxed); }
        }
    }
public:
    explicit AsyncReportQueue(std::function<void(const Report&)> sink)
        : consume(std::move(sink)), worker([this] { Run(); }) {}
    AsyncReportQueue(const AsyncReportQueue&) = delete;
    AsyncReportQueue& operator=(const AsyncReportQueue&) = delete;
    ~AsyncReportQueue() {
        { std::lock_guard lock(mutex); stopping = true; }
        ready.notify_one();
        worker.join(); // Only teardown waits; drain accepted reports in order.
    }
    bool Submit(Report report) noexcept {
        try {
            std::unique_lock lock(mutex, std::try_to_lock);
            if (!lock.owns_lock() || stopping || count == Capacity) {
                dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            pending[(first + count) % Capacity].emplace(std::move(report));
            ++count;
            lock.unlock();
            ready.notify_one();
            return true;
        } catch (...) {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }
    size_t Dropped() const noexcept { return dropped.load(std::memory_order_relaxed); }
    size_t Failed() const noexcept { return failed.load(std::memory_order_relaxed); }
};

struct PeriodicReport {
    std::string metrics, profileBeforeRss, profileAfterRss;
    std::vector<std::string> messages;
};

class PeriodicReportFiles {
    std::filesystem::path directory;
    std::string sessionToken;
    std::function<void(const std::string&)> debug;
    bool profileHeaderWritten = false;

    void ReplaceMetrics(const std::string& text) const {
        const auto target = directory / "mmvr-frame-timing.json";
        const auto temporary = directory / "mmvr-frame-timing.json.pending";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file << text;
            file.close();
            if (!file) throw std::runtime_error("Diagnostic snapshot write failed");
        }
        // Readers see the previous complete report or the new complete report.
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING))
            throw std::runtime_error("Diagnostic snapshot replacement failed");
#else
        std::filesystem::rename(temporary, target);
#endif
    }
public:
    static constexpr const char* ProfileHeader =
        "scene,nativeFrame,actors,nativeLogicDrawMs,focused,submitHz,cpuMeanMs,cpuP95Ms,gpuEyeUiMeanMs,gpuSamples,rssKiB,materialCacheEntries,sampleFrames,appWorkMaxMs,workOverBudgetFrames,workOver2xBudgetFrames,workOver50MsFrames\n";
    PeriodicReportFiles(std::filesystem::path path, std::string token,
                        std::function<void(const std::string&)> debugSink)
        : directory(std::move(path)), sessionToken(std::move(token)), debug(std::move(debugSink)) {}
    void Write(const PeriodicReport& report) {
        ReplaceMetrics(report.metrics);
        long rss = 0;
#ifdef __ANDROID__
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream value(line.substr(6)); value >> rss; break;
        }
#endif
        std::ofstream profile(directory / "mmvr-scene-performance.csv", std::ios::app);
        if (!profileHeaderWritten) {
            profile << ProfileHeader;
            if (!sessionToken.empty())
                std::ofstream(directory / "mmvr-scene-performance-session.txt") << sessionToken;
        }
        profile << report.profileBeforeRss << rss << report.profileAfterRss;
        profile.close();
        if (!profile) throw std::runtime_error("Diagnostic profile write failed");
        profileHeaderWritten = true;
        std::ofstream log(directory / "mmvr.log", std::ios::app);
        for (const auto& message : report.messages) {
            log << message << '\n';
            if (debug) debug(message);
        }
        log.close();
        if (!log) throw std::runtime_error("Diagnostic log write failed");
    }
};

class PeriodicReportWriter {
    PeriodicReportFiles files; // Remains alive while queue destructor drains.
    AsyncReportQueue<PeriodicReport> queue;
public:
    PeriodicReportWriter(std::filesystem::path path, std::string token,
                         std::function<void(const std::string&)> debug)
        : files(std::move(path), std::move(token), std::move(debug)),
          queue([this](const PeriodicReport& report) { files.Write(report); }) {}
    bool Submit(PeriodicReport report) noexcept { return queue.Submit(std::move(report)); }
    size_t Dropped() const noexcept { return queue.Dropped(); }
    size_t Failed() const noexcept { return queue.Failed(); }
};
} // namespace mmvr

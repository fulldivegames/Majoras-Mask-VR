#pragma once
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace SaveFileIO {
// A failed write must not truncate the only usable ordinary save. The staging
// file lives beside its destination so replacement never crosses filesystems.
inline void Write(const std::filesystem::path& destination, const std::string& bytes) {
    static std::atomic<unsigned long long> serial{0};
    std::filesystem::create_directories(destination.parent_path());
    auto stage=destination;
#ifdef _WIN32
    const auto process=GetCurrentProcessId();
#else
    const auto process=getpid();
#endif
    stage += ".pending-" + std::to_string(process) + "-" + std::to_string(++serial);
    if (std::filesystem::is_symlink(destination) || std::filesystem::exists(stage))
        throw std::runtime_error("Unsafe save destination");
#ifdef _WIN32
    FILE* file=_wfopen(stage.c_str(),L"wbx");
#else
    FILE* file=std::fopen(stage.c_str(),"wbx");
#endif
    if (!file) throw std::runtime_error("Cannot create save staging file");
    bool ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();
    ok=std::fflush(file)==0 && ok;
#ifdef _WIN32
    ok=_commit(_fileno(file))==0 && ok;
#else
    ok=fsync(fileno(file))==0 && ok;
#endif
    ok=std::fclose(file)==0 && ok;
    if (ok) {
#ifdef _WIN32
        ok=MoveFileExW(stage.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        ok=std::rename(stage.c_str(),destination.c_str())==0;
#endif
    }
    if (!ok) {
        std::error_code ignored;
        std::filesystem::remove(stage,ignored);
        throw std::runtime_error("Save write failed; previous file retained");
    }
}
}

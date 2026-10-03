#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND) && defined(MMVR_LOCAL_TEST_TOOLS)
#include "NativeStateVisitor.h"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace mmvrgame {
// Private phase fixtures exercise the same manual block visitors used by the
// archive. Each snapshot restores the caller's live values on every exit path.
class NativePhaseCheckSnapshot {
    struct Field {
        std::string id;
        void* address;
        std::vector<unsigned char> bytes;
    };
    std::vector<Field> fields;
    static void Capture(void* context, const char* id, void* address, size_t bytes) {
        auto& self = *static_cast<NativePhaseCheckSnapshot*>(context);
        if (!id || !*id || !address || !bytes || bytes > 64) {
            throw std::runtime_error("Invalid scalar phase field");
        }
        for (const auto& field : self.fields) {
            if (field.id == id || field.address == address) throw std::runtime_error("Duplicate scalar phase field");
        }
        Field field{id, address, std::vector<unsigned char>(bytes)};
        std::memcpy(field.bytes.data(), address, bytes);
        self.fields.push_back(std::move(field));
    }
public:
    explicit NativePhaseCheckSnapshot(void (*visit)(MMVR_StateSink*)) {
        MMVR_StateSink sink{};
        sink.context = this;
        sink.block = Capture;
        visit(&sink);
        if (fields.empty()) throw std::runtime_error("Missing native phase adapter");
    }
    NativePhaseCheckSnapshot(const NativePhaseCheckSnapshot&) = delete;
    NativePhaseCheckSnapshot& operator=(const NativePhaseCheckSnapshot&) = delete;
    ~NativePhaseCheckSnapshot() { Restore(); }
    void Restore() const noexcept {
        for (const auto& field : fields) std::memcpy(field.address, field.bytes.data(), field.bytes.size());
    }
    size_t Count() const { return fields.size(); }
};
}
#endif

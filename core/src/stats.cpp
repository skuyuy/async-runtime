#include <atomic>

#include <asyncrt/core/stats.hpp>

namespace asyncrt::core {

static std::atomic_size_t frames{0};

void add_frame() {
    const auto curr_frames = frames.load();
    frames.fetch_add(1);
}

void remove_frame() {
    const auto curr_frames = frames.load();
    frames.fetch_sub(1);
}

bool check() {
    const auto curr_frames = frames.load();
    return frames == 0;
}

}
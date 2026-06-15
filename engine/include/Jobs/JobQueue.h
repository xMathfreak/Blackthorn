#pragma once

#include <cassert>
#include <deque>
#include <memory>

#include "Core/Export.h"
#include "Jobs/Job.h"

namespace Blackthorn::Jobs {

class BLACKTHORN_API JobQueue {
public:
    JobQueue() = default;

    bool push(std::unique_ptr<Job> job) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push_back(std::move(job));
        return true;
    }

    std::unique_ptr<Job> pop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) return nullptr;
        auto job = std::move(queue.front());
        queue.pop_front();
        return job;
    }

    std::unique_ptr<Job> steal() {
        if (queue.empty())
            return nullptr;

        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty())
            return nullptr;

        auto job = std::move(queue.back());
        queue.pop_back();
        return job;
    }

private:
    std::deque<std::unique_ptr<Job>> queue;
    std::mutex mtx;
};

} // namespace Blackthorn::Jobs
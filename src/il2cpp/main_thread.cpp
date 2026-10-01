#include "pch.h"

#include "main_thread.h"
#include "il2cpp.h"
#include "core/logger.h"

#include <sstream>

std::thread::id MainThread::boundId{};
std::atomic_bool MainThread::bound{ false };
std::deque<MainThread::Task> MainThread::queue{};
std::mutex MainThread::queueMutex{};

void MainThread::BindCurrentThread() {
    if (bound.load(std::memory_order_relaxed)) return;

    boundId = std::this_thread::get_id();
    bound.store(true, std::memory_order_release);

    std::ostringstream stream;
    stream << "[MainThread] Bound script thread " << boundId;
    Logger::Log(stream.str());
}

void MainThread::Pump() {
    BindCurrentThread();

    std::deque<Task> pending;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (queue.empty()) return;
        pending.swap(queue);
    }

    Il2Cpp::ThreadAttach();
    for (Task& task : pending) {
        if (task) task();
    }
}

void MainThread::Post(Task task) {
    if (!task) return;

    if (!MainThreadKnown() || IsCurrentThread()) {
        Il2Cpp::ThreadAttach();
        task();
        return;
    }

    std::lock_guard<std::mutex> lock(queueMutex);
    queue.push_back(std::move(task));
}

bool MainThread::MainThreadKnown() {
    return bound.load(std::memory_order_acquire);
}

bool MainThread::IsCurrentThread() {
    return MainThreadKnown() && std::this_thread::get_id() == boundId;
}

size_t MainThread::PendingCount() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return queue.size();
}

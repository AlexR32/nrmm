#pragma once
#include "pch.h"

#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

// Unity owns its managed state on the thread that runs the player loop (the
// "main"/script thread); ImGui renders from the D3D11 Present hook on a
// different thread, so invoking game functions from there races the player loop

// The menu posts a callable with Post() and the GodConstant.Update hook calls
// Pump() once a frame to run everything queued. Post() runs inline when it is
// already on the script thread

class MainThread {
public:
    using Task = std::function<void()>;

    // Runs on the game's script thread. Binds that thread on the first call,
    // then executes everything queued since the previous frame
    static void Pump();

    // Runs a callable on the script thread. If the caller is already on it the
    // task runs immediately, otherwise it is queued for the next Pump(). It is
    // never run inline from another thread, so a render-thread caller cannot
    // end up invoking managed code by accident; early tasks simply wait until
    // the script thread is bound
    static void Post(Task task);

    // True once the game hook has run at least once and identified the thread
    // that owns the managed state
    static bool MainThreadKnown();

    static bool IsCurrentThread();
    static size_t PendingCount();

private:
    static void BindCurrentThread();

    static constexpr size_t kMaxQueuedTasks = 256;

    static std::thread::id boundId;
    static std::atomic_bool bound;
    static std::deque<Task> queue;
    static std::mutex queueMutex;
};

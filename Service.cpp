//
// Copyright (C) 2020 Assured Information Security, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Service.hpp"
#include "Args.hpp"
#include "BlkBackend.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <windows.h>

/* Time to wait on pending state transitions (in miliseconds) */
static constexpr DWORD SERVICE_WAIT_HINT = 10000;

/* Mask for accepting no service controls */
static constexpr DWORD SERVICE_ACCEPT_NONE = 0;

static constexpr char *SERVICE_NAME = "us-blkback";

static SERVICE_STATUS service_status;
static SERVICE_STATUS_HANDLE service_handle;
static HANDLE service_stop_event;
static std::mutex service_mutex;
static std::atomic_bool stop_event_ready;
static HANDLE blkback_thread;
static std::mutex log_mutex;

DWORD service_ctrl_handler(DWORD ctrl_code,
                           DWORD event_type,
                           LPVOID event_data,
                           LPVOID context);
static void mark_checkpoint();
static void service_wait_for_stop_signal() noexcept;

static void log(const char *fmt, ...) noexcept
{
    try {
        constexpr uint32_t LOG_MSG_SIZE = 256U;
        char msg[LOG_MSG_SIZE];
        va_list args;

        std::lock_guard lock(log_mutex);

        va_start(args, fmt);
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        OutputDebugString(msg);
    } catch (std::exception &e) {
        std::cerr << __func__ << ": failed (what=" << e.what() << ")\n";
    }
}

static bool init()
{
    std::lock_guard lock(service_mutex);
    stop_event_ready.store(false, std::memory_order_release);

    service_handle = RegisterServiceCtrlHandlerEx(SERVICE_NAME,
                                                  service_ctrl_handler,
                                                  NULL);
    if (!service_handle) {
        log("%s: failed to register ctrl handler (err=0x%x)\n",
                __func__, GetLastError());
        return false;
    }

    ZeroMemory(&service_status, sizeof(service_status));

    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwWaitHint = SERVICE_WAIT_HINT;

    return true;
}

static void wait_on_blkback_thread() noexcept
{
    DWORD wait_time = 0;

    while (1) {
        constexpr DWORD TIMEOUT = 200; /* miliseconds */

        DWORD ret = WaitForSingleObject(blkback_thread, TIMEOUT);
        wait_time += TIMEOUT;

        if (ret == WAIT_TIMEOUT && wait_time < SERVICE_WAIT_HINT) {
            mark_checkpoint();
            continue;
        }

        return;
    }
}

static void send_stop_signal()
{
    if (!stop_event_ready.load(std::memory_order_acquire)) {
        log("%s: stop event not ready!\n", __func__);
        return;
    }

    if (!SetEvent(service_stop_event)) {
        log("%s: failed to signal stop event (err=0x%x)\n",
                __func__, GetLastError());
    }
}

// Caller must hold service_mutex
static bool __set_status()
{
    if (!SetServiceStatus(service_handle, &service_status)) {
        log("%s: failed to set status (err=0x%x)\n", __func__, GetLastError());
        return false;
    }

    return true;
}

static bool set_status(const DWORD ctrls,
                       const DWORD state,
                       const DWORD exit_code)
{
    std::lock_guard lock(service_mutex);

    service_status.dwControlsAccepted = ctrls;
    service_status.dwCurrentState = state;
    service_status.dwWin32ExitCode = exit_code;
    service_status.dwCheckPoint = 0;

    return __set_status();
}

static void stop_with_error(const DWORD exit_code)
{
    constexpr DWORD ctrls = SERVICE_ACCEPT_NONE;
    constexpr DWORD state = SERVICE_STOPPED;

    if (!set_status(ctrls, state, exit_code)) {
        log("%s: failed to stop with err=0x%x\n", __func__, exit_code);
    }
}

static void mark_checkpoint()
{
    std::lock_guard lock(service_mutex);
    service_status.dwCheckPoint++;

    if (!__set_status()) {
        DWORD state = service_status.dwCurrentState;

        switch (state) {
        case SERVICE_CONTINUE_PENDING:
            log("%s: failed for CONTINUE_PENDING\n", __func__);
            break;
        case SERVICE_PAUSE_PENDING:
            log("%s: failed for PAUSE_PENDING\n", __func__);
            break;
        case SERVICE_START_PENDING:
            log("%s: failed for START_PENDING\n", __func__);
            break;
        case SERVICE_STOP_PENDING:
            log("%s: failed for STOP_PENDING\n", __func__);
            break;
        default:
            log("%s: invalid state: 0x%x\n", __func__, state);
            break;
        }
    }
}

DWORD WINAPI blkback_worker(LPVOID param)
{
    (void)param;

    try {
        args_type args = parseOrigArgs();

        log("%s: starting blkback\n", __func__);

        BlkBackend blkBackend(args.count("wait") != 0);
        blkBackend.start();
        service_wait_for_stop_signal();
        blkBackend.stop();
    } catch (const std::exception &e) {
        log("%s: caught exception: what = %s\n", __func__, e.what());
    }

    return EXIT_FAILURE;
}

void WINAPI service_main(DWORD argc, LPTSTR *argv)
{
    if (!init()) {
        log("%s: init failed\n", __func__);
        return;
    }

    DWORD ctrls = SERVICE_ACCEPT_NONE;
    DWORD state = SERVICE_START_PENDING;
    DWORD exit_code = NO_ERROR;

    if (!set_status(ctrls, state, exit_code)) {
        log("%s: failed to set START_PENDING\n", __func__);
    }

    service_stop_event = CreateEvent(NULL,  /* default attributes */
                                     TRUE,  /* manual reset */
                                     FALSE, /* initially nonsignaled */
                                     NULL); /* no name */
    if (!service_stop_event) {
        exit_code = GetLastError();

        log("%s: failed to create stop event (err=0x%x)\n",
                __func__, exit_code);

        stop_with_error(exit_code);
        return;
    }

    mark_checkpoint();
    stop_event_ready.store(true, std::memory_order_release);

    blkback_thread = CreateThread(NULL,           /* default attributes */
                                  0,              /* default stack size */
                                  blkback_worker, /* thread entry point */
                                  NULL,           /* entry point parameter */
                                  0,              /* creation flags */
                                  NULL);          /* thread ID storage */
    if (!blkback_thread) {
        exit_code = GetLastError();

        log("%s: failed to create vm thread (err=0x%x)\n",
                __func__, exit_code);

        CloseHandle(service_stop_event);
        stop_with_error(exit_code);
        return;
    }

    mark_checkpoint();

    /*
     * Only accept preshutdown controls. This gives the VM thread
     * 3 minutes (by default according to SERVICE_CONFIG_PRESHUTDOWN_INFO)
     * to release resources back to the system. Note that handling PRESHUTDOWN
     * precludes the handling of SHUTDOWN, since the service must exit
     * the PRESHUTDOWN event in the SERVICE_STOPPED state.
     */
    ctrls = SERVICE_ACCEPT_PRESHUTDOWN | SERVICE_ACCEPT_STOP;
    state = SERVICE_RUNNING;
    exit_code = NO_ERROR;

    if (!set_status(ctrls, state, exit_code)) {
        DWORD wait_time = 0;

        log("%s: failed to set RUNNING\n", __func__);

        send_stop_signal();
        wait_on_blkback_thread();
    } else {
        DWORD ret = WaitForSingleObject(blkback_thread, INFINITE);
        if (ret != WAIT_OBJECT_0) {
            log("%s: wait on blkback_thread failed (err=0x%x)\n", __func__, ret);
        }
    }

    set_status(SERVICE_ACCEPT_NONE, SERVICE_STOPPED, NO_ERROR);

    CloseHandle(service_stop_event);
    CloseHandle(blkback_thread);
}

void service_wait_for_stop_signal() noexcept
{
    if (!stop_event_ready.load(std::memory_order_acquire)) {
        return;
    }

    DWORD ret = WaitForSingleObject(service_stop_event, INFINITE);

    if (ret != WAIT_OBJECT_0) {
        log("%s: weird wait return: %d\n", __func__, ret);
    }

    ResetEvent(service_stop_event);
}

void serviceStart() noexcept
{
    SERVICE_TABLE_ENTRY table[] = {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)service_main},
        {NULL, NULL}
    };

    if (!StartServiceCtrlDispatcher(table)) {
        log("%s: failed to start ctrl dispatcher (err=0x%x)\n",
            __func__, GetLastError());
    }
}

DWORD service_ctrl_handler(DWORD ctrl_code,
                           DWORD event_type,
                           LPVOID event_data,
                           LPVOID context)
{
    (void)event_type;
    (void)event_data;
    (void)context;

    switch (ctrl_code) {
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_PRESHUTDOWN:
        set_status(SERVICE_ACCEPT_NONE, SERVICE_STOP_PENDING, NO_ERROR);
        send_stop_signal();
        wait_on_blkback_thread();
        set_status(SERVICE_ACCEPT_NONE, SERVICE_STOPPED, NO_ERROR);
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

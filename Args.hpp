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

#ifndef BLKBACK_ARGS_H
#define BLKBACK_ARGS_H

#ifdef _WIN64
#pragma warning(push)
#pragma warning(disable:4267)
#include <malloc.h>
#endif

#include "cxxopts.hpp"

#ifdef _WIN64
#pragma warning(pop)
#endif

#include <cstdlib>
#include <mutex>

using args_type = cxxopts::ParseResult;

inline std::mutex orig_arg_mutex;
inline int orig_argc = 0;
inline char **orig_argv = nullptr;

inline args_type
parseArgs(int argc, char *argv[])
{
        using namespace cxxopts;
        cxxopts::Options options("us-blkback - xen blkback in userspace");

        options.add_options()
        ("h,help", "Print this help menu")
        ("a,affinity", "Run on a specific cpu", cxxopts::value<uint64_t>(), "[cpu #]")
#ifdef _WIN32
        ("s,windows-svc", "Run as a windows service")
#endif
        ("w,wait", "Wait for xeniface driver");

        auto args = options.parse(argc, argv);
        if (args.count("help")) {
            std::cout << options.help() << '\n';
            exit(EXIT_SUCCESS);
        }

        return args;
}

inline args_type parseOrigArgs()
{
    std::lock_guard lock(orig_arg_mutex);

    return parseArgs(orig_argc, orig_argv);
}

inline int copyArgs(int argc, char **argv)
{
    if (argc == 0) {
        return -1;
    }

    std::lock_guard lock(orig_arg_mutex);

    orig_argc = argc;
    orig_argv = (char **)malloc(sizeof(char *) * argc);

    if (!orig_argv) {
        std::cerr << "failed to malloc orig_argv\n";
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        orig_argv[i] = (char *)malloc(strlen(argv[i]) + 1);

        if (!orig_argv[i]) {
            std::cerr << "failed to malloc orig_argv[" << i << "]\n";

            for (int j = i - 1; j >= 0; j--) {
                free(orig_argv[j]);
            }

            goto free_argv;
        }

        memcpy(orig_argv[i], argv[i], strlen(argv[i]) + 1);
    }

    return 0;

free_argv:
    free(orig_argv);
    orig_argv = nullptr;

    return -1;
}

inline void freeArgs()
{
    std::lock_guard lock(orig_arg_mutex);

    if (!orig_argv) {
        return;
    }

    for (int i = 0; i < orig_argc; i++) {
        free(orig_argv[i]);
    }

    free(orig_argv);
    orig_argv = nullptr;
}

#endif

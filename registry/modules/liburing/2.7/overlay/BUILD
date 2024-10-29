# SPDX-License-Identifier: Apache-2.0

# This genrule facilitates the installed kernel headers to configure a
# compatibility header file. While that effectively breaks hermeticity, there
# is not much we can do about that. We can't really bring the kernel headers
# into BCR as they correspond to the host kernel. The good news is that the
# resulting library is portable due to the compatibility layer at compile
# time.
genrule(
    name = "generate_headers",
    srcs = [
        "Makefile.common",
        "liburing.spec",
    ],
    outs = [
        "config-host.h",
        "src/include/liburing/compat.h",
        "src/include/liburing/io_uring_version.h",
    ],
    cmd = """
            export CC=$(CC)

            # switch to the package dir to execute "configure" right there
            pushd $$(dirname $(location configure))
              mkdir -p src/include/liburing
              ./configure --use-libc
            popd

            # collect the outputs
            for out in $(OUTS); do
              cp $$(realpath --relative-to=$(BINDIR) $$out) $$out
            done
          """,
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
    tools = ["configure"],
)

cc_library(
    name = "uring",
    srcs = [
        "config-host.h",
        "src/include/liburing/compat.h",
        "src/include/liburing/io_uring_version.h",
        "src/queue.c",
        "src/register.c",
        "src/setup.c",
        "src/syscall.c",
        "src/version.c",
    ] + glob([
        "src/arch/**/*.h",
        "src/*.h",
    ]),
    hdrs = glob(["src/include/**/*.h"]),

    # cflags aligned with upstream:
    # https://github.com/axboe/liburing/blob/master/src/Makefile#L13
    copts = [
        "-D_GNU_SOURCE",
        "-D_LARGEFILE_SOURCE",
        "-D_FILE_OFFSET_BITS=64",
        "-DLIBURING_INTERNAL",
        "-O3",
        "-Wall",
        "-Wextra",
        "-fno-stack-protector",
        "-include config-host.h",
    ],
    includes = ["src/include"],
    visibility = ["//visibility:public"],
)

alias(
    name = "liburing",
    actual = ":uring",
    visibility = ["//visibility:public"],
)

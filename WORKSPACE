workspace(name = 'com_baidu_babylon')

load('@bazel_tools//tools/build_defs/repo:http.bzl', 'http_archive')

http_archive(
  name = 'com_google_absl',
  urls = ['https://github.com/abseil/abseil-cpp/archive/refs/tags/20240116.2.tar.gz'],
  strip_prefix = 'abseil-cpp-20240116.2',
  sha256 = '733726b8c3a6d39a4120d7e45ea8b41a434cdacde401cba500f14236c49b39dc',
)

http_archive(
  name = 'com_google_protobuf',
  urls = ['https://github.com/protocolbuffers/protobuf/archive/refs/tags/v25.3.tar.gz'],
  strip_prefix = 'protobuf-25.3',
  sha256 = 'd19643d265b978383352b3143f04c0641eea75a75235c111cc01a1350173180e',
  patches = ['@com_baidu_babylon//:registry/modules/protobuf/25.3.arenastring/patches/arenastring.patch'],
  patch_args = ['-p1'],
)
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

http_archive(
  name = 'com_github_nelhage_rules_boost',
  urls = ['https://github.com/nelhage/rules_boost/archive/4ab574f9a84b42b1809978114a4664184716f4bf.tar.gz'],
  strip_prefix = 'rules_boost-4ab574f9a84b42b1809978114a4664184716f4bf',
  sha256 = '2215e6910eb763a971b1f63f53c45c0f2b7607df38c96287666d94d954da8cdc',
)
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()

################################################################################
# test only dependency
http_archive(
  name = 'com_google_googletest',
  urls = ['https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz'],
  strip_prefix = 'googletest-1.14.0',
  sha256 = '8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7',
)

http_archive(
  name = "rules_cuda",
  urls = ["https://github.com/bazel-contrib/rules_cuda/archive/3482c70dc60d9ab1ad26b768c117fcd61ee12494.tar.gz"],
  strip_prefix = "rules_cuda-3482c70dc60d9ab1ad26b768c117fcd61ee12494",
  sha256 = 'c7bf1da41b5a31480a0477f4ced49eed08ab1cb3aff77c704e373cf9c52694f5',
)
load("@rules_cuda//cuda:repositories.bzl", "register_detected_cuda_toolchains", "rules_cuda_dependencies")
rules_cuda_dependencies()
register_detected_cuda_toolchains()
################################################################################

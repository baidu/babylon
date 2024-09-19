workspace(name = 'com_baidu_babylon')

load('@bazel_tools//tools/build_defs/repo:http.bzl', 'http_archive')

http_archive(
  name = 'com_google_absl',
  urls = ['https://github.com/abseil/abseil-cpp/archive/refs/tags/20211102.0.tar.gz'],
  strip_prefix = 'abseil-cpp-20211102.0',
  sha256 = 'dcf71b9cba8dc0ca9940c4b316a0c796be8fab42b070bb6b7cab62b48f0e66c4',
)

http_archive(
  name = 'com_google_protobuf',
  urls = ['https://github.com/protocolbuffers/protobuf/archive/refs/tags/v3.19.6.zip'],
  strip_prefix = 'protobuf-3.19.6',
  sha256 = '387e2c559bb2c7c1bc3798c4e6cff015381a79b2758696afcbf8e88730b47389',
)
load('@com_google_protobuf//:protobuf_deps.bzl', 'protobuf_deps')
protobuf_deps()

http_archive(
  name = 'com_github_nelhage_rules_boost',
  urls = ['https://github.com/nelhage/rules_boost/archive/4ab574f9a84b42b1809978114a4664184716f4bf.tar.gz'],
  strip_prefix = 'rules_boost-4ab574f9a84b42b1809978114a4664184716f4bf',
  sha256 = '2215e6910eb763a971b1f63f53c45c0f2b7607df38c96287666d94d954da8cdc',
)
load('@com_github_nelhage_rules_boost//:boost/boost.bzl', 'boost_deps')
boost_deps()

################################################################################
# test only dependency
http_archive(
  name = 'com_google_googletest',
  urls = ['https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz'],
  strip_prefix = 'googletest-1.15.2',
  sha256 = '7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926',
)

http_archive(
  name = 'rules_foreign_cc',
  urls = ['https://github.com/bazelbuild/rules_foreign_cc/releases/download/0.12.0/rules_foreign_cc-0.12.0.tar.gz'],
  strip_prefix = 'rules_foreign_cc-0.12.0',
  sha256 = 'a2e6fb56e649c1ee79703e99aa0c9d13c6cc53c8d7a0cbb8797ab2888bbc99a3',
)
load('@rules_foreign_cc//foreign_cc:repositories.bzl', 'rules_foreign_cc_dependencies')
rules_foreign_cc_dependencies()
load("@bazel_features//:deps.bzl", "bazel_features_deps")
bazel_features_deps()

http_archive(
  name = 'libcoro',
  urls = ['https://github.com/jbaldwin/libcoro/archive/refs/tags/v0.12.1.tar.gz'],
  strip_prefix = 'libcoro-0.12.1',
  sha256 = '2cb6f45fc73dad6008cc930d92939785684835e03b12df422b98fcab9e393add',
  build_file = '@//:registry/modules/libcoro/0.12.1/overlay/BUILD',
)

http_archive(
  name = 'rules_cuda',
  urls = ['https://github.com/bazel-contrib/rules_cuda/releases/download/v0.2.3/rules_cuda-v0.2.3.tar.gz'],
  strip_prefix = 'rules_cuda-v0.2.3',
  sha256 = 'c92b334d769a07cd991b7675b2f6076b8b95cd3b28b14268a2f379f8baae58e0',
)
load('@rules_cuda//cuda:repositories.bzl', 'register_detected_cuda_toolchains', 'rules_cuda_dependencies')
rules_cuda_dependencies()
register_detected_cuda_toolchains()
################################################################################

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
  urls = ['https://github.com/protocolbuffers/protobuf/archive/refs/tags/v3.19.6.tar.gz'],
  strip_prefix = 'protobuf-3.19.6',
  sha256 = '9a301cf94a8ddcb380b901e7aac852780b826595075577bb967004050c835056',
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

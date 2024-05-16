load('@bazel_tools//tools/cpp:cc_configure.bzl', 'cc_autoconf_impl')

def cross_config_cc_impl(repository_ctx):
  cc_autoconf_impl(repository_ctx,
                   overriden_tools = {
                     'gcc': repository_ctx.getenv('CROSS_CC')})

cross_config_cc = repository_rule(
  implementation = cross_config_cc_impl,
  configure = True,
)

def cross_config_toolchain_impl(repository_ctx):
  repository_ctx.file('BUILD', 
  '''
load('@platforms//host:constraints.bzl', 'HOST_CONSTRAINTS')
constraint_setting(name = 'compile_mode')
constraint_value(
  name = 'cross_compile',
  constraint_setting = 'compile_mode',
)
platform(
  name = 'cross',
  constraint_values = [':cross_compile'],
)
toolchain(
  name = 'cross-toolchain',
  exec_compatible_with = HOST_CONSTRAINTS,
  target_compatible_with = [':cross_compile'],
  toolchain = '@cross_config_cc//:cc-compiler-k8',
  toolchain_type = '@bazel_tools//tools/cpp:toolchain_type',
)
  ''')

cross_config_toolchain = repository_rule(
  implementation = cross_config_toolchain_impl,
)

def cross_config_impl(module_ctx):
  cross_config_cc(name = 'cross_config_cc')
  cross_config_toolchain(name = 'cross_config_toolchain')

cross_config = module_extension(
  implementation = cross_config_impl,
)

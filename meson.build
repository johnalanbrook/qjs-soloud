project('qjs-soloud', 'c')

libtype = get_option('default_library')
if libtype == 'shared'
  add_project_arguments('-DJS_SHARED_LIBRARY', language:'c')
endif

quickjs = dependency('quickjs', static:true)
soloud = dependency('soloud', static:true)

cc = meson.get_compiler('c')
deps = []
if host_machine.system() == 'linux'
  deps += cc.find_library('asound', required:true)
endif

qjs_soloud_lib = library('soloud',
  sources: 'qjs_soloud.c',
  dependencies: [quickjs, soloud] + deps,
  name_suffix: 'so',
  name_prefix: '',
)

qjs_soloud_dep = declare_dependency(
  link_with: qjs_soloud_lib,
)

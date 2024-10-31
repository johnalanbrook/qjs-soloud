project('qjs-soloud', 'c')

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
  c_shared_args: '-DJS_SHARED_LIBRARY',  
)

qjs_soloud_dep = declare_dependency(
  link_with: qjs_soloud_lib,
)

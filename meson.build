project(
  'json',
  'c',
  version: '0.1',
  default_options: [
    'warning_level=3',
    'b_sanitize=none',
    #'prefer_static=true',
  ],
)
#libs
hm_proj = subproject('hashmap', required: true)
hm_lib = hm_proj.get_variable('hm_lib')
json = shared_library('json', 'json.c', install: false, dependencies: hm_lib)
json_lib = declare_dependency(
  link_with: [json],
  include_directories: [include_directories('.'), hm_proj.get_variable('incdir')],
)

#tests
exe = executable('testing', 'print_json.c', install: false, dependencies: [hm_lib, json_lib])
serialize = executable('serialize', 'serialize.c', install: false, dependencies: [hm_lib, json_lib])

#root=meson.source_root()
root = meson.project_source_root()

testfiles = [
  ['basic_json', '/test.json'],
  ['and', '/sls/and.slij'],
  ['every', '/sls/every.slj'],
  ['simple', '/sls/simple-ic.slij'],
  ['dep', '/sls/dep.slij'],
  ['mem64', '/sls/Memory (64 Byte).slij'],
]
foreach f : testfiles
  test(f[0], exe, args: root + f[1])
  test(f[0] + '_ser', serialize, args: [root + f[1], root + '/output' + f[1]])
endforeach

#extra
ncurses = dependency('ncurses')
menu = dependency('menu')
executable(
  'explore',
  'explorer.c',
  install: false,
  dependencies: [hm_lib, json_lib, ncurses, menu],
)
executable('jprint', 'print_json.c', install: false, dependencies: [hm_lib, json_lib])

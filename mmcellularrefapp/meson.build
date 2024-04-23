
sources = files(
  'mm-cellular-ref-app.c',
)

deps = [libmm_glib_dep]

executable(
  'mmCellularRefApp',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  install: true,
)
install_data('bring_up_call.sh', install_mode : 'rwxr-xr-x', install_dir: '/usr/bin')


sources = files(
  'mm-telephony-app.c',
)

gtkdep = dependency('gtk+-3.0')
deps = [libmm_glib_dep, gtkdep]

executable(
  'mmTelephonyApp',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  install: true,
)

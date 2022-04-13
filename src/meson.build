pavucontrol_sources = [
  'cardwidget.cc',
  'channelwidget.cc',
  'devicewidget.cc',
  'mainwindow.cc',
  'minimalstreamwidget.cc',
  'pavuapplication.cc',
  'pavucontrol.cc',
  'rolewidget.cc',
  'sinkinputwidget.cc',
  'sinkwidget.cc',
  'sourceoutputwidget.cc',
  'sourcewidget.cc',
  'streamwidget.cc',
]

pavucontrol_deps = [gtkmm_dep, sigcpp_dep, canberragtk_dep, libpulse_dep, libpulsemlglib_dep]
pavucontrol_defines = ['-DHAVE_CONFIG_H']

if have_pulse_messaging_api
  pavucontrol_deps += [json_glib_dep]
  pavucontrol_defines += ['-DHAVE_PULSE_MESSAGING_API']
endif

executable('pavucontrol',
  pavucontrol_sources,
  install: true,
  cpp_args: pavucontrol_defines,
  include_directories : configinc,
  dependencies : pavucontrol_deps,
)

install_data('pavucontrol.glade')

desktop_file = i18n.merge_file(
  input : 'pavucontrol.desktop.in',
  output : 'pavucontrol.desktop',
  po_dir : po_dir,
  type : 'desktop',
  install : true,
  install_dir : join_paths(datadir, 'applications'),
)

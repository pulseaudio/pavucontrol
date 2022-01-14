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

executable('pavucontrol',
  pavucontrol_sources,
  install: true,
  cpp_args: ['-DHAVE_CONFIG_H'],
  include_directories : configinc,
  dependencies : [gtkmm_dep, sigcpp_dep, canberragtk_dep, libpulse_dep, libpulsemlglib_dep],
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

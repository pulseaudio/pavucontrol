gnome = import('gnome')
resources = gnome.compile_resources('resources',
  'resources.gresource.xml',
  source_dir: '.',
  c_name: 'resources'
)

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
  resources
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

desktop_file = i18n.merge_file(
  input : 'org.pulseaudio.pavucontrol.desktop.in',
  output : 'org.pulseaudio.pavucontrol.desktop',
  po_dir : po_dir,
  type : 'desktop',
  install : true,
  install_dir : join_paths(datadir, 'applications'),
)

appdata_file = i18n.merge_file(
  input : 'org.pulseaudio.pavucontrol.metainfo.xml.in',
  output : 'org.pulseaudio.pavucontrol.metainfo.xml',
  po_dir : po_dir,
  type : 'xml',
  install : true,
  install_dir : datadir / 'metainfo',
)

# TODO: use the Devel version on beta builds?
icon_filename = 'org.pulseaudio.pavucontrol.svg'
install_data(
  icon_filename,
  install_dir: datadir / 'icons' / 'hicolor' / 'scalable' / 'apps'
)

icon_filename = 'org.pulseaudio.pavucontrol-symbolic.svg'
install_data(
  icon_filename,
  install_dir: datadir / 'icons' / 'hicolor' / 'symbolic' / 'apps'
)

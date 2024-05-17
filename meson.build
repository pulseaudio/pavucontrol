project('pavucontrol', 'cpp',
        version : '6.0',
        meson_version : '>= 0.50.0',
        default_options : [ 'c_std=gnu11', 'cpp_std=c++17' ]
        )

with_lynx = get_option('lynx')

cpp = meson.get_compiler('cpp')

gtkmm_dep = dependency('gtkmm-4.0', version : '>= 4.0', required : true)
sigcpp_dep = dependency('sigc++-3.0', required : true)
canberragtk_dep = dependency('libcanberra', version : '>= 0.16', required : false)

libpulse_dep = dependency('libpulse', version : '>= 5.0', required : true)
libpulsemlglib_dep = dependency('libpulse-mainloop-glib', version : '>= 0.9.16', required : true)

have_pulse_messaging_api = cpp.has_function('pa_context_send_message_to_object',
  dependencies: [libpulse_dep],
)

json_glib_dep = dependency('json-glib-1.0', required : have_pulse_messaging_api)

lynx = find_program('lynx', required: with_lynx)
tidy = find_program('tidy', required: false)

configinc = include_directories('.')

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
docdir = join_paths(datadir, 'doc', 'pavucontrol')
localedir = join_paths(prefix, get_option('localedir'))

cdata = configuration_data()
cdata.set('ENABLE_NLS', 1)
cdata.set_quoted('PACKAGE', 'pavucontrol')
cdata.set_quoted('PACKAGE_NAME', 'pavucontrol')
cdata.set_quoted('PACKAGE_STRING', 'pavucontrol')
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set_quoted('GETTEXT_PACKAGE', 'pavucontrol')
cdata.set_quoted('LOCALEDIR', localedir)

cdata.set('HAVE_LIBCANBERRA', canberragtk_dep.found())

# Now generate config.h from everything above
configure_file(output : 'config.h', configuration : cdata)

subdir('doc')
subdir('po')
subdir('src')

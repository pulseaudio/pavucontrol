doc_cdata = configuration_data()

doc_cdata.set('PACKAGE_VERSION', meson.project_version())
doc_cdata.set('PACKAGE_URL', 'http://freedesktop.org/software/pulseaudio/pavucontrol/')

readme_html = configure_file(
    input : 'README.html.in',
    output : 'README.html',
    configuration : doc_cdata,
    install : true,
    install_dir : docdir,
)

if tidy.found()
  test('XHTML documentation uses correct mark-up', tidy, args: ['-e', readme_html])
endif

if with_lynx
  custom_target(
    'README',
    input: readme_html,
    output: 'README',
    command: [find_program('./html2txt'), '@INPUT@'],
    capture: true,
    build_by_default: true,
    install : true,
    install_dir : docdir,
  )
endif

install_data(
    sources : 'style.css',
    install_dir : docdir,
)

project ('showq',
    ['cpp', 'c'],
    default_options : ['cpp_std=c++11'],
    version: '0.5',
    license: 'GPLv3+',
    meson_version: '>= 0.37.1')

gnome = import ('gnome')
i18n = import ('i18n')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('LOCALEDIR',
                join_paths(get_option('prefix'),
                           get_option('localedir')))
conf.set_quoted('DATADIR',
                join_paths(get_option('prefix'),
                           get_option('datadir')))
conf.set_quoted('LIBDIR',
                join_paths(get_option('prefix'),
                           get_option('libdir')))
conf.set_quoted('BINDIR',
                join_paths(get_option('prefix'),
                           get_option('bindir')))
conf.set_quoted('SYSCONFDIR',
                join_paths(get_option('prefix'),
                           get_option('sysconfdir')))
conf.set_quoted('LOCALSTATEDIR',
                join_paths(get_option('prefix'),
                           get_option('localstatedir')))
conf.set_quoted('LIBEXECDIR',
                join_paths(get_option('prefix'),
                           get_option('libexecdir')))
configure_file(
  output : 'config.h',
  configuration : conf
)


# Dependencies

gtkmm_dep = dependency('gtkmm-2.4', version : '>= 2.24')
#gtkmm_dep = dependency('gtkmm-3.0', version : '>= 3.0')
glibmm_dep = dependency('glibmm-2.4', version : '>= 2.44')
giomm_dep = dependency('giomm-2.4', version : '>= 2.44')
gthread_dep = dependency('gthread-2.0')
libxmlmm_dep = dependency('libxml++-2.6')
alsa_dep = dependency('alsa')
jack_dep = dependency('jack')
snd_file_dep = dependency('sndfile')
samplerate_dep = dependency('samplerate')
uuid_dep = dependency('uuid')

subdir('src')


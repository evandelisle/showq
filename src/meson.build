resources = gnome.compile_resources('resources', 'showq.gresource.xml',
                                     source_dir: '../',
                                     c_name: 'resources')

showq = executable('showq',
    [   'app.cc', 'main.cc', 'pref.cc', 'editcue.cc',
        'audio.cc', 'editstop.cc', 'editmidi.cc', 'editwave.cc', 'editfade.cc',
        'editgroup.cc', 'renumber.cc', 'utils.cc', 'file.cc', 'patch.cc'
    ] + resources,
    dependencies: [
        gtkmm_dep,
        gthread_dep,
        libxmlmm_dep,
        alsa_dep,
        jack_dep,
        snd_file_dep,
        samplerate_dep,
        uuid_dep],
    install: true)



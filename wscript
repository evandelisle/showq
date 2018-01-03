#! /usr/bin/env python
# encoding: utf-8

import os

APPNAME='showq'
VERSION='0.5.0'
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_cxx')

def configure(conf):
    conf.recurse('src ui')
    conf.define('GETTEXT_PACKAGE', 'showq')
    conf.define('PROGRAMNAME_LOCALEDIR', os.path.join(conf.options.prefix, 'share/showq'))
    conf.define('UI_DIR', os.path.join(conf.options.prefix, 'share/showq/ui/'))
    conf.define('VERSIONSTRING', VERSION)
    conf.write_config_header('config.h')

def build(bld):
    bld.recurse('src ui')

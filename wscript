#! /usr/bin/env python
# encoding: utf-8

APPNAME='showq'
VERSION='0.5.0'
srcdir = '.'
blddir = 'output'

def set_options(opt):
    opt.sub_options('src')
    opt.sub_options('ui')

def configure(conf):
    conf.sub_config('src')
    conf.sub_config('ui')

def build(bld):
    bld.add_subdirs('src')
    bld.add_subdirs('ui')

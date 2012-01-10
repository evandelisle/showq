#! /usr/bin/env python
# encoding: utf-8

import os,sys
import Configure,Options,Utils,TaskGen
import ccroot,ar
from Configure import conftest
def find_gcc(conf):
	v=conf.env
	cc=None
	if v['CC']:
		cc=v['CC']
	elif'CC'in conf.environ:
		cc=conf.environ['CC']
	if not cc:cc=conf.find_program('gcc',var='CC')
	if not cc:cc=conf.find_program('cc',var='CC')
	if not cc:conf.fatal('gcc was not found')
	cc=conf.cmd_to_list(cc)
	ccroot.get_cc_version(conf,cc,gcc=True)
	v['CC_NAME']='gcc'
	v['CC']=cc
def gcc_common_flags(conf):
	v=conf.env
	v['CC_SRC_F']=''
	v['CC_TGT_F']=['-c','-o','']
	v['CPPPATH_ST']='-I%s'
	if not v['LINK_CC']:v['LINK_CC']=v['CC']
	v['CCLNK_SRC_F']=''
	v['CCLNK_TGT_F']=['-o','']
	v['LIB_ST']='-l%s'
	v['LIBPATH_ST']='-L%s'
	v['STATICLIB_ST']='-l%s'
	v['STATICLIBPATH_ST']='-L%s'
	v['RPATH_ST']='-Wl,-rpath,%s'
	v['CCDEFINES_ST']='-D%s'
	v['SONAME_ST']='-Wl,-h,%s'
	v['SHLIB_MARKER']='-Wl,-Bdynamic'
	v['STATICLIB_MARKER']='-Wl,-Bstatic'
	v['FULLSTATIC_MARKER']='-static'
	v['program_PATTERN']='%s'
	v['shlib_CCFLAGS']=['-fPIC','-DPIC']
	v['shlib_LINKFLAGS']=['-shared']
	v['shlib_PATTERN']='lib%s.so'
	v['staticlib_LINKFLAGS']=['-Wl,-Bstatic']
	v['staticlib_PATTERN']='lib%s.a'
	v['LINKFLAGS_MACBUNDLE']=['-bundle','-undefined','dynamic_lookup']
	v['CCFLAGS_MACBUNDLE']=['-fPIC']
	v['macbundle_PATTERN']='%s.bundle'
def gcc_modifier_win32(conf):
	v=conf.env
	v['program_PATTERN']='%s.exe'
	v['shlib_PATTERN']='%s.dll'
	v['staticlib_PATTERN']='%s.lib'
	v['shlib_CCFLAGS']=[]
	v['staticlib_LINKFLAGS']=[]
def gcc_modifier_cygwin(conf):
	return conf.gcc_modifier_win32()
def gcc_modifier_darwin(conf):
	v=conf.env
	v['shlib_CCFLAGS']=['-fPIC','-compatibility_version','1','-current_version','1']
	v['shlib_LINKFLAGS']=['-dynamiclib']
	v['shlib_PATTERN']='lib%s.dylib'
	v['staticlib_LINKFLAGS']=[]
	v['SHLIB_MARKER']=''
	v['STATICLIB_MARKER']=''
def gcc_modifier_aix5(conf):
	v=conf.env
	v['program_LINKFLAGS']=['-Wl,-brtl']
	v['shlib_LINKFLAGS']=['-shared','-Wl,-brtl,-bexpfull']
	v['SHLIB_MARKER']=''
def detect(conf):
	conf.find_gcc()
	conf.find_cpp()
	conf.find_ar()
	conf.gcc_common_flags()
	target_platform=conf.env['TARGET_PLATFORM']or sys.platform
	gcc_modifier_func=globals().get('gcc_modifier_'+target_platform)
	if gcc_modifier_func:
		gcc_modifier_func(conf)
	conf.cc_load_tools()
	conf.cc_add_flags()

conftest(find_gcc)
conftest(gcc_common_flags)
conftest(gcc_modifier_win32)
conftest(gcc_modifier_cygwin)
conftest(gcc_modifier_darwin)
conftest(gcc_modifier_aix5)

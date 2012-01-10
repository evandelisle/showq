#! /usr/bin/env python
# encoding: utf-8

import os,sys
import Task
from Configure import conftest
ar_str='${AR} ${ARFLAGS} ${TGT} ${SRC}'
cls=Task.simple_task_type('ar_link_static',ar_str,color='YELLOW',ext_in='.o',shell=False)
cls.maxjobs=1
old=cls.run
def wrap(self):
	try:os.remove(self.outputs[0].abspath(self.env))
	except OSError:pass
	return old(self)
setattr(cls,'run',wrap)
def detect(conf):
	comp=conf.environ.get('AR','')
	if not comp:comp=conf.env['AR']
	if not comp:comp=conf.find_program('ar',var='AR')
	if not comp:return
	ranlib=conf.environ.get('RANLIB','')
	if not ranlib:ranlib=conf.env['RANLIB']
	if not ranlib:ranlib=conf.find_program('ranlib',var='RANLIB')
	if not ranlib:return
	v=conf.env
	v['AR']=comp
	v['ARFLAGS']='rcs'
	v['RANLIB']=ranlib
	v['RANLIBFLAGS']=''
def find_ar(conf):
	v=conf.env
	conf.check_tool('ar')
	if not v['AR']:conf.fatal('ar is required for static libraries - not found')

conftest(find_ar)

#! /usr/bin/env python
# encoding: utf-8

import os,re
import TaskGen,Task,Utils,Runner,Options,Build
from TaskGen import feature,before,taskgen
from Logs import error
class intltool_in_taskgen(TaskGen.task_gen):
	def __init__(self,*k,**kw):
		TaskGen.task_gen.__init__(self,*k,**kw)
def iapply_intltool_in_f(self):
	try:self.meths.remove('apply_core')
	except ValueError:pass
	for i in self.to_list(self.source):
		node=self.path.find_resource(i)
		podir=getattr(self,'podir','po')
		podirnode=self.path.find_dir(podir)
		if not podirnode:
			error("could not find the podir %r"%podir)
			continue
		cache=getattr(self,'intlcache','.intlcache')
		self.env['INTLCACHE']=os.path.join(self.path.bldpath(self.env),podir,cache)
		self.env['INTLPODIR']=podirnode.srcpath(self.env)
		self.env['INTLFLAGS']=getattr(self,'flags',['-q','-u','-c'])
		task=self.create_task('intltool')
		task.set_inputs(node)
		task.set_outputs(node.change_ext(''))
		task.install_path=self.install_path
class intltool_po_taskgen(TaskGen.task_gen):
	def __init__(self,*k,**kw):
		TaskGen.task_gen.__init__(self,*k,**kw)
def apply_intltool_po(self):
	try:self.meths.remove('apply_core')
	except ValueError:pass
	self.default_install_path='${LOCALEDIR}'
	appname=getattr(self,'appname','set_your_app_name')
	podir=getattr(self,'podir','')
	def install_translation(task):
		out=task.outputs[0]
		filename=out.name
		(langname,ext)=os.path.splitext(filename)
		inst_file=langname+os.sep+'LC_MESSAGES'+os.sep+appname+'.mo'
		self.bld.install_as(os.path.join(self.install_path,inst_file),out.abspath(self.env),chmod=self.chmod)
	linguas=self.path.find_resource(os.path.join(podir,'LINGUAS'))
	if linguas:
		file=open(linguas.abspath())
		langs=[]
		for line in file.readlines():
			if not line.startswith('#'):
				langs+=line.split()
		file.close()
		re_linguas=re.compile('[-a-zA-Z_@.]+')
		for lang in langs:
			if re_linguas.match(lang):
				node=self.path.find_resource(os.path.join(podir,re_linguas.match(lang).group()+'.po'))
				task=self.create_task('po')
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.mo'))
				if self.bld.is_install:task.install=install_translation
	else:
		Utils.pprint('RED',"Error no LINGUAS file found in po directory")
Task.simple_task_type('po','${POCOM} -o ${TGT} ${SRC}',color='BLUE',shell=False)
Task.simple_task_type('intltool','${INTLTOOL} ${INTLFLAGS} ${INTLCACHE} ${INTLPODIR} ${SRC} ${TGT}',color='BLUE',after="cc_link cxx_link",shell=False)
def detect(conf):
	pocom=conf.find_program('msgfmt')
	if not pocom:
		conf.fatal('The program msgfmt (gettext) is mandatory!')
	conf.env['POCOM']=pocom
	intltool=conf.find_program('intltool-merge')
	if not intltool:
		conf.fatal('The program intltool-merge (intltool, gettext-devel) is mandatory!')
	conf.env['INTLTOOL']=intltool
	def getstr(varname):
		return getattr(Options.options,varname,'')
	prefix=conf.env['PREFIX']
	datadir=getstr('datadir')
	if not datadir:datadir=os.path.join(prefix,'share')
	conf.define('LOCALEDIR',os.path.join(datadir,'locale'))
	conf.define('DATADIR',datadir)
	conf.check(header_name='locale.h')
def set_options(opt):
	opt.add_option('--want-rpath',type='int',default=1,dest='want_rpath',help='set rpath to 1 or 0 [Default 1]')
	opt.add_option('--datadir',type='string',default='',dest='datadir',help='read-only application data')

before('apply_core')(iapply_intltool_in_f)
feature('intltool_in')(iapply_intltool_in_f)
feature('intltool_po')(apply_intltool_po)

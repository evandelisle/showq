#! /usr/bin/env python
# encoding: utf-8

import os
import Task,Options,Utils
from Configure import conf
from TaskGen import extension,taskgen,feature,before
xsubpp_str='${PERL} ${XSUBPP} -noprototypes -typemap ${EXTUTILS_TYPEMAP} ${SRC} > ${TGT}'
EXT_XS=['.xs']
def init_perlext(self):
	self.uselib=self.to_list(getattr(self,'uselib',''))
	if not'PERL'in self.uselib:self.uselib.append('PERL')
	if not'PERLEXT'in self.uselib:self.uselib.append('PERLEXT')
	self.env['shlib_PATTERN']=self.env['perlext_PATTERN']
def xsubpp_file(self,node):
	gentask=self.create_task('xsubpp')
	gentask.set_inputs(node)
	outnode=node.change_ext('.c')
	gentask.set_outputs(outnode)
	self.allnodes.append(outnode)
Task.simple_task_type('xsubpp',xsubpp_str,color='BLUE',before="cc cxx",shell=False)
def check_perl_version(conf,minver=None):
	res=True
	if not getattr(Options.options,'perlbinary',None):
		perl=conf.find_program("perl",var="PERL")
		if not perl:
			return False
	else:
		perl=Options.options.perlbinary
		conf.env['PERL']=perl
	version=Utils.cmd_output(perl+" -e'printf \"%vd\", $^V'")
	if not version:
		res=False
		version="Unknown"
	elif not minver is None:
		ver=tuple(map(int,version.split(".")))
		if ver<minver:
			res=False
	if minver is None:
		cver=""
	else:
		cver=".".join(map(str,minver))
	conf.check_message("perl",cver,res,version)
	return res
def check_perl_module(conf,module):
	cmd=[conf.env['PERL'],'-e','use %s'%module]
	r=Utils.pproc.call(cmd,stdout=Utils.pproc.PIPE,stderr=Utils.pproc.PIPE)==0
	conf.check_message("perl module %s"%module,"",r)
	return r
def check_perl_ext_devel(conf):
	if not conf.env['PERL']:
		return False
	perl=conf.env['PERL']
	def read_out(cmd):
		return Utils.to_list(Utils.cmd_output(perl+cmd))
	conf.env["LINKFLAGS_PERLEXT"]=read_out(" -MConfig -e'print $Config{lddlflags}'")
	conf.env["CPPPATH_PERLEXT"]=read_out(" -MConfig -e'print \"$Config{archlib}/CORE\"'")
	conf.env["CCFLAGS_PERLEXT"]=read_out(" -MConfig -e'print \"$Config{ccflags} $Config{cccdlflags}\"'")
	conf.env["XSUBPP"]=read_out(" -MConfig -e'print \"$Config{privlib}/ExtUtils/xsubpp$Config{exe_ext}\"'")
	conf.env["EXTUTILS_TYPEMAP"]=read_out(" -MConfig -e'print \"$Config{privlib}/ExtUtils/typemap\"'")
	if not getattr(Options.options,'perlarchdir',None):
		conf.env["ARCHDIR_PERL"]=Utils.cmd_output(perl+" -MConfig -e'print $Config{sitearch}'")
	else:
		conf.env["ARCHDIR_PERL"]=getattr(Options.options,'perlarchdir')
	conf.env['perlext_PATTERN']='%s.'+Utils.cmd_output(perl+" -MConfig -e'print $Config{dlext}'")
	return True
def set_options(opt):
	opt.add_option("--with-perl-binary",type="string",dest="perlbinary",help='Specify alternate perl binary',default=None)
	opt.add_option("--with-perl-archdir",type="string",dest="perlarchdir",help='Specify directory where to install arch specific files',default=None)

before('apply_incpaths','apply_type_vars','apply_lib_vars')(init_perlext)
feature('perlext')(init_perlext)
extension(EXT_XS)(xsubpp_file)
conf(check_perl_version)
conf(check_perl_module)
conf(check_perl_ext_devel)

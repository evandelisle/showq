#! /usr/bin/env python
# encoding: utf-8
import sys
if sys.hexversion < 0x020400f0: from sets import Set as set
import os,re
from Configure import conf
import TaskGen,Task,Utils
from TaskGen import feature,before,taskgen
class_check_source='''
public class Test {
	public static void main(String[] argv) {
		Class lib;
		if (argv.length < 1) {
			System.err.println("Missing argument");
			System.exit(77);
		}
		try {
			lib = Class.forName(argv[0]);
		} catch (ClassNotFoundException e) {
			System.err.println("ClassNotFoundException");
			System.exit(1);
		}
		lib = null;
		System.exit(0);
	}
}
'''
def jar_files(self):
	basedir=getattr(self,'basedir','.')
	destfile=getattr(self,'destfile','test.jar')
	jaropts=getattr(self,'jaropts',[])
	dir=self.path.find_dir(basedir)
	if not dir:raise
	jaropts.append('-C')
	jaropts.append(dir.abspath(self.env))
	jaropts.append('.')
	out=self.path.find_or_declare(destfile)
	tsk=self.create_task('jar_create')
	tsk.set_outputs(out)
	tsk.inputs=[x for x in dir.find_iter(src=0,bld=1)if x.id!=out.id]
	tsk.env['JAROPTS']=jaropts
	tsk.env['JARCREATE']='cf'
def apply_java(self):
	Utils.def_attrs(self,jarname='',jaropts='',classpath='',source_root='.',source_re='[A-Za-z0-9]+\.java$',jar_mf_attributes={},jar_mf_classpath=[])
	nodes_lst=[]
	if not self.classpath:
		if not self.env['CLASSPATH']:
			self.env['CLASSPATH']='..'+os.pathsep+'.'
	else:
		self.env['CLASSPATH']=self.classpath
	source_root_node=self.path.find_dir(self.source_root)
	re_foo=re.compile(self.source_re)
	def acc(node,name):
		return re_foo.search(name)>-1
	def prune(node,name):
		if name=='.svn':return True
		return False
	src_nodes=[x for x in source_root_node.find_iter_impl(dir=False,accept_name=acc,is_prune=prune)]
	bld_nodes=[x.change_ext('.class')for x in src_nodes]
	self.env['OUTDIR']=[source_root_node.abspath(self.env)]
	tsk=self.create_task('javac')
	tsk.set_inputs(src_nodes)
	tsk.set_outputs(bld_nodes)
	if self.jarname:
		tsk=self.create_task('jar_create')
		tsk.set_inputs(bld_nodes)
		tsk.set_outputs(self.path.find_or_declare(self.jarname))
		if not self.env['JAROPTS']:
			if self.jaropts:
				self.env['JAROPTS']=self.jaropts
			else:
				dirs='.'
				self.env['JAROPTS']=['-C',''.join(self.env['OUTDIR']),dirs]
Task.simple_task_type('jar_create','${JAR} ${JARCREATE} ${TGT} ${JAROPTS}',color='GREEN')
cls=Task.simple_task_type('javac','${JAVAC} -classpath ${CLASSPATH} -d ${OUTDIR} ${JAVACFLAGS} ${SRC}')
cls.color='BLUE'
def post_run_javac(self):
	par={}
	for x in self.inputs:
		par[x.parent.id]=x.parent
	inner={}
	for k in par.values():
		path=k.abspath(self.env)
		lst=os.listdir(path)
		for u in lst:
			if u.find('$')>=0:
				inner_class_node=k.find_or_declare(u)
				inner[inner_class_node.id]=inner_class_node
	to_add=set(inner.keys())-set([x.id for x in self.outputs])
	for x in to_add:
		self.outputs.append(inner[x])
	return Task.Task.post_run(self)
cls.post_run=post_run_javac
def detect(conf):
	java_path=conf.environ['PATH'].split(os.pathsep)
	v=conf.env
	if'JAVA_HOME'in conf.environ:
		java_path=[os.path.join(conf.environ['JAVA_HOME'],'bin')]+java_path
		conf.env['JAVA_HOME']=[conf.environ['JAVA_HOME']]
	for x in'javac java jar'.split():
		conf.find_program(x,var=x.upper(),path_list=java_path)
		conf.env[x.upper()]=conf.cmd_to_list(conf.env[x.upper()])
	v['JAVA_EXT']=['.java']
	if'CLASSPATH'in conf.environ:
		v['CLASSPATH']=conf.environ['CLASSPATH']
	if not v['JAR']:conf.fatal('jar is required for making java packages')
	if not v['JAVAC']:conf.fatal('javac is required for compiling java classes')
	v['JARCREATE']='cf'
def check_java_class(self,classname,with_classpath=None):
	import shutil
	javatestdir='.waf-javatest'
	classpath=javatestdir
	if self.env['CLASSPATH']:
		classpath+=os.pathsep+self.env['CLASSPATH']
	if isinstance(with_classpath,str):
		classpath+=os.pathsep+with_classpath
	shutil.rmtree(javatestdir,True)
	os.mkdir(javatestdir)
	java_file=open(os.path.join(javatestdir,'Test.java'),'w')
	java_file.write(class_check_source)
	java_file.close()
	Utils.exec_command(self.env['JAVAC']+[os.path.join(javatestdir,'Test.java')],shell=False)
	cmd=self.env['JAVA']+['-cp',classpath,'Test',classname]
	self.log.write("%s\n"%str(cmd))
	found=Utils.exec_command(cmd,shell=False,log=self.log)
	self.check_message('Java class %s'%classname,"",not found)
	shutil.rmtree(javatestdir,True)
	return found

feature('jar')(jar_files)
before('apply_core')(jar_files)
feature('javac')(apply_java)
before('apply_core')(apply_java)
conf(check_java_class)

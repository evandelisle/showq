#! /usr/bin/env python
# encoding: utf-8

import os,sys,shutil,traceback,datetime,inspect
import Utils,Configure,Build,Logs,Options,Environment,Task
from Logs import error,warn,info
from Constants import*
g_gz='bz2'
commands=[]
def prepare_impl(t,cwd,ver,wafdir):
	Options.tooldir=[t]
	Options.launch_dir=cwd
	if'--version'in sys.argv:
		opt_obj=Options.Handler()
		opt_obj.curdir=cwd
		opt_obj.parse_args()
		sys.exit(0)
	msg1='Waf: Please run waf from a directory containing a file named "%s" or run distclean'%WSCRIPT_FILE
	build_dir_override=None
	candidate=None
	lst=os.listdir(cwd)
	search_for_candidate=True
	if WSCRIPT_FILE in lst:
		candidate=cwd
	elif'configure'in sys.argv and not WSCRIPT_BUILD_FILE in lst:
		calldir=os.path.abspath(os.path.dirname(sys.argv[0]))
		if WSCRIPT_FILE in os.listdir(calldir):
			candidate=calldir
			search_for_candidate=False
		else:
			error('arg[0] directory does not contain a wscript file')
			sys.exit(1)
		build_dir_override=cwd
	while search_for_candidate:
		if len(cwd)<=3:
			break
		dirlst=os.listdir(cwd)
		if WSCRIPT_FILE in dirlst:
			candidate=cwd
		if'configure'in sys.argv and candidate:
			break
		if Options.lockfile in dirlst:
			env=Environment.Environment()
			env.load(os.path.join(cwd,Options.lockfile))
			candidate=env['cwd']or cwd
			break
		cwd=os.path.dirname(cwd)
	if not candidate:
		if'-h'in sys.argv or'--help'in sys.argv:
			warn('No wscript file found: the help message may be incomplete')
			opt_obj=Options.Handler()
			opt_obj.curdir=cwd
			opt_obj.parse_args()
		else:
			error(msg1)
		sys.exit(0)
	try:
		os.chdir(candidate)
	except OSError:
		raise Utils.WafError("the folder %r is unreadable"%candidate)
	Utils.set_main_module(os.path.join(candidate,WSCRIPT_FILE))
	if build_dir_override:
		d=getattr(Utils.g_module,BLDDIR,None)
		if d:
			msg=' Overriding build directory %s with %s'%(d,build_dir_override)
			warn(msg)
		Utils.g_module.blddir=build_dir_override
	def set_def(obj,name=''):
		n=name or obj.__name__
		if not n in Utils.g_module.__dict__:
			setattr(Utils.g_module,n,obj)
	for k in[dist,distclean,distcheck,build,clean,install,uninstall]:
		set_def(k)
	set_def(Configure.ConfigurationContext,'configure_context')
	for k in['build','clean','install','uninstall']:
		set_def(Build.BuildContext,k+'_context')
	opt_obj=Options.Handler(Utils.g_module)
	opt_obj.curdir=candidate
	try:
		f=Utils.g_module.set_options
	except AttributeError:
		pass
	else:
		opt_obj.sub_options([''])
	opt_obj.parse_args()
	if not'init'in Utils.g_module.__dict__:
		Utils.g_module.init=Utils.nada
	if not'shutdown'in Utils.g_module.__dict__:
		Utils.g_module.shutdown=Utils.nada
	main()
def prepare(t,cwd,ver,wafdir):
	if WAFVERSION!=ver:
		msg='Version mismatch: waf %s <> wafadmin %s (wafdir %s)'%(ver,WAFVERSION,wafdir)
		print('\033[91mError: %s\033[0m'%msg)
		sys.exit(1)
	try:
		prepare_impl(t,cwd,ver,wafdir)
	except Utils.WafError,e:
		error(e)
		sys.exit(1)
	except KeyboardInterrupt:
		Utils.pprint('RED','Interrupted')
		sys.exit(68)
def main():
	global commands
	commands=Options.arg_line[:]
	while commands:
		x=commands.pop(0)
		ini=datetime.datetime.now()
		if x=='configure':
			fun=configure
		elif x=='build':
			fun=build
		else:
			fun=getattr(Utils.g_module,x,None)
		if not fun:
			raise Utils.WscriptError('No such command %r'%x)
		ctx=getattr(Utils.g_module,x+'_context',Utils.Context)()
		if x in['init','shutdown','dist','distclean','distcheck']:
			try:
				fun(ctx)
			except TypeError:
				fun()
		else:
			fun(ctx)
		ela=''
		if not Options.options.progress_bar:
			ela=' (%s)'%Utils.get_elapsed_time(ini)
		if x!='init'and x!='shutdown':
			info('%r finished successfully%s'%(x,ela))
		if not commands and x!='shutdown':
			commands.append('shutdown')
def configure(conf):
	src=getattr(Options.options,SRCDIR,None)
	if not src:src=getattr(Utils.g_module,SRCDIR,None)
	if not src:
		src='.'
		incomplete_src=1
	src=os.path.abspath(src)
	bld=getattr(Options.options,BLDDIR,None)
	if not bld:
		bld=getattr(Utils.g_module,BLDDIR,None)
		if bld=='.':
			raise Utils.WafError('Setting blddir="." may cause distclean problems')
	if not bld:
		bld='build'
		incomplete_bld=1
	bld=os.path.abspath(bld)
	try:os.makedirs(bld)
	except OSError:pass
	targets=Options.options.compile_targets
	Options.options.compile_targets=None
	Options.is_install=False
	conf.srcdir=src
	conf.blddir=bld
	conf.post_init()
	if'incomplete_src'in vars():
		conf.check_message_1('Setting srcdir to')
		conf.check_message_2(src)
	if'incomplete_bld'in vars():
		conf.check_message_1('Setting blddir to')
		conf.check_message_2(bld)
	conf.sub_config([''])
	conf.store()
	env=Environment.Environment()
	env[BLDDIR]=bld
	env[SRCDIR]=src
	env['argv']=sys.argv
	env['commands']=Options.commands
	env['options']=Options.options.__dict__
	env['hash']=conf.hash
	env['files']=conf.files
	env['environ']=dict(conf.environ)
	env['cwd']=os.path.split(Utils.g_module.root_path)[0]
	if Utils.g_module.root_path!=src:
		env.store(os.path.join(src,Options.lockfile))
	env.store(Options.lockfile)
	Options.options.compile_targets=targets
def clean(bld):
	'''removes the build files'''
	try:
		proj=Environment.Environment(Options.lockfile)
	except IOError:
		raise Utils.WafError('Nothing to clean (project not configured)')
	bld.load_dirs(proj[SRCDIR],proj[BLDDIR])
	bld.load_envs()
	bld.is_install=0
	bld.add_subdirs([os.path.split(Utils.g_module.root_path)[0]])
	try:
		bld.clean()
	finally:
		bld.save()
def check_configured(bld):
	if not Configure.autoconfig:
		return bld
	conf_cls=getattr(Utils.g_module,'configure_context',Utils.Context)
	bld_cls=getattr(Utils.g_module,'build_context',Utils.Context)
	def reconf(proj):
		back=(Options.commands,Options.options.__dict__,Logs.zones,Logs.verbose)
		Options.commands=proj['commands']
		Options.options.__dict__=proj['options']
		conf=conf_cls()
		conf.environ=proj['environ']
		configure(conf)
		(Options.commands,Options.options.__dict__,Logs.zones,Logs.verbose)=back
	try:
		proj=Environment.Environment(Options.lockfile)
	except IOError:
		conf=conf_cls()
		configure(conf)
	else:
		try:
			bld=bld_cls()
			bld.load_dirs(proj[SRCDIR],proj[BLDDIR])
			bld.load_envs()
		except Utils.WafError:
			reconf(proj)
			return bld_cls()
	try:
		proj=Environment.Environment(Options.lockfile)
	except IOError:
		raise Utils.WafError('Auto-config: project does not configure (bug)')
	h=0
	try:
		for file in proj['files']:
			if file.endswith('configure'):
				h=hash((h,Utils.readf(file)))
			else:
				mod=Utils.load_module(file)
				h=hash((h,mod.waf_hash_val))
	except(OSError,IOError):
		warn('Reconfiguring the project: a file is unavailable')
		reconf(proj)
	else:
		if(h!=proj['hash']):
			warn('Reconfiguring the project: the configuration has changed')
			reconf(proj)
	return bld_cls()
def install(bld):
	'''installs the build files'''
	bld=check_configured(bld)
	Options.commands['install']=True
	Options.commands['uninstall']=False
	Options.is_install=True
	bld.is_install=INSTALL
	build_impl(bld)
	bld.install()
def uninstall(bld):
	'''removes the installed files'''
	Options.commands['install']=False
	Options.commands['uninstall']=True
	Options.is_install=True
	bld.is_install=UNINSTALL
	try:
		def runnable_status(self):
			return SKIP_ME
		setattr(Task.Task,'runnable_status_back',Task.Task.runnable_status)
		setattr(Task.Task,'runnable_status',runnable_status)
		build_impl(bld)
		bld.install()
	finally:
		setattr(Task.Task,'runnable_status',Task.Task.runnable_status_back)
def build(bld):
	bld=check_configured(bld)
	Options.commands['install']=False
	Options.commands['uninstall']=False
	Options.is_install=False
	bld.is_install=0
	return build_impl(bld)
def build_impl(bld):
	try:
		proj=Environment.Environment(Options.lockfile)
	except IOError:
		raise Utils.WafError("Project not configured (run 'waf configure' first)")
	bld.load_dirs(proj[SRCDIR],proj[BLDDIR])
	bld.load_envs()
	info("Waf: Entering directory `%s'"%bld.bldnode.abspath())
	bld.add_subdirs([os.path.split(Utils.g_module.root_path)[0]])
	bld.pre_build()
	try:
		bld.compile()
	finally:
		if Options.options.progress_bar:print('')
		info("Waf: Leaving directory `%s'"%bld.bldnode.abspath())
	bld.post_build()
	bld.install()
excludes='.bzr .bzrignore .git .gitignore .svn CVS .cvsignore .arch-ids {arch} SCCS BitKeeper .hg Makefile Makefile.in config.log'.split()
dist_exts='~ .rej .orig .pyc .pyo .bak .tar.bz2 tar.gz .zip .swp'.split()
def dont_dist(name,src,build_dir):
	global excludes,dist_exts
	if(name.startswith(',,')or name.startswith('++')or name.startswith('.waf-1.')or(src=='.'and name==Options.lockfile)or name in excludes or name==build_dir):
		return True
	for ext in dist_exts:
		if name.endswith(ext):
			return True
	return False
def copytree(src,dst,build_dir):
	names=os.listdir(src)
	os.makedirs(dst)
	for name in names:
		srcname=os.path.join(src,name)
		dstname=os.path.join(dst,name)
		if dont_dist(name,src,build_dir):
			continue
		if os.path.isdir(srcname):
			copytree(srcname,dstname,build_dir)
		else:
			shutil.copy2(srcname,dstname)
def distclean(ctx=None):
	'''removes the build directory'''
	lst=os.listdir('.')
	for f in lst:
		if f==Options.lockfile:
			try:
				proj=Environment.Environment(f)
				shutil.rmtree(proj[BLDDIR])
			except(OSError,IOError):
				pass
			try:
				os.remove(f)
			except(OSError,IOError):
				pass
		if f.startswith('.waf-'):
			shutil.rmtree(f,ignore_errors=True)
def dist(appname='',version=''):
	'''makes a tarball for redistributing the sources'''
	import tarfile
	if not appname:appname=getattr(Utils.g_module,APPNAME,'noname')
	if not version:version=getattr(Utils.g_module,VERSION,'1.0')
	tmp_folder=appname+'-'+version
	arch_name=tmp_folder+'.tar.'+g_gz
	try:
		shutil.rmtree(tmp_folder)
	except(OSError,IOError):
		pass
	try:
		os.remove(arch_name)
	except(OSError,IOError):
		pass
	copytree('.',tmp_folder,getattr(Utils.g_module,BLDDIR,None))
	dist_hook=getattr(Utils.g_module,'dist_hook',None)
	if dist_hook:
		back=os.getcwd()
		os.chdir(tmp_folder)
		try:
			dist_hook()
		finally:
			os.chdir(back)
	tar=tarfile.open(arch_name,'w:'+g_gz)
	tar.add(tmp_folder)
	tar.close()
	info('The archive is ready: %s'%arch_name)
	if os.path.exists(tmp_folder):shutil.rmtree(tmp_folder)
	return arch_name
def distcheck(appname='',version=''):
	'''checks if the sources compile (tarball from 'dist')'''
	import tempfile,tarfile
	if not appname:appname=getattr(Utils.g_module,APPNAME,'noname')
	if not version:version=getattr(Utils.g_module,VERSION,'1.0')
	waf=os.path.abspath(sys.argv[0])
	tarball=dist(appname,version)
	t=tarfile.open(tarball)
	for x in t:t.extract(x)
	t.close()
	path=appname+'-'+version
	instdir=tempfile.mkdtemp('.inst','%s-%s'%(appname,version))
	ret=Utils.pproc.Popen([waf,'configure','install','uninstall','--destdir='+instdir],cwd=path).wait()
	if ret:
		raise Utils.WafError('distcheck failed with code %i'%ret)
	if os.path.exists(instdir):
		raise Utils.WafError('distcheck succeeded, but files were left in %s'%instdir)
	shutil.rmtree(path)
def add_subdir(dir,bld):
	bld.recurse(dir,'build')


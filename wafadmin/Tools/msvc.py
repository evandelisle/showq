#! /usr/bin/env python
# encoding: utf-8

import os,sys,re,string,optparse
import Utils,TaskGen,Runner,Configure,Task,Options
from Logs import debug,error,warn
from TaskGen import after,before,feature
from Configure import conftest,conf
import ccroot,cc,cxx,ar,winres
from libtool import read_la_file
import _winreg
all_msvc_platforms=[('x64','amd64'),('x86','x86'),('ia64','ia64'),('x86_amd64','amd64'),('x86_ia64','ia64')]
all_icl_platforms=[('Itanium','ia64'),('intel64','amd64'),('em64t','amd64'),('ia32','x86')]
def setup_msvc(conf,versions):
	platforms=Utils.to_list(conf.env['MSVC_TARGETS'])or[i for i,j in all_msvc_platforms+all_icl_platforms]
	desired_versions=conf.env['MSVC_VERSIONS']or[v for v,_ in versions][::-1]
	versiondict=dict(versions)
	for version in desired_versions:
		try:
			targets=dict(versiondict[version])
			for target in platforms:
				try:
					arch,(p1,p2,p3)=targets[target]
					compiler,version=version.split()
					return compiler,p1,p2,p3
				except KeyError:continue
		except KeyError:continue
	conf.fatal('msvc: Impossible to find a valid architecture for building')
def get_msvc_version(conf,version,target,vcvars):
	batfile=os.path.join(conf.blddir,"waf-print-msvc.bat")
	f=open(batfile,'w')
	f.write("""@echo off
set INCLUDE=
set LIB=
call %1 %2
echo PATH=%PATH%
echo INCLUDE=%INCLUDE%
echo LIB=%LIB%
""")
	f.close()
	sout=Utils.cmd_output(['cmd','/E:on','/V:on','/C',batfile,vcvars,target])
	lines=sout.splitlines()
	if lines[0].find("Setting environment")==-1 and lines[0].find("Setting SDK environment")==-1 and lines[1].find('Intel(R) C++ Compiler')==-1:
		conf.fatal('msvc: Impossible to find a valid architecture for building')
	for line in lines[1:]:
		if line.startswith('PATH='):
			MSVC_PATH=line[5:].split(';')
		elif line.startswith('INCLUDE='):
			MSVC_INCDIR=[i for i in line[8:].split(';')if i]
		elif line.startswith('LIB='):
			MSVC_LIBDIR=[i for i in line[4:].split(';')if i]
	return(MSVC_PATH,MSVC_INCDIR,MSVC_LIBDIR)
def gather_wsdk_versions(conf,versions):
	version_pattern=re.compile('^v..?.?\...?.?')
	try:
		all_versions=_winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Wow6432node\\Microsoft\\Microsoft SDKs\\Windows')
	except WindowsError:
		try:
			all_versions=_winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows')
		except WindowsError:
			return
	index=0
	while 1:
		try:
			version=_winreg.EnumKey(all_versions,index)
		except WindowsError:
			break
		index=index+1
		if not version_pattern.match(version):
			continue
		try:
			msvc_version=_winreg.OpenKey(all_versions,version)
			path,type=_winreg.QueryValueEx(msvc_version,'InstallationFolder')
		except WindowsError:
			continue
		if os.path.isfile(os.path.join(path,'bin','SetEnv.cmd')):
			targets=[]
			for target,arch in all_msvc_platforms:
				try:
					targets.append((target,(arch,conf.get_msvc_version(version,'/'+target,os.path.join(path,'bin','SetEnv.cmd')))))
				except Configure.ConfigurationError:
					pass
			versions.append(('wsdk '+version[1:],targets))
def gather_msvc_versions(conf,versions):
	version_pattern=re.compile('^..?\...?')
	for vcver,vcvar in[('VCExpress','exp'),('VisualStudio','')]:
		try:
			all_versions=_winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Wow6432node\\Microsoft\\'+vcver)
		except WindowsError:
			try:
				all_versions=_winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Microsoft\\'+vcver)
			except WindowsError:
				continue
		index=0
		while 1:
			try:
				version=_winreg.EnumKey(all_versions,index)
			except WindowsError:
				break
			index=index+1
			if not version_pattern.match(version):
				continue
			try:
				msvc_version=_winreg.OpenKey(all_versions,version+"\\Setup\\VS")
				path,type=_winreg.QueryValueEx(msvc_version,'ProductDir')
				targets=[]
				if os.path.isfile(os.path.join(path,'VC','vcvarsall.bat')):
					for target,realtarget in all_msvc_platforms[::-1]:
						try:
							targets.append((target,(realtarget,conf.get_msvc_version(version,target,os.path.join(path,'VC','vcvarsall.bat')))))
						except:
							pass
				elif os.path.isfile(os.path.join(path,'Common7','Tools','vsvars32.bat')):
					try:
						targets.append(('x86',('x86',conf.get_msvc_version(version,'x86',os.path.join(path,'Common7','Tools','vsvars32.bat')))))
					except Configure.ConfigurationError:
						pass
				versions.append(('msvc '+version,targets))
			except WindowsError:
				continue
def gather_icl_versions(conf,versions):
	version_pattern=re.compile('^...?.?\....?.?')
	try:
		all_versions=_winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Wow6432node\\Intel\\Compilers\\C++')
	except WindowsError:
		try:
			all_versions=_winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Intel\\Compilers\\C++')
		except WindowsError:
			return
	index=0
	while 1:
		try:
			version=_winreg.EnumKey(all_versions,index)
		except WindowsError:
			break
		index=index+1
		if not version_pattern.match(version):
			continue
		targets=[]
		for target,arch in all_icl_platforms:
			try:
				icl_version=_winreg.OpenKey(all_versions,version+'\\'+target)
				path,type=_winreg.QueryValueEx(icl_version,'ProductDir')
				if os.path.isfile(os.path.join(path,'bin','iclvars.bat')):
					try:
						targets.append((target,(arch,conf.get_msvc_version(version,target,os.path.join(path,'bin','iclvars.bat')))))
					except Configure.ConfigurationError:
						pass
			except WindowsError:
				continue
		major=version[0:2]
		versions.append(('intel '+major,targets))
def get_msvc_versions(conf):
	if not conf.env['MSVC_INSTALLED_VERSIONS']:
		conf.env['MSVC_INSTALLED_VERSIONS']=[]
		conf.gather_msvc_versions(conf.env['MSVC_INSTALLED_VERSIONS'])
		conf.gather_wsdk_versions(conf.env['MSVC_INSTALLED_VERSIONS'])
		conf.gather_icl_versions(conf.env['MSVC_INSTALLED_VERSIONS'])
	return conf.env['MSVC_INSTALLED_VERSIONS']
def detect_msvc(conf):
	versions=get_msvc_versions(conf)
	return setup_msvc(conf,versions)
def msvc_linker(task):
	static=task.__class__.name.find('static')>0
	e=env=task.env
	subsystem=getattr(task.generator,'subsystem','')
	if subsystem:
		subsystem='/subsystem:%s'%subsystem
	outfile=task.outputs[0].bldpath(e)
	manifest=outfile+'.manifest'
	def to_list(xx):
		if isinstance(xx,str):return[xx]
		return xx
	lst=[]
	if static:
		lst.extend(to_list(env['STLIBLINK']))
	else:
		lst.extend(to_list(env['LINK']))
	lst.extend(to_list(subsystem))
	if static:
		lst.extend(to_list(env['STLINKFLAGS']))
	else:
		lst.extend(to_list(env['LINKFLAGS']))
	lst.extend([a.srcpath(env)for a in task.inputs])
	lst.extend(to_list('/OUT:%s'%outfile))
	lst=[x for x in lst if x]
	lst=[lst]
	ret=task.exec_command(*lst)
	if ret:return ret
	pdbnode=task.outputs[0].change_ext('.pdb')
	pdbfile=pdbnode.bldpath(e)
	if os.path.exists(pdbfile):
		task.outputs.append(pdbnode)
	if not static and os.path.exists(manifest):
		debug('msvc: manifesttool')
		mtool=e['MT']
		if not mtool:
			return 0
		mode=''
		if'cprogram'in task.generator.features:
			mode='1'
		elif'cshlib'in task.generator.features:
			mode='2'
		debug('msvc: embedding manifest')
		lst=[]
		lst.extend(to_list(e['MT']))
		lst.extend(to_list(e['MTFLAGS']))
		lst.extend(to_list("-manifest"))
		lst.extend(to_list(manifest))
		lst.extend(to_list("-outputresource:%s;%s"%(outfile,mode)))
		lst=[lst]
		ret=task.exec_command(*lst)
	return ret
g_msvc_systemlibs="""
aclui activeds ad1 adptif adsiid advapi32 asycfilt authz bhsupp bits bufferoverflowu cabinet
cap certadm certidl ciuuid clusapi comctl32 comdlg32 comsupp comsuppd comsuppw comsuppwd comsvcs
credui crypt32 cryptnet cryptui d3d8thk daouuid dbgeng dbghelp dciman32 ddao35 ddao35d
ddao35u ddao35ud delayimp dhcpcsvc dhcpsapi dlcapi dnsapi dsprop dsuiext dtchelp
faultrep fcachdll fci fdi framedyd framedyn gdi32 gdiplus glauxglu32 gpedit gpmuuid
gtrts32w gtrtst32hlink htmlhelp httpapi icm32 icmui imagehlp imm32 iphlpapi iprop
kernel32 ksguid ksproxy ksuser libcmt libcmtd libcpmt libcpmtd loadperf lz32 mapi
mapi32 mgmtapi minidump mmc mobsync mpr mprapi mqoa mqrt msacm32 mscms mscoree
msdasc msimg32 msrating mstask msvcmrt msvcurt msvcurtd mswsock msxml2 mtx mtxdm
netapi32 nmapinmsupp npptools ntdsapi ntdsbcli ntmsapi ntquery odbc32 odbcbcp
odbccp32 oldnames ole32 oleacc oleaut32 oledb oledlgolepro32 opends60 opengl32
osptk parser pdh penter pgobootrun pgort powrprof psapi ptrustm ptrustmd ptrustu
ptrustud qosname rasapi32 rasdlg rassapi resutils riched20 rpcndr rpcns4 rpcrt4 rtm
rtutils runtmchk scarddlg scrnsave scrnsavw secur32 sensapi setupapi sfc shell32
shfolder shlwapi sisbkup snmpapi sporder srclient sti strsafe svcguid tapi32 thunk32
traffic unicows url urlmon user32 userenv usp10 uuid uxtheme vcomp vcompd vdmdbg
version vfw32 wbemuuid  webpost wiaguid wininet winmm winscard winspool winstrm
wintrust wldap32 wmiutils wow32 ws2_32 wsnmp32 wsock32 wst wtsapi32 xaswitch xolehlp
""".split()
def find_lt_names_msvc(self,libname,is_static=False):
	lt_names=['lib%s.la'%libname,'%s.la'%libname,]
	for path in self.env['LIBPATH']:
		for la in lt_names:
			laf=os.path.join(path,la)
			dll=None
			if os.path.exists(laf):
				ltdict=read_la_file(laf)
				lt_libdir=None
				if ltdict.get('libdir',''):
					lt_libdir=ltdict['libdir']
				if not is_static and ltdict.get('library_names',''):
					dllnames=ltdict['library_names'].split()
					dll=dllnames[0].lower()
					dll=re.sub('\.dll$','',dll)
					return(lt_libdir,dll,False)
				elif ltdict.get('old_library',''):
					olib=ltdict['old_library']
					if os.path.exists(os.path.join(path,olib)):
						return(path,olib,True)
					elif lt_libdir!=''and os.path.exists(os.path.join(lt_libdir,olib)):
						return(lt_libdir,olib,True)
					else:
						return(None,olib,True)
				else:
					raise Utils.WafError('invalid libtool object file: %s'%laf)
	return(None,None,None)
def libname_msvc(self,libname,is_static=False,mandatory=False):
	lib=libname.lower()
	lib=re.sub('\.lib$','',lib)
	if lib in g_msvc_systemlibs:
		return lib
	lib=re.sub('^lib','',lib)
	if lib=='m':
		return None
	(lt_path,lt_libname,lt_static)=self.find_lt_names_msvc(lib,is_static)
	if lt_path!=None and lt_libname!=None:
		if lt_static==True:
			return os.path.join(lt_path,lt_libname)
	if lt_path!=None:
		_libpaths=[lt_path]+self.env['LIBPATH']
	else:
		_libpaths=self.env['LIBPATH']
	static_libs=['%ss.lib'%lib,'lib%ss.lib'%lib,'%s.lib'%lib,'lib%s.lib'%lib,]
	dynamic_libs=['lib%s.dll.lib'%lib,'lib%s.dll.a'%lib,'%s.dll.lib'%lib,'%s.dll.a'%lib,'lib%s_d.lib'%lib,'%s_d.lib'%lib,'%s.lib'%lib,]
	libnames=static_libs
	if not is_static:
		libnames=dynamic_libs+static_libs
	for path in _libpaths:
		for libn in libnames:
			if os.path.exists(os.path.join(path,libn)):
				debug('msvc: lib found: %s'%os.path.join(path,libn))
				return re.sub('\.lib$','',libn)
	if mandatory:
		self.fatal("The library %r could not be found"%libname)
	return re.sub('\.lib$','',libname)
def check_lib_msvc(self,libname,is_static=False,uselib_store=None,mandatory=False):
	libn=self.libname_msvc(libname,is_static,mandatory)
	if not uselib_store:
		uselib_store=libname.upper()
	if is_static:
		self.env['LIB_'+uselib_store]=[libn]
	else:
		self.env['STATICLIB_'+uselib_store]=[libn]
def check_libs_msvc(self,libnames,is_static=False,mandatory=False):
	for libname in Utils.to_list(libnames):
		self.check_lib_msvc(libname,is_static,mandatory=mandatory)
def apply_obj_vars_msvc(self):
	if self.env['CC_NAME']!='msvc':
		return
	try:
		self.meths.remove('apply_obj_vars')
	except ValueError:
		pass
	env=self.env
	app=env.append_unique
	cpppath_st=env['CPPPATH_ST']
	lib_st=env['LIB_ST']
	staticlib_st=env['STATICLIB_ST']
	libpath_st=env['LIBPATH_ST']
	staticlibpath_st=env['STATICLIBPATH_ST']
	for i in env['LIBPATH']:
		app('LINKFLAGS',libpath_st%i)
		if not self.libpaths.count(i):
			self.libpaths.append(i)
	for i in env['LIBPATH']:
		app('LINKFLAGS',staticlibpath_st%i)
		if not self.libpaths.count(i):
			self.libpaths.append(i)
	if not env['FULLSTATIC']:
		if env['STATICLIB']or env['LIB']:
			app('LINKFLAGS',env['SHLIB_MARKER'])
	for i in env['STATICLIB']:
		app('LINKFLAGS',lib_st%i)
	for i in env['LIB']:
		app('LINKFLAGS',lib_st%i)
def apply_link_msvc(self):
	if self.env['CC_NAME']!='msvc':
		return
	link=getattr(self,'link',None)
	if not link:
		if'cstaticlib'in self.features:link='msvc_link_static'
		elif'cxx'in self.features:link='msvc_cxx_link'
		else:link='msvc_cc_link'
		self.vnum=''
	self.link=link
def init_msvc(self):
	try:getattr(self,'libpaths')
	except AttributeError:self.libpaths=[]
Task.task_type_from_func('msvc_link_static',vars=['STLIBLINK','STLINKFLAGS'],color='YELLOW',func=msvc_linker,ext_in='.o')
Task.task_type_from_func('msvc_cc_link',vars=['LINK','LINK_SRC_F','LINKFLAGS','MT','MTFLAGS'],color='YELLOW',func=msvc_linker,ext_in='.o')
Task.task_type_from_func('msvc_cxx_link',vars=['LINK','LINK_SRC_F','LINKFLAGS','MT','MTFLAGS'],color='YELLOW',func=msvc_linker,ext_in='.o')
rc_str='${RC} ${RCFLAGS} /fo ${TGT} ${SRC}'
Task.simple_task_type('rc',rc_str,color='GREEN',before='cc cxx',shell=False)
def no_autodetect(conf):
	conf.eval_rules(detect.replace('autodetect',''))
detect='''
autodetect
find_msvc
msvc_common_flags
cc_load_tools
cxx_load_tools
cc_add_flags
cxx_add_flags
'''
def autodetect(conf):
	v=conf.env
	compiler,path,includes,libdirs=detect_msvc(conf)
	v['PATH']=path
	v['CPPPATH']=includes
	v['LIBPATH']=libdirs
	v['MSVC_COMPILER']=compiler
def find_msvc(conf):
	if sys.platform!='win32':
		conf.fatal('MSVC module only works under native Win32 Python! cygwin is not supported yet')
	v=conf.env
	compiler,path,includes,libdirs=detect_msvc(conf)
	v['PATH']=path
	v['CPPPATH']=includes
	v['LIBPATH']=libdirs
	if compiler=='msvc'or compiler=='wsdk':
		compiler_name='CL'
		linker_name='LINK'
		lib_name='LIB'
	elif compiler=='intel':
		compiler_name='ICL'
		linker_name='XILINK'
		lib_name='XILIB'
	else:
		conf.fatal('Unknown compiler type : %s'%compiler)
	cxx=None
	if v['CXX']:cxx=v['CXX']
	elif'CXX'in conf.environ:cxx=conf.environ['CXX']
	if not cxx:cxx=conf.find_program(compiler_name,var='CXX',path_list=path)
	if not cxx:conf.fatal('%s was not found (compiler)'%compiler_name)
	cxx=conf.cmd_to_list(cxx)
	env=dict(conf.environ)
	env.update(PATH=';'.join(path))
	if not Utils.cmd_output([cxx,'/nologo','/?'],silent=True,env=env):
		conf.fatal('the msvc compiler could not be identified')
	v['CC']=v['CXX']=cxx
	v['CC_NAME']=v['CXX_NAME']='msvc'
	try:v.prepend_value('CPPPATH',conf.environ['INCLUDE'])
	except KeyError:pass
	try:v.prepend_value('LIBPATH',conf.environ['LIB'])
	except KeyError:pass
	if not v['LINK_CXX']:
		link=conf.find_program(linker_name,path_list=path)
		if link:v['LINK_CXX']=link
		else:conf.fatal('%s was not found (linker)'%linker_name)
	v['LINK']=link
	if not v['LINK_CC']:v['LINK_CC']=v['LINK_CXX']
	if not v['STLIBLINK']:
		stliblink=conf.find_program(lib_name,path_list=path)
		if not stliblink:return
		v['STLIBLINK']=stliblink
		v['STLINKFLAGS']=['/NOLOGO']
	manifesttool=conf.find_program('MT',path_list=path)
	if manifesttool:
		v['MT']=manifesttool
		v['MTFLAGS']=['/NOLOGO']
	conf.check_tool('winres')
	if not conf.env['WINRC']:
		warn('Resource compiler not found. Compiling resource file is disabled')
def exec_command_msvc(self,*k,**kw):
	if self.env['CC_NAME']=='msvc':
		if isinstance(k[0],list):
			lst=[]
			carry=''
			for a in k[0]:
				if(len(a)==3 and(a.startswith('/F')or a.startswith('/Y')))or(a=='/doc'):
					carry=a
				else:
					lst.append(carry+a)
					carry=''
			k=[lst]
		env=dict(os.environ)
		env.update(PATH=';'.join(self.env['PATH']))
		kw['env']=env
	return self.generator.bld.exec_command(*k,**kw)
for k in'cc cxx msvc_cc_link msvc_cxx_link msvc_link_static winrc'.split():
	cls=Task.TaskBase.classes.get(k,None)
	if cls:
		cls.exec_command=exec_command_msvc
def msvc_common_flags(conf):
	v=conf.env
	v['CPPFLAGS']=['/W3','/nologo','/EHsc']
	v['CCDEFINES']=['WIN32']
	v['CXXDEFINES']=['WIN32']
	v['_CCINCFLAGS']=[]
	v['_CCDEFFLAGS']=[]
	v['_CXXINCFLAGS']=[]
	v['_CXXDEFFLAGS']=[]
	v['CC_SRC_F']=''
	v['CC_TGT_F']=['/c','/Fo']
	v['CXX_SRC_F']=''
	v['CXX_TGT_F']=['/c','/Fo']
	v['CPPPATH_ST']='/I%s'
	v['CPPFLAGS_CONSOLE']=['/SUBSYSTEM:CONSOLE']
	v['CPPFLAGS_NATIVE']=['/SUBSYSTEM:NATIVE']
	v['CPPFLAGS_POSIX']=['/SUBSYSTEM:POSIX']
	v['CPPFLAGS_WINDOWS']=['/SUBSYSTEM:WINDOWS']
	v['CPPFLAGS_WINDOWSCE']=['/SUBSYSTEM:WINDOWSCE']
	v['CPPFLAGS_CRT_MULTITHREADED']=['/MT']
	v['CPPFLAGS_CRT_MULTITHREADED_DLL']=['/MD']
	v['CPPDEFINES_CRT_MULTITHREADED']=['_MT']
	v['CPPDEFINES_CRT_MULTITHREADED_DLL']=['_MT','_DLL']
	v['CPPFLAGS_CRT_MULTITHREADED_DBG']=['/MTd']
	v['CPPFLAGS_CRT_MULTITHREADED_DLL_DBG']=['/MDd']
	v['CPPDEFINES_CRT_MULTITHREADED_DBG']=['_DEBUG','_MT']
	v['CPPDEFINES_CRT_MULTITHREADED_DLL_DBG']=['_DEBUG','_MT','_DLL']
	v['CCFLAGS']=['/TC']
	v['CCFLAGS_OPTIMIZED']=['/O2','/DNDEBUG']
	v['CCFLAGS_RELEASE']=['/O2','/DNDEBUG']
	v['CCFLAGS_DEBUG']=['/Od','/RTC1','/D_DEBUG','/ZI']
	v['CCFLAGS_ULTRADEBUG']=['/Od','/RTC1','/D_DEBUG','/ZI']
	v['CXXFLAGS']=['/TP']
	v['CXXFLAGS_OPTIMIZED']=['/O2','/DNDEBUG']
	v['CXXFLAGS_RELEASE']=['/O2','/DNDEBUG']
	v['CXXFLAGS_DEBUG']=['/Od','/RTC1','/D_DEBUG','/ZI']
	v['CXXFLAGS_ULTRADEBUG']=['/Od','/RTC1','/D_DEBUG','/ZI']
	v['LIB']=[]
	v['LINK_TGT_F']='/OUT:'
	v['LINK_SRC_F']=''
	v['LIB_ST']='%s.lib'
	v['LIBPATH_ST']='/LIBPATH:%s'
	v['STATICLIB_ST']='%s.lib'
	v['STATICLIBPATH_ST']='/LIBPATH:%s'
	v['CCDEFINES_ST']='/D%s'
	v['CXXDEFINES_ST']='/D%s'
	v['LINKFLAGS']=['/NOLOGO','/MANIFEST']
	v['shlib_CCFLAGS']=['']
	v['shlib_CXXFLAGS']=['']
	v['shlib_LINKFLAGS']=['/DLL']
	v['shlib_PATTERN']='%s.dll'
	v['staticlib_LINKFLAGS']=['']
	v['staticlib_PATTERN']='%s.lib'
	v['program_PATTERN']='%s.exe'

conf(get_msvc_version)
conf(gather_wsdk_versions)
conf(gather_msvc_versions)
conf(gather_icl_versions)
conf(get_msvc_versions)
conf(find_lt_names_msvc)
conf(libname_msvc)
conf(check_lib_msvc)
conf(check_libs_msvc)
feature('cprogram','cshlib','cstaticlib')(apply_obj_vars_msvc)
after('apply_lib_vars')(apply_obj_vars_msvc)
before('apply_obj_vars')(apply_obj_vars_msvc)
feature('cprogram','cshlib','cstaticlib')(apply_link_msvc)
before('apply_link')(apply_link_msvc)
feature('cc','cxx')(init_msvc)
after('init_cc','init_cxx')(init_msvc)
before('apply_type_vars','apply_core')(init_msvc)
conftest(no_autodetect)
conftest(autodetect)
conftest(find_msvc)
conftest(msvc_common_flags)

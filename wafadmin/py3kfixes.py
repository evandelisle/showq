#! /usr/bin/env python
# encoding: utf-8

import os
all_modifs={}
def modif(filename,fun):
	f=open(filename,'r')
	txt=f.read()
	f.close()
	txt=fun(txt)
	f=open(filename,'w')
	f.write(txt)
	f.close()
def subst(filename):
	def do_subst(fun):
		global all_modifs
		try:
			all_modifs[filename]+=fun
		except KeyError:
			all_modifs[filename]=[fun]
		return fun
	return do_subst
def r1(code):
	code=code.replace("'iluvcuteoverload'","b'iluvcuteoverload'")
	code=code.replace("ABI=7","ABI=37")
	return code
def r2(code):
	code=code.replace("p.stdin.write('\\n')","p.stdin.write(b'\\n')")
	code=code.replace("out=str(out)","out=out.decode('utf-8')")
	return code
def r3(code):
	code=code.replace("m.update(str(lst))","m.update(str(lst).encode())")
	return code
def r4(code):
	code=code.replace("up(self.__class__.__name__)","up(self.__class__.__name__.encode())")
	code=code.replace("up(self.env.variant())","up(self.env.variant().encode())")
	code=code.replace("up(x.parent.abspath())","up(x.parent.abspath().encode())")
	code=code.replace("up(x.name)","up(x.name.encode())")
	return code
def r5(code):
	code=code.replace("cPickle.dump(data,file,-1)","cPickle.dump(data,file)")
	return code
def fixdir(dir):
	import subprocess
	try:
		proc=subprocess.Popen("2to3 -x imports -x imports2 -x import -w -n wafadmin".split(),stdout=subprocess.PIPE,stderr=subprocess.PIPE)
		stdout,setderr=proc.communicate()
	except:
		import sys,shutil
		shutil.rmtree(dir)
		raise
	global all_modifs
	for k in all_modifs:
		for v in all_modifs[k]:
			modif(os.path.join(dir,'wafadmin',k),v)

subst('Constants.py')(r1)
subst('Tools/ccroot.py')(r2)
subst('Utils.py')(r3)
subst('Task.py')(r4)
subst('Build.py')(r5)

#! /usr/bin/env python
# encoding: utf-8

import os,sys
import Build,TaskGen,Utils,Options,Logs
class unit_test(object):
	def __init__(self):
		self.returncode_ok=0
		self.num_tests_ok=0
		self.num_tests_failed=0
		self.num_tests_err=0
		self.total_num_tests=0
		self.max_label_length=0
		self.unit_tests=Utils.ordered_dict()
		self.unit_test_results={}
		self.unit_test_erroneous={}
		self.change_to_testfile_dir=False
		self.want_to_see_test_output=False
		self.want_to_see_test_error=False
		self.run_if_waf_does='check'
	def run(self):
		self.num_tests_ok=0
		self.num_tests_failed=0
		self.num_tests_err=0
		self.total_num_tests=0
		self.max_label_length=0
		self.unit_tests=Utils.ordered_dict()
		self.unit_test_results={}
		self.unit_test_erroneous={}
		ld_library_path=[]
		if not Options.commands[self.run_if_waf_does]:return
		for obj in Build.bld.all_task_gen:
			try:
				link_task=obj.link_task
			except AttributeError:
				pass
			else:
				lib_path=link_task.outputs[0].parent.abspath(obj.env)
				if lib_path not in ld_library_path:
					ld_library_path.append(lib_path)
			unit_test=getattr(obj,'unit_test','')
			if unit_test and'cprogram'in obj.features:
				try:
					output=obj.path
					filename=os.path.join(output.abspath(obj.env),obj.target)
					srcdir=output.abspath()
					label=os.path.join(output.bldpath(obj.env),obj.target)
					self.max_label_length=max(self.max_label_length,len(label))
					self.unit_tests[label]=(filename,srcdir)
				except KeyError:
					pass
		self.total_num_tests=len(self.unit_tests)
		Utils.pprint('GREEN','Running the unit tests')
		count=0
		result=1
		for label in self.unit_tests.allkeys:
			file_and_src=self.unit_tests[label]
			filename=file_and_src[0]
			srcdir=file_and_src[1]
			count+=1
			line=Build.bld.progress_line(count,self.total_num_tests,Logs.colors.GREEN,Logs.colors.NORMAL)
			if Options.options.progress_bar and line:
				sys.stderr.write(line)
				sys.stderr.flush()
			try:
				kwargs={}
				kwargs['env']=os.environ.copy()
				if self.change_to_testfile_dir:
					kwargs['cwd']=srcdir
				if not self.want_to_see_test_output:
					kwargs['stdout']=Utils.pproc.PIPE
				if not self.want_to_see_test_error:
					kwargs['stderr']=Utils.pproc.PIPE
				if ld_library_path:
					if sys.platform=='win32':
						kwargs['env']['PATH']=';'.join(ld_library_path+[os.environ.get('PATH','')])
					else:
						kwargs['env']['LD_LIBRARY_PATH']=':'.join(ld_library_path+[os.environ.get('LD_LIBRARY_PATH','')])
				pp=Utils.pproc.Popen(filename,**kwargs)
				pp.wait()
				result=int(pp.returncode==self.returncode_ok)
				if result:
					self.num_tests_ok+=1
				else:
					self.num_tests_failed+=1
				self.unit_test_results[label]=result
				self.unit_test_erroneous[label]=0
			except OSError:
				self.unit_test_erroneous[label]=1
				self.num_tests_err+=1
			except KeyboardInterrupt:
				pass
		if Options.options.progress_bar:sys.stdout.write(Logs.colors.cursor_on)
	def print_results(self):
		if not Options.commands[self.run_if_waf_does]:return
		p=Utils.pprint
		if self.total_num_tests==0:
			p('YELLOW','No unit tests present')
			return
		p('GREEN','Running unit tests')
		p('NORMAL','')
		for label in self.unit_tests.allkeys:
			filename=self.unit_tests[label]
			err=0
			result=0
			try:err=self.unit_test_erroneous[label]
			except KeyError:pass
			try:result=self.unit_test_results[label]
			except KeyError:pass
			n=self.max_label_length-len(label)
			if err:n+=4
			elif result:n+=7
			else:n+=3
			line='%s %s'%(label,'.'*n)
			if err:p('RED','%sERROR'%line)
			elif result:p('GREEN','%sOK'%line)
			else:p('YELLOW','%sFAILED'%line)
		percentage_ok=float(self.num_tests_ok)/float(self.total_num_tests)*100.0
		percentage_failed=float(self.num_tests_failed)/float(self.total_num_tests)*100.0
		percentage_erroneous=float(self.num_tests_err)/float(self.total_num_tests)*100.0
		p('NORMAL','''
Successful tests:      %i (%.1f%%)
Failed tests:          %i (%.1f%%)
Erroneous tests:       %i (%.1f%%)

Total number of tests: %i
'''%(self.num_tests_ok,percentage_ok,self.num_tests_failed,percentage_failed,self.num_tests_err,percentage_erroneous,self.total_num_tests))
		p('GREEN','Unit tests finished')


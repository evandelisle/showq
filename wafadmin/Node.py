#! /usr/bin/env python
# encoding: utf-8

import os,sys,fnmatch,re
import Utils
UNDEFINED=0
DIR=1
FILE=2
BUILD=3
type_to_string={UNDEFINED:"unk",DIR:"dir",FILE:"src",BUILD:"bld"}
exclude_regs='''
**/*~
**/#*#
**/.#*
**/%*%
**/._*
**/CVS
**/CVS/**
**/.cvsignore
**/SCCS
**/SCCS/**
**/vssver.scc
**/.svn
**/.svn/**
**/.DS_Store'''.split()
exc_fun=None
def default_excludes():
	global exc_fun
	if exc_fun:
		return exc_fun
	regs=[Utils.jar_regexp(x)for x in exclude_regs]
	def mat(path):
		for x in regs:
			if x.match(path):
				return True
		return False
	exc_fun=mat
	return exc_fun
class Node(object):
	__slots__=("name","parent","id","childs")
	def __init__(self,name,parent,node_type=UNDEFINED):
		self.name=name
		self.parent=parent
		self.__class__.bld.id_nodes+=4
		self.id=self.__class__.bld.id_nodes+node_type
		if node_type==DIR:self.childs={}
		if parent and name in parent.childs:
			raise Utils.WafError('node %s exists in the parent files %r already'%(name,parent))
		if parent:parent.childs[name]=self
	def __setstate__(self,data):
		if len(data)==4:
			(self.parent,self.name,self.id,self.childs)=data
		else:
			(self.parent,self.name,self.id)=data
	def __getstate__(self):
		if getattr(self,'childs',None)is None:
			return(self.parent,self.name,self.id)
		else:
			return(self.parent,self.name,self.id,self.childs)
	def __str__(self):
		if not self.parent:return''
		return"%s://%s"%(type_to_string[self.id&3],self.abspath())
	def __repr__(self):
		return self.__str__()
	def __hash__(self):
		raise Utils.WafError('nodes, you are doing it wrong')
	def __copy__(self):
		raise Utils.WafError('nodes are not supposed to be cloned')
	def get_type(self):
		return self.id&3
	def set_type(self,t):
		self.id=self.id+t-self.id&3
	def dirs(self):
		return[x for x in self.childs.values()if x.id&3==DIR]
	def files(self):
		return[x for x in self.childs.values()if x.id&3==FILE]
	def get_dir(self,name,default=None):
		node=self.childs.get(name,None)
		if not node or node.id&3!=DIR:return default
		return node
	def get_file(self,name,default=None):
		node=self.childs.get(name,None)
		if not node or node.id&3!=FILE:return default
		return node
	def get_build(self,name,default=None):
		node=self.childs.get(name,None)
		if not node or node.id&3!=BUILD:return default
		return node
	def find_resource(self,lst):
		if isinstance(lst,str):
			lst=Utils.split_path(lst)
		if len(lst)==1:
			parent=self
		else:
			parent=self.find_dir(lst[:-1])
			if not parent:return None
		self.__class__.bld.rescan(parent)
		name=lst[-1]
		node=parent.childs.get(name,None)
		if node:
			tp=node.id&3
			if tp==FILE or tp==BUILD:
				return node
			else:
				return None
		tree=self.__class__.bld
		if not name in tree.cache_dir_contents[parent.id]:
			return None
		path=parent.abspath()+os.sep+name
		try:
			st=Utils.h_file(path)
		except IOError:
			return None
		child=self.__class__(name,parent,FILE)
		tree.node_sigs[0][child.id]=st
		return child
	def find_or_declare(self,lst):
		if isinstance(lst,str):
			lst=Utils.split_path(lst)
		if len(lst)==1:
			parent=self
		else:
			parent=self.find_dir(lst[:-1])
			if not parent:return None
		self.__class__.bld.rescan(parent)
		name=lst[-1]
		node=parent.childs.get(name,None)
		if node:
			tp=node.id&3
			if tp!=BUILD:
				raise Utils.WafError("find_or_declare returns a build node, not a source nor a directory %r"%lst)
			return node
		node=self.__class__(name,parent,BUILD)
		return node
	def find_dir(self,lst):
		if isinstance(lst,str):
			lst=Utils.split_path(lst)
		current=self
		for name in lst:
			self.__class__.bld.rescan(current)
			prev=current
			if not current.parent and name==current.name:
				continue
			elif not name:
				continue
			elif name=='.':
				continue
			elif name=='..':
				current=current.parent or current
			else:
				current=prev.childs.get(name,None)
				if current is None:
					dir_cont=self.__class__.bld.cache_dir_contents
					if prev.id in dir_cont and name in dir_cont[prev.id]:
						if not prev.name:
							if os.sep=='/':
								dirname=os.sep+name
							else:
								dirname=name
						else:
							dirname=prev.abspath()+os.sep+name
						if not os.path.isdir(dirname):
							return None
						current=self.__class__(name,prev,DIR)
					elif(not prev.name and len(name)==2 and name[1]==':')or name.startswith('\\\\'):
						current=self.__class__(name,prev,DIR)
					else:
						return None
				else:
					if current.id&3!=DIR:
						return None
		return current
	def ensure_dir_node_from_path(self,lst):
		if isinstance(lst,str):
			lst=Utils.split_path(lst)
		current=self
		for name in lst:
			if not name:
				continue
			elif name=='.':
				continue
			elif name=='..':
				current=current.parent or current
			else:
				prev=current
				current=prev.childs.get(name,None)
				if current is None:
					current=self.__class__(name,prev,DIR)
		return current
	def exclusive_build_node(self,path):
		lst=Utils.split_path(path)
		name=lst[-1]
		if len(lst)>1:
			parent=None
			try:
				parent=self.find_dir(lst[:-1])
			except OSError:
				pass
			if not parent:
				parent=self.ensure_dir_node_from_path(lst[:-1])
				self.__class__.bld.cache_scanned_folders[parent.id]=1
			else:
				try:
					self.__class__.bld.rescan(parent)
				except OSError:
					pass
		else:
			parent=self
		node=parent.childs.get(name,None)
		if not node:
			node=self.__class__(name,parent,BUILD)
		return node
	def path_to_parent(self,parent):
		lst=[]
		p=self
		h1=parent.height()
		h2=p.height()
		while h2>h1:
			h2-=1
			lst.append(p.name)
			p=p.parent
		if lst:
			lst.reverse()
			ret=os.path.join(*lst)
		else:
			ret=''
		return ret
	def find_ancestor(self,node):
		dist=self.height()-node.height()
		if dist<0:return node.find_ancestor(self)
		cand=self
		while dist>0:
			cand=cand.parent
			dist-=1
		if cand==node:return cand
		cursor=node
		while cand.parent:
			cand=cand.parent
			cursor=cursor.parent
			if cand==cursor:return cand
	def relpath_gen(self,going_to):
		if self==going_to:return'.'
		if going_to.parent==self:return'..'
		ancestor=self.find_ancestor(going_to)
		lst=[]
		cand=self
		while not cand.id==ancestor.id:
			lst.append(cand.name)
			cand=cand.parent
		cand=going_to
		while not cand.id==ancestor.id:
			lst.append('..')
			cand=cand.parent
		lst.reverse()
		return os.sep.join(lst)
	def nice_path(self,env=None):
		tree=self.__class__.bld
		ln=tree.launch_node()
		if self.id&3==FILE:return self.relpath_gen(ln)
		else:return os.path.join(tree.bldnode.relpath_gen(ln),env.variant(),self.relpath_gen(tree.srcnode))
	def is_child_of(self,node):
		p=self
		diff=self.height()-node.height()
		while diff>0:
			diff-=1
			p=p.parent
		return p.id==node.id
	def variant(self,env):
		if not env:return 0
		elif self.id&3==FILE:return 0
		else:return env.variant()
	def height(self):
		d=self
		val=-1
		while d:
			d=d.parent
			val+=1
		return val
	def abspath(self,env=None):
		variant=(env and(self.id&3!=FILE)and env.variant())or 0
		ret=self.__class__.bld.cache_node_abspath[variant].get(self.id,None)
		if ret:return ret
		if not variant:
			if not self.parent:
				val=os.sep=='/'and os.sep or''
			elif not self.parent.name:
				val=(os.sep=='/'and os.sep or'')+self.name
			else:
				val=self.parent.abspath()+os.sep+self.name
		else:
			val=os.sep.join((self.__class__.bld.bldnode.abspath(),env.variant(),self.path_to_parent(self.__class__.bld.srcnode)))
		self.__class__.bld.cache_node_abspath[variant][self.id]=val
		return val
	def change_ext(self,ext):
		name=self.name
		k=name.rfind('.')
		if k>=0:
			name=name[:k]+ext
		else:
			name=name+ext
		return self.parent.find_or_declare([name])
	def src_dir(self,env):
		return self.parent.srcpath(env)
	def bld_dir(self,env):
		return self.parent.bldpath(env)
	def bld_base(self,env):
		s=os.path.splitext(self.name)[0]
		return os.path.join(self.bld_dir(env),s)
	def bldpath(self,env=None):
		if self.id&3==FILE:
			return self.relpath_gen(self.__class__.bld.bldnode)
		if self.path_to_parent(self.__class__.bld.srcnode)is not'':
			return os.path.join(env.variant(),self.path_to_parent(self.__class__.bld.srcnode))
		return env.variant()
	def srcpath(self,env=None):
		if self.id&3==BUILD:
			return self.bldpath(env)
		return self.relpath_gen(self.__class__.bld.bldnode)
	def read(self,env):
		return Utils.readf(self.abspath(env))
	def dir(self,env):
		return self.parent.abspath(env)
	def file(self):
		return self.name
	def file_base(self):
		return os.path.splitext(self.name)[0]
	def suffix(self):
		k=max(0,self.name.rfind('.'))
		return self.name[k:]
	def find_iter_impl(self,src=True,bld=True,dir=True,accept_name=None,is_prune=None,maxdepth=25):
		self.__class__.bld.rescan(self)
		for name in self.__class__.bld.cache_dir_contents[self.id]:
			if accept_name(self,name):
				node=self.find_resource(name)
				if node:
					if src and node.id&3==FILE:
						yield node
				else:
					node=self.find_dir(name)
					if node and node.id!=self.__class__.bld.bldnode.id:
						if dir:
							yield node
						if not is_prune(self,name):
							if maxdepth:
								for k in node.find_iter_impl(src,bld,dir,accept_name,is_prune,maxdepth=maxdepth-1):
									yield k
			else:
				if not is_prune(self,name):
					node=self.find_resource(name)
					if not node:
						node=self.find_dir(name)
						if node and node.id!=self.__class__.bld.bldnode.id:
							if dir:
								yield node
							if maxdepth:
								for k in node.find_iter_impl(src,bld,dir,accept_name,is_prune,maxdepth=maxdepth-1):
									yield k
		if bld:
			for node in self.childs.values():
				if node.id==self.__class__.bld.bldnode.id:
					continue
				if node.id&3==BUILD:
					if accept_name(self,node.name):
						yield node
		raise StopIteration
	def find_iter(self,in_pat=['*'],ex_pat=[],prune_pat=['.svn'],src=True,bld=True,dir=False,maxdepth=25,flat=False):
		if not(src or bld or dir):
			raise StopIteration
		if self.id&3!=DIR:
			raise StopIteration
		in_pat=Utils.to_list(in_pat)
		ex_pat=Utils.to_list(ex_pat)
		prune_pat=Utils.to_list(prune_pat)
		def accept_name(node,name):
			for pat in ex_pat:
				if fnmatch.fnmatchcase(name,pat):
					return False
			for pat in in_pat:
				if fnmatch.fnmatchcase(name,pat):
					return True
			return False
		def is_prune(node,name):
			for pat in prune_pat:
				if fnmatch.fnmatchcase(name,pat):
					return True
			return False
		ret=self.find_iter_impl(src,bld,dir,accept_name,is_prune,maxdepth=maxdepth)
		if flat:
			return" ".join([x.relpath_gen(self)for x in ret])
		return ret
	def ant_glob(self,*k,**kw):
		regex=Utils.jar_regexp(k[0])
		def accept(node,name):
			ts=node.relpath_gen(self)+'/'+name
			return regex.match(ts)
		def reject(node,name):
			ts=node.relpath_gen(self)+'/'+name
			return default_excludes()(ts)
		ret=[x for x in self.find_iter_impl(accept_name=accept,is_prune=reject,src=kw.get('src',1),bld=kw.get('bld',1),dir=kw.get('dir',0),maxdepth=kw.get('maxdepth',25))]
		if kw.get('flat',True):
			return" ".join([x.relpath_gen(self)for x in ret])
		return ret
class Nodu(Node):
	pass


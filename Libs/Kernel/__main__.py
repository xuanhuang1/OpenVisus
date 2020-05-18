import os, sys, glob, subprocess, platform, shutil, sysconfig, re

# *** NOTE: this file must be self-contained ***

"""
Linux:
	QT_DEBUG_PLUGINS=1 LD_DEBUG=libs,files  ./visusviewer.sh
	LD_DEBUG=libs,files ldd bin/visus
	readelf -d bin/visus

OSX:
	otool -L libname.dylib
	otool -l libVisusGui.dylib  | grep -i "rpath"
	DYLD_PRINT_LIBRARIES=1 QT_DEBUG_PLUGINS=1 visusviewer.app/Contents/MacOS/visusviewer
"""

WIN32=platform.system()=="Windows" or platform.system()=="win32"
APPLE=platform.system()=="Darwin"

# /////////////////////////////////////////////////////////////////////////
def ReadTextFile(filename):
	file = open(filename, "r") 
	ret=file.read().strip()
	file.close()
	return ret
	
# /////////////////////////////////////////////////////////////////////////
def GetFilenameWithoutExtension(filename):
	return os.path.splitext(os.path.basename(filename))[0]

# /////////////////////////////////////////////////////////////////////////
def GetCommandOutput(cmd):
	output=subprocess.check_output(cmd)
	if sys.version_info >= (3, 0): output=output.decode("utf-8")
	return output.strip()

# /////////////////////////////////////////////////////////////////////////
def ExecuteCommand(cmd):	
	print("Executing command", cmd)
	return subprocess.call(cmd, shell=False)

# ////////////////////////////////////////////////
def UsePyQt5():
	
	"""
	python -m pip install johnnydep

	johnnydep PyQt5~=5.14.0	-> PyQt5-sip<13,>=12.7
	johnnydep PyQt5~=5.13.0	-> PyQt5_sip<13,>=4.19.19
	johnnydep PyQt5~=5.12.0 -> PyQt5_sip<13,>=4.19.14
	johnnydep PyQt5~=5.11.0 -> empty
	johnnydep PyQt5~=5.10.0 -> empty
	johnnydep PyQt5~=5.9.0  -> empty

	johnnydep PyQtWebEngine~=5.14.0 -> PyQt5>=5.14 
	johnnydep PyQtWebEngine~=5.13.0 -> PyQt5>=5.13
	johnnydep PyQtWebEngine~=5.12.0 -> PyQt5>=5.12
	johnnydep PyQtWebEngine~=5.11.0 -> does not exist
	johnnydep PyQtWebEngine~=5.10.0 -> does not exist
	johnnydep PyQtWebEngine~=5.19.0 -> does not exist

	johnnydep PyQt5-sip~=12.7.0 -> empty
	"""

	QT_VERSION=ReadTextFile("QT_VERSION")
	print("Installing PyQt5...",QT_VERSION)
	major,minor=QT_VERSION.split('.')[0:2]

	try:
		import conda.cli
		bIsConda=True
	except:
		bIsConda=False
		
	if bIsConda:
		
		conda.cli.main('conda', 'install',    '-y', "pyqt={}.{}".format(major,minor))
		# do I need PyQtWebEngine for conda? considers Qt is 5.9 (very old)
		# it has webengine and sip included

	else: 

		cmd=[sys.executable,"-m", "pip", "install", "--progress-bar", "off", "PyQt5~={}.{}.0".format(major,minor)]

		if int(major)==5 and int(minor)>=12:
			cmd+=["PyQtWebEngine~={}.{}.0".format(major,minor)]
			cmd+=["PyQt5-sip<13,>=12.7"] 

		ExecuteCommand(cmd)

	try:
		import PyQt5
		PyQt5_HOME=os.path.dirname(PyQt5.__file__)

	except:
		# this should cover the case where I just installed PyQt5
		PyQt5_HOME=GetCommandOutput([sys.executable,"-c","import os,PyQt5;print(os.path.dirname(PyQt5.__file__))"]).strip()

	print("Linking PyQt5_HOME",PyQt5_HOME)
	
	if not os.path.isdir(PyQt5_HOME):
		print("Error directory does not exists")
		raise Exception("internal error")

	# on windows it's enough to use sys.path (see *.i %pythonbegin section)
	if not WIN32:
		
		if APPLE:		

			def SetRPath(filename,value):
	
				def GetRPath(filename):
					command = "otool -l '%s' | grep -A2 LC_RPATH | grep path " % filename
					result  = subprocess.check_output(command, shell = True, universal_newlines=True)
					path_re = re.compile("^\s*path (.*) \(offset \d*\)$")
					return [path_re.search(line).group(1).strip() for line in result.splitlines()]
					
				def RemoveRPath(filename,value):
					ExecuteCommand(["install_name_tool","-delete_rpath",value, filename])
					
				def AddRPath(filename,value):
					ExecuteCommand(["install_name_tool","-add_rpath", value, filename])
						
				for it in GetRPath(filename):
					RemoveRPath(filename,it)
			
				for it in value.split(":"):
					AddRPath(filename,it)
					
				print(filename,GetRPath(filename))
				
			apps=["%s/Contents/MacOS/%s"   % (it,GetFilenameWithoutExtension(it)) for it in glob.glob("bin/*.app")]

			value="@loader_path/:@loader_path/bin:@loader_path/../../..:" + os.path.join(PyQt5_HOME,'Qt/lib')

			for filename in glob.glob("*.so") + glob.glob("bin/*.dylib") + apps:
				SetRPath(filename,value)
				
		else:
			
			def SetRPath(filename,value):
				ExecuteCommand(["patchelf","--set-rpath",value, filename])			
			
			
			value="$ORIGIN:$ORIGIN/bin:" + os.path.join(PyQt5_HOME,'Qt/lib')
			
			for filename in glob.glob("*.so") + glob.glob("bin/*.so") + ["bin/visus","bin/visusviewer"]:
				SetRPath(filename,value)

# ////////////////////////////////////////////////
def Main():

	action=sys.argv[1] if len(sys.argv)>=2 else ""

	# no arguments
	if action=="":
		sys.exit(0)

	this_dir=os.path.dirname(os.path.abspath(__file__))

	# _____________________________________________
	if action=="dirname":
		print(this_dir)
		sys.exit(0)

	print([sys.executable,"-m","OpenVisus"] + sys.argv)
	print("this_dir",this_dir)

	# _____________________________________________
	if action=="configure":
		os.chdir(this_dir)
		UsePyQt5()
		print(action,"done")
		sys.exit(0)

	# _____________________________________________
	if action=="test":
		for filename in ["Array.py","Dataflow.py","Dataflow2.py","Idx.py"]: # ,"XIdx.py" ,"DataConversion1.py","DataConversion2.py"
			ExecuteCommand([sys.executable,os.path.join(this_dir, "Samples", "python", filename)]) 
		sys.exit(0)

	# _____________________________________________
	if action=="server":

		from OpenVisus.VisusKernelPy import NetServer
		from OpenVisus.VisusDbPy import ModVisus
		os.chdir(this_dir)
		modvisus = ModVisus()
		modvisus.configureDatasets()
		server=NetServer(10000, modvisus)
		server.runInThisThread()
		sys.exit(0)

	# _____________________________________________
	if action=="test-idx":
		from OpenVisus.VisusDbPy import SelfTestIdx
		os.chdir(this_dir)
		SelfTestIdx(300)
		sys.exit(0)

	# _____________________________________________
	if action=="convert":
		# example: python -m OpenVisus convert ...
		from OpenVisus.VisusKernelPy import SetCommandLine
		from OpenVisus.VisusDbPy     import DbModule,VisusConvert
		os.chdir(this_dir)
		SetCommandLine("__main__")
		DbModule.attach()
		convert=VisusConvert()
		convert.run(sys.argv[2:])
		DbModule.detach()
		sys.exit(0)

	# _____________________________________________
	if action=="viewer":
		# example: python -m OpenVisus viewer ....
		from OpenVisus.VisusKernelPy import SetCommandLine
		from OpenVisus.VisusGuiPy    import GuiModule, Viewer
		os.chdir(this_dir)
		SetCommandLine("__main__")
		GuiModule.createApplication()
		GuiModule.attach()
		viewer=Viewer()
		viewer.configureFromArgs(sys.argv[2:])
		GuiModule.execApplication()
		viewer=None
		GuiModule.detach()
		GuiModule.destroyApplication()
		print("All done")
		sys.exit(0)

	# _____________________________________________
	if action=="viewer1":
		ExecuteCommand([sys.executable,os.path.join(this_dir, "Samples", "python", "TestViewer1.py")]) 
		sys.exit(0)

	# _____________________________________________
	if action=="viewer2":
		ExecuteCommand([sys.executable,os.path.join(this_dir, "Samples", "python", "TestViewer2.py")]) 
		sys.exit(0)

	# _____________________________________________
	if action=="visible-human":
		ExecuteCommand([sys.executable,os.path.join(this_dir, "Samples", "python", "VisibleHuman.py")]) 
		sys.exit(0)

	# _____________________________________________
	if action=="slam" or action=="slam3d":

		# example: python -m OpenVisus slam D:\GoogleSci\visus_slam\TaylorGrant
		# example: python -m OpenVisus slam D:\GoogleSci\visus_slam\Alfalfa
		# example: python -m OpenVisus slam D:\GoogleSci\visus_slam\RedEdge (THIS IS MICASENSE)

		# example: python -m OpenVisus slam3d  D:\GoogleSci\visus_dataset\male\RAW\Fullcolor\fullbody

		from OpenVisus.Slam.Main import Main as SlamMain
		SlamMain()
		sys.exit(0)	

	print("Wrong arguments",sys.argv)
	sys.exit(-1)
  

# //////////////////////////////////////////
if __name__ == "__main__":
	Main()





import sys
from subprocess import call
import glob

scene = sys.argv[1]
targetsDir = sys.argv[2]

editModes = [8.1, 6, 8]

targetImages = glob.glob(targetsDir + "/*.png")

exe = "../Builds/VisualStudio2015NoArnold/x64/Release/AttributesInterface.exe"

for imgPath in targetImages:
	logDir = "../analysis/precompute/"

	for editMode in editModes:
		if editMode == 8.1:
			cmd = exe + " --preload " + scene + " --auto 8 --editUpdateMode 1 --img-attr " + imgPath + " --more --samples 1000 --out " + logDir + " --jnd 0.1 --timeout 10 --session-name 8.1"
		else:
			cmd = exe + " --preload " + scene + " --auto " + str(editMode) + " --img-attr " + imgPath + " --more --samples 1000 --out " + logDir + " --jnd 0.1 --timeout 10"
		print cmd
		call(cmd)

	call("python processData.py " + logDir)

# call("python crossCompareT.py")

#for i in range(0, 194):
#	call(exe + " --preload " + scene + " --auto 0 --img-attr auto --more --samples 400 --out C:/Users/eshimizu/Documents/AttributesInterface/logs/ --jnd 1.5 --timeout 10")
	#call(exe + " --preload " + scene + " --auto 1 --img-attr " + imgPath + " --more --samples 200 --out C:/Users/eshimizu/Documents/AttributesInterface/logs/ --jnd 1")
#	call(exe + " --preload " + scene + " --auto 4 --img-attr auto --more --samples 400 --out C:/Users/eshimizu/Documents/AttributesInterface/logs/ --jnd 1.5 --timeout 10")
#	call(exe + " --preload " + scene + " --auto 5 --img-attr auto --more --samples 400 --out C:/Users/eshimizu/Documents/AttributesInterface/logs/ --jnd 1.5 --timeout 10")
#	call(exe + " --preload " + scene + " --auto 6 --img-attr auto --more --samples 400 --out C:/Users/eshimizu/Documents/AttributesInterface/logs/ --jnd 2 --timeout 10")
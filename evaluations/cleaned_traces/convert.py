#! /usr/bin/python
import sys
if(len(sys.argv) != 2 ) :
	print "Usage : convert.py <file-name>"
	exit
file_name=sys.argv[1];
fh_read=open(file_name,"r");
fh_writ=open(file_name.split(".")[0]+".tr","w");
start_time = -1
for line in fh_read.readlines() :
	ts = (int)(round(int(line.split("=")[1].split(",")[0])/1.0e6))
	if(start_time<0):
		start_time = ts
	fh_writ.write(str(ts-start_time)+"\n")

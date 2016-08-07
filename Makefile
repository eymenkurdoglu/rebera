# For basics on 'Make':
# https://www.cs.bu.edu/teaching/cpp/writing-makefiles/
# https://www.cs.duke.edu/courses/cps108/doc/makefileinfo/sample.html
# http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

common_obj=socket.o util.o select.o
sender_obj=dfs.o encoder.o senderstate.o v4l2.o
receiver_obj=videostate.o receiverstate.o
executables=rbr-send rbr-recv rbr-yuvsend
x264_dir=$(HOME)/Source/x264
dest = bin/

CXX = g++
CXXFLAGS = -Wall -g -std=c++11
LDFLAGS = -L $(x264_dir)
LDLIBS = -lavformat -lavcodec -lavutil -lSDL2 -lx264 -lpthread -ldl


all: $(common_obj) $(sender_obj) $(receiver_obj) $(executables)

select.o : select.cc select.h
	$(CXX) $(CXXFLAGS) -c select.cc $(LDFLAGS)

dfs.o : dfs.cc dfs.hh
	$(CXX) $(CXXFLAGS) -c dfs.cc $(LDFLAGS)

socket.o : socket.cc socket.hh
	$(CXX) $(CXXFLAGS) -c socket.cc $(LDFLAGS)

videostate.o : videostate.cc videostate.hh
	$(CXX) $(CXXFLAGS) -c videostate.cc $(LDFLAGS)

receiverstate.o : receiverstate.cc receiverstate.hh
	$(CXX) $(CXXFLAGS) -c receiverstate.cc $(LDFLAGS)

senderstate.o : senderstate.cc socket.hh payload.hh dfs.hh util.hh
	$(CXX) $(CXXFLAGS) -c senderstate.cc $(LDFLAGS)

util.o : util.cc util.hh
	$(CXX) $(CXXFLAGS) -c util.cc $(LDFLAGS)

encoder.o : encoder.cc encoder.hh
	$(CXX) $(CXXFLAGS) -c encoder.cc $(LDFLAGS)

v4l2.o : v4l2.cc v4l2.hh encoder.cc encoder.hh
	$(CXX) $(CXXFLAGS) -c v4l2.cc $(LDFLAGS)

rbr-send : reberasender.cc senderstate.hh encoder.hh v4l2.hh dfs.hh socket.hh payload.hh util.hh select.h
	$(CXX) $(CXXFLAGS) -o rbr-send reberasender.cc $(common_obj) $(sender_obj) $(LDFLAGS) $(LDLIBS)

rbr-recv : reberareceiver.cc receiverstate.hh videostate.hh socket.hh payload.hh util.hh select.h
	$(CXX) $(CXXFLAGS) -o rbr-recv reberareceiver.cc $(common_obj) $(receiver_obj) $(LDFLAGS) $(LDLIBS)

#rbr-yuvsend : yuvsender.cc senderstate.hh encoder.hh v4l2.hh dfs.hh socket.hh payload.hh util.hh select.h
#	$(CXX) $(CXXFLAGS) -o rbr-yuvsend yuvsender.cc $(common_obj) $(sender_obj) $(LDFLAGS) $(LDLIBS)

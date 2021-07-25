CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g -Wall
else
    CXXFLAGS += -O2 -Wall
endif

src = $(wildcard ./*.cpp ./log/*.cpp ./mysql/*.cpp ./timer/*.cpp ./http_module/*.cpp)

#obj = $(patsubst %.cpp %.o,&(src))

AUX =-lpthread -std=c++11 -lmysqlclient 

sw_server:$(src)
	$(CXX) $^ -o $@ $(CXXFLAGS) $(AUX)

.PHONY: clean
clean:
	-rm -rf $@ 

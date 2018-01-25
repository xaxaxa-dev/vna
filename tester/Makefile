CXX = g++ -O2
LDFLAGS = 

vna_tester: main.C
	$(CXX) main.C ../libxavna/xavna.C -I ../include -o vna_tester --std=c++0x -lwiringPi -lpthread `pkg-config --cflags --libs cairomm-1.0`


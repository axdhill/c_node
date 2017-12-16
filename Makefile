c_node: c_node.cpp serialib.cpp adc_lib.cpp
	g++ c_node.cpp adc_lib.cpp serialib.cpp -o c_node -lbcm2835 -pthread --std=c++11 -O3 -I.
speedtest: speedtest.cpp
	g++ speedtest.cpp -o speedtest -pthread --std=c++11 -O3 -I.

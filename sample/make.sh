reset
g++ -pthread -std=c++11 -g -Wall ../src/errorCode.cpp ../src/log.cpp ../src/timer/timer.cpp ../src/worker.cpp ../src/protocol.cpp main.cpp  -I ../src/timer -I ../src 


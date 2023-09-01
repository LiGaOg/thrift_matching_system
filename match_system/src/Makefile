all:
	@g++ -c main.cpp ./match_server/*.cpp
	@g++ *.o -o main -lthrift -lpthread
	@rm *.o
clean:
	@rm main

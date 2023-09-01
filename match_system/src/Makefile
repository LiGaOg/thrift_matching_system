all:
	@g++ -c main.cpp ./match_server/*.cpp
	@g++ *.o -o main -lthrift
	@rm *.o
clean:
	@rm main

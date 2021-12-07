.PHONY: all
all: main

main: main.cpp
	g++ main.cpp -o main -lpthread

.PHONY: clean
	rm main

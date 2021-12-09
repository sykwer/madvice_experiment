.PHONY: all
all: shmem_madvise

shmem_madvise: shmem_madvise.cpp
	g++ shmem_madvise.cpp -o shmem_madvise -lpthread

.PHONY: clean
	rm shmem_madvise

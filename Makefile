.PHONY: all
all: shmem_madvise shmem_madvise2 anti_lru

shmem_madvise: shmem_madvise.cpp
	g++ shmem_madvise.cpp -o shmem_madvise -lpthread

shmem_madvise2: shmem_madvise2.cpp
	g++ shmem_madvise2.cpp -o shmem_madvise2 -lpthread

anti_lru: anti_lru.cpp
	g++ anti_lru.cpp -o anti_lru -lpthread

.PHONY: clean
	rm shmem_madvise shmem_madvise2 anti_lru

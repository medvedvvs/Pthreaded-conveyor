# Pthreaded-conveyor
Pthreaded conveyor for heavy calculations of sequential data blocks. If one tired of openmp.

Conveyor can have loopback for buffer index control for zerocopy (if there is a need to eliminate memcpy from step to step; in current example loopback works connecting 2 sequential conveyors).

It is a toy only. But works. Can load processor cores hard with calculations.

To compile:
g++ threaded_conveyor.cpp -O3 --fast-math -pthread -o threaded_conveyor

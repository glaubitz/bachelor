SET(CMAKE_C_COMPILER mpicc)
SET(CMAKE_CXX_COMPILER mpicxx)
add_executable(hs-mpi main.cpp mpi_master.cpp mpi_slave.cpp mpi_job.cpp mpi_iface.cpp)
OptimizeForArchitecture(hs-mpi)
target_link_libraries(hs-mpi ${Boost_LIBRARIES} mpi hslib)

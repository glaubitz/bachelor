MACRO(add_dbg_exe name files)
  add_executable(${name} ${files})
  target_link_libraries(${name} hslib ${Boost_LIBRARIES})
  OptimizeForArchitecture(${name})
ENDMACRO(add_dbg_exe)

add_dbg_exe(memcheck memcheck.cpp)
add_dbg_exe(resfield resfield.cpp)
add_dbg_exe(hamiltonian hamiltonian.cpp)
add_dbg_exe(orca orca.cpp)
add_dbg_exe(orca2easy orca2easy.cpp)
add_dbg_exe(euler euler.cpp)
add_dbg_exe(orca2orca orca2orca.cpp)
add_dbg_exe(grid grid.cpp)

add_executable(bench_eigen bench_eigen.cpp)
target_link_libraries(bench_eigen)
OptimizeForArchitecture(bench_eigen)

find_package(MKL)
if(MKL_FOUND)
  include_directories(${MKL_INCLUDE_DIR})
  add_executable(bench_mkl bench_mkl.cpp)
  target_link_libraries(bench_mkl ${MKL_LIBRARIES})
  OptimizeForArchitecture(bench_mkl)
endif(MKL_FOUND)
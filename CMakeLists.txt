cmake_minimum_required(VERSION 3.15...4.1)
project(CAVModelCheckers VERSION 1.0
                         DESCRIPTION "A bounded model checker and an interpolation-based model checker using MiniSAT"
                         LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# -------- MiniSat --------

file(GLOB MINISAT_SOURCES
        minisat/*.C
)

add_library(minisat STATIC ${MINISAT_SOURCES})

target_include_directories(minisat PUBLIC
        minisat
)

target_compile_options(minisat PRIVATE
        -Wall
        -ffloat-store
)

target_link_libraries(minisat PUBLIC
        z
)

# -------- Bounded Model Checker --------

add_executable(bmc
        bmc_main.cpp
        BoundedModelChecker.cpp
        BoundedModelChecker.h
        Interpolator.cpp
        Interpolator.h
)

target_link_libraries(bmc PRIVATE minisat)

# -------- Interpolated Model Checker --------

add_executable(imc
        imc_main.cpp
        BoundedModelChecker.cpp
        BoundedModelChecker.h
        Interpolator.cpp
        Interpolator.h
        InterpolationBasedModelChecker.cpp
        InterpolationBasedModelChecker.h
)

target_link_libraries(imc PRIVATE minisat)

add_executable(imc_batch
        imc_batch_main.cpp
        BoundedModelChecker.cpp
        BoundedModelChecker.h
        Interpolator.cpp
        Interpolator.h
        InterpolationBasedModelChecker.cpp
        InterpolationBasedModelChecker.h
)

target_link_libraries(imc_batch PRIVATE minisat)
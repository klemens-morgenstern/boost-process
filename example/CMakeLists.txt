cmake_minimum_required(VERSION 2.8)

project(boost-process-examples)

set(CMAKE_CXX_STANDARD 14)

# find boost
set(BOOST_ROOT            "/media/data/code/cpp/lib/boost/1.64.0")  # custom boost install directory
set(Boost_USE_STATIC_LIBS ON)
find_package(
    Boost
    REQUIRED
    COMPONENTS filesystem system)

# extra libs
if (NOT WIN32)
    set(extra_libs pthread)
endif()

# create and configure a target for a given example
function(make_example target_name)
    add_executable(${target_name} "${target_name}.cpp")
    target_include_directories(${target_name} PRIVATE "../include" "${Boost_INCLUDE_DIRS}")
    target_link_libraries(${target_name} ${Boost_LIBRARIES} ${extra_libs})
endfunction()

## cross platform examples

set(examples
    args
    async_io
    env
    error_handling
    intro
    io
    start_dir
    sync_io
    terminate
    wait
)

foreach(example ${examples})
    make_example(${example})
endforeach()

## posix examples

if (NOT WIN32)
    set(posix_examples
        posix)

    foreach(example ${posix_examples})
        make_example(${example})
    endforeach()
endif()

## windows examples

if (WIN32)
    set(win_examples
        windows)

    foreach(example ${win_examples})
        make_example(${example})
    endforeach()
endif()

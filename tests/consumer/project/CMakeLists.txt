cmake_minimum_required(VERSION 3.25)

project(sherpa_consumer LANGUAGES CXX)

find_package(Sherpa CONFIG REQUIRED)

add_executable(sherpa_consumer ../api_consumer.cpp)
target_compile_features(sherpa_consumer PRIVATE cxx_std_20)
target_link_libraries(sherpa_consumer PRIVATE Sherpa::Sherpa)

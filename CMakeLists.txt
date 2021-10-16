cmake_minimum_required(VERSION 3.20)
project(azure-freebsd-minimal
        VERSION 0.0
        DESCRIPTION "Minimal tooling for FreeBSD VMs on Azure"
        LANGUAGES CXX)

option(USE_SYSTEM_LIBUCL "Use the base system's version of libUCL" TRUE)
option(BUILD_TESTS "Use the base system's version of libUCL" FALSE)

add_executable(azure-imds-query azure-imds-query.cc)
target_compile_features(azure-imds-query PRIVATE cxx_std_20)

if (USE_SYSTEM_LIBUCL)
	target_include_directories(azure-imds-query PRIVATE /usr/include/private/ucl)
	target_link_libraries(azure-imds-query PRIVATE privateucl)
else ()
	find_package(ucl CONFIG REQUIRED)
	target_link_libraries(azure-imds-query PRIVATE ucl::ucl)
endif ()

install(TARGETS azure-imds-query)
install(FILES azure-imds-query.8 TYPE MAN)



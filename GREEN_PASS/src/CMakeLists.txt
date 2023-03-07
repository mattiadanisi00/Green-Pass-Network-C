cmake_minimum_required(VERSION 3.23)
project(green_pass C)

set(CMAKE_C_STANDARD 99)

add_executable(GREEN_PASS server/ServerV.c server/ServerV.h "centro-vaccinale/CentroVaccinale.c" "centro-vaccinale/CentroVaccinale.h" server/ServerG.c server/ServerG.h wrapper/Wrapper.c wrapper/Wrapper.h clients/ClientS.c clients/ClientA.c clients/ClientT.c)

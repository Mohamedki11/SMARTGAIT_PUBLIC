cmake_minimum_required(VERSION 3.12)

project(SmartGait)
# standard Cpp Rules
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_subdirectory(pybind11)

find_package(OpenCV REQUIRED)


#pybind11_add_module(test_pybind Cpp_test_pybind/test_somme.cpp)
pybind11_add_module(CAM_engine
                    Modules/CAMERA/CamEngine.cpp
                    Modules/CAMERA/CamHardware.cpp
                    Modules/CAMERA/CamStorage.cpp
                    Modules/CAMERA/CamAI.cpp
                    )

#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python_test_pybind11)
set_target_properties(CAM_engine PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Modules/CAMERA)


#pybind11_add_module(test_pybind Cpp_test_pybind/test_somme.cpp)
pybind11_add_module(RADAR_engine
                    Modules/RADAR/RadarEngine.cpp
                    Modules/RADAR/RadarHardware.cpp
                    Modules/RADAR/RadarStorage.cpp
                    Modules/RADAR/RadarRealTime.cpp
                    )


#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python_test_pybind11)
set_target_properties(RADAR_engine PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Modules/RADAR)



target_link_libraries(CAM_engine PRIVATE ${OpenCV_LIBS})

target_link_libraries(RADAR_engine PRIVATE ${OpenCV_LIBS})
target_link_libraries(RADAR_engine PUBLIC usb-1.0 fftw3)
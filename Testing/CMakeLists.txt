ADD_DEFINITIONS(-DMPICH_SKIP_MPICXX)
#--------------------------------------------------
# DSM Test apps
#--------------------------------------------------

IF(VTK_USE_MPI)
  ADD_EXECUTABLE(TestH5MB ${CMAKE_CURRENT_SOURCE_DIR}/TestH5MB.cxx)
  TARGET_LINK_LIBRARIES(TestH5MB CSCS_DSM)

  ADD_EXECUTABLE(namepub_client ${CMAKE_CURRENT_SOURCE_DIR}/namepub_client.cpp)
  TARGET_LINK_LIBRARIES(namepub_client CSCS_DSM)

  ADD_EXECUTABLE(namepub_server ${CMAKE_CURRENT_SOURCE_DIR}/namepub_server.cpp)
  TARGET_LINK_LIBRARIES(namepub_server CSCS_DSM)

  ADD_EXECUTABLE(socket_client ${CMAKE_CURRENT_SOURCE_DIR}/socket_client.cxx)
  TARGET_LINK_LIBRARIES(socket_client CSCS_DSM)

  ADD_EXECUTABLE(socket_server ${CMAKE_CURRENT_SOURCE_DIR}/socket_server.cxx)
  TARGET_LINK_LIBRARIES(socket_server CSCS_DSM)
  
  ADD_EXECUTABLE(socket_mpi_client ${CMAKE_CURRENT_SOURCE_DIR}/socket_mpi_client.cxx)
  TARGET_LINK_LIBRARIES(socket_mpi_client CSCS_DSM)

  ADD_EXECUTABLE(socket_mpi_server ${CMAKE_CURRENT_SOURCE_DIR}/socket_mpi_server.cxx)
  TARGET_LINK_LIBRARIES(socket_mpi_server CSCS_DSM)

  ADD_EXECUTABLE(TestDSM ${CMAKE_CURRENT_SOURCE_DIR}/TestDSM.cxx)
  TARGET_LINK_LIBRARIES(TestDSM CSCS_DSM)

#  ADD_EXECUTABLE(H5FDdsmvtkWRTest ${CMAKE_CURRENT_SOURCE_DIR}/H5FDdsmvtkWRTest.cxx)
#  TARGET_LINK_LIBRARIES(H5FDdsmvtkWRTest CSCS_DSM)
  
ENDIF (VTK_USE_MPI)


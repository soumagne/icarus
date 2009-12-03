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

  ADD_EXECUTABLE(TestDSM ${CMAKE_CURRENT_SOURCE_DIR}/TestDSM.cxx)
  TARGET_LINK_LIBRARIES(TestDSM CSCS_DSM)

  ADD_EXECUTABLE(XdmfDsmvtkWRTest ${CMAKE_CURRENT_SOURCE_DIR}/XdmfDsmvtkWRTest.cxx)
  TARGET_LINK_LIBRARIES(XdmfDsmvtkWRTest CSCS_DSM)
  
  ADD_EXECUTABLE(TestGenerator ${CMAKE_CURRENT_SOURCE_DIR}/TestGenerator.cxx)
  TARGET_LINK_LIBRARIES(TestGenerator CSCS_DSM)
ENDIF (VTK_USE_MPI)


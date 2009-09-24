ADD_DEFINITIONS(-DMPICH_SKIP_MPICXX)

#--------------------------------------------------
# DSM Test apps
#--------------------------------------------------

IF(VTK_USE_MPI)
#  ADD_EXECUTABLE(TestH5MB ${CMAKE_CURRENT_SOURCE_DIR}/TestH5MB.cxx)
#  TARGET_LINK_LIBRARIES(TestH5MB vtkXdmf CSCS_DSM)

  ADD_EXECUTABLE(namepub_client ${CMAKE_CURRENT_SOURCE_DIR}/namepub_client.cpp)
  TARGET_LINK_LIBRARIES(namepub_client vtkXdmf CSCS_DSM)

  ADD_EXECUTABLE(namepub_server ${CMAKE_CURRENT_SOURCE_DIR}/namepub_server.cpp)
  TARGET_LINK_LIBRARIES(namepub_server vtkXdmf CSCS_DSM)

  ADD_EXECUTABLE(TestDSM ${CMAKE_CURRENT_SOURCE_DIR}/TestDSM.cxx)
  TARGET_LINK_LIBRARIES(TestDSM vtkXdmf)

  ADD_EXECUTABLE(XdmfDsmvtkWRTest ${CMAKE_CURRENT_SOURCE_DIR}/XdmfDsmvtkWRTest.cxx)
  TARGET_LINK_LIBRARIES(XdmfDsmvtkWRTest CSCS_DSM ${XDMF_MPI_LIBRARIES})
ENDIF (VTK_USE_MPI)


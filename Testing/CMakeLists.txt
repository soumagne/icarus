ADD_DEFINITIONS(-DMPICH_SKIP_MPICXX)

#--------------------------------------------------
# DSM Test apps
#--------------------------------------------------
IF(VTK_USE_MPI)
  ADD_EXECUTABLE(TestDSM ${CMAKE_CURRENT_SOURCE_DIR}/TestDSM.cxx)
  TARGET_LINK_LIBRARIES(TestDSM vtkXdmf)

  ADD_EXECUTABLE(XdmfDsmvtkWRTest ${CMAKE_CURRENT_SOURCE_DIR}/XdmfDsmvtkWRTest.cxx)
  TARGET_LINK_LIBRARIES(XdmfDsmvtkWRTest CSCS_DSM ${XDMF_MPI_LIBRARIES})
ENDIF (VTK_USE_MPI)


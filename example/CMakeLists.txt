#--------------------------------------------------
# DSM Test apps
#--------------------------------------------------

set(DRIVER_SRCS
  IcarusTestDriver.cxx
)

include_directories(
  ${MPI_INCLUDE_PATH} 
  ${HDF5_INCLUDE_DIR} 
  ${H5FDdsm_INCLUDE_DIR} 
)

add_executable(IcarusTestDriver
  ${DRIVER_SRCS}
)

target_link_libraries(IcarusTestDriver
  vtkxdmf2  
  hdf5
  hdf5_hl 
  H5FDdsm
)  

#--------------------------------------------------

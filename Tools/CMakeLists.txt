#--------------------------------------------------
# Converter tools
#--------------------------------------------------
# Setup some convenience flags depending on machine
SET(COMPUTERNAME $ENV{COMPUTERNAME})
IF(COMPUTERNAME)
  MESSAGE("Setting computername #DEFINE to MACHINE_$ENV{COMPUTERNAME}")
  ADD_DEFINITIONS(-DMACHINE_$ENV{COMPUTERNAME})
ENDIF(COMPUTERNAME)

SET(Converter_LIBS CSCS_DSM hdf5_hl)

IF(CSCS_PLUGIN_CFX)
  MESSAGE("Xdmf converter supporting CFX " ${CSCS_CFX_DIR})
  SET(HAS_CFX ON)
  ADD_DEFINITIONS(-DHAS_CFX)
  INCLUDE_DIRECTORIES(${CSCS_CFX_DIR})
  LINK_DIRECTORIES(${CSCS_CFX_LINK_DIRS})
  SET(Converter_LIBS ${Converter_LIBS} CSCS_CFX)
ENDIF(CSCS_PLUGIN_CFX)
  
IF(CSCS_PLUGIN_Fluent)
  MESSAGE("Xdmf converter supporting Fluent " ${CSCS_Fluent_DIR})
  SET(HAS_FLUENT ON)
  ADD_DEFINITIONS(-DHAS_FLUENT)
  INCLUDE_DIRECTORIES(${CSCS_Fluent_DIR})
  SET(Converter_LIBS ${Converter_LIBS} CSCS_Fluent)
ENDIF(CSCS_PLUGIN_Fluent)

ADD_EXECUTABLE(
  ConvertToXdmf
  ${CMAKE_CURRENT_SOURCE_DIR}/vtkSubdivideUnstructuredGrid.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/vtkSplitBlocksFilter.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/vtkRedistributeBlocksFilter.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/ConvertToXdmf.cxx
)

TARGET_LINK_LIBRARIES(ConvertToXdmf ${Converter_LIBS})

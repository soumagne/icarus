PROJECT("DSMMANAGER_PLUGIN")

CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

IF(WIN32 AND MSVC)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32 AND MSVC)

#--------------------------------------------------
# Find and Use ParaView
#--------------------------------------------------
IF(NOT PARAVIEW_LOADED)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
  INCLUDE(${PARAVIEW_VTK_DIR}/VTKConfig.cmake)
  SET(PARAVIEW_LOADED 1)
  INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})
ELSE(NOT PARAVIEW_LOADED)
# This is apparently no longer required
  IF (PARAVIEW_BUILD_QT_GUI)
    INCLUDE(${QT_USE_FILE})
  ENDIF (PARAVIEW_BUILD_QT_GUI)
  INCLUDE_DIRECTORIES(
    ${PARAVIEW_INCLUDE_DIRS}
    ${PARAVIEW_GUI_INCLUDE_DIRS}
    ${PARAVIEW_KWSYS_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIR}
    ${MPI_INCLUDE_PATH}
  )
ENDIF(NOT PARAVIEW_LOADED)

IF (ParaView_BINARY_DIR)
  IF (VTK_USE_SYSTEM_LIBXML2)
    INCLUDE_DIRECTORIES(${LIBXML2_INCLUDE_DIR})
  ELSE (VTK_USE_SYSTEM_LIBXML2)
    INCLUDE_DIRECTORIES(${ParaView_BINARY_DIR}/VTK/Utilities/vtklibxml2)
  ENDIF (VTK_USE_SYSTEM_LIBXML2)
  INCLUDE_DIRECTORIES(
    ${ParaView_CMAKE_DIR}/../Utilities/H5FDdsm/src
    ${ParaView_BINARY_DIR}/Utilities/H5FDdsm/src
    ${ParaView_CMAKE_DIR}/../Utilities/hdf5-1.8/hl/src
  )
ENDIF(ParaView_BINARY_DIR)

#--------------------------------------------------
# Set project include directories 
#--------------------------------------------------
INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/XdmfGenerator
  ${CMAKE_CURRENT_BINARY_DIR}/XdmfGenerator
  )

#--------------------------------------------------
# Setup GUI custom Qt panel sources and wrapping
#--------------------------------------------------
IF(PARAVIEW_BUILD_QT_GUI)
  OPTION(CSCS_DSM_BUILD_TESTING "Build DSM plugin testing" OFF)
  IF(CSCS_DSM_BUILD_TESTING)
    SET(DSM_PANEL_CLASS pqDSMViewerPanel_testing)
  ELSE(CSCS_DSM_BUILD_TESTING)
    SET(DSM_PANEL_CLASS pqDSMViewerPanel)
  ENDIF(CSCS_DSM_BUILD_TESTING)

  QT4_WRAP_CPP(
    DSM_MOC_SRCS 
      ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.h
  )
  # invoke macro that adds the refinement and display controls panel
  ADD_PARAVIEW_DOCK_WINDOW(
    DSM_IFACE 
    DSM_IFACE_SRCS 
    CLASS_NAME 
      ${DSM_PANEL_CLASS}
    DOCK_AREA 
      Left Right Top Bottom
  )
  QT4_WRAP_UI(
    DSM_UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.ui
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)

encode_files_as_strings(ENCODED_FILTERS
  ${CMAKE_CURRENT_SOURCE_DIR}/CustomFilter_TransformBlock.cpd
  ${CMAKE_CURRENT_SOURCE_DIR}/CustomFilter_XdmfReaderBlock.cpd
)

MESSAGE("${ENCODED_FILTERS}")

#--------------------------------------------------
# Define Plugin
#--------------------------------------------------
ADD_PARAVIEW_PLUGIN(
  CSCS_DSM
  "1.0" 

  SERVER_MANAGER_XML 
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDSMManager.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter4.xml
  SERVER_MANAGER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDSMManager.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter4.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.cxx
  SERVER_SOURCES
    ${ENCODED_FILTERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDSMProxyHelper.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkCustomPipelineHelper.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/H5MButil.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/XdmfH5MBCallback.cxx
  GUI_INTERFACES 
    ${DSM_IFACE}
  GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/XdmfSteeringParser.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.cxx
    ${DSM_IFACE_SRCS}
    ${DSM_MOC_SRCS}
    ${DSM_UI_SOURCES}
  GUI_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/IcarusQtResources.qrc
  GUI_RESOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Icarus_Readers.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/Filters.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.ui
)

TARGET_LINK_LIBRARIES(
  CSCS_DSM
    H5FDdsm
    hdf5_hl
    Xdmf
    XdmfGenerator
    ${PARAVIEW_HDF5_LIBRARIES} 
)

#--------------------------------------------------
# XdmfGenerator
#--------------------------------------------------
# USE_H5FD_DSM must be defined to enable DSM suppport in XdmfGenerator
IF(VTK_USE_MPI)
  SET(USE_MPI 1)
ENDIF(VTK_USE_MPI)
SET(USE_H5FD_DSM 1)
ADD_SUBDIRECTORY(XdmfGenerator)

#--------------------------------------------------
# Testing
#--------------------------------------------------
IF(CSCS_DSM_BUILD_TESTING)
  ADD_SUBDIRECTORY(Testing)
ENDIF(CSCS_DSM_BUILD_TESTING)

#--------------------------------------------------
# Converter
#--------------------------------------------------
IF(CSCS_DSM_BUILD_TESTING)
  ADD_SUBDIRECTORY(Tools)
ENDIF(CSCS_DSM_BUILD_TESTING)

#--------------------------------------------------
# Install
#--------------------------------------------------
SET(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}/lib/paraview-${PARAVIEW_VERSION_MAJOR}.${PARAVIEW_VERSION_MINOR}")

INSTALL(
  FILES ${TOOL_CFG_FILES}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

IF(PARAVIEW_BUILD_QT_GUI)
  INSTALL(
    TARGETS
      CSCS_DSM
    DESTINATION ${INSTALL_PATH}
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)

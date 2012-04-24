PROJECT("ICARUS_PLUGIN")

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
SET(QT_USE_QTNETWORK TRUE)
IF (ParaView_SOURCE_DIR)
  INCLUDE(${QT_USE_FILE})
  INCLUDE_DIRECTORIES(${HDF5_INCLUDE_DIR}
    ${PARAVIEW_INCLUDE_DIRS}
    ${PARAVIEW_GUI_INCLUDE_DIRS}
    ${PARAVIEW_KWSYS_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIR}
  )
ELSE (ParaView_SOURCE_DIR)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
ENDIF (ParaView_SOURCE_DIR)
INCLUDE(${ParaView_BINARY_DIR}/Utilities/Xdmf2/XDMFConfig.cmake)
INCLUDE_DIRECTORIES(${XDMF_MPI_INCLUDE_PATH} ${XDMF_LIBXML2_INCLUDE})

#--------------------------------------------------
# Find and Use System libs (only needed on WIN32)
#--------------------------------------------------
IF(WIN32 AND MSVC)
  IF (NOT TARGET "hdf5")
    FIND_PACKAGE(HDF5 REQUIRED NO_MODULE PATHS "$ENV{ProgramFiles}/HDF5")
    INCLUDE_DIRECTORIES(${HDF5_INCLUDE_DIR})
  ENDIF (NOT TARGET "hdf5")
  #--------------------------------------------------
  IF (NOT TARGET "H5FDdsm")
    FIND_PACKAGE(H5FDdsm REQUIRED NO_MODULE PATHS "$ENV{ProgramFiles}/H5FDdsm")
    INCLUDE_DIRECTORIES(${H5FD_DSM_INCLUDE_DIR})
  ENDIF (NOT TARGET "H5FDdsm")
#--------------------------------------------------
ENDIF(WIN32 AND MSVC)

#--------------------------------------------------
# DSM H5Part Reader needs pv-meshless
#--------------------------------------------------
INCLUDE_DIRECTORIES(
  ${CSCS_Plugins_SOURCE_DIR}/pv-meshless
  ${CSCS_Plugins_SOURCE_DIR}/pv-meshless/h5part
)

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
#  IF(CSCS_DSM_BUILD_TESTING)
#    SET(DSM_PANEL_CLASS pqDsmViewerPanel_testing)
#  ELSE(CSCS_DSM_BUILD_TESTING)
    SET(DSM_PANEL_CLASS pqDsmViewerPanel)
#  ENDIF(CSCS_DSM_BUILD_TESTING)

  QT4_WRAP_CPP(
    DSM_MOC_SRCS 
      ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.h
      ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmObjectInspector.h
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
  ${CMAKE_CURRENT_SOURCE_DIR}/CustomFilter_TransformBlock.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/CustomFilter_XdmfReaderBlock.xml
)

#MESSAGE("${ENCODED_FILTERS}")

#--------------------------------------------------
# Define Plugin
#--------------------------------------------------
ADD_PARAVIEW_PLUGIN(
  CSCS_DSM
  "1.0" 

  SERVER_MANAGER_XML 
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmManager.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter4.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkFlattenOneBlock.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkSteeringWriter.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkH5PartDsmReader.xml
  SERVER_MANAGER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmManager.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter4.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfReader4.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkFlattenOneBlock.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkSteeringWriter.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkH5PartDsmReader.cxx
  SERVER_SOURCES
    ${ENCODED_FILTERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDsmProxyHelper.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkCustomPipelineHelper.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/H5MButil.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/XdmfH5MBCallback.cxx
  GUI_INTERFACES 
    ${DSM_IFACE}
  GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/XdmfSteeringParser.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/${DSM_PANEL_CLASS}.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDsmObjectInspector.cxx
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
    pv_meshless
    H5FDdsm
    hdf5
    hdf5_hl
    XdmfGenerator
)

#--------------------------------------------------
# XdmfGenerator
#--------------------------------------------------
# USE_H5FD_DSM must be defined to enable DSM suppport in XdmfGenerator
IF(XDMF_BUILD_MPI)
  SET(USE_MPI 1)
ENDIF(XDMF_BUILD_MPI)
SET(USE_H5FD_DSM 1)
ADD_SUBDIRECTORY(XdmfGenerator)

#--------------------------------------------------
# Converter
#--------------------------------------------------
IF(CSCS_DSM_BUILD_TESTING)
  ADD_SUBDIRECTORY(Tools)
ENDIF(CSCS_DSM_BUILD_TESTING)

#--------------------------------------------------
# Testing
#--------------------------------------------------
IF(CSCS_DSM_BUILD_TESTING)
  ADD_SUBDIRECTORY(Testing)
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

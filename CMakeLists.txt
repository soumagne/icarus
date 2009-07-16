PROJECT("DSMMANAGER_PLUGIN")

CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

#--------------------------------------------------
# Find and Use ParaView
#--------------------------------------------------
IF (ParaView_SOURCE_DIR)
  INCLUDE(${QT_USE_FILE}) 
  INCLUDE_DIRECTORIES(
    ${PARAVIEW_INCLUDE_DIRS}
    ${PARAVIEW_GUI_INCLUDE_DIRS}
    ${PARAVIEW_KWSYS_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIR}
    ${MPI_INCLUDE_PATH}
    ${ParaView_SOURCE_DIR}/VTK/Utilities/vtklibxml2/include
    ${ParaView_DIR}/VTK/Utilities/vtklibxml2/libxml
  )
ELSE (ParaView_SOURCE_DIR)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
  INCLUDE_DIRECTORIES(
    ${ParaView_SOURCE_DIR}/VTK/Utilities/vtklibxml2/include
    ${ParaView_DIR}/VTK/Utilities/vtklibxml2/libxml
  )
ENDIF (ParaView_SOURCE_DIR)

#--------------------------------------------------
# Find and Use Boost
#--------------------------------------------------
   set(Boost_DEBUG ON)
   set(Boost_ADDITIONAL_VERSIONS "1.39" "1.39.0" "1.40" "1.40.0")
   set(BOOST_ROOT "C:/Program Files/Boost")
   set(Boost_USE_STATIC_LIBS   ON)
   set(Boost_USE_MULTITHREADED ON)
   find_package(Boost 1.38.0 COMPONENTS thread) 

   if(Boost_FOUND)
      include_directories(${Boost_INCLUDE_DIRS})
      MESSAGE(${Boost_INCLUDE_DIRS})
      ADD_DEFINITIONS(-DHAVE_BOOST_THREADS)
   endif()

#--------------------------------------------------
# Set project include directories 
#--------------------------------------------------
FIND_PATH(CSCSCommon_SOURCE_DIR vtkStreamUtils.h
  ${ParaView_SOURCE_DIR}/CSCS/vtkCSCSCommon
  ${CMAKE_CURRENT_SOURCE_DIR}/../../vtkCSCSCommon
)
INCLUDE_DIRECTORIES(${CSCSCommon_SOURCE_DIR})

#--------------------------------------------------
IF (NOT PARAVIEW_HDF5_LIBRARIES)
  INCLUDE(FindHDF5.cmake)
  SET(PARAVIEW_HDF5_LIBRARIES ${HDF_LIBRARY})
  INCLUDE_DIRECTORIES(${HDF5_INCLUDE_DIR})
ENDIF (NOT PARAVIEW_HDF5_LIBRARIES)

ADD_DEFINITIONS(-DH5_USE_16_API)
ADD_DEFINITIONS(-DH5FDmpio_DEBUG)

#--------------------------------------------------
# Set Definitions 
#--------------------------------------------------
ADD_DEFINITIONS(-DMPICH_SKIP_MPICXX)

#--------------------------------------------------
# Define Server Plugin
#--------------------------------------------------
ADD_PARAVIEW_PLUGIN(
  CSCS_DSM_SMPlugin 
  "1.0"
  
  SERVER_MANAGER_XML 
    ${CMAKE_CURRENT_SOURCE_DIR}/CSCS_DSMManager_Server.xml
  SERVER_MANAGER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkDSMManager.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkXdmfWriter2.cxx
  SOURCES
)

TARGET_LINK_LIBRARIES(
  CSCS_DSM_SMPlugin
    ${PARAVIEW_HDF5_LIBRARIES}
    Xdmf
    ${Boost_LIBRARIES}
)

#--------------------------------------------------
# Setup GUI custom Qt panel sources and wrapping
#--------------------------------------------------
IF(PARAVIEW_BUILD_QT_GUI)

QT4_WRAP_CPP(
  DSM_MOC_SRCS 
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDSMViewerPanel.h
)

# invoke macro that adds the refinement and display controls panel
ADD_PARAVIEW_DOCK_WINDOW(
  DSM_IFACE 
  DSM_IFACE_SRCS 
  CLASS_NAME 
    pqDSMViewerPanel 
  DOCK_AREA 
    Left Right Top Bottom
)

IF(PARAVIEW_BUILD_QT_GUI)
  QT4_WRAP_UI(
    DSM_UI_SOURCES
    pqDSMViewerPanel.ui
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)

#--------------------------------------------------
# Define Plugin Panel
#--------------------------------------------------
ADD_PARAVIEW_PLUGIN(
  CSCS_DSM_GUIPlugin 
  "1.0" 
   
  GUI_INTERFACES 
    ${DSM_IFACE}
  SOURCES
    pqDSMViewerPanel.cxx
    ${DSM_IFACE_SRCS}
    ${DSM_MOC_SRCS}
    ${DSM_UI_SOURCES}
  GUI_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CSCS_DSMManager.qrc
  GUI_RESOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/pqDSMViewerPanel.ui
)

ENDIF(PARAVIEW_BUILD_QT_GUI)

#--------------------------------------------------
# Define GUI Plugin
#--------------------------------------------------

TARGET_LINK_LIBRARIES(CSCS_DSM_GUIPlugin CSCS_DSM_SMPlugin)

#--------------------------------------------------
# Install
#--------------------------------------------------
SET(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}/lib/paraview-${PARAVIEW_VERSION_MAJOR}.${PARAVIEW_VERSION_MINOR}")

INSTALL(
  FILES ${TOOL_CFG_FILES}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

INSTALL(
  TARGETS 
    CSCS_DSM_SMPlugin 
  DESTINATION ${INSTALL_PATH}
)
IF(PARAVIEW_BUILD_QT_GUI)
  INSTALL(
    TARGETS
      CSCS_DSM_GUIPlugin
    DESTINATION ${INSTALL_PATH}
  )
ENDIF(PARAVIEW_BUILD_QT_GUI)



// .NAME vtkNetCDFCFDsmReader
//
// .SECTION Description
//
// Reads netCDF files that follow the CF convention from DSM.
//

#ifndef __vtkNetCDFCFDsmReader_h
#define __vtkNetCDFCFDsmReader_h

#include "vtkNetCDFCFReader.h"
class vtkHDF5DsmManager;

class VTK_EXPORT vtkNetCDFCFDsmReader : public vtkNetCDFCFReader
{
public:
  vtkTypeMacro(vtkNetCDFCFDsmReader, vtkNetCDFCFReader);
  static vtkNetCDFCFDsmReader *New();

  // Description:
  // Set/Get DsmBuffer Manager and enable DSM data reading instead of using
  // disk. The DSM manager is assumed to be created and initialized before
  // being passed into this routine
  virtual void SetDsmManager(vtkHDF5DsmManager*);
  vtkGetObjectMacro(DsmManager, vtkHDF5DsmManager);

  virtual int OpenFile(const char *name, int mode, int *id);
  virtual void SetFileModified();

  virtual int RequestInformation(vtkInformation *request,
                                 vtkInformationVector **inputVector,
                                 vtkInformationVector *outputVector);

protected:
   vtkNetCDFCFDsmReader();
  ~vtkNetCDFCFDsmReader();

  vtkHDF5DsmManager *DsmManager;

private:
  vtkNetCDFCFDsmReader(const vtkNetCDFCFDsmReader &); // Not implemented
  void operator=(const vtkNetCDFCFDsmReader &);    // Not implemented
};

#endif //__vtkNetCDFCFDsmReader_h


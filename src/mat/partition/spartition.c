#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: spartition.c,v 1.3 1997/10/19 03:26:35 bsmith Exp bsmith $";
#endif
 
#include "petsc.h"
#include "mat.h"

extern int MatGetPartitioning_Current(Mat,MatPartitioning, int,ISPartitioning *);

#undef __FUNC__  
#define __FUNC__ "MatPartitioningRegisterAll" 
/*@C
  MatPartitioningRegisterAll - Registers all of the matrix Partitioning routines in PETSc.

  Adding new methods:
  To add a new method to the registry. Copy this routine and 
  modify it to incorporate a call to MatPartitioningRegister() for 
  the new method, after the current list.

  Restricting the choices: To prevent all of the methods from being
  registered and thus save memory, copy this routine and modify it to
  register a zero, instead of the function name, for those methods you
  do not wish to register.  Make sure that the replacement routine is
  linked before libpetscmat.a.

.keywords: matrix, Partitioning, register, all

.seealso: MatPartitioningRegister(), MatPartitioningRegisterDestroy()
@*/
int MatPartitioningRegisterAll()
{
  int         ierr;

  PetscFunctionBegin;
  MatPartitioningRegisterAllCalled = 1;  
  ierr = MatPartitioningRegister(PARTITIONING_CURRENT,0,"natural",MatGetPartitioning_Current);CHKERRQ(ierr);


  PetscFunctionReturn(0);
}




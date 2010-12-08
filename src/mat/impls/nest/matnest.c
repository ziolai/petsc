#define PETSCMAT_DLL

#include "matnestimpl.h" /*I   "petscmat.h"   I*/

/* private functions */
#undef __FUNCT__ 
#define __FUNCT__ "MatNestGetSize_Recursive"
static PetscErrorCode MatNestGetSize_Recursive(Mat A,PetscBool globalsize,PetscInt *M,PetscInt *N)
{
  Mat_Nest       *bA = (Mat_Nest*)A->data;
  PetscInt       sizeM,sizeN,i,j,index = -1;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* rows */
  /* now descend recursively */
  for (i=0; i<bA->nr; i++) {
    for (j=0; j<bA->nc; j++) {
      if (bA->m[i][j]) { index = j; break; }
    }
    if (bA->m[i][index]) {
      if (globalsize) { ierr = MatGetSize(bA->m[i][index],&sizeM,&sizeN);CHKERRQ(ierr); }
      else {            ierr = MatGetLocalSize(bA->m[i][index],&sizeM,&sizeN);CHKERRQ(ierr); }
      *M = *M + sizeM;
    }
  }

  /* cols */
  /* now descend recursively */
  for (j=0; j<bA->nc; j++) {
    for (i=0; i<bA->nr; i++) {
      if (bA->m[i][j]) { index = i; break; }
    }
    if (bA->m[index][j]) {
      if (globalsize) { ierr = MatGetSize(bA->m[index][j],&sizeM,&sizeN);CHKERRQ(ierr); }
      else {            ierr = MatGetLocalSize(bA->m[index][j],&sizeM,&sizeN);CHKERRQ(ierr); }
      *N = *N + sizeN;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PETSc_MatNest_CheckMatVecCompatibility2"
PETSC_UNUSED
static PetscErrorCode PETSc_MatNest_CheckMatVecCompatibility2(Mat A,Vec x,Vec y)
{
  PetscBool      isANest, isxNest, isyNest;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  isANest = isxNest = isyNest = PETSC_FALSE;
  ierr = PetscTypeCompare( (PetscObject)A, MATNEST, &isANest );CHKERRQ(ierr);
  ierr = PetscTypeCompare( (PetscObject)x, VECNEST, &isxNest );CHKERRQ(ierr);
  ierr = PetscTypeCompare( (PetscObject)y, VECNEST, &isyNest );CHKERRQ(ierr);

  if (isANest && isxNest && isyNest) {
    PetscFunctionReturn(0);
  } else {
    SETERRQ( ((PetscObject)A)->comm, PETSC_ERR_SUP, "Operation only valid for Mat(Nest)-Vec(Nest) combinations");
    PetscFunctionReturn(0);
  }
  PetscFunctionReturn(0);
}

/*
 This function is called every time we insert a sub matrix the Nest.
 It ensures that every Nest along row r, and col c has the same dimensions
 as the submat being inserted.
*/
#undef __FUNCT__  
#define __FUNCT__ "PETSc_MatNest_CheckConsistency"
PETSC_UNUSED
static PetscErrorCode PETSc_MatNest_CheckConsistency(Mat A,Mat submat,PetscInt r,PetscInt c)
{
  Mat_Nest       *b = (Mat_Nest*)A->data;
  PetscInt       i,j;
  PetscInt       nr,nc;
  PetscInt       sM,sN,M,N;
  Mat            mat;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  nr = b->nr;
  nc = b->nc;
  ierr = MatGetSize(submat,&sM,&sN);CHKERRQ(ierr);
  for (i=0; i<nr; i++) {
    mat = b->m[i][c];
    if (mat) {
      ierr = MatGetSize(mat,&M,&N);CHKERRQ(ierr);
      /* Check columns match */
      if (sN != N) {
        SETERRQ3(((PetscObject)A)->comm,PETSC_ERR_SUP,"Inserted incompatible submatrix into Nest at (%D,%D). Submatrix must have %D rows",r,c,N );
      }
    }
  }

  for (j=0; j<nc; j++) {
    mat = b->m[r][j];
    if (mat) {
      ierr = MatGetSize(mat,&M,&N);CHKERRQ(ierr);
      /* Check rows match */
      if (sM != M) {
        SETERRQ3(((PetscObject)A)->comm,PETSC_ERR_SUP,"Inserted incompatible submatrix into Nest at (%D,%D). Submatrix must have %D cols",r,c,M );
      }
    }
  }
  PetscFunctionReturn(0);
}

/*
 Checks if the row/col's contain a complete set of IS's.
 Once they are filled, the offsets are computed. This saves having to update
 the index set entries each time we insert something new.
 */
#undef __FUNCT__  
#define __FUNCT__ "PETSc_MatNest_UpdateStructure"
PETSC_UNUSED
static PetscErrorCode PETSc_MatNest_UpdateStructure(Mat A,PetscInt ridx,PetscInt cidx)
{
  Mat_Nest       *b = (Mat_Nest*)A->data;
  PetscInt       m,n,i,j;
  PetscBool      fullrow,fullcol;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = MatGetLocalSize(b->m[ridx][cidx],&m,&n);CHKERRQ(ierr);
  b->row_len[ridx] = m;
  b->col_len[cidx] = n;

  fullrow = PETSC_TRUE;
  for (i=0; i<b->nr; i++) {
    if (b->row_len[i]==-1) { fullrow = PETSC_FALSE; break; }
  }
  if ( (fullrow) && (!b->isglobal.row[0]) ){
    PetscInt cnt = 0;
    for (i=0; i<b->nr; i++) {
      ierr = ISCreateStride(PETSC_COMM_SELF,b->row_len[i],cnt,1,&b->isglobal.row[i]);CHKERRQ(ierr);
      cnt = cnt + b->row_len[i];
    }
  }

  fullcol = PETSC_TRUE;
  for (j=0; j<b->nc; j++) {
    if (b->col_len[j]==-1) { fullcol = PETSC_FALSE; break; }
  }
  if( (fullcol) && (!b->isglobal.col[0]) ){
    PetscInt cnt = 0;
    for (j=0; j<b->nc; j++) {
      ierr = ISCreateStride(PETSC_COMM_SELF,b->col_len[j],cnt,1,&b->isglobal.col[j]);CHKERRQ(ierr);
      cnt = cnt + b->col_len[j];
    }
  }
  PetscFunctionReturn(0);
}

/* operations */
#undef __FUNCT__  
#define __FUNCT__ "MatMult_Nest"
static PetscErrorCode MatMult_Nest(Mat A,Vec x,Vec y)
{
  Mat_Nest       *bA = (Mat_Nest*)A->data;
  Vec            *bx = bA->right,*by = bA->left;
  PetscInt       i,j,nr = bA->nr,nc = bA->nc;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  for (i=0; i<nr; i++) {ierr = VecGetSubVector(y,bA->isglobal.row[i],&by[i]);CHKERRQ(ierr);}
  for (i=0; i<nc; i++) {ierr = VecGetSubVector(x,bA->isglobal.col[i],&bx[i]);CHKERRQ(ierr);}
  for (i=0; i<nr; i++) {
    ierr = VecZeroEntries(by[i]);CHKERRQ(ierr);
    for (j=0; j<nc; j++) {
      if (!bA->m[i][j]) continue;
      /* y[i] <- y[i] + A[i][j] * x[j] */
      ierr = MatMultAdd(bA->m[i][j],bx[j],by[i],by[i]);CHKERRQ(ierr);
    }
  }
  for (i=0; i<nr; i++) {ierr = VecRestoreSubVector(y,bA->isglobal.row[i],&by[i]);CHKERRQ(ierr);}
  for (i=0; i<nc; i++) {ierr = VecRestoreSubVector(x,bA->isglobal.col[i],&bx[i]);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatMultTranspose_Nest"
static PetscErrorCode MatMultTranspose_Nest(Mat A,Vec x,Vec y)
{
  Mat_Nest       *bA = (Mat_Nest*)A->data;
  Vec            *bx = bA->left,*by = bA->right;
  PetscInt       i,j,nr = bA->nr,nc = bA->nc;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (A->symmetric) {
    ierr = MatMult_Nest(A,x,y);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  for (i=0; i<nr; i++) {ierr = VecGetSubVector(y,bA->isglobal.row[i],&bx[i]);CHKERRQ(ierr);}
  for (i=0; i<nc; i++) {ierr = VecGetSubVector(x,bA->isglobal.col[i],&by[i]);CHKERRQ(ierr);}
  for (i=0; i<nr; i++) {
    ierr = VecZeroEntries(by[i]);CHKERRQ(ierr);
    for (j=0; j<nc; j++) {
      if (!bA->m[j][i]) continue;
      /* y[i] <- y[i] + A^T[i][j] * x[j], so we swap i,j in mat[][] */
      ierr = MatMultTransposeAdd(bA->m[j][i],bx[j],by[i],by[i]);CHKERRQ(ierr);
    }
  }
  for (i=0; i<nr; i++) {ierr = VecRestoreSubVector(y,bA->isglobal.row[i],&bx[i]);CHKERRQ(ierr);}
  for (i=0; i<nc; i++) {ierr = VecRestoreSubVector(x,bA->isglobal.col[i],&by[i]);CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNestDestroyISList"
static PetscErrorCode MatNestDestroyISList(PetscInt n,IS **list)
{
  PetscErrorCode ierr;
  IS             *lst = *list;
  PetscInt       i;

  PetscFunctionBegin;
  if (!lst) PetscFunctionReturn(0);
  for (i=0; i<n; i++) if (lst[i]) {ierr = ISDestroy(lst[i]);CHKERRQ(ierr);}
  ierr = PetscFree(lst);CHKERRQ(ierr);
  *list = PETSC_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDestroy_Nest"
static PetscErrorCode MatDestroy_Nest(Mat A)
{
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  PetscInt       i,j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* release the matrices and the place holders */
  ierr = MatNestDestroyISList(vs->nr,&vs->isglobal.row);CHKERRQ(ierr);
  ierr = MatNestDestroyISList(vs->nc,&vs->isglobal.col);CHKERRQ(ierr);
  ierr = MatNestDestroyISList(vs->nr,&vs->islocal.row);CHKERRQ(ierr);
  ierr = MatNestDestroyISList(vs->nc,&vs->islocal.col);CHKERRQ(ierr);

  ierr = PetscFree(vs->row_len);CHKERRQ(ierr);
  ierr = PetscFree(vs->col_len);CHKERRQ(ierr);

  ierr = PetscFree2(vs->left,vs->right);CHKERRQ(ierr);

  /* release the matrices and the place holders */
  if (vs->m) {
    for (i=0; i<vs->nr; i++) {
      for (j=0; j<vs->nc; j++) {

        if (vs->m[i][j]) {
          ierr = MatDestroy(vs->m[i][j]);CHKERRQ(ierr);
          vs->m[i][j] = PETSC_NULL;
        }
      }
      ierr = PetscFree( vs->m[i] );CHKERRQ(ierr);
      vs->m[i] = PETSC_NULL;
    }
    ierr = PetscFree(vs->m);CHKERRQ(ierr);
    vs->m = PETSC_NULL;
  }
  ierr = PetscFree(vs);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatAssemblyBegin_Nest"
static PetscErrorCode MatAssemblyBegin_Nest(Mat A,MatAssemblyType type)
{
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  PetscInt       i,j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  for (i=0; i<vs->nr; i++) {
    for (j=0; j<vs->nc; j++) {
      if (vs->m[i][j]) { ierr = MatAssemblyBegin(vs->m[i][j],type);CHKERRQ(ierr); }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatAssemblyEnd_Nest"
static PetscErrorCode MatAssemblyEnd_Nest(Mat A, MatAssemblyType type)
{
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  PetscInt       i,j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  for (i=0; i<vs->nr; i++) {
    for (j=0; j<vs->nc; j++) {
      if (vs->m[i][j]) { ierr = MatAssemblyEnd(vs->m[i][j],type);CHKERRQ(ierr); }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNestFindNonzeroSubMatRow"
static PetscErrorCode MatNestFindNonzeroSubMatRow(Mat A,PetscInt row,Mat *B)
{
  PetscErrorCode ierr;
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  PetscInt       j;
  Mat            sub;

  PetscFunctionBegin;
  sub = vs->m[row][row];        /* Prefer to find on the diagonal */
  for (j=0; !sub && j<vs->nc; j++) sub = vs->m[row][j];
  if (!sub) SETERRQ1(((PetscObject)A)->comm,PETSC_ERR_ARG_WRONG,"No nonzero submatrix in row %D",row);
  ierr = MatPreallocated(sub);CHKERRQ(ierr); /* Ensure that the sizes are available */
  *B = sub;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNestFindNonzeroSubMatCol"
static PetscErrorCode MatNestFindNonzeroSubMatCol(Mat A,PetscInt col,Mat *B)
{
  PetscErrorCode ierr;
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  PetscInt       i;
  Mat            sub;

  PetscFunctionBegin;
  sub = vs->m[col][col];        /* Prefer to find on the diagonal */
  for (i=0; !sub && i<vs->nr; i++) sub = vs->m[i][col];
  if (!sub) SETERRQ1(((PetscObject)A)->comm,PETSC_ERR_ARG_WRONG,"No nonzero submatrix in column %D",col);
  ierr = MatPreallocated(sub);CHKERRQ(ierr); /* Ensure that the sizes are available */
  *B = sub;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNestFindIS"
static PetscErrorCode MatNestFindIS(Mat A,PetscInt n,const IS list[],IS is,PetscInt *found)
{
  PetscErrorCode ierr;
  PetscInt       i;
  PetscBool      flg;

  PetscFunctionBegin;
  PetscValidPointer(list,3);
  PetscValidHeaderSpecific(is,IS_CLASSID,4);
  PetscValidIntPointer(found,5);
  *found = -1;
  for (i=0; i<n; i++) {
    if (!list[i]) continue;
    ierr = ISEqual(list[i],is,&flg);CHKERRQ(ierr);
    if (flg) {
      *found = i;
      PetscFunctionReturn(0);
    }
  }
  SETERRQ(((PetscObject)A)->comm,PETSC_ERR_ARG_INCOMP,"Could not find index set");
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNestFindSubMat"
static PetscErrorCode MatNestFindSubMat(Mat A,struct MatNestISPair *is,IS isrow,IS iscol,Mat *B)
{
  PetscErrorCode ierr;
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  PetscInt       row,col;

  PetscFunctionBegin;
  ierr = MatNestFindIS(A,vs->nr,is->row,isrow,&row);CHKERRQ(ierr);
  ierr = MatNestFindIS(A,vs->nc,is->col,iscol,&col);CHKERRQ(ierr);
  *B = vs->m[row][col];
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetSubMatrix_Nest"
static PetscErrorCode MatGetSubMatrix_Nest(Mat A,IS isrow,IS iscol,MatReuse reuse,Mat *B)
{
  PetscErrorCode ierr;
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  Mat            sub;

  PetscFunctionBegin;
  ierr = MatNestFindSubMat(A,&vs->isglobal,isrow,iscol,&sub);CHKERRQ(ierr);
  switch (reuse) {
  case MAT_INITIAL_MATRIX:
    ierr = PetscObjectReference((PetscObject)sub);CHKERRQ(ierr);
    *B = sub;
    break;
  case MAT_REUSE_MATRIX:
    if (sub != *B) SETERRQ(((PetscObject)A)->comm,PETSC_ERR_ARG_WRONGSTATE,"Submatrix was not used before in this call");
    break;
  case MAT_IGNORE_MATRIX:       /* Nothing to do */
    break;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetLocalSubMatrix_Nest"
PetscErrorCode MatGetLocalSubMatrix_Nest(Mat A,IS isrow,IS iscol,Mat *B)
{
  PetscErrorCode ierr;
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  Mat            sub;

  PetscFunctionBegin;
  ierr = MatNestFindSubMat(A,&vs->islocal,isrow,iscol,&sub);CHKERRQ(ierr);
  /* We allow the submatrix to be NULL, perhaps it would be better for the user to return an empty matrix instead */
  if (sub) {ierr = PetscObjectReference((PetscObject)sub);CHKERRQ(ierr);}
  *B = sub;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreLocalSubMatrix_Nest"
static PetscErrorCode MatRestoreLocalSubMatrix_Nest(Mat A,IS isrow,IS iscol,Mat *B)
{
  PetscErrorCode ierr;
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  Mat            sub;

  PetscFunctionBegin;
  ierr = MatNestFindSubMat(A,&vs->islocal,isrow,iscol,&sub);CHKERRQ(ierr);
  if (*B != sub) SETERRQ(((PetscObject)A)->comm,PETSC_ERR_ARG_WRONGSTATE,"Local submatrix has not been gotten");
  if (sub) {
    if (((PetscObject)sub)->refct <= 1) SETERRQ(((PetscObject)A)->comm,PETSC_ERR_ARG_WRONGSTATE,"Local submatrix has had reference count decremented too many times");
    ierr = MatDestroy(*B);CHKERRQ(ierr);
  }
  *B = PETSC_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetVecs_Nest"
static PetscErrorCode MatGetVecs_Nest(Mat A,Vec *right,Vec *left)
{
  Mat_Nest       *bA = (Mat_Nest*)A->data;
  Vec            *L,*R;
  MPI_Comm       comm;
  PetscInt       i,j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  comm = ((PetscObject)A)->comm;
  if (right) {
    /* allocate R */
    ierr = PetscMalloc( sizeof(Vec) * bA->nc, &R );CHKERRQ(ierr);
    /* Create the right vectors */
    for (j=0; j<bA->nc; j++) {
      for (i=0; i<bA->nr; i++) {
        if (bA->m[i][j]) {
          ierr = MatGetVecs(bA->m[i][j],&R[j],PETSC_NULL);CHKERRQ(ierr);
          break;
        }
      }
      if (i==bA->nr) {
        /* have an empty column */
        SETERRQ( ((PetscObject)A)->comm, PETSC_ERR_ARG_WRONG, "Mat(Nest) contains a null column.");
      }
    }
    ierr = VecCreateNest(comm,bA->nc,bA->isglobal.col,R,right);CHKERRQ(ierr);
    /* hand back control to the nest vector */
    for (j=0; j<bA->nc; j++) {
      ierr = VecDestroy(R[j]);CHKERRQ(ierr);
    }
    ierr = PetscFree(R);CHKERRQ(ierr);
  }

  if (left) {
    /* allocate L */
    ierr = PetscMalloc( sizeof(Vec) * bA->nr, &L );CHKERRQ(ierr);
    /* Create the left vectors */
    for (i=0; i<bA->nr; i++) {
      for (j=0; j<bA->nc; j++) {
        if (bA->m[i][j]) {
          ierr = MatGetVecs(bA->m[i][j],PETSC_NULL,&L[i]);CHKERRQ(ierr);
          break;
        }
      }
      if (j==bA->nc) {
        /* have an empty row */
        SETERRQ( ((PetscObject)A)->comm, PETSC_ERR_ARG_WRONG, "Mat(Nest) contains a null row.");
      }
    }

    ierr = VecCreateNest(comm,bA->nr,bA->isglobal.row,L,left);CHKERRQ(ierr);
    for (i=0; i<bA->nr; i++) {
      ierr = VecDestroy(L[i]);CHKERRQ(ierr);
    }

    ierr = PetscFree(L);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatView_Nest"
static PetscErrorCode MatView_Nest(Mat A,PetscViewer viewer)
{
  Mat_Nest       *bA = (Mat_Nest*)A->data;
  PetscBool      isascii;
  PetscInt       i,j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {

    PetscViewerASCIIPrintf(viewer,"Matrix object: \n" );
    PetscViewerASCIIPushTab( viewer );    /* push0 */
    PetscViewerASCIIPrintf( viewer, "type=nest, rows=%d, cols=%d \n",bA->nr,bA->nc);

    PetscViewerASCIIPrintf(viewer,"MatNest structure: \n" );
    for (i=0; i<bA->nr; i++) {
      for (j=0; j<bA->nc; j++) {
        const MatType type;
        const char *name;
        PetscInt NR,NC;
        PetscBool isNest = PETSC_FALSE;

        if (!bA->m[i][j]) {
          PetscViewerASCIIPrintf( viewer, "(%D,%D) : PETSC_NULL \n",i,j);
          continue;
        }
        ierr = MatGetSize(bA->m[i][j],&NR,&NC);CHKERRQ(ierr);
        ierr = MatGetType( bA->m[i][j], &type );CHKERRQ(ierr);
        name = ((PetscObject)bA->m[i][j])->prefix;
        ierr = PetscTypeCompare((PetscObject)bA->m[i][j],MATNEST,&isNest);CHKERRQ(ierr);

        PetscViewerASCIIPrintf( viewer, "(%D,%D) : name=\"%s\", type=%s, rows=%D, cols=%D \n",i,j,name,type,NR,NC);

        if (isNest) {
          PetscViewerASCIIPushTab( viewer );  /* push1 */
          ierr = MatView( bA->m[i][j], viewer );CHKERRQ(ierr);
          PetscViewerASCIIPopTab(viewer);    /* pop1 */
        }
      }
    }
    PetscViewerASCIIPopTab(viewer);    /* pop0 */
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatZeroEntries_Nest"
static PetscErrorCode MatZeroEntries_Nest(Mat A)
{
  Mat_Nest       *bA = (Mat_Nest*)A->data;
  PetscInt       i,j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  for (i=0; i<bA->nr; i++) {
    for (j=0; j<bA->nc; j++) {
      if (!bA->m[i][j]) continue;
      ierr = MatZeroEntries(bA->m[i][j]);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatDuplicate_Nest"
static PetscErrorCode MatDuplicate_Nest(Mat A,MatDuplicateOption op,Mat *B)
{
  Mat_Nest       *bA = (Mat_Nest*)A->data;
  Mat            *b;
  PetscInt       i,j,nr = bA->nr,nc = bA->nc;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscMalloc(nr*nc*sizeof(Mat),&b);CHKERRQ(ierr);
  for (i=0; i<nr; i++) {
    for (j=0; j<nc; j++) {
      if (bA->m[i][j]) {
        ierr = MatDuplicate(bA->m[i][j],op,&b[i*nc+j]);CHKERRQ(ierr);
      } else {
        b[i*nc+j] = PETSC_NULL;
      }
    }
  }
  ierr = MatCreateNest(((PetscObject)A)->comm,nr,bA->isglobal.row,nc,bA->isglobal.col,b,B);CHKERRQ(ierr);
  /* Give the new MatNest exclusive ownership */
  for (i=0; i<nr*nc; i++) {
    if (b[i]) {ierr = MatDestroy(b[i]);CHKERRQ(ierr);}
  }
  ierr = PetscFree(b);CHKERRQ(ierr);

  ierr = MatAssemblyBegin(*B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* nest api */
EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatNestGetSubMat_Nest"
PetscErrorCode MatNestGetSubMat_Nest(Mat A,PetscInt idxm,PetscInt jdxm,Mat *mat)
{
  Mat_Nest *bA = (Mat_Nest*)A->data;
  PetscFunctionBegin;
  if (idxm >= bA->nr) SETERRQ2(((PetscObject)A)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Row too large: row %D max %D",idxm,bA->nr-1);
  if (jdxm >= bA->nc) SETERRQ2(((PetscObject)A)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Col too large: row %D max %D",jdxm,bA->nc-1);
  *mat = bA->m[idxm][jdxm];
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatNestGetSubMat"
/*@C
 MatNestGetSubMat - Returns a single, sub-matrix from a nest matrix.

 Not collective

 Input Parameters:
 .  A  - nest matrix
 .  idxm - index of the matrix within the nest matrix
 .  jdxm - index of the matrix within the nest matrix

 Output Parameter:
 .  sub - matrix at index idxm,jdxm within the nest matrix

 Notes:

 Level: developer

 .seealso: MatNestGetSize(), MatNestGetSubMats()
@*/
PetscErrorCode  MatNestGetSubMat(Mat A,PetscInt idxm,PetscInt jdxm,Mat *sub)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscUseMethod(A,"MatNestGetSubMat_C",(Mat,PetscInt,PetscInt,Mat*),(A,idxm,jdxm,sub));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatNestGetSubMats_Nest"
PetscErrorCode MatNestGetSubMats_Nest(Mat A,PetscInt *M,PetscInt *N,Mat ***mat)
{
  Mat_Nest *bA = (Mat_Nest*)A->data;
  PetscFunctionBegin;
  if (M)   { *M   = bA->nr; }
  if (N)   { *N   = bA->nc; }
  if (mat) { *mat = bA->m;  }
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatNestGetSubMats"
/*@C
 MatNestGetSubMats - Returns the entire two dimensional array of matrices defining a nest matrix.

 Not collective

 Input Parameters:
 .  A  - nest matri

 Output Parameter:
 .  M - number of rows in the nest matrix
 .  N - number of cols in the nest matrix
 .  mat - 2d array of matrices

 Notes:

 The user should not free the array mat.

 Level: developer

 .seealso: MatNestGetSize(), MatNestGetSubMat()
@*/
PetscErrorCode  MatNestGetSubMats(Mat A,PetscInt *M,PetscInt *N,Mat ***mat)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscUseMethod(A,"MatNestGetSubMats_C",(Mat,PetscInt*,PetscInt*,Mat***),(A,M,N,mat));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatNestGetSize_Nest"
PetscErrorCode  MatNestGetSize_Nest(Mat A,PetscInt *M,PetscInt *N)
{
  Mat_Nest  *bA = (Mat_Nest*)A->data;

  PetscFunctionBegin;
  if (M) { *M  = bA->nr; }
  if (N) { *N  = bA->nc; }
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatNestGetSize"
/*@C
 MatNestGetSize - Returns the size of the nest matrix.

 Not collective

 Input Parameters:
 .  A  - nest matrix

 Output Parameter:
 .  M - number of rows in the nested mat
 .  N - number of cols in the nested mat

 Notes:

 Level: developer

 .seealso: MatNestGetSubMat(), MatNestGetSubMats()
@*/
PetscErrorCode  MatNestGetSize(Mat A,PetscInt *M,PetscInt *N)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscUseMethod(A,"MatNestGetSize_C",(Mat,PetscInt*,PetscInt*),(A,M,N));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "MatNestSetVecType_Nest"
PetscErrorCode  MatNestSetVecType_Nest(Mat A,const VecType vtype)
{
  PetscErrorCode ierr;
  PetscBool      flg;

  PetscFunctionBegin;
  ierr = PetscStrcmp(vtype,VECNEST,&flg);CHKERRQ(ierr);
  /* In reality, this only distinguishes VECNEST and "other" */
  A->ops->getvecs = flg ? MatGetVecs_Nest : 0;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatNestSetVecType"
/*@C
 MatNestSetVecType - Sets the type of Vec returned by MatGetVecs()

 Not collective

 Input Parameters:
+  A  - nest matrix
-  vtype - type to use for creating vectors

 Notes:

 Level: developer

 .seealso: MatGetVecs()
@*/
PetscErrorCode  MatNestSetVecType(Mat A,const VecType vtype)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscTryMethod(A,"MatNestSetVecType_C",(Mat,const VecType),(A,vtype));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* constructor */
#undef __FUNCT__  
#define __FUNCT__ "MatNestSetOps_Private"
static PetscErrorCode MatNestSetOps_Private(struct _MatOps* ops)
{
  PetscFunctionBegin;
  /* 0*/
  ops->setvalues  = 0;
  ops->getrow     = 0;
  ops->restorerow = 0;
  ops->mult       = MatMult_Nest;
  ops->multadd    = 0;
  /* 5*/
  ops->multtranspose    = MatMultTranspose_Nest;
  ops->multtransposeadd = 0;
  ops->solve            = 0;
  ops->solveadd         = 0;
  ops->solvetranspose   = 0;
  /*10*/
  ops->solvetransposeadd = 0;
  ops->lufactor          = 0;
  ops->choleskyfactor    = 0;
  ops->sor               = 0;
  ops->transpose         = 0;
  /*15*/
  ops->getinfo       = 0;
  ops->equal         = 0;
  ops->getdiagonal   = 0;
  ops->diagonalscale = 0;
  ops->norm          = 0;
  /*20*/
  ops->assemblybegin = MatAssemblyBegin_Nest;
  ops->assemblyend   = MatAssemblyEnd_Nest;
  ops->setoption     = 0;
  ops->zeroentries   = MatZeroEntries_Nest;
  /*24*/
  ops->zerorows               = 0;
  ops->lufactorsymbolic       = 0;
  ops->lufactornumeric        = 0;
  ops->choleskyfactorsymbolic = 0;
  ops->choleskyfactornumeric  = 0;
  /*29*/
  ops->setuppreallocation = 0;
  ops->ilufactorsymbolic  = 0;
  ops->iccfactorsymbolic  = 0;
  ops->getarray           = 0;
  ops->restorearray       = 0;
  /*34*/
  ops->duplicate     = MatDuplicate_Nest;
  ops->forwardsolve  = 0;
  ops->backwardsolve = 0;
  ops->ilufactor     = 0;
  ops->iccfactor     = 0;
  /*39*/
  ops->axpy            = 0;
  ops->getsubmatrices  = 0;
  ops->increaseoverlap = 0;
  ops->getvalues       = 0;
  ops->copy            = 0;
  /*44*/
  ops->getrowmax   = 0;
  ops->scale       = 0;
  ops->shift       = 0;
  ops->diagonalset = 0;
  ops->zerorowscolumns  = 0;
  /*49*/
  ops->setblocksize    = 0;
  ops->getrowij        = 0;
  ops->restorerowij    = 0;
  ops->getcolumnij     = 0;
  ops->restorecolumnij = 0;
  /*54*/
  ops->fdcoloringcreate = 0;
  ops->coloringpatch    = 0;
  ops->setunfactored    = 0;
  ops->permute          = 0;
  ops->setvaluesblocked = 0;
  /*59*/
  ops->getsubmatrix  = MatGetSubMatrix_Nest;
  ops->destroy       = MatDestroy_Nest;
  ops->view          = MatView_Nest;
  ops->convertfrom   = 0;
  ops->usescaledform = 0;
  /*64*/
  ops->scalesystem             = 0;
  ops->unscalesystem           = 0;
  ops->setlocaltoglobalmapping = 0;
  ops->setvalueslocal          = 0;
  ops->zerorowslocal           = 0;
  /*69*/
  ops->getrowmaxabs    = 0;
  ops->getrowminabs    = 0;
  ops->convert         = 0;/*MatConvert_Nest;*/
  ops->setcoloring     = 0;
  ops->setvaluesadic   = 0;
  /* 74 */
  ops->setvaluesadifor = 0;
  ops->fdcoloringapply              = 0;
  ops->setfromoptions               = 0;
  ops->multconstrained              = 0;
  ops->multtransposeconstrained     = 0;
  /*79*/
  ops->dummy           = 0;
  ops->mults           = 0;
  ops->solves          = 0;
  ops->getinertia      = 0;
  ops->load            = 0;
  /*84*/
  ops->issymmetric             = 0;
  ops->ishermitian             = 0;
  ops->isstructurallysymmetric = 0;
  ops->dummy4                  = 0;
  ops->getvecs                 = 0; /* Use VECNEST by calling MatNestSetVecType(A,VECNEST) */
  /*89*/
  ops->matmult         = 0;/*MatMatMult_Nest;*/
  ops->matmultsymbolic = 0;
  ops->matmultnumeric  = 0;
  ops->ptap            = 0;
  ops->ptapsymbolic    = 0;
  /*94*/
  ops->ptapnumeric              = 0;
  ops->matmulttranspose         = 0;
  ops->matmulttransposesymbolic = 0;
  ops->matmulttransposenumeric  = 0;
  ops->ptapsymbolic_seqaij      = 0;
  /*99*/
  ops->ptapnumeric_seqaij  = 0;
  ops->ptapsymbolic_mpiaij = 0;
  ops->ptapnumeric_mpiaij  = 0;
  ops->conjugate           = 0;
  ops->setsizes            = 0;
  /*104*/
  ops->setvaluesrow              = 0;
  ops->realpart                  = 0;
  ops->imaginarypart             = 0;
  ops->getrowuppertriangular     = 0;
  ops->restorerowuppertriangular = 0;
  /*109*/
  ops->matsolve           = 0;
  ops->getredundantmatrix = 0;
  ops->getrowmin          = 0;
  ops->getcolumnvector    = 0;
  ops->missingdiagonal    = 0;
  /* 114 */
  ops->getseqnonzerostructure = 0;
  ops->create                 = 0;
  ops->getghosts              = 0;
  ops->getlocalsubmatrix      = MatGetLocalSubMatrix_Nest;
  ops->restorelocalsubmatrix  = MatRestoreLocalSubMatrix_Nest;
  /* 119 */
  ops->multdiagonalblock         = 0;
  ops->hermitiantranspose        = 0;
  ops->multhermitiantranspose    = 0;
  ops->multhermitiantransposeadd = 0;
  ops->getmultiprocblock         = 0;
  /* 124 */
  ops->dummy1                 = 0;
  ops->dummy2                 = 0;
  ops->dummy3                 = 0;
  ops->dummy4                 = 0;
  ops->getsubmatricesparallel = 0;

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatSetUp_Nest_Private"
static PetscErrorCode MatSetUp_Nest_Private(Mat A,PetscInt nr,PetscInt nc,const Mat *sub)
{
  Mat_Nest       *ctx = (Mat_Nest*)A->data;
  PetscInt       i,j;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if(ctx->setup_called) PetscFunctionReturn(0);

  ctx->nr = nr;
  ctx->nc = nc;

  /* Create space */
  ierr = PetscMalloc(sizeof(Mat*)*ctx->nr,&ctx->m);CHKERRQ(ierr);
  for (i=0; i<ctx->nr; i++) {
    ierr = PetscMalloc(sizeof(Mat)*ctx->nc,&ctx->m[i]);CHKERRQ(ierr);
  }
  for (i=0; i<ctx->nr; i++) {
    for (j=0; j<ctx->nc; j++) {
      ctx->m[i][j] = sub[i*nc+j];
      if (sub[i*nc+j]) {
        ierr = PetscObjectReference((PetscObject)sub[i*nc+j]);CHKERRQ(ierr);
      }
    }
  }

  ierr = PetscMalloc(sizeof(IS)*ctx->nr,&ctx->isglobal.row);CHKERRQ(ierr);
  ierr = PetscMalloc(sizeof(IS)*ctx->nc,&ctx->isglobal.col);CHKERRQ(ierr);

  ierr = PetscMalloc(sizeof(PetscInt)*ctx->nr,&ctx->row_len);CHKERRQ(ierr);
  ierr = PetscMalloc(sizeof(PetscInt)*ctx->nc,&ctx->col_len);CHKERRQ(ierr);
  for (i=0;i<ctx->nr;i++) ctx->row_len[i]=-1;
  for (j=0;j<ctx->nc;j++) ctx->col_len[j]=-1;

  ctx->setup_called = PETSC_TRUE;

  PetscFunctionReturn(0);
}


/* If an IS was provided, there is nothing Nest needs to do, otherwise Nest will build a strided IS */
/*
  nprocessors = NP
  Nest x^T = ( (g_0,g_1,...g_nprocs-1), (h_0,h_1,...h_NP-1) )
       proc 0: => (g_0,h_0,)
       proc 1: => (g_1,h_1,)
       ...
       proc nprocs-1: => (g_NP-1,h_NP-1,)

            proc 0:                      proc 1:                    proc nprocs-1:
    is[0] = ( 0,1,2,...,nlocal(g_0)-1 )  ( 0,1,...,nlocal(g_1)-1 )  ( 0,1,...,nlocal(g_NP-1) )

            proc 0:
    is[1] = ( nlocal(g_0),nlocal(g_0)+1,...,nlocal(g_0)+nlocal(h_0)-1 )
            proc 1:
    is[1] = ( nlocal(g_1),nlocal(g_1)+1,...,nlocal(g_1)+nlocal(h_1)-1 )

            proc NP-1:
    is[1] = ( nlocal(g_NP-1),nlocal(g_NP-1)+1,...,nlocal(g_NP-1)+nlocal(h_NP-1)-1 )
*/
#undef __FUNCT__  
#define __FUNCT__ "MatSetUp_NestIS_Private"
static PetscErrorCode MatSetUp_NestIS_Private(Mat A,PetscInt nr,const IS is_row[],PetscInt nc,const IS is_col[])
{
  Mat_Nest       *vs = (Mat_Nest*)A->data;
  PetscInt       i,j,offset,n,bs;
  PetscErrorCode ierr;
  Mat            sub;

  PetscFunctionBegin;
  if (is_row) { /* valid IS is passed in */
    /* refs on is[] are incremeneted */
    for (i=0; i<vs->nr; i++) {
      ierr = PetscObjectReference((PetscObject)is_row[i]);CHKERRQ(ierr);
      vs->isglobal.row[i] = is_row[i];
    }
  } else {                      /* Create the ISs by inspecting sizes of a submatrix in each row */
    offset = A->rmap->rstart;
    for (i=0; i<vs->nr; i++) {
      ierr = MatNestFindNonzeroSubMatRow(A,i,&sub);CHKERRQ(ierr);
      ierr = MatGetLocalSize(sub,&n,PETSC_NULL);CHKERRQ(ierr);
      if (n < 0) SETERRQ(((PetscObject)A)->comm,PETSC_ERR_ARG_WRONGSTATE,"Sizes have not yet been set for submatrix");
      ierr = MatGetBlockSize(sub,&bs);CHKERRQ(ierr);
      ierr = ISCreateStride(((PetscObject)sub)->comm,n,offset,1,&vs->isglobal.row[i]);CHKERRQ(ierr);
      ierr = ISSetBlockSize(vs->isglobal.row[i],bs);CHKERRQ(ierr);
      offset += n;
    }
  }

  if (is_col) { /* valid IS is passed in */
    /* refs on is[] are incremeneted */
    for (j=0; j<vs->nc; j++) {
      ierr = PetscObjectReference((PetscObject)is_col[j]);CHKERRQ(ierr);
      vs->isglobal.col[j] = is_col[j];
    }
  } else {                      /* Create the ISs by inspecting sizes of a submatrix in each column */
    offset = A->cmap->rstart;
    for (j=0; j<vs->nc; j++) {
      ierr = MatNestFindNonzeroSubMatCol(A,j,&sub);CHKERRQ(ierr);
      ierr = MatGetLocalSize(sub,PETSC_NULL,&n);CHKERRQ(ierr);
      if (n < 0) SETERRQ(((PetscObject)A)->comm,PETSC_ERR_ARG_WRONGSTATE,"Sizes have not yet been set for submatrix");
      ierr = MatGetBlockSize(sub,&bs);CHKERRQ(ierr);
      ierr = ISCreateStride(((PetscObject)sub)->comm,n,offset,1,&vs->isglobal.col[j]);CHKERRQ(ierr);
      ierr = ISSetBlockSize(vs->isglobal.col[j],bs);CHKERRQ(ierr);
      offset += n;
    }
  }

  /* Set up the local ISs */
  ierr = PetscMalloc(vs->nr*sizeof(IS),&vs->islocal.row);CHKERRQ(ierr);
  ierr = PetscMalloc(vs->nc*sizeof(IS),&vs->islocal.col);CHKERRQ(ierr);
  for (i=0,offset=0; i<vs->nr; i++) {
    IS                     isloc;
    ISLocalToGlobalMapping rmap;
    PetscInt               nlocal,bs;
    ierr = MatNestFindNonzeroSubMatRow(A,i,&sub);CHKERRQ(ierr);
    ierr = MatGetLocalToGlobalMapping(sub,&rmap,PETSC_NULL);CHKERRQ(ierr);
    if (rmap) {
      ierr = MatGetBlockSize(sub,&bs);CHKERRQ(ierr);
      ierr = ISLocalToGlobalMappingGetSize(rmap,&nlocal);CHKERRQ(ierr);
      ierr = ISCreateStride(PETSC_COMM_SELF,nlocal,offset,1,&isloc);CHKERRQ(ierr);
      ierr = ISSetBlockSize(isloc,bs);CHKERRQ(ierr);
    } else {
      nlocal = 0;
      isloc  = PETSC_NULL;
    }
    vs->islocal.row[i] = isloc;
    offset += nlocal;
  }
  for (i=0,offset=0; i<vs->nr; i++) {
    IS                     isloc;
    ISLocalToGlobalMapping cmap;
    PetscInt               nlocal,bs;
    ierr = MatNestFindNonzeroSubMatCol(A,i,&sub);CHKERRQ(ierr);
    ierr = MatGetLocalToGlobalMapping(sub,PETSC_NULL,&cmap);CHKERRQ(ierr);
    if (cmap) {
      ierr = MatGetBlockSize(sub,&bs);CHKERRQ(ierr);
      ierr = ISLocalToGlobalMappingGetSize(cmap,&nlocal);CHKERRQ(ierr);
      ierr = ISCreateStride(PETSC_COMM_SELF,nlocal,offset,1,&isloc);CHKERRQ(ierr);
      ierr = ISSetBlockSize(isloc,bs);CHKERRQ(ierr);
    } else {
      nlocal = 0;
      isloc  = PETSC_NULL;
    }
    vs->islocal.col[i] = isloc;
    offset += nlocal;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatCreateNest"
PetscErrorCode  MatCreateNest(MPI_Comm comm,PetscInt nr,const IS is_row[],PetscInt nc,const IS is_col[],const Mat a[],Mat *B)
{
  Mat            A;
  Mat_Nest       *s;
  PetscInt       i,m,n,M,N;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (nr < 0) SETERRQ(comm,PETSC_ERR_ARG_OUTOFRANGE,"Number of rows cannot be negative");
  if (nr && is_row) {
    PetscValidPointer(is_row,3);
    for (i=0; i<nr; i++) PetscValidHeaderSpecific(is_row[i],IS_CLASSID,3);
  }
  if (nc < 0) SETERRQ(comm,PETSC_ERR_ARG_OUTOFRANGE,"Number of columns cannot be negative");
  if (nc && is_row) {
    PetscValidPointer(is_col,5);
    for (i=0; i<nr; i++) PetscValidHeaderSpecific(is_col[i],IS_CLASSID,5);
  }
  if (nr*nc) PetscValidPointer(a,6);
  PetscValidPointer(B,7);

  ierr = MatCreate(comm,&A);CHKERRQ(ierr);

  ierr = PetscMalloc( sizeof(Mat_Nest), &s );CHKERRQ(ierr);
  ierr = PetscMemzero( s, sizeof(Mat_Nest) );CHKERRQ(ierr);
  A->data         = (void*)s;
  s->setup_called = PETSC_FALSE;
  s->nr = s->nc   = -1;
  s->m            = PETSC_NULL;

  /* define the operations */
  ierr = MatNestSetOps_Private(A->ops);CHKERRQ(ierr);
  A->spptr            = 0;
  A->same_nonzero     = PETSC_FALSE;
  A->assembled        = PETSC_FALSE;

  ierr = PetscObjectChangeTypeName((PetscObject) A, MATNEST );CHKERRQ(ierr);
  ierr = MatSetUp_Nest_Private(A,nr,nc,a);CHKERRQ(ierr);

  m = n = 0;
  M = N = 0;
  ierr = MatNestGetSize_Recursive(A,PETSC_TRUE,&M,&N);CHKERRQ(ierr);
  ierr = MatNestGetSize_Recursive(A,PETSC_FALSE,&m,&n);CHKERRQ(ierr);

  ierr = PetscLayoutSetSize(A->rmap,M);CHKERRQ(ierr);
  ierr = PetscLayoutSetLocalSize(A->rmap,m);CHKERRQ(ierr);
  ierr = PetscLayoutSetBlockSize(A->rmap,1);CHKERRQ(ierr);
  ierr = PetscLayoutSetSize(A->cmap,N);CHKERRQ(ierr);
  ierr = PetscLayoutSetLocalSize(A->cmap,n);CHKERRQ(ierr);
  ierr = PetscLayoutSetBlockSize(A->cmap,1);CHKERRQ(ierr);

  ierr = PetscLayoutSetUp(A->rmap);CHKERRQ(ierr);
  ierr = PetscLayoutSetUp(A->cmap);CHKERRQ(ierr);

  ierr = MatSetUp_NestIS_Private(A,nr,is_row,nc,is_col);CHKERRQ(ierr);
  ierr = PetscMalloc2(nr,Vec,&s->left,nc,Vec,&s->right);CHKERRQ(ierr);

  /* expose Nest api's */
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)A,"MatNestGetSubMat_C","MatNestGetSubMat_Nest",MatNestGetSubMat_Nest);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)A,"MatNestGetSubMats_C","MatNestGetSubMats_Nest",MatNestGetSubMats_Nest);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)A,"MatNestGetSize_C","MatNestGetSize_Nest",MatNestGetSize_Nest);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)A,"MatNestSetVecType_C","MatNestSetVecType_Nest",MatNestSetVecType_Nest);CHKERRQ(ierr);

  *B = A;
  PetscFunctionReturn(0);
}
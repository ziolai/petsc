/*$Id: baijfact3.c,v 1.6 2001/08/06 21:15:36 bsmith Exp $*/
/*
    Factorization code for BAIJ format. 
*/
#include "src/mat/impls/baij/seq/baij.h"
#include "src/vec/vecimpl.h"
#include "src/inline/ilu.h"

/*
    The symbolic factorization code is identical to that for AIJ format,
  except for very small changes since this is now a SeqBAIJ datastructure.
  NOT good code reuse.
*/
#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorSymbolic_SeqBAIJ"
int MatLUFactorSymbolic_SeqBAIJ(Mat A,IS isrow,IS iscol,MatLUInfo *info,Mat *B)
{
  Mat_SeqBAIJ *a = (Mat_SeqBAIJ*)A->data,*b;
  IS          isicol;
  int         *r,*ic,ierr,i,n = a->mbs,*ai = a->i,*aj = a->j;
  int         *ainew,*ajnew,jmax,*fill,*ajtmp,nz,bs = a->bs,bs2=a->bs2;
  int         *idnew,idx,row,m,fm,nnz,nzi,realloc = 0,nzbd,*im;
  PetscReal   f = 1.0;

  PetscFunctionBegin;
  if (A->M != A->N) SETERRQ(PETSC_ERR_ARG_WRONG,"matrix must be square");
  ierr = ISInvertPermutation(iscol,PETSC_DECIDE,&isicol);CHKERRQ(ierr);
  ierr = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISGetIndices(isicol,&ic);CHKERRQ(ierr);

  f = info->fill;
  /* get new row pointers */
  ierr     = PetscMalloc((n+1)*sizeof(int),&ainew);CHKERRQ(ierr);
  ainew[0] = 0;
  /* don't know how many column pointers are needed so estimate */
  jmax     = (int)(f*ai[n] + 1);
  ierr     = PetscMalloc((jmax)*sizeof(int),&ajnew);CHKERRQ(ierr);
  /* fill is a linked list of nonzeros in active row */
  ierr     = PetscMalloc((2*n+1)*sizeof(int),&fill);CHKERRQ(ierr);
  im       = fill + n + 1;
  /* idnew is location of diagonal in factor */
  ierr     = PetscMalloc((n+1)*sizeof(int),&idnew);CHKERRQ(ierr);
  idnew[0] = 0;

  for (i=0; i<n; i++) {
    /* first copy previous fill into linked list */
    nnz     = nz    = ai[r[i]+1] - ai[r[i]];
    if (!nz) SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Empty row in matrix");
    ajtmp   = aj + ai[r[i]];
    fill[n] = n;
    while (nz--) {
      fm  = n;
      idx = ic[*ajtmp++];
      do {
        m  = fm;
        fm = fill[m];
      } while (fm < idx);
      fill[m]   = idx;
      fill[idx] = fm;
    }
    row = fill[n];
    while (row < i) {
      ajtmp = ajnew + idnew[row] + 1;
      nzbd  = 1 + idnew[row] - ainew[row];
      nz    = im[row] - nzbd;
      fm    = row;
      while (nz-- > 0) {
        idx = *ajtmp++;
        nzbd++;
        if (idx == i) im[row] = nzbd;
        do {
          m  = fm;
          fm = fill[m];
        } while (fm < idx);
        if (fm != idx) {
          fill[m]   = idx;
          fill[idx] = fm;
          fm        = idx;
          nnz++;
        }
      }
      row = fill[row];
    }
    /* copy new filled row into permanent storage */
    ainew[i+1] = ainew[i] + nnz;
    if (ainew[i+1] > jmax) {

      /* estimate how much additional space we will need */
      /* use the strategy suggested by David Hysom <hysom@perch-t.icase.edu> */
      /* just double the memory each time */
      int maxadd = jmax;
      /* maxadd = (int)((f*(ai[n]+1)*(n-i+5))/n); */
      if (maxadd < nnz) maxadd = (n-i)*(nnz+1);
      jmax += maxadd;

      /* allocate a longer ajnew */
      ierr  = PetscMalloc(jmax*sizeof(int),&ajtmp);CHKERRQ(ierr);
      ierr  = PetscMemcpy(ajtmp,ajnew,ainew[i]*sizeof(int));CHKERRQ(ierr);
      ierr  = PetscFree(ajnew);CHKERRQ(ierr);
      ajnew = ajtmp;
      realloc++; /* count how many times we realloc */
    }
    ajtmp = ajnew + ainew[i];
    fm    = fill[n];
    nzi   = 0;
    im[i] = nnz;
    while (nnz--) {
      if (fm < i) nzi++;
      *ajtmp++ = fm;
      fm       = fill[fm];
    }
    idnew[i] = ainew[i] + nzi;
  }

  if (ai[n] != 0) {
    PetscReal af = ((PetscReal)ainew[n])/((PetscReal)ai[n]);
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqBAIJ:Reallocs %d Fill ratio:given %g needed %g\n",realloc,f,af);
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqBAIJ:Run with -pc_lu_fill %g or use \n",af);
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqBAIJ:PCLUSetFill(pc,%g);\n",af);
    PetscLogInfo(A,"MatLUFactorSymbolic_SeqBAIJ:for best performance.\n");
  } else {
     PetscLogInfo(A,"MatLUFactorSymbolic_SeqBAIJ:Empty matrix.\n");
  }

  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);

  ierr = PetscFree(fill);CHKERRQ(ierr);

  /* put together the new matrix */
  ierr = MatCreateSeqBAIJ(A->comm,bs,bs*n,bs*n,0,PETSC_NULL,B);CHKERRQ(ierr);
  PetscLogObjectParent(*B,isicol); 
  b = (Mat_SeqBAIJ*)(*B)->data;
  ierr = PetscFree(b->imax);CHKERRQ(ierr);
  b->singlemalloc = PETSC_FALSE;
  /* the next line frees the default space generated by the Create() */
  ierr = PetscFree(b->a);CHKERRQ(ierr);
  ierr = PetscFree(b->ilen);CHKERRQ(ierr);
  ierr          = PetscMalloc((ainew[n]+1)*sizeof(MatScalar)*bs2,&b->a);CHKERRQ(ierr);
  b->j          = ajnew;
  b->i          = ainew;
  b->diag       = idnew;
  b->ilen       = 0;
  b->imax       = 0;
  b->row        = isrow;
  b->col        = iscol;
  b->pivotinblocks = (info->pivotinblocks) ? PETSC_TRUE : PETSC_FALSE;
  ierr          = PetscObjectReference((PetscObject)isrow);CHKERRQ(ierr);
  ierr          = PetscObjectReference((PetscObject)iscol);CHKERRQ(ierr);
  b->icol       = isicol;
  ierr = PetscMalloc((bs*n+bs)*sizeof(PetscScalar),&b->solve_work);CHKERRQ(ierr);
  /* In b structure:  Free imax, ilen, old a, old j.  
     Allocate idnew, solve_work, new a, new j */
  PetscLogObjectMemory(*B,(ainew[n]-n)*(sizeof(int)+sizeof(MatScalar)));
  b->maxnz = b->nz = ainew[n];
  
  (*B)->factor                 = FACTOR_LU;
  (*B)->info.factor_mallocs    = realloc;
  (*B)->info.fill_ratio_given  = f;
  if (ai[n] != 0) {
    (*B)->info.fill_ratio_needed = ((PetscReal)ainew[n])/((PetscReal)ai[n]);
  } else {
    (*B)->info.fill_ratio_needed = 0.0;
  }
  PetscFunctionReturn(0); 
}

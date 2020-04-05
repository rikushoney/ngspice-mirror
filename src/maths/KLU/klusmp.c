/*
 *  Spice3 COMPATIBILITY MODULE
 *
 *  Author:                     Advising professor:
 *     Kenneth S. Kundert           Alberto Sangiovanni-Vincentelli
 *     UC Berkeley
 *
 *  This module contains routines that make Sparse1.3 a direct
 *  replacement for the SMP sparse matrix package in Spice3c1 or Spice3d1.
 *  Sparse1.3 is in general a faster and more robust package than SMP.
 *  These advantages become significant on large circuits.
 *
 *  >>> User accessible functions contained in this file:
 *  SMPaddElt
 *  SMPmakeElt
 *  SMPcClear
 *  SMPclear
 *  SMPcLUfac
 *  SMPluFac
 *  SMPcReorder
 *  SMPreorder
 *  SMPcaSolve
 *  SMPcSolve
 *  SMPsolve
 *  SMPmatSize
 *  SMPnewMatrix
 *  SMPdestroy
 *  SMPpreOrder
 *  SMPprint
 *  SMPgetError
 *  SMPcProdDiag
 *  LoadGmin
 *  SMPfindElt
 *  SMPcombine
 *  SMPcCombine
 */

/*
 *  To replace SMP with Sparse, rename the file spSpice3.h to
 *  spMatrix.h and place Sparse in a subdirectory of SPICE called
 *  `sparse'.  Then on UNIX compile Sparse by executing `make spice'.
 *  If not on UNIX, after compiling Sparse and creating the sparse.a
 *  archive, compile this file (spSMP.c) and spSMP.o to the archive,
 *  then copy sparse.a into the SPICE main directory and rename it
 *  SMP.a.  Finally link SPICE.
 *
 *  To be compatible with SPICE, the following Sparse compiler options
 *  (in spConfig.h) should be set as shown below:
 *
 *      EXPANDABLE                      YES
 *      TRANSLATE                       NO
 *      INITIALIZE                      NO or YES, YES for use with test prog.
 *      DIAGONAL_PIVOTING               YES
 *      MODIFIED_MARKOWITZ              NO
 *      DELETE                          NO
 *      STRIP                           NO
 *      MODIFIED_NODAL                  YES
 *      QUAD_ELEMENT                    NO
 *      TRANSPOSE                       YES
 *      SCALING                         NO
 *      DOCUMENTATION                   YES
 *      MULTIPLICATION                  NO
 *      DETERMINANT                     YES
 *      STABILITY                       NO
 *      CONDITION                       NO
 *      PSEUDOCONDITION                 NO
 *      DEBUG                           YES
 *
 *      spREAL  double
 */

/*
 *  Revision and copyright information.
 *
 *  Copyright (c) 1985,86,87,88,89,90
 *  by Kenneth S. Kundert and the University of California.
 *
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and supporting
 *  documentation and that the authors and the University of California
 *  are properly credited.  The authors and the University of California
 *  make no representations as to the suitability of this software for
 *  any purpose.  It is provided `as is', without express or implied warranty.
 */

/*
 *  IMPORTS
 *
 *  >>> Import descriptions:
 *  spMatrix.h
 *     Sparse macros and declarations.
 *  SMPdefs.h
 *     Spice3's matrix macro definitions.
 */

#include "ngspice/config.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "ngspice/spmatrix.h"
#include "../sparse/spdefs.h"
#include "ngspice/smpdefs.h"

#if defined (_MSC_VER)
extern double scalbn(double, int);
#define logb _logb
extern double logb(double);
#endif

static void LoadGmin_CSC (double **diag, int n, double Gmin) ;
static void LoadGmin (SMPmatrix *eMatrix, double Gmin) ;

void
SMPmatrix_CSC (SMPmatrix *Matrix)
{
    spMatrix_CSC (Matrix->SPmatrix, Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx,
                  Matrix->CKTkluAx_Complex, Matrix->CKTkluN, Matrix->CKTbindStruct, Matrix->CKTdiag_CSC) ;

//    spMatrix_CSC_dump (Matrix->SPmatrix, 1, NULL) ;

    return ;
}

void
SMPnnz (SMPmatrix *Matrix)
{
    Matrix->CKTklunz = Matrix->SPmatrix->Elements ;

    return ;
}

#ifdef CIDER
typedef struct sElement {
  unsigned int row ;
  unsigned int col ;
  double *pointer ;
} Element ;

static int
CompareRow (const void *a, const void *b)
{
    Element *A = (Element *) a ;
    Element *B = (Element *) b ;

    return
        (A->row > B->row) ?  1 :
        (A->row < B->row) ? -1 :
        0 ;
}

static int
CompareColumn (const void *a, const void *b)
{
    Element *A = (Element *) a ;
    Element *B = (Element *) b ;

    return
        (A->col > B->col) ?  1 :
        (A->col < B->col) ? -1 :
        0 ;
}

static void
Compress (unsigned int *Ai, unsigned int *Bp, unsigned int n, unsigned int nz)
{
    unsigned int i, j ;

    for (i = 0 ; i <= Ai [0] ; i++)
        Bp [i] = 0 ;

    j = Ai [0] + 1 ;
    for (i = 1 ; i < nz ; i++)
    {
        if (Ai [i] == Ai [i - 1] + 1)
        {
            Bp [j] = i ;
            j++ ;
        }
        else if (Ai [i] > Ai [i - 1] + 1)
        {
            for ( ; j <= Ai [i] ; j++)
                Bp [j] = i ;
        }
    }

    for ( ; j <= n ; j++)
        Bp [j] = i ;
}

int
BindKluCompareCOO (const void *a, const void *b)
{
    BindKluElementCOO *A = (BindKluElementCOO *) a ;
    BindKluElementCOO *B = (BindKluElementCOO *) b ;

    return
        (A->COO > B->COO) ?  1 :
        (A->COO < B->COO) ? -1 :
        0 ;
}

int
BindKluCompareCSC (const void *a, const void *b)
{
    BindKluElementCOO *A = (BindKluElementCOO *) a ;
    BindKluElementCOO *B = (BindKluElementCOO *) b ;

    return
        (A->CSC_Complex > B->CSC_Complex) ?  1 :
        (A->CSC_Complex < B->CSC_Complex) ? -1 :
        0 ;
}

void SMPconvertCOOtoCSCKLUforCIDER (SMPmatrix *Matrix)
{
    Element *MatrixCOO ;
    unsigned int *Ap_COO, i, j, nz ;

    /* Count the non-zero elements and store it */
    nz = 0 ;
    for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixN * Matrix->SMPkluMatrix->KLUmatrixN ; i++) {
        if ((Matrix->SMPkluMatrix->KLUmatrixRowCOO [i] != -1) && (Matrix->SMPkluMatrix->KLUmatrixColCOO [i] != -1)) {
            nz++ ;
        }
    }
    Matrix->SMPkluMatrix->KLUmatrixNZ = nz ;

    /* Allocate the compressed COO elements */
    MatrixCOO = (Element *) malloc (nz * sizeof(Element)) ;

    /* Allocate the temporary COO Column Index */
    Ap_COO = (unsigned int *) malloc (nz * sizeof(unsigned int)) ;

    /* Allocate the needed KLU data structures */
    Matrix->SMPkluMatrix->KLUmatrixAp = (int *) malloc ((Matrix->SMPkluMatrix->KLUmatrixN + 1) * sizeof(int)) ;
    Matrix->SMPkluMatrix->KLUmatrixAi = (int *) malloc (nz * sizeof(int)) ;
    Matrix->SMPkluMatrix->KLUmatrixBindStructCOO = (BindKluElementCOO *) malloc (nz * sizeof(BindKluElementCOO)) ;
    Matrix->SMPkluMatrix->KLUmatrixAxComplex = (double *) malloc (2 * nz * sizeof(double)) ;
    Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex = (double *) malloc (2 * Matrix->SMPkluMatrix->KLUmatrixN * sizeof(double)) ;

    /* Populate the compressed COO elements and COO value of Binding Table */
    j = 0 ;
    for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixN * Matrix->SMPkluMatrix->KLUmatrixN ; i++) {
        if ((Matrix->SMPkluMatrix->KLUmatrixRowCOO [i] != -1) && (Matrix->SMPkluMatrix->KLUmatrixColCOO [i] != -1)) {
            MatrixCOO [j].row = (unsigned int)Matrix->SMPkluMatrix->KLUmatrixRowCOO [i] ;
            MatrixCOO [j].col = (unsigned int)Matrix->SMPkluMatrix->KLUmatrixColCOO [i] ;
            MatrixCOO [j].pointer = &(Matrix->SMPkluMatrix->KLUmatrixValueComplexCOO [2 * i]) ;
            j++ ;
        }
    }

    /* Order the MatrixCOO along the columns */
    qsort (MatrixCOO, nz, sizeof(Element), CompareColumn) ;

    /* Order the MatrixCOO along the rows */
    i = 0 ;
    while (i < nz)
    {
        /* Look for the next column */
        for (j = i + 1 ; j < nz ; j++)
        {
            if (MatrixCOO [j].col != MatrixCOO [i].col)
            {
                break ;
            }
        }

        qsort (MatrixCOO + i, j - i, sizeof(Element), CompareRow) ;

        i = j ;
    }

    /* Copy back the Matrix in partial CSC */
    for (i = 0 ; i < nz ; i++)
    {
        Ap_COO [i] = MatrixCOO [i].col ;
        Matrix->SMPkluMatrix->KLUmatrixAi [i] = (int)MatrixCOO [i].row ;
        Matrix->SMPkluMatrix->KLUmatrixBindStructCOO [i].COO = MatrixCOO [i].pointer ;
        Matrix->SMPkluMatrix->KLUmatrixBindStructCOO [i].CSC_Complex = &(Matrix->SMPkluMatrix->KLUmatrixAxComplex [2 * i]) ;
    }

    /* Compress the COO Column Index to CSC Column Index */
    Compress (Ap_COO, (unsigned int *)Matrix->SMPkluMatrix->KLUmatrixAp, Matrix->SMPkluMatrix->KLUmatrixN, nz) ;

    /* Free the temporary stuff */
    free (Ap_COO) ;
    free (MatrixCOO) ;

    /* Sort the Binding Table */
    qsort (Matrix->SMPkluMatrix->KLUmatrixBindStructCOO, nz, sizeof(BindKluElementCOO), BindKluCompareCOO) ;

    return ;
}
#endif

/*
 * SMPaddElt()
 */
int
SMPaddElt (SMPmatrix *Matrix, int Row, int Col, double Value)
{
    *spGetElement (Matrix->SPmatrix, Row, Col) = Value ;
    return spError (Matrix->SPmatrix) ;
}

/*
 * SMPmakeElt()
 */
double *
SMPmakeElt (SMPmatrix *Matrix, int Row, int Col)
{
    return spGetElement (Matrix->SPmatrix, Row, Col) ;
}

#ifdef CIDER
double *
SMPmakeEltKLUforCIDER (SMPmatrix *Matrix, int Row, int Col)
{
    if (Matrix->CKTkluMODE) {
        if ((Row > 0) && (Col > 0)) {
            Row = Row - 1 ;
            Col = Col - 1 ;
            Matrix->SMPkluMatrix->KLUmatrixRowCOO [Row * (int)Matrix->SMPkluMatrix->KLUmatrixN + Col] = Row ;
            Matrix->SMPkluMatrix->KLUmatrixColCOO [Row * (int)Matrix->SMPkluMatrix->KLUmatrixN + Col] = Col ;
            return &(Matrix->SMPkluMatrix->KLUmatrixValueComplexCOO [2 * (Row * (int)Matrix->SMPkluMatrix->KLUmatrixN + Col)]) ;
        } else {
            return Matrix->SMPkluMatrix->KLUmatrixTrashCOO ;
        }
    } else {
        return spGetElement (Matrix->SPmatrix, Row, Col) ;
    }
}
#endif

/*
 * SMPcClear()
 */

void
SMPcClear (SMPmatrix *Matrix)
{
    int i ;
    if (Matrix->CKTkluMODE)
    {
        spClear (Matrix->SPmatrix) ;
        if (Matrix->CKTkluAx_Complex != NULL)
        {
            for (i = 0 ; i < 2 * Matrix->CKTklunz ; i++)
                Matrix->CKTkluAx_Complex [i] = 0 ;
        }
    } else {
        spClear (Matrix->SPmatrix) ;
    }
}

/*
 * SMPclear()
 */

void
SMPclear (SMPmatrix *Matrix)
{
    int i ;
    if (Matrix->CKTkluMODE)
    {
        spClear (Matrix->SPmatrix) ;
        if (Matrix->CKTkluAx != NULL)
        {
            for (i = 0 ; i < Matrix->CKTklunz ; i++)
                Matrix->CKTkluAx [i] = 0 ;
        }
    } else {
        spClear (Matrix->SPmatrix) ;
    }
}

#ifdef CIDER
void
SMPclearKLUforCIDER (SMPmatrix *Matrix)
{
    unsigned int i ;

    for (i = 0 ; i < 2 * Matrix->SMPkluMatrix->KLUmatrixNZ ; i++) {
        Matrix->SMPkluMatrix->KLUmatrixAxComplex [i] = 0 ;
    }
}
#endif

#define NG_IGNORE(x)  (void)x

/*
 * SMPcLUfac()
 */
/*ARGSUSED*/

int
SMPcLUfac (SMPmatrix *Matrix, double PivTol)
{
    int ret ;

    NG_IGNORE (PivTol) ;

    if (Matrix->CKTkluMODE)
    {
        spSetComplex (Matrix->SPmatrix) ;
        ret = klu_z_refactor (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx_Complex,
                              Matrix->CKTkluSymbolic, Matrix->CKTkluNumeric, Matrix->CKTkluCommon) ;

        if (Matrix->CKTkluCommon->status == KLU_EMPTY_MATRIX)
        {
            return 0 ;
        }
        return (!ret) ;
    } else {
        spSetComplex (Matrix->SPmatrix) ;
        return spFactor (Matrix->SPmatrix) ;
    }
}

/*
 * SMPluFac()
 */
/*ARGSUSED*/

int
SMPluFac (SMPmatrix *Matrix, double PivTol, double Gmin)
{
    int ret ;

    NG_IGNORE (PivTol) ;

    if (Matrix->CKTkluMODE)
    {
        spSetReal (Matrix->SPmatrix) ;
        LoadGmin_CSC (Matrix->CKTdiag_CSC, Matrix->CKTkluN, Gmin) ;
        ret = klu_refactor (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx,
                            Matrix->CKTkluSymbolic, Matrix->CKTkluNumeric, Matrix->CKTkluCommon) ;

        if (Matrix->CKTkluCommon->status == KLU_EMPTY_MATRIX)
        {
            return 0 ;
        }
        return (!ret) ;

//        if (ret == 1)
//            return 0 ;
//        else if (ret == 0)
//            return (E_SINGULAR) ;
//        else {
//            fprintf (stderr, "KLU Error in re-factor!") ;
//            return 1 ;
//        }
    } else {
        spSetReal (Matrix->SPmatrix) ;
        LoadGmin (Matrix, Gmin) ;
        return spFactor (Matrix->SPmatrix) ;
    }
}

#ifdef CIDER
int
SMPluFacKLUforCIDER (SMPmatrix *Matrix)
{
    unsigned int i ;
    double *KLUmatrixAx ;
    int ret ;

    if (Matrix->CKTkluMODE)
    {
        if (Matrix->SMPkluMatrix->KLUmatrixIsComplex) {
            ret = klu_z_refactor (Matrix->SMPkluMatrix->KLUmatrixAp, Matrix->SMPkluMatrix->KLUmatrixAi, Matrix->SMPkluMatrix->KLUmatrixAxComplex,
                                  Matrix->SMPkluMatrix->KLUmatrixSymbolic, Matrix->SMPkluMatrix->KLUmatrixNumeric, Matrix->SMPkluMatrix->KLUmatrixCommon) ;
        } else {
            /* Allocate the Real Matrix */
            KLUmatrixAx = (double *) malloc (Matrix->SMPkluMatrix->KLUmatrixNZ * sizeof(double)) ;

            /* Copy the Complex Matrix into the Real Matrix */
            for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixNZ ; i++) {
                KLUmatrixAx [i] = Matrix->SMPkluMatrix->KLUmatrixAxComplex [2 * i] ;
            }

            /* Re-Factor the Real Matrix */
            ret = klu_refactor (Matrix->SMPkluMatrix->KLUmatrixAp, Matrix->SMPkluMatrix->KLUmatrixAi, KLUmatrixAx,
                                Matrix->SMPkluMatrix->KLUmatrixSymbolic, Matrix->SMPkluMatrix->KLUmatrixNumeric, Matrix->SMPkluMatrix->KLUmatrixCommon) ;

            /* Free the Real Matrix Storage */
            free (KLUmatrixAx) ;
        }
        if (Matrix->SMPkluMatrix->KLUmatrixCommon->status == KLU_EMPTY_MATRIX) {
            printf ("CIDER: KLU Empty Matrix\n") ;
            return 0 ;
        }
        return (!ret) ;
    } else {
        return spFactor (Matrix->SPmatrix) ;
    }
}
#endif

/*
 * SMPcReorder()
 */

int
SMPcReorder (SMPmatrix *Matrix, double PivTol, double PivRel, int *NumSwaps)
{
    if (Matrix->CKTkluMODE)
    {
        *NumSwaps = 1 ;
        spSetComplex (Matrix->SPmatrix) ;
//        Matrix->CKTkluCommon->tol = PivTol ;

        if (Matrix->CKTkluNumeric != NULL)
        {
            klu_z_free_numeric (&(Matrix->CKTkluNumeric), Matrix->CKTkluCommon) ;
            Matrix->CKTkluNumeric = klu_z_factor (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx_Complex, Matrix->CKTkluSymbolic, Matrix->CKTkluCommon) ;
        } else
            Matrix->CKTkluNumeric = klu_z_factor (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx_Complex, Matrix->CKTkluSymbolic, Matrix->CKTkluCommon) ;

        if (Matrix->CKTkluNumeric == NULL)
        {
            if (Matrix->CKTkluCommon->status == KLU_EMPTY_MATRIX)
            {
                return 0 ;
            }
            return 1 ;
        }
        else
            return 0 ;
    } else {
        *NumSwaps = 1 ;
        spSetComplex (Matrix->SPmatrix) ;
        return spOrderAndFactor (Matrix->SPmatrix, NULL, (spREAL)PivRel, (spREAL)PivTol, YES) ;
    }
}

/*
 * SMPreorder()
 */

int
SMPreorder (SMPmatrix *Matrix, double PivTol, double PivRel, double Gmin)
{
    if (Matrix->CKTkluMODE)
    {
        spSetReal (Matrix->SPmatrix) ;
        LoadGmin_CSC (Matrix->CKTdiag_CSC, Matrix->CKTkluN, Gmin) ;
//        Matrix->CKTkluCommon->tol = PivTol ;

        if (Matrix->CKTkluNumeric != NULL)
        {
            klu_free_numeric (&(Matrix->CKTkluNumeric), Matrix->CKTkluCommon) ;
            Matrix->CKTkluNumeric = klu_factor (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx, Matrix->CKTkluSymbolic, Matrix->CKTkluCommon) ;
        } else
            Matrix->CKTkluNumeric = klu_factor (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx, Matrix->CKTkluSymbolic, Matrix->CKTkluCommon) ;

        if (Matrix->CKTkluNumeric == NULL)
        {
            if (Matrix->CKTkluCommon->status == KLU_EMPTY_MATRIX)
            {
                return 0 ;
            }
            return 1 ;
        }
        else
            return 0 ;
    } else {
        spSetReal (Matrix->SPmatrix) ;
        LoadGmin (Matrix, Gmin) ;
        return spOrderAndFactor (Matrix->SPmatrix, NULL, (spREAL)PivRel, (spREAL)PivTol, YES) ;
    }
}

#ifdef CIDER
int
SMPreorderKLUforCIDER (SMPmatrix *Matrix)
{
    unsigned int i ;
    double *KLUmatrixAx ;

    if (Matrix->CKTkluMODE)
    {
//        Matrix->CKTkluCommon->tol = PivTol ;

        if (Matrix->SMPkluMatrix->KLUmatrixNumeric != NULL) {
            klu_free_numeric (&(Matrix->SMPkluMatrix->KLUmatrixNumeric), Matrix->SMPkluMatrix->KLUmatrixCommon) ;
        }
        if (Matrix->SMPkluMatrix->KLUmatrixIsComplex) {
            Matrix->SMPkluMatrix->KLUmatrixNumeric = klu_z_factor (Matrix->SMPkluMatrix->KLUmatrixAp, Matrix->SMPkluMatrix->KLUmatrixAi,
                                                                   Matrix->SMPkluMatrix->KLUmatrixAxComplex, Matrix->SMPkluMatrix->KLUmatrixSymbolic,
                                                                   Matrix->SMPkluMatrix->KLUmatrixCommon) ;
        } else {
            /* Allocate the Real Matrix */
            KLUmatrixAx = (double *) malloc (Matrix->SMPkluMatrix->KLUmatrixNZ * sizeof(double)) ;

            /* Copy the Complex Matrix into the Real Matrix */
            for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixNZ ; i++) {
                KLUmatrixAx [i] = Matrix->SMPkluMatrix->KLUmatrixAxComplex [2 * i] ;
            }

            /* Factor the Real Matrix */
            Matrix->SMPkluMatrix->KLUmatrixNumeric = klu_factor (Matrix->SMPkluMatrix->KLUmatrixAp, Matrix->SMPkluMatrix->KLUmatrixAi,
                                                                 KLUmatrixAx, Matrix->SMPkluMatrix->KLUmatrixSymbolic,
                                                                 Matrix->SMPkluMatrix->KLUmatrixCommon) ;

            /* Free the Real Matrix Storage */
            free (KLUmatrixAx) ;
        }
        if (Matrix->SMPkluMatrix->KLUmatrixNumeric == NULL)
        {
            printf ("CIDER: KLU Factorization Error\n") ;
            if (Matrix->SMPkluMatrix->KLUmatrixCommon->status == KLU_EMPTY_MATRIX)
            {
                return 0 ;
            }
            return 1 ;
        }
        else
            return 0 ;
    } else {
        return spFactor (Matrix->SPmatrix) ;
    }
}
#endif

/*
 * SMPcaSolve()
 */
void
SMPcaSolve (SMPmatrix *Matrix, double RHS[], double iRHS[], double Spare[], double iSpare[])
{
    int ret, i, *pExtOrder ;

    NG_IGNORE (iSpare) ;
    NG_IGNORE (Spare) ;

    if (Matrix->CKTkluMODE)
    {
        pExtOrder = &Matrix->SPmatrix->IntToExtRowMap [Matrix->CKTkluN] ;
        for (i = 2 * Matrix->CKTkluN - 1 ; i > 0 ; i -= 2)
        {
            Matrix->CKTkluIntermediate_Complex [i] = iRHS [*(pExtOrder)] ;
            Matrix->CKTkluIntermediate_Complex [i - 1] = RHS [*(pExtOrder--)] ;
        }

        ret = klu_z_tsolve (Matrix->CKTkluSymbolic, Matrix->CKTkluNumeric, Matrix->CKTkluN, 1, Matrix->CKTkluIntermediate_Complex, 0, Matrix->CKTkluCommon) ;

        pExtOrder = &Matrix->SPmatrix->IntToExtColMap [Matrix->CKTkluN] ;
        for (i = 2 * Matrix->CKTkluN - 1 ; i > 0 ; i -= 2)
        {
            iRHS [*(pExtOrder)] = Matrix->CKTkluIntermediate_Complex [i] ;
            RHS [*(pExtOrder--)] = Matrix->CKTkluIntermediate_Complex [i - 1] ;
        }
    } else {
        spSolveTransposed (Matrix->SPmatrix, RHS, RHS, iRHS, iRHS) ;
    }
}

/*
 * SMPcSolve()
 */

void
SMPcSolve (SMPmatrix *Matrix, double RHS[], double iRHS[], double Spare[], double iSpare[])
{
    int ret, i, *pExtOrder ;

    NG_IGNORE (iSpare) ;
    NG_IGNORE (Spare) ;

    if (Matrix->CKTkluMODE)
    {
        pExtOrder = &Matrix->SPmatrix->IntToExtRowMap [Matrix->CKTkluN] ;
        for (i = 2 * Matrix->CKTkluN - 1 ; i > 0 ; i -= 2)
        {
            Matrix->CKTkluIntermediate_Complex [i] = iRHS [*(pExtOrder)] ;
            Matrix->CKTkluIntermediate_Complex [i - 1] = RHS [*(pExtOrder--)] ;
        }

        ret = klu_z_solve (Matrix->CKTkluSymbolic, Matrix->CKTkluNumeric, Matrix->CKTkluN, 1, Matrix->CKTkluIntermediate_Complex, Matrix->CKTkluCommon) ;

        pExtOrder = &Matrix->SPmatrix->IntToExtColMap [Matrix->CKTkluN] ;
        for (i = 2 * Matrix->CKTkluN - 1 ; i > 0 ; i -= 2)
        {
            iRHS [*(pExtOrder)] = Matrix->CKTkluIntermediate_Complex [i] ;
            RHS [*(pExtOrder--)] = Matrix->CKTkluIntermediate_Complex [i - 1] ;
        }

    } else {

        spSolve (Matrix->SPmatrix, RHS, RHS, iRHS, iRHS) ;
    }
}

#ifdef CIDER
void
SMPsolveKLUforCIDER (SMPmatrix *Matrix, double RHS[], double RHSsolution[], double iRHS[], double iRHSsolution[])
{
    int ret ;
    unsigned int i ;
    double *KLUmatrixIntermediate ;

    if (Matrix->CKTkluMODE)
    {
        if (Matrix->SMPkluMatrix->KLUmatrixIsComplex) {
            for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixN ; i++)
            {
                Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex [2 * i] = RHS [i + 1] ;
                Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex [2 * i + 1] = iRHS [i + 1] ;
            }

            ret = klu_z_solve (Matrix->SMPkluMatrix->KLUmatrixSymbolic, Matrix->SMPkluMatrix->KLUmatrixNumeric, (int)Matrix->SMPkluMatrix->KLUmatrixN, 1,
                               Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex, Matrix->SMPkluMatrix->KLUmatrixCommon) ;

            for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixN ; i++)
            {
                RHSsolution [i + 1] = Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex [2 * i] ;
                iRHSsolution [i + 1] = Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex [2 * i + 1] ;
            }
        } else {
            /* Allocate the Intermediate Vector */
            KLUmatrixIntermediate = (double *) malloc (Matrix->SMPkluMatrix->KLUmatrixN * sizeof(double)) ;

            for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixN ; i++) {
                KLUmatrixIntermediate [i] = RHS [i + 1] ;
            }

            ret = klu_solve (Matrix->SMPkluMatrix->KLUmatrixSymbolic, Matrix->SMPkluMatrix->KLUmatrixNumeric, (int)Matrix->SMPkluMatrix->KLUmatrixN, 1,
                             KLUmatrixIntermediate, Matrix->SMPkluMatrix->KLUmatrixCommon) ;

            for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixN ; i++) {
                RHSsolution [i + 1] = KLUmatrixIntermediate [i] ;
            }

            /* Free the Intermediate Vector */
            free (KLUmatrixIntermediate) ;
        }

    } else {

        spSolve (Matrix->SPmatrix, RHS, RHSsolution, iRHS, iRHSsolution) ;
    }
}
#endif

/*
 * SMPsolve()
 */

void
SMPsolve (SMPmatrix *Matrix, double RHS[], double Spare[])
{
    int ret, i, *pExtOrder ;

    NG_IGNORE (Spare) ;

    if (Matrix->CKTkluMODE) {

        pExtOrder = &Matrix->SPmatrix->IntToExtRowMap [Matrix->CKTkluN] ;
        for (i = Matrix->CKTkluN - 1 ; i >= 0 ; i--)
            Matrix->CKTkluIntermediate [i] = RHS [*(pExtOrder--)] ;

        ret = klu_solve (Matrix->CKTkluSymbolic, Matrix->CKTkluNumeric, Matrix->CKTkluN, 1, Matrix->CKTkluIntermediate, Matrix->CKTkluCommon) ;

        pExtOrder = &Matrix->SPmatrix->IntToExtColMap [Matrix->CKTkluN] ;
        for (i = Matrix->CKTkluN - 1 ; i >= 0 ; i--)
            RHS [*(pExtOrder--)] = Matrix->CKTkluIntermediate [i] ;
    } else {
        spSolve (Matrix->SPmatrix, RHS, RHS, NULL, NULL) ;
    }
}

/*
 * SMPmatSize()
 */
int
SMPmatSize (SMPmatrix *Matrix)
{
    return spGetSize (Matrix->SPmatrix, 1) ;
}

/*
 * SMPnewMatrix()
 */
int
SMPnewMatrix (SMPmatrix *Matrix, int size)
{
    int Error ;
    Matrix->SPmatrix = spCreate (size, 1, &Error) ;
    return Error ;
}

#ifdef CIDER
int
SMPnewMatrixKLUforCIDER (SMPmatrix *Matrix, int size, unsigned int KLUmatrixIsComplex)
{
    int Error ;
    unsigned int i ;

    if (Matrix->CKTkluMODE) {
        /* Allocate the KLU Matrix Data Structure */
        Matrix->SMPkluMatrix = (KLUmatrix *) malloc (sizeof (KLUmatrix)) ;

        /* Initialize the KLU Matrix Internal Pointers */
        Matrix->SMPkluMatrix->KLUmatrixCommon = (klu_common *) malloc (sizeof (klu_common)) ; ;
        Matrix->SMPkluMatrix->KLUmatrixSymbolic = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixNumeric = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixAp = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixAi = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixAxComplex = NULL ;
        if (KLUmatrixIsComplex) {
            Matrix->SMPkluMatrix->KLUmatrixIsComplex = CKTkluMatrixComplex ;
        } else {
            Matrix->SMPkluMatrix->KLUmatrixIsComplex = CKTkluMatrixReal ;
        }
        Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixNZ = 0 ;
        Matrix->SMPkluMatrix->KLUmatrixBindStructCOO = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixValueComplexCOO = NULL ;

        /* Initialize the KLU Common Data Structure */
        klu_defaults (Matrix->SMPkluMatrix->KLUmatrixCommon) ;

        /* Allocate KLU data structures */
        Matrix->SMPkluMatrix->KLUmatrixN = (unsigned int)size ;
        Matrix->SMPkluMatrix->KLUmatrixColCOO = (int *) malloc (Matrix->SMPkluMatrix->KLUmatrixN * Matrix->SMPkluMatrix->KLUmatrixN * sizeof(int)) ;
        Matrix->SMPkluMatrix->KLUmatrixRowCOO = (int *) malloc (Matrix->SMPkluMatrix->KLUmatrixN * Matrix->SMPkluMatrix->KLUmatrixN * sizeof(int)) ;
        Matrix->SMPkluMatrix->KLUmatrixTrashCOO = (double *) malloc (sizeof(double)) ;
        Matrix->SMPkluMatrix->KLUmatrixValueComplexCOO = (double *) malloc (2 * Matrix->SMPkluMatrix->KLUmatrixN * Matrix->SMPkluMatrix->KLUmatrixN * sizeof(double)) ;

        /* Pre-set the values of Row and Col */
        for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixN * Matrix->SMPkluMatrix->KLUmatrixN ; i++) {
            Matrix->SMPkluMatrix->KLUmatrixRowCOO [i] = -1 ;
            Matrix->SMPkluMatrix->KLUmatrixColCOO [i] = -1 ;
        }

        return spOKAY ;
    } else {
        Matrix->SPmatrix = spCreate (size, (int)KLUmatrixIsComplex, &Error) ;
        return Error ;
    }
}
#endif

/*
 * SMPdestroy()
 */

void
SMPdestroy (SMPmatrix *Matrix)
{
    spDestroy (Matrix->SPmatrix) ;

    if (Matrix->CKTkluMODE)
    {
        klu_free_numeric (&(Matrix->CKTkluNumeric), Matrix->CKTkluCommon) ;
        klu_free_symbolic (&(Matrix->CKTkluSymbolic), Matrix->CKTkluCommon) ;
        free (Matrix->CKTkluAp) ;
        free (Matrix->CKTkluAi) ;
        free (Matrix->CKTkluAx) ;
        free (Matrix->CKTkluIntermediate) ;
        free (Matrix->CKTbindStruct) ;
        free (Matrix->CKTdiag_CSC) ;
        free (Matrix->CKTkluAx_Complex) ;
        free (Matrix->CKTkluIntermediate_Complex) ;
        Matrix->CKTkluNumeric = NULL ;
        Matrix->CKTkluSymbolic = NULL ;
        Matrix->CKTkluAp = NULL ;
        Matrix->CKTkluAi = NULL ;
        Matrix->CKTkluAx = NULL ;
        Matrix->CKTkluIntermediate = NULL ;
        Matrix->CKTbindStruct = NULL ;
        Matrix->CKTdiag_CSC = NULL ;
        Matrix->CKTkluAx_Complex = NULL ;
        Matrix->CKTkluIntermediate_Complex = NULL ;
    }
}

#ifdef CIDER
void
SMPdestroyKLUforCIDER (SMPmatrix *Matrix)
{
    if (Matrix->CKTkluMODE)
    {
        klu_free_numeric (&(Matrix->SMPkluMatrix->KLUmatrixNumeric), Matrix->SMPkluMatrix->KLUmatrixCommon) ;
        klu_free_symbolic (&(Matrix->SMPkluMatrix->KLUmatrixSymbolic), Matrix->SMPkluMatrix->KLUmatrixCommon) ;
        free (Matrix->SMPkluMatrix->KLUmatrixAp) ;
        free (Matrix->SMPkluMatrix->KLUmatrixAi) ;
        free (Matrix->SMPkluMatrix->KLUmatrixAxComplex) ;
        free (Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex) ;
        free (Matrix->SMPkluMatrix->KLUmatrixBindStructCOO) ;
        free (Matrix->SMPkluMatrix->KLUmatrixColCOO) ;
        free (Matrix->SMPkluMatrix->KLUmatrixRowCOO) ;
        free (Matrix->SMPkluMatrix->KLUmatrixValueComplexCOO) ;
        free (Matrix->SMPkluMatrix->KLUmatrixTrashCOO) ;
        Matrix->SMPkluMatrix->KLUmatrixAp = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixAi = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixAxComplex = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixIntermediateComplex = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixBindStructCOO = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixColCOO = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixRowCOO = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixValueComplexCOO = NULL ;
        Matrix->SMPkluMatrix->KLUmatrixTrashCOO = NULL ;
    } else {
        spDestroy (Matrix->SPmatrix) ;
    }
}
#endif

/*
 * SMPpreOrder()
 */

int
SMPpreOrder (SMPmatrix *Matrix)
{
    if (Matrix->CKTkluMODE)
    {
        Matrix->CKTkluSymbolic = klu_analyze (Matrix->CKTkluN, Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluCommon) ;

        if (Matrix->CKTkluSymbolic == NULL)
        {
            if (Matrix->CKTkluCommon->status == KLU_EMPTY_MATRIX)
            {
                return 0 ;
            }
            return 1 ;
        } else {
            return 0 ;
        }
    } else {
        spMNA_Preorder (Matrix->SPmatrix) ;
        return spError (Matrix->SPmatrix) ;
    }
}

/*
 * SMPprintRHS()
 */

void
SMPprintRHS (SMPmatrix *Matrix, char *Filename, RealVector RHS, RealVector iRHS)
{
    if (!Matrix->CKTkluMODE)
        spFileVector (Matrix->SPmatrix, Filename, RHS, iRHS) ;
}

/*
 * SMPprint()
 */

void
SMPprint (SMPmatrix *Matrix, char *Filename)
{
    if (Matrix->CKTkluMODE)
    {
        if (Matrix->CKTkluMatrixIsComplex)
        {
            klu_z_print (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx_Complex, Matrix->CKTkluN, Matrix->SPmatrix->IntToExtRowMap, Matrix->SPmatrix->IntToExtColMap) ;
        } else {
            klu_print (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx, Matrix->CKTkluN, Matrix->SPmatrix->IntToExtRowMap, Matrix->SPmatrix->IntToExtColMap) ;
        }
    } else {
        if (Filename)
            spFileMatrix (Matrix->SPmatrix, Filename, "Circuit Matrix", 0, 1, 1) ;
        else
            spPrint (Matrix->SPmatrix, 0, 1, 1) ;
    }
}

#ifdef CIDER
void
SMPprintKLUforCIDER (SMPmatrix *Matrix, char *Filename)
{
    unsigned int i ;
    double *KLUmatrixAx ;

    if (Matrix->CKTkluMODE)
    {
        if (Matrix->SMPkluMatrix->KLUmatrixIsComplex)
        {
            klu_z_print (Matrix->SMPkluMatrix->KLUmatrixAp, Matrix->SMPkluMatrix->KLUmatrixAi, Matrix->SMPkluMatrix->KLUmatrixAxComplex,
                         (int)Matrix->SMPkluMatrix->KLUmatrixN, NULL, NULL) ;
        } else {
            /* Allocate the Real Matrix */
            KLUmatrixAx = (double *) malloc (Matrix->SMPkluMatrix->KLUmatrixNZ * sizeof(double)) ;

            /* Copy the Complex Matrix into the Real Matrix */
            for (i = 0 ; i < Matrix->SMPkluMatrix->KLUmatrixNZ ; i++) {
                KLUmatrixAx [i] = Matrix->SMPkluMatrix->KLUmatrixAxComplex [2 * i] ;
            }

            /* Print the Real Matrix */
            klu_print (Matrix->SMPkluMatrix->KLUmatrixAp, Matrix->SMPkluMatrix->KLUmatrixAi, KLUmatrixAx, (int)Matrix->SMPkluMatrix->KLUmatrixN, NULL, NULL) ;

            /* Free the Real Matrix Storage */
            free (KLUmatrixAx) ;
        }
    } else {
        if (Filename)
            spFileMatrix (Matrix->SPmatrix, Filename, "Circuit Matrix", 0, 1, 1) ;
        else
            spPrint (Matrix->SPmatrix, 0, 1, 1) ;
    }
}
#endif

/*
 * SMPgetError()
 */
void
SMPgetError (SMPmatrix *Matrix, int *Col, int *Row)
{
    if (Matrix->CKTkluMODE)
    {
        *Row = Matrix->SPmatrix->IntToExtRowMap [Matrix->CKTkluCommon->singular_col + 1] ;
        *Col = Matrix->SPmatrix->IntToExtColMap [Matrix->CKTkluCommon->singular_col + 1] ;
    } else {
        spWhereSingular (Matrix->SPmatrix, Row, Col) ;
    }
}

void
spDeterminant_KLU (SMPmatrix *Matrix, int *pExponent, RealNumber *pDeterminant, RealNumber *piDeterminant)
{
    int I, Size ;
    RealNumber Norm, nr, ni ;
    ComplexNumber Pivot, cDeterminant, Udiag ;

    int *P, *Q ;
    double *Rs, *Ux, *Uz ;
    unsigned int nSwap, nSwapP, nSwapQ ;

#define  NORM(a)     (nr = ABS((a).Real), ni = ABS((a).Imag), MAX (nr,ni))

    *pExponent = 0 ;

    if (Matrix->CKTkluCommon->status == KLU_SINGULAR)
    {
	*pDeterminant = 0.0 ;
        if (Matrix->CKTkluMatrixIsComplex == CKTkluMatrixComplex)
        {
            *piDeterminant = 0.0 ;
        }
        return ;
    }

    Size = Matrix->CKTkluN ;
    I = 0 ;

    P = (int *) malloc ((size_t)Matrix->CKTkluN * sizeof (int)) ;
    Q = (int *) malloc ((size_t)Matrix->CKTkluN * sizeof (int)) ;

    Ux = (double *) malloc ((size_t)Matrix->CKTkluN * sizeof (double)) ;

    Rs = (double *) malloc ((size_t)Matrix->CKTkluN * sizeof (double)) ;

    if (Matrix->CKTkluMatrixIsComplex == CKTkluMatrixComplex)        /* Complex Case. */
    {
	cDeterminant.Real = 1.0 ;
        cDeterminant.Imag = 0.0 ;

        Uz = (double *) malloc ((size_t)Matrix->CKTkluN * sizeof (double)) ;
/*
        int *Lp, *Li, *Up, *Ui, *Fp, *Fi, *P, *Q ;
        double *Lx, *Lz, *Ux, *Uz, *Fx, *Fz, *Rs ;
        Lp = (int *) malloc (((size_t)Matrix->CKTkluN + 1) * sizeof (int)) ;
        Li = (int *) malloc ((size_t)Matrix->CKTkluNumeric->lnz * sizeof (int)) ;
        Lx = (double *) malloc ((size_t)Matrix->CKTkluNumeric->lnz * sizeof (double)) ;
        Lz = (double *) malloc ((size_t)Matrix->CKTkluNumeric->lnz * sizeof (double)) ;
        Up = (int *) malloc (((size_t)Matrix->CKTkluN + 1) * sizeof (int)) ;
        Ui = (int *) malloc ((size_t)Matrix->CKTkluNumeric->unz * sizeof (int)) ;
        Ux = (double *) malloc ((size_t)Matrix->CKTkluNumeric->unz * sizeof (double)) ;
        Uz = (double *) malloc ((size_t)Matrix->CKTkluNumeric->unz * sizeof (double)) ;
        Fp = (int *) malloc (((size_t)Matrix->CKTkluN + 1) * sizeof (int)) ;
        Fi = (int *) malloc ((size_t)Matrix->CKTkluNumeric->Offp [Matrix->CKTkluN] * sizeof (int)) ;
        Fx = (double *) malloc ((size_t)Matrix->CKTkluNumeric->Offp [Matrix->CKTkluN] * sizeof (double)) ;
        Fz = (double *) malloc ((size_t)Matrix->CKTkluNumeric->Offp [Matrix->CKTkluN] * sizeof (double)) ;
        klu_z_extract (Matrix->CKTkluNumeric, Matrix->CKTkluSymbolic,
                       Lp, Li, Lx, Lz,
                       Up, Ui, Ux, Uz,
                       Fp, Fi, Fx, Fz,
                       P, Q, Rs, NULL,
                       Matrix->CKTkluCommon) ;
*/
        klu_z_extract_Udiag (Matrix->CKTkluNumeric, Matrix->CKTkluSymbolic, Ux, Uz, P, Q, Rs, Matrix->CKTkluCommon) ;
/*
        for (I = 0 ; I < Matrix->CKTkluNumeric->lnz ; I++)
        {
            printf ("L - Value: %-.9g\t%-.9g\n", Lx [I], Lz [I]) ;
        }
        for (I = 0 ; I < Matrix->CKTkluNumeric->unz ; I++)
        {
            printf ("U - Value: %-.9g\t%-.9g\n", Ux [I], Uz [I]) ;
        }
        for (I = 0 ; I < Matrix->CKTkluNumeric->Offp [Matrix->CKTkluN] ; I++)
        {
            printf ("F - Value: %-.9g\t%-.9g\n", Fx [I], Fz [I]) ;
        }

        for (I = 0 ; I < Matrix->CKTkluN ; I++)
        {
            printf ("U - Value: %-.9g\t%-.9g\n", Ux [I], Uz [I]) ;
        }
*/
        nSwapP = 0 ;
        for (I = 0 ; I < Matrix->CKTkluN ; I++)
        {
            if (P [I] != I)
            {
                nSwapP++ ;
            }
        }
        nSwapP /= 2 ;

        nSwapQ = 0 ;
        for (I = 0 ; I < Matrix->CKTkluN ; I++)
        {
            if (Q [I] != I)
            {
                nSwapQ++ ;
            }
        }
        nSwapQ /= 2 ;

        nSwap = nSwapP + nSwapQ ;
/*
        free (Lp) ;
        free (Li) ;
        free (Lx) ;
        free (Lz) ;
        free (Up) ;
        free (Ui) ;
        free (Fp) ;
        free (Fi) ;
        free (Fx) ;
        free (Fz) ;
*/
        I = 0 ;
        while (I < Size)
        {
            Udiag.Real = 1 / (Ux [I] * Rs [I]) ;
            Udiag.Imag = Uz [I] * Rs [I] ;

//            printf ("Udiag.Real: %-.9g\tUdiag.Imag %-.9g\n", Udiag.Real, Udiag.Imag) ;

            CMPLX_RECIPROCAL (Pivot, Udiag) ;
            CMPLX_MULT_ASSIGN (cDeterminant, Pivot) ;

//            printf ("cDeterminant.Real: %-.9g\tcDeterminant.Imag %-.9g\n", cDeterminant.Real, cDeterminant.Imag) ;

	    /* Scale Determinant. */
            Norm = NORM (cDeterminant) ;
            if (Norm != 0.0)
            {
		while (Norm >= 1.0e12)
                {
		    cDeterminant.Real *= 1.0e-12 ;
                    cDeterminant.Imag *= 1.0e-12 ;
                    *pExponent += 12 ;
                    Norm = NORM (cDeterminant) ;
                }
                while (Norm < 1.0e-12)
                {
		    cDeterminant.Real *= 1.0e12 ;
                    cDeterminant.Imag *= 1.0e12 ;
                    *pExponent -= 12 ;
                    Norm = NORM (cDeterminant) ;
                }
            }
            I++ ;
        }

	/* Scale Determinant again, this time to be between 1.0 <= x < 10.0. */
        Norm = NORM (cDeterminant) ;
        if (Norm != 0.0)
        {
	    while (Norm >= 10.0)
            {
		cDeterminant.Real *= 0.1 ;
                cDeterminant.Imag *= 0.1 ;
                (*pExponent)++ ;
                Norm = NORM (cDeterminant) ;
            }
            while (Norm < 1.0)
            {
		cDeterminant.Real *= 10.0 ;
                cDeterminant.Imag *= 10.0 ;
                (*pExponent)-- ;
                Norm = NORM (cDeterminant) ;
            }
        }
        if (nSwap % 2 != 0)
        {
            CMPLX_NEGATE (cDeterminant) ;
        }

        *pDeterminant = cDeterminant.Real ;
        *piDeterminant = cDeterminant.Imag ;

        free (Uz) ;
    }
    else
    {
	/* Real Case. */
        *pDeterminant = 1.0 ;

        klu_extract_Udiag (Matrix->CKTkluNumeric, Matrix->CKTkluSymbolic, Ux, P, Q, Rs, Matrix->CKTkluCommon) ;

        nSwapP = 0 ;
        for (I = 0 ; I < Matrix->CKTkluN ; I++)
        {
            if (P [I] != I)
            {
                nSwapP++ ;
            }
        }
        nSwapP /= 2 ;

        nSwapQ = 0 ;
        for (I = 0 ; I < Matrix->CKTkluN ; I++)
        {
            if (Q [I] != I)
            {
                nSwapQ++ ;
            }
        }
        nSwapQ /= 2 ;

        nSwap = nSwapP + nSwapQ ;

        while (I < Size)
        {
            *pDeterminant /= (Ux [I] * Rs [I]) ;

	    /* Scale Determinant. */
            if (*pDeterminant != 0.0)
            {
		while (ABS(*pDeterminant) >= 1.0e12)
                {
		    *pDeterminant *= 1.0e-12 ;
                    *pExponent += 12 ;
                }
                while (ABS(*pDeterminant) < 1.0e-12)
                {
		    *pDeterminant *= 1.0e12 ;
                    *pExponent -= 12 ;
                }
            }
            I++ ;
        }

	/* Scale Determinant again, this time to be between 1.0 <= x <
           10.0. */
        if (*pDeterminant != 0.0)
        {
	    while (ABS(*pDeterminant) >= 10.0)
            {
		*pDeterminant *= 0.1 ;
                (*pExponent)++ ;
            }
            while (ABS(*pDeterminant) < 1.0)
            {
		*pDeterminant *= 10.0 ;
                (*pExponent)-- ;
            }
        }
        if (nSwap % 2 != 0)
        {
            *pDeterminant = -*pDeterminant ;
        }
    }

    free (P) ;
    free (Q) ;
    free (Ux) ;
    free (Rs) ;
}

/*
 * SMPcProdDiag()
 *    note: obsolete for Spice3d2 and later
 */
int
SMPcProdDiag (SMPmatrix *Matrix, SPcomplex *pMantissa, int *pExponent)
{
    if (Matrix->CKTkluMODE)
    {
        spDeterminant_KLU (Matrix, pExponent, &(pMantissa->real), &(pMantissa->imag)) ;
    } else {
        spDeterminant (Matrix->SPmatrix, pExponent, &(pMantissa->real), &(pMantissa->imag)) ;
    }
    return spError (Matrix->SPmatrix) ;
}

/*
 * SMPcDProd()
 */
int
SMPcDProd (SMPmatrix *Matrix, SPcomplex *pMantissa, int *pExponent)
{
    double	re, im, x, y, z;
    int		p;

    if (Matrix->CKTkluMODE)
    {
        spDeterminant_KLU (Matrix, &p, &re, &im) ;
    } else {
        spDeterminant (Matrix->SPmatrix, &p, &re, &im) ;
    }

#ifndef M_LN2
#define M_LN2   0.69314718055994530942
#endif
#ifndef M_LN10
#define M_LN10  2.30258509299404568402
#endif

#ifdef debug_print
    printf ("Determinant 10: (%20g,%20g)^%d\n", re, im, p) ;
#endif

    /* Convert base 10 numbers to base 2 numbers, for comparison */
    y = p * M_LN10 / M_LN2;
    x = (int) y;
    y -= x;

    /* ASSERT
     *	x = integral part of exponent, y = fraction part of exponent
     */

    /* Fold in the fractional part */
#ifdef debug_print
    printf (" ** base10 -> base2 int =  %g, frac = %20g\n", x, y) ;
#endif
    z = pow (2.0, y) ;
    re *= z ;
    im *= z ;
#ifdef debug_print
    printf (" ** multiplier = %20g\n", z) ;
#endif

    /* Re-normalize (re or im may be > 2.0 or both < 1.0 */
    if (re != 0.0)
    {
	y = logb (re) ;
	if (im != 0.0)
	    z = logb (im) ;
	else
	    z = 0 ;
    } else if (im != 0.0) {
	z = logb (im) ;
	y = 0 ;
    } else {
	/* Singular */
	/*printf("10 -> singular\n");*/
	y = 0 ;
	z = 0 ;
    }

#ifdef debug_print
    printf (" ** renormalize changes = %g,%g\n", y, z) ;
#endif
    if (y < z)
	y = z ;

    *pExponent = (int)(x + y) ;
    x = scalbn (re, (int) -y) ;
    z = scalbn (im, (int) -y) ;
#ifdef debug_print
    printf (" ** values are: re %g, im %g, y %g, re' %g, im' %g\n", re, im, y, x, z) ;
#endif
    pMantissa->real = scalbn (re, (int) -y) ;
    pMantissa->imag = scalbn (im, (int) -y) ;

#ifdef debug_print
    printf ("Determinant 10->2: (%20g,%20g)^%d\n", pMantissa->real, pMantissa->imag, *pExponent) ;
#endif

    if (Matrix->CKTkluMODE)
    {
        return 0 ;
    } else {
        return spError (Matrix->SPmatrix) ;
    }
}



/*
 *  The following routines need internal knowledge of the Sparse data
 *  structures.
 */

/*
 *  LOAD GMIN
 *
 *  This routine adds Gmin to each diagonal element.  Because Gmin is
 *  added to the current diagonal, which may bear little relation to
 *  what the outside world thinks is a diagonal, and because the
 *  elements that are diagonals may change after calling spOrderAndFactor,
 *  use of this routine is not recommended.  It is included here simply
 *  for compatibility with Spice3.
 */


static void
LoadGmin_CSC (double **diag, int n, double Gmin)
{
    int i ;

    if (Gmin != 0.0)
        for (i = 0 ; i < n ; i++)
            if (diag [i] != NULL)
                *(diag [i]) += Gmin ;
}

static void
LoadGmin (SMPmatrix *eMatrix, double Gmin)
{
    MatrixPtr Matrix = eMatrix->SPmatrix ;
    int I ;
    ArrayOfElementPtrs Diag ;
    ElementPtr diag ;

    /* Begin `LoadGmin'. */
    assert (IS_SPARSE (Matrix)) ;

    if (Gmin != 0.0) {
	Diag = Matrix->Diag ;
	for (I = Matrix->Size ; I > 0 ; I--)
        {
	    if ((diag = Diag [I]) != NULL)
		diag->Real += Gmin ;
	}
    }
    return ;
}




/*
 *  FIND ELEMENT
 *
 *  This routine finds an element in the matrix by row and column number.
 *  If the element exists, a pointer to it is returned.  If not, then NULL
 *  is returned unless the CreateIfMissing flag is TRUE, in which case a
 *  pointer to the new element is returned.
 */

SMPelement *
SMPfindElt (SMPmatrix *eMatrix, int Row, int Col, int CreateIfMissing)
{
    MatrixPtr Matrix = eMatrix->SPmatrix ;

    if (eMatrix->CKTkluMODE)
    {
        int i ;

        Row = Matrix->ExtToIntRowMap [Row] ;
        Col = Matrix->ExtToIntColMap [Col] ;
        Row = Row - 1 ;
        Col = Col - 1 ;
        if ((Row < 0) || (Col < 0)) {
            printf ("Error: Cannot find an element with row '%d' and column '%d' in the KLU matrix\n", Row, Col) ;
            return NULL ;
        }
        for (i = eMatrix->CKTkluAp [Col] ; i < eMatrix->CKTkluAp [Col + 1] ; i++) {
            if (eMatrix->CKTkluAi [i] == Row) {
                if (eMatrix->CKTkluMatrixIsComplex == CKTkluMatrixReal) {
                    return (SMPelement *) &(eMatrix->CKTkluAx [i]) ;
                } else if (eMatrix->CKTkluMatrixIsComplex == CKTkluMatrixComplex) {
                    return (SMPelement *) &(eMatrix->CKTkluAx_Complex [2 * i]) ;
                } else {
                    return NULL ;
                }
            }
        }
        return NULL ;
    } else {
        ElementPtr Element ;

        /* Begin `SMPfindElt'. */
        assert (IS_SPARSE (Matrix)) ;
        Row = Matrix->ExtToIntRowMap [Row] ;
        Col = Matrix->ExtToIntColMap [Col] ;
        Element = Matrix->FirstInCol [Col] ;
        Element = spcFindElementInCol (Matrix, &Element, Row, Col, CreateIfMissing) ;
        return (SMPelement *)Element ;
    }
}

/* XXX The following should probably be implemented in spUtils */

/*
 * SMPcZeroCol()
 */
int
SMPcZeroCol (SMPmatrix *eMatrix, int Col)
{
    MatrixPtr Matrix = eMatrix->SPmatrix ;
    ElementPtr	Element ;

    Col = Matrix->ExtToIntColMap [Col] ;

    if (eMatrix->CKTkluMODE)
    {
        int i ;
        for (i = eMatrix->CKTkluAp [Col - 1] ; i < eMatrix->CKTkluAp [Col] ; i++)
        {
            eMatrix->CKTkluAx_Complex [2 * i] = 0 ;
            eMatrix->CKTkluAx_Complex [2 * i + 1] = 0 ;
        }
        return 0 ;
    } else {
        for (Element = Matrix->FirstInCol [Col] ; Element != NULL ; Element = Element->NextInCol)
        {
            Element->Real = 0.0 ;
            Element->Imag = 0.0 ;
        }
        return spError (Matrix) ;
    }
}

/*
 * SMPcAddCol()
 */
int
SMPcAddCol (SMPmatrix *eMatrix, int Accum_Col, int Addend_Col)
{
    MatrixPtr Matrix = eMatrix->SPmatrix ;
    ElementPtr	Accum, Addend, *Prev ;

    Accum_Col = Matrix->ExtToIntColMap [Accum_Col] ;
    Addend_Col = Matrix->ExtToIntColMap [Addend_Col] ;

    Addend = Matrix->FirstInCol [Addend_Col] ;
    Prev = &Matrix->FirstInCol [Accum_Col] ;
    Accum = *Prev;

    while (Addend != NULL)
    {
	while (Accum && Accum->Row < Addend->Row)
        {
	    Prev = &Accum->NextInCol ;
	    Accum = *Prev ;
	}
	if (!Accum || Accum->Row > Addend->Row)
        {
	    Accum = spcCreateElement (Matrix, Addend->Row, Accum_Col, Prev, 0) ;
	}
	Accum->Real += Addend->Real ;
	Accum->Imag += Addend->Imag ;
	Addend = Addend->NextInCol ;
    }

    return spError (Matrix) ;
}

/*
 * SMPzeroRow()
 */
int
SMPzeroRow (SMPmatrix *eMatrix, int Row)
{
    MatrixPtr Matrix = eMatrix->SPmatrix ;
    ElementPtr	Element ;

    Row = Matrix->ExtToIntColMap [Row] ;

    if (Matrix->RowsLinked == NO)
	spcLinkRows (Matrix) ;

    if (Matrix->PreviousMatrixWasComplex || Matrix->Complex)
    {
	for (Element = Matrix->FirstInRow[Row] ; Element != NULL; Element = Element->NextInRow)
	{
	    Element->Real = 0.0 ;
	    Element->Imag = 0.0 ;
	}
    } else {
	for (Element = Matrix->FirstInRow [Row] ; Element != NULL ; Element = Element->NextInRow)
	{
	    Element->Real = 0.0 ;
	}
    }

    return spError (Matrix) ;
}

/*
 * SMPconstMult()
 */
void
SMPconstMult (SMPmatrix *Matrix, double constant)
{
    if (Matrix->CKTkluMODE)
    {
        if (Matrix->CKTkluMatrixIsComplex)
        {
            klu_z_constant_multiply (Matrix->CKTkluAp, Matrix->CKTkluAx_Complex, Matrix->CKTkluN, Matrix->CKTkluCommon, constant) ;
        } else {
            klu_constant_multiply (Matrix->CKTkluAp, Matrix->CKTkluAx, Matrix->CKTkluN, Matrix->CKTkluCommon, constant) ;
        }
    } else {
        spConstMult (Matrix->SPmatrix, constant) ;
    }
}

/*
 * SMPmultiply()
 */
void
SMPmultiply (SMPmatrix *Matrix, double *RHS, double *Solution, double *iRHS, double *iSolution)
{
    if (Matrix->CKTkluMODE)
    {
        int *Ap_CSR, *Ai_CSR ;
        double *Ax_CSR ;

        Ap_CSR = (int *) malloc ((size_t)(Matrix->CKTkluN + 1) * sizeof (int)) ;
        Ai_CSR = (int *) malloc ((size_t)Matrix->CKTklunz * sizeof (int)) ;

        if (Matrix->CKTkluMatrixIsComplex)
        {
            Ax_CSR = (double *) malloc ((size_t)(2 * Matrix->CKTklunz) * sizeof (double)) ;
            klu_z_convert_matrix_in_CSR (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx_Complex, Ap_CSR, Ai_CSR, Ax_CSR, Matrix->CKTkluN, Matrix->CKTklunz, Matrix->CKTkluCommon) ;
            klu_z_matrix_vector_multiply (Ap_CSR, Ai_CSR, Ax_CSR, RHS, Solution, iRHS, iSolution, Matrix->SPmatrix->IntToExtRowMap,
                                          Matrix->SPmatrix->IntToExtColMap, Matrix->CKTkluN, Matrix->CKTkluCommon) ;
        } else {
            Ax_CSR = (double *) malloc ((size_t)Matrix->CKTklunz * sizeof (double)) ;
            klu_convert_matrix_in_CSR (Matrix->CKTkluAp, Matrix->CKTkluAi, Matrix->CKTkluAx, Ap_CSR, Ai_CSR, Ax_CSR, Matrix->CKTkluN, Matrix->CKTklunz, Matrix->CKTkluCommon) ;
            klu_matrix_vector_multiply (Ap_CSR, Ai_CSR, Ax_CSR, RHS, Solution, Matrix->SPmatrix->IntToExtRowMap,
                                        Matrix->SPmatrix->IntToExtColMap, Matrix->CKTkluN, Matrix->CKTkluCommon) ;
            iSolution = iRHS ;
        }

        free (Ap_CSR) ;
        free (Ai_CSR) ;
        free (Ax_CSR) ;
    } else {
        spMultiply (Matrix->SPmatrix, RHS, Solution, iRHS, iSolution) ;
    }
}

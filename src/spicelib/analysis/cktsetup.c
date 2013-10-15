/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Thomas L. Quarles
**********/

    /* CKTsetup(ckt)
     * this is a driver program to iterate through all the various
     * setup functions provided for the circuit elements in the
     * given circuit
     */

#include "ngspice/ngspice.h"
#include "ngspice/smpdefs.h"
#include "ngspice/cktdefs.h"
#include "ngspice/devdefs.h"
#include "ngspice/sperror.h"

#ifdef USE_OMP
#include <omp.h>
#include "ngspice/cpextern.h"
int nthreads;
#endif

#define CKALLOC(var,size,type) \
    if(size && ((var = TMALLOC(type, size)) == NULL)){\
            return(E_NOMEM);\
}


int
CKTsetup(CKTcircuit *ckt)
{
    int i;
    int error;
#ifdef XSPICE
 /* gtri - begin - Setup for adding rshunt option resistors */
    CKTnode *node;
    int     num_nodes;
 /* gtri - end - Setup for adding rshunt option resistors */
#endif
    SMPmatrix *matrix;
    ckt->CKTnumStates=0;

#ifdef WANT_SENSE2
    if(ckt->CKTsenInfo){
        error = CKTsenSetup(ckt);
        if (error)
            return(error);
    }
#endif

    if (ckt->CKTisSetup)
        return E_NOCHANGE;

    error = NIinit(ckt);
    if (error) return(error);
    ckt->CKTisSetup = 1;

    matrix = ckt->CKTmatrix;

#ifdef USE_OMP
    if (!cp_getvar("num_threads", CP_NUM, &nthreads))
        nthreads = 2;

    omp_set_num_threads(nthreads);
/*    if (nthreads == 1)
      printf("OpenMP: %d thread is requested in ngspice\n", nthreads);
    else
      printf("OpenMP: %d threads are requested in ngspice\n", nthreads);*/
#endif

    for (i=0;i<DEVmaxnum;i++) {
#ifdef HAS_PROGREP
        SetAnalyse( "Device Setup", 0 );
#endif
        if ( DEVices[i] && DEVices[i]->DEVsetup && ckt->CKThead[i] ) {
            error = DEVices[i]->DEVsetup (matrix, ckt->CKThead[i], ckt,
                    &ckt->CKTnumStates);
            if(error) return(error);
        }
    }
    for(i=0;i<=MAX(2,ckt->CKTmaxOrder)+1;i++) { /* dctran needs 3 states as minimum */
        CKALLOC(ckt->CKTstates[i],ckt->CKTnumStates,double);
    }
#ifdef WANT_SENSE2
    if(ckt->CKTsenInfo){
        /* to allocate memory to sensitivity structures if
         * it is not done before */

        error = NIsenReinit(ckt);
        if(error) return(error);
    }
#endif
    if(ckt->CKTniState & NIUNINITIALIZED) {
        error = NIreinit(ckt);
        if(error) return(error);
    }
#ifdef XSPICE
  /* gtri - begin - Setup for adding rshunt option resistors */

    if(ckt->enh->rshunt_data.enabled) {

        /* Count number of voltage nodes in circuit */
        for(num_nodes = 0, node = ckt->CKTnodes; node; node = node->next)
            if((node->type == SP_VOLTAGE) && (node->number != 0))
                num_nodes++;

        /* Allocate space for the matrix diagonal data */
        if(num_nodes > 0) {
            ckt->enh->rshunt_data.diag =
                 TMALLOC(double *, num_nodes);
        }

        /* Set the number of nodes in the rshunt data */
        ckt->enh->rshunt_data.num_nodes = num_nodes;

        /* Get/create matrix diagonal entry following what RESsetup does */
        for(i = 0, node = ckt->CKTnodes; node; node = node->next) {
            if((node->type == SP_VOLTAGE) && (node->number != 0)) {
                ckt->enh->rshunt_data.diag[i] =
                      SMPmakeElt(matrix,node->number,node->number);
                i++;
            }
        }

    }

    /* gtri - end - Setup for adding rshunt option resistors */
#endif

#ifdef KIRCHHOFF
    /**
     * Gmin Stepping
     */
//    CKTnode *node ;

//    node = ckt->CKTnodes ;
//    for (i = 1 ; i <= SMPmatSize (ckt->CKTmatrix) ; i++)
//    {
//        node = node->next ;

//        if (node->type == SP_VOLTAGE)
//        {
//            ckt->CKTdiag [i] = SMPmakeElt (ckt->CKTmatrix, i, i) ;
//        }
//    }

    /** Marking node as Non-Linear when needed
     *  By default every node is Linear
     */
    for (i = 0 ; i < DEVmaxnum ; i++)
    {
        if (DEVices[i] && DEVices[i]->DEVnodeIsNonLinear && ckt->CKThead[i])
        {
            error = DEVices[i]->DEVnodeIsNonLinear (ckt->CKThead[i], ckt) ;
            if (error)
                return (error) ;
        }
    }

    /**
     * Reordering nodes for convergence tests
     */
    int j, non_linear_nodes ;
    non_linear_nodes = 0 ;
    for (i = 1 ; i <= SMPmatSize (ckt->CKTmatrix) ; i++)
    {
        if (!ckt->CKTnodeIsLinear [i])
        {
            non_linear_nodes++ ;
        }
    }

    CKALLOC (ckt->CKTrhsOrdered, non_linear_nodes, double*) ;
    CKALLOC (ckt->CKTrhsOldOrdered, non_linear_nodes, double*) ;
    CKALLOC (ckt->CKTmkCurKCLarrayOrdered, non_linear_nodes, CKTmkCurKCLnode*) ;
    CKALLOC (ckt->CKTfvkOrdered, non_linear_nodes, double*) ;
    j = 0 ;
    node = ckt->CKTnodes ;
    for (i = 1 ; i <= SMPmatSize (ckt->CKTmatrix) ; i++)
    {
        node = node->next ;

        if ((node->type == SP_VOLTAGE) && (!ckt->CKTnodeIsLinear [i]))
        {
            ckt->CKTrhsOrdered [j] = &(ckt->CKTrhs [i]) ;
            ckt->CKTrhsOldOrdered [j] = &(ckt->CKTrhsOld [i]) ;
            ckt->CKTmkCurKCLarrayOrdered [j] = ckt->CKTmkCurKCLarray [i] ;
            ckt->CKTfvkOrdered [j] = &(ckt->CKTfvk [i]) ;
            j++ ;
        }
    }
    ckt->CKTvoltageNonLinearNodes = j ;

    node = ckt->CKTnodes ;
    for (i = 1 ; i <= SMPmatSize (ckt->CKTmatrix) ; i++)
    {
        node = node->next ;

        if ((node->type == SP_CURRENT) && (!ckt->CKTnodeIsLinear [i]))
        {
            ckt->CKTrhsOrdered [j] = &(ckt->CKTrhs [i]) ;
            ckt->CKTrhsOldOrdered [j] = &(ckt->CKTrhsOld [i]) ;
            j++ ;
        }
    }
    ckt->CKTcurrentNonLinearNodes = j - ckt->CKTvoltageNonLinearNodes ;


#endif

    return(OK);
}

int
CKTunsetup(CKTcircuit *ckt)
{
    int i, error, e2;
    CKTnode *node;

    error = OK;
    if (!ckt->CKTisSetup)
        return OK;

    for(i=0;i<=ckt->CKTmaxOrder+1;i++) {
        tfree(ckt->CKTstates[i]);
    }

    /* added by HT 050802*/
    for(node=ckt->CKTnodes;node;node=node->next){
        if(node->icGiven || node->nsGiven) {
            node->ptr=0;
        }
    }

    for (i=0;i<DEVmaxnum;i++) {
        if ( DEVices[i] && DEVices[i]->DEVunsetup && ckt->CKThead[i] ) {
            e2 = DEVices[i]->DEVunsetup (ckt->CKThead[i], ckt);
            if (!error && e2)
                error = e2;
        }
    }
    ckt->CKTisSetup = 0;
    if(error) return(error);

    NIdestroy(ckt);
    /*
    if (ckt->CKTmatrix)
        SMPdestroy(ckt->CKTmatrix);
    ckt->CKTmatrix = NULL;
    */

    return OK;
}

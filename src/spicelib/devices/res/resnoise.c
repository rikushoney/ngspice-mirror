/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1987 Gary W. Ng
Modified: Apr 2000 - Paolo Nenzi
**********/

#include <ngspice/ngspice.h>
#include "resdefs.h"
#include <ngspice/cktdefs.h>
#include <ngspice/iferrmsg.h>
#include <ngspice/noisedef.h>

/*
 * RESnoise (mode, operation, firstModel, ckt, data, OnDens)
 *    This routine names and evaluates all of the noise sources
 *    associated with resistors.  It starts with the model *firstModel
 *    and traverses all of its instances.  It then proceeds to any other
 *    models on the linked list.  The total output noise density
 *    generated by all the resistors is summed in the variable "OnDens".
 *
 * Paolo Nenzi 2003: 
 *    Added flicker noise (Kf-Af) calculation to simulate
 *    carbon resistors.
 *
 *    Added "noisy" switch to simulate noiseless resistors.
 */


int
RESnoise (int mode, int operation, GENmodel *genmodel, CKTcircuit *ckt, 
          Ndata *data, double *OnDens)
{
    NOISEAN *job = (NOISEAN *) ckt->CKTcurJob;

    RESmodel *firstModel = (RESmodel *) genmodel;
    RESmodel *model;
    RESinstance *inst;
    char name[N_MXVLNTH];
    double tempOutNoise;
    double tempInNoise;
    double noizDens[RESNSRCS];
    double lnNdens[RESNSRCS];
    int i;

     /* define the names of the noise sources */

    static char *RESnNames[RESNSRCS] = {
        /* Note that we have to keep the order consistent with the
         * strchr definitions in RESdefs.h */
        "_thermal",         /* Thermal noise */
        "_1overf",          /* flicker (1/f) noise */
        ""                  /* total resistor noise */
    };

    
    for (model = firstModel; model != NULL; model = model->RESnextModel) {
        for (inst = model->RESinstances; inst != NULL; 
                inst = inst->RESnextInstance) {
            
            if (inst->RESowner != ARCHme) continue;
            
            if(!inst->RESnoisy) continue; /* Quiet resistors are skipped */
            
            switch (operation) {

            case N_OPEN:

                /* 
                 * See if we have to to produce a summary report
                 * if so, name the noise generator 
                 */

                if (job->NStpsSm != 0) {
                    switch (mode) {

                    case N_DENS:
                        for (i=0; i < RESNSRCS; i++) {
                        (void)sprintf(name,"onoise_%s%s",
                                      inst->RESname, RESnNames[i]);

                        data->namelist = TREALLOC(IFuid, data->namelist, data->numPlots + 1);
                        if (!data->namelist) return(E_NOMEM);
                        SPfrontEnd->IFnewUid (ckt,
                            &(data->namelist[data->numPlots++]),
                            NULL, name, UID_OTHER, NULL);
                                /* we've added one more plot */
                        }
                        break;

                    case INT_NOIZ:
                        for (i=0; i < RESNSRCS; i++) {
                            (void)sprintf(name,"onoise_total_%s%s",
                                          inst->RESname, RESnNames[i]);


                            data->namelist = TREALLOC(IFuid, data->namelist, data->numPlots + 1);
                            if (!data->namelist) return(E_NOMEM);
                            SPfrontEnd->IFnewUid (ckt,
                                &(data->namelist[data->numPlots++]),
                                NULL, name, UID_OTHER, NULL);
                                /* we've added one more plot */


                            (void)sprintf(name,"inoise_total_%s%s",
                                inst->RESname,RESnNames[i]);


                            data->namelist = TREALLOC(IFuid, data->namelist, data->numPlots + 1);
                            if (!data->namelist) return(E_NOMEM);
                            SPfrontEnd->IFnewUid (ckt,
                                &(data->namelist[data->numPlots++]),
                                NULL, name, UID_OTHER, NULL);
                                /* we've added one more plot */

                        }
                        break;
                    }
                }
                break;

            case N_CALC:
                switch (mode) {

                case N_DENS:
                
                       NevalSrcInstanceTemp(&noizDens[RESTHNOIZ],&lnNdens[RESTHNOIZ],
                                ckt,THERMNOISE, inst->RESposNode,inst->RESnegNode,
                                inst->RESconduct * inst->RESm, inst->RESdtemp); 

                       NevalSrcInstanceTemp(&noizDens[RESFLNOIZ], NULL, ckt,
                                N_GAIN,inst->RESposNode, inst->RESnegNode,
                                (double)0.0, (double)0.0);
                                
#if 0
                       printf("DC current in resistor %s: %e\n",inst->RESname, inst->REScurrent);
#endif
                       
                       noizDens[RESFLNOIZ] *= inst->RESm * model->RESfNcoef * exp(model->RESfNexp *
                                              log(MAX(fabs(inst->REScurrent),
                                              N_MINLOG))) / data->freq;
                       lnNdens[RESFLNOIZ]   = log(MAX(noizDens[RESFLNOIZ],N_MINLOG));

                       noizDens[RESTOTNOIZ] = noizDens[RESTHNOIZ] + noizDens[RESFLNOIZ];
                       lnNdens[RESTOTNOIZ]  = log(noizDens[RESTOTNOIZ]);                 

                    *OnDens += noizDens[RESTOTNOIZ];

                    if (data->delFreq == 0.0) { 

                        /* if we haven't done any previous integration, we need to */
                        /* initialize our "history" variables                      */

                        for (i=0; i < RESNSRCS; i++) {
                             inst->RESnVar[LNLSTDENS][i] = lnNdens[i];
                        }
                        
                        /* clear out our integration variable if it's the first pass */

                        if (data->freq == job->NstartFreq) {
                            for (i=0; i < RESNSRCS; i++) {
                            inst->RESnVar[OUTNOIZ][i] = 0.0; /* Clear output noise */
                            inst->RESnVar[INNOIZ][i] = 0.0;  /* Clear input noise */ 
                            }
                        }
                    } else {   /* data->delFreq != 0.0 (we have to integrate) */
                    
/* In order to get the best curve fit, we have to integrate each component separately */

                             for (i = 0; i < RESNSRCS; i++) {
                               if (i != RESTOTNOIZ) {                    
                                   tempOutNoise = Nintegrate(noizDens[i], lnNdens[i],
                                                  inst->RESnVar[LNLSTDENS][i], data);
                                   tempInNoise = Nintegrate(noizDens[i] * 
                                                            data->GainSqInv ,lnNdens[i] 
                                                               + data->lnGainInv,
                                                            inst->RESnVar[LNLSTDENS][i] 
                                                            + data->lnGainInv,
                                                            data);
                                   inst->RESnVar[LNLSTDENS][i] = lnNdens[i];
                                   data->outNoiz += tempOutNoise;
                                   data->inNoise += tempInNoise;                         
                                   if (job->NStpsSm != 0) {
                                       inst->RESnVar[OUTNOIZ][i] += tempOutNoise;
                                       inst->RESnVar[OUTNOIZ][RESTOTNOIZ] += tempOutNoise;
                                       inst->RESnVar[INNOIZ][i] += tempInNoise;
                                       inst->RESnVar[INNOIZ][RESTOTNOIZ] += tempInNoise;
                                }
                        }
                }
                                
        }
                    if (data->prtSummary) {
                        for (i=0; i < RESNSRCS; i++) {     /* print a summary report */
                            data->outpVector[data->outNumber++] = noizDens[i];
                        }
                    }
                    break;

                case INT_NOIZ:        /* already calculated, just output */
                    if (job->NStpsSm != 0) {
                        for (i=0; i < RESNSRCS; i++) {
                            data->outpVector[data->outNumber++] = inst->RESnVar[OUTNOIZ][i];
                            data->outpVector[data->outNumber++] = inst->RESnVar[INNOIZ][i];
                        }
                    }    /* if */
                    break;
                }    /* switch (mode) */
                break;

            case N_CLOSE:
                return (OK);         /* do nothing, the main calling routine will close */
                break;               /* the plots */
            }    /* switch (operation) */
        }    /* for inst */
    }    /* for model */

return(OK);
}
            


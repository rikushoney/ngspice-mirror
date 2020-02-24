/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1987 Gary W. Ng
Modified: 2000 AlansFixes
**********/

#include "ngspice/ngspice.h"
#include "vdmosdefs.h"
#include "ngspice/cktdefs.h"
#include "ngspice/iferrmsg.h"
#include "ngspice/noisedef.h"
#include "ngspice/suffix.h"

/*
 * VDMOSnoise (mode, operation, firstModel, ckt, data, OnDens)
 *    This routine names and evaluates all of the noise sources
 *    associated with MOSFET's.  It starts with the model *firstModel and
 *    traverses all of its insts.  It then proceeds to any other models
 *    on the linked list.  The total output noise density generated by
 *    all of the MOSFET's is summed with the variable "OnDens".
 */


int
VDMOSnoise (int mode, int operation, GENmodel *genmodel, CKTcircuit *ckt,
           Ndata *data, double *OnDens)
{
    NOISEAN *job = (NOISEAN *) ckt->CKTcurJob;

    VDMOSmodel *firstModel = (VDMOSmodel *) genmodel;
    VDMOSmodel *model;
    VDMOSinstance *inst;
    double coxSquared;
    double tempOnoise;
    double tempInoise;
    double noizDens[VDMOSNSRCS];
    double lnNdens[VDMOSNSRCS];
    int i;
    double tempRatioSH;

    /* define the names of the noise sources */

    static char *VDMOSnNames[VDMOSNSRCS] = {       /* Note that we have to keep the order */
    "_rd",              /* noise due to rd */        /* consistent with thestrchr definitions */
    "_rs",              /* noise due to rs */        /* in VDMOSdefs.h */
    "_id",              /* noise due to id */
    "_1overf",          /* flicker (1/f) noise */
    ""                  /* total transistor noise */
    };

    for (model=firstModel; model != NULL; model=VDMOSnextModel(model)) {

    /* Oxide capacitance can be zero in MOS level 1.  Since this will give us problems in our 1/f */
    /* noise model, we ASSUME an actual "tox" of 1e-7 */

    if (model->VDMOSoxideCapFactor == 0.0) {
        coxSquared = 3.9 * 8.854214871e-12 / 1e-7;
        } else {
        coxSquared = model->VDMOSoxideCapFactor;
        }
    coxSquared *= coxSquared;
    for (inst=VDMOSinstances(model); inst != NULL; inst=VDMOSnextInstance(inst)) {
        
        switch (operation) {

        case N_OPEN:

        /* see if we have to to produce a summary report */
        /* if so, name all the noise generators */

        if (job->NStpsSm != 0) {
            switch (mode) {

            case N_DENS:
            for (i=0; i < VDMOSNSRCS; i++) {
                NOISE_ADD_OUTVAR(ckt, data, "onoise_%s%s", inst->VDMOSname, VDMOSnNames[i]);
            }
            break;

            case INT_NOIZ:
            for (i=0; i < VDMOSNSRCS; i++) {
                NOISE_ADD_OUTVAR(ckt, data, "onoise_total_%s%s", inst->VDMOSname, VDMOSnNames[i]);
                NOISE_ADD_OUTVAR(ckt, data, "inoise_total_%s%s", inst->VDMOSname, VDMOSnNames[i]);
            }
            break;
            }
        }
        break;

        case N_CALC:
        switch (mode) {

        case N_DENS:
            if ((inst->VDMOSthermalGiven) && (model->VDMOSrthjcGiven))  
                tempRatioSH = inst->VDMOSTempSH / ckt->CKTtemp;
            else
                tempRatioSH = 1.0;
            NevalSrc(&noizDens[VDMOSRDNOIZ],&lnNdens[VDMOSRDNOIZ],
                 ckt,THERMNOISE,inst->VDMOSdNodePrime,inst->VDMOSdNode,
                 inst->VDMOSdrainConductance * tempRatioSH);

            NevalSrc(&noizDens[VDMOSRSNOIZ],&lnNdens[VDMOSRSNOIZ],
                 ckt,THERMNOISE,inst->VDMOSsNodePrime,inst->VDMOSsNode,
                 inst->VDMOSsourceConductance * tempRatioSH);

            NevalSrc(&noizDens[VDMOSIDNOIZ],&lnNdens[VDMOSIDNOIZ],
                 ckt,THERMNOISE,inst->VDMOSdNodePrime,inst->VDMOSsNodePrime,
                                 (2.0/3.0 * fabs(inst->VDMOSgm)) * tempRatioSH);

            NevalSrc(&noizDens[VDMOSFLNOIZ], NULL, ckt,
                 N_GAIN,inst->VDMOSdNodePrime, inst->VDMOSsNodePrime,
                 (double)0.0);
            noizDens[VDMOSFLNOIZ] *= model->VDMOSfNcoef * 
                 exp(model->VDMOSfNexp *
                 log(MAX(fabs(inst->VDMOScd),N_MINLOG))) /
                 (data->freq * inst->VDMOSw * 
                 inst->VDMOSm *
                 inst->VDMOSl * coxSquared);
            lnNdens[VDMOSFLNOIZ] = 
                 log(MAX(noizDens[VDMOSFLNOIZ],N_MINLOG));

            noizDens[VDMOSTOTNOIZ] = noizDens[VDMOSRDNOIZ] +
                             noizDens[VDMOSRSNOIZ] +
                             noizDens[VDMOSIDNOIZ] +
                             noizDens[VDMOSFLNOIZ];
            lnNdens[VDMOSTOTNOIZ] = 
                 log(MAX(noizDens[VDMOSTOTNOIZ], N_MINLOG));

            *OnDens += noizDens[VDMOSTOTNOIZ];

            if (data->delFreq == 0.0) { 

            /* if we haven't done any previous integration, we need to */
            /* initialize our "history" variables                      */

            for (i=0; i < VDMOSNSRCS; i++) {
                inst->VDMOSnVar[LNLSTDENS][i] = lnNdens[i];
            }

            /* clear out our integration variables if it's the first pass */

            if (data->freq == job->NstartFreq) {
                for (i=0; i < VDMOSNSRCS; i++) {
                inst->VDMOSnVar[OUTNOIZ][i] = 0.0;
                inst->VDMOSnVar[INNOIZ][i] = 0.0;
                }
            }
            } else {   /* data->delFreq != 0.0 (we have to integrate) */
            for (i=0; i < VDMOSNSRCS; i++) {
                if (i != VDMOSTOTNOIZ) {
                tempOnoise = Nintegrate(noizDens[i], lnNdens[i],
                      inst->VDMOSnVar[LNLSTDENS][i], data);
                tempInoise = Nintegrate(noizDens[i] * data->GainSqInv ,
                      lnNdens[i] + data->lnGainInv,
                      inst->VDMOSnVar[LNLSTDENS][i] + data->lnGainInv,
                      data);
                inst->VDMOSnVar[LNLSTDENS][i] = lnNdens[i];
                data->outNoiz += tempOnoise;
                data->inNoise += tempInoise;
                if (job->NStpsSm != 0) {
                    inst->VDMOSnVar[OUTNOIZ][i] += tempOnoise;
                    inst->VDMOSnVar[OUTNOIZ][VDMOSTOTNOIZ] += tempOnoise;
                    inst->VDMOSnVar[INNOIZ][i] += tempInoise;
                    inst->VDMOSnVar[INNOIZ][VDMOSTOTNOIZ] += tempInoise;
                                }
                }
            }
            }
            if (data->prtSummary) {
            for (i=0; i < VDMOSNSRCS; i++) {     /* print a summary report */
                data->outpVector[data->outNumber++] = noizDens[i];
            }
            }
            break;

        case INT_NOIZ:        /* already calculated, just output */
            if (job->NStpsSm != 0) {
            for (i=0; i < VDMOSNSRCS; i++) {
                data->outpVector[data->outNumber++] = inst->VDMOSnVar[OUTNOIZ][i];
                data->outpVector[data->outNumber++] = inst->VDMOSnVar[INNOIZ][i];
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

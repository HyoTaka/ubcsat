/*

      ##  ##  #####    #####   $$$$$   $$$$   $$$$$$    
      ##  ##  ##  ##  ##      $$      $$  $$    $$      
      ##  ##  #####   ##       $$$$   $$$$$$    $$      
      ##  ##  ##  ##  ##          $$  $$  $$    $$      
       ####   #####    #####  $$$$$   $$  $$    $$      
  ======================================================
  SLS SAT Solver from The University of British Columbia
  ======================================================
  ...Developed by Dave Tompkins (davet [@] cs.ubc.ca)...
  ------------------------------------------------------
  .......consult legal.txt for legal information........
  ......consult revisions.txt for revision history......
  ------------------------------------------------------
  ... project website: http://www.satlib.org/ubcsat ....
  ------------------------------------------------------
  .....e-mail ubcsat-help [@] cs.ubc.ca for support.....
  ------------------------------------------------------

*/

#include "ubcsat.h"

#ifdef __cplusplus 
namespace ubcsat {
#endif

void PickNCVW();
void InitNCVW();
void PostFlipNCVW();

FLOAT fNcvwGamma;
FLOAT fNcvwDelta;
FLOAT fNcvwPi;

UINT32 iSelectedAlg;

void AddNCVW() {

  ALGORITHM *pCurAlg;

  pCurAlg = CreateAlgorithm("ncvw","",FALSE,
   "NCVWr: Switch between non (adaptG2WSAT*) clause (RSAPS) and variable (VW) weighting (paper version)",
    "Wei, Li, Zhang  [CP 08]",
    "PickNCVW,InitNCVW,InitRSAPS,PostFlipNCVW,ConfigureG2WSatGeneral,InitNoVWSmoothing",
    "DefaultProcedures,Flip+TrackChanges+FCL,DecPromVars,FalseClauseList,VarLastChange,VW2Weights,MakeBreakPenaltyFL,VarInFalse",
    "default","default");

  AddParmFloat(&pCurAlg->parmList,"-gamma","NCVW switching criteria [default %s]","paramater to adjust selecting VW over others~use VW if max.vw2w > gamma * avg.vw2w","",&fNcvwGamma,7.5);
  AddParmFloat(&pCurAlg->parmList,"-delta","NCVW switching criteria [default %s]","use RSAPS if max.cw >= delta * avg.cw ","",&fNcvwDelta,3.0f);
  AddParmFloat(&pCurAlg->parmList,"-pi","NCVW switching criteria [default %s]","use RSAPS if avg.cw <= pi","",&fNcvwPi,15.0f);

  AddParmUInt(&pCurAlg->parmList,"-sub","G2WSAT sub-algortihm [default %s]","0: Novelty+~1:Novelty++~2:Novelty+p~3:Novelty++p~4:Novelty++0~5:Novelty++1~6:Novelty++2","",&iG2WsatSubAlgID,0);
  AddParmBool(&pCurAlg->parmList,"-psel","decreasing promising variable selection [default %s]","0: best~1:oldest~2:random","",&iG2WsatPromSelectID,1);
  // todo add auto-smoothing

  AddParmProbability(&pCurAlg->parmList,"-wp","RSAPS walk probability [default %s]","within RSAPS, when a local minimum is encountered,~flip a random variable with probability PR","",&iWp,0.05);
  AddParmFloat(&pCurAlg->parmList,"-alpha","RSAPS scaling parameter alpha [default %s]","when a local minimum is encountered,~multiply all unsatisfied cluase penalties by FL","",&fAlpha,1.3f);
  AddParmFloat(&pCurAlg->parmList,"-rho","RSAPS smoothing parameter rho [default %s]","when smoothing occurs, smooth penalties by a factor of FL","",&fRho,0.8f);

  CreateTrigger("PickNCVW",ChooseCandidate,PickNCVW,"","");
  CreateTrigger("InitNCVW",InitStateInfo,InitNCVW,"","");
  CreateTrigger("PostFlipNCVW",PostFlip,PostFlipNCVW,"","PostFlipRSAPS");


  pCurAlg = CreateAlgorithm("ncvw","2009",FALSE,
   "NCVWr: NCVW 2009 SAT competition version",
    "Wei, Li, Zhang  [CP 08]",
    "PickNCVW,InitNCVW,InitRSAPS,PostFlipNCVW,ConfigureG2WSatGeneral,InitVW2Auto,UpdateVW2Auto",
    "DefaultProcedures,Flip+TrackChanges+FCL,DecPromVars,FalseClauseList,VarLastChange,VW2Weights,MakeBreakPenaltyFL,VarInFalse",
    "default","default");

  AddParmFloat(&pCurAlg->parmList,"-gamma","NCVW switching criteria [default %s]","paramater to adjust selecting VW over others~use VW if max.vw2w > gamma * avg.vw2w","",&fNcvwGamma,1.0122);
  AddParmFloat(&pCurAlg->parmList,"-delta","NCVW switching criteria [default %s]","use RSAPS if max.cw >= delta * avg.cw ","",&fNcvwDelta,2.75f);
  AddParmFloat(&pCurAlg->parmList,"-pi","NCVW switching criteria [default %s]","use RSAPS if avg.cw <= pi","",&fNcvwPi,15.0f);
  // No VW smoothing parameter for now -- randomized
  AddParmProbability(&pCurAlg->parmList,"-wp","RSAPS walk probability [default %s]","within RSAPS, when a local minimum is encountered,~flip a random variable with probability PR","",&iWp,0.05);
  AddParmFloat(&pCurAlg->parmList,"-alpha","RSAPS scaling parameter alpha [default %s]","when a local minimum is encountered,~multiply all unsatisfied cluase penalties by FL","",&fAlpha,1.3f);
  AddParmFloat(&pCurAlg->parmList,"-rho","RSAPS smoothing parameter rho [default %s]","when smoothing occurs, smooth penalties by a factor of FL","",&fRho,0.8f);

  AddParmUInt(&pCurAlg->parmList,"-sub","G2WSAT sub-algortihm [default %s]","0: Novelty+~1:Novelty++~2:Novelty+p~3:Novelty++p~4:Novelty++0~5:Novelty++1~6:Novelty++2","",&iG2WsatSubAlgID,0);
  AddParmBool(&pCurAlg->parmList,"-psel","decreasing promising variable selection [default %s]","0: best~1:oldest~2:random","",&iG2WsatPromSelectID,1);

}

void InitNCVW() {
  InitHybridInfo();
  fPenaltyImprove = -1.0e-01f;
}

void PickNCVW() {
  FLOAT fAveragePenalty = fTotalPenaltyFL / (FLOAT) iNumClauses;
  if (fVW2WeightMax >= fNcvwGamma * fVW2WeightMean) {
    PickVW2Auto();
    iMultiAlgCurrent = 3;
  } else {
    fAveragePenalty = fTotalPenaltyFL / (FLOAT) iNumClauses;
    if ((fAveragePenalty <= fNcvwPi) || (fMaxPenaltyFL >= fNcvwDelta * fAveragePenalty)) {
      PickSAPS();
      iMultiAlgCurrent = 4;
    } else {
      PickG2WSatGeneral();
    }
  }
}

void PostFlipNCVW() {
  if (iFlipCandidate) {
    /* as per original NCVW implementation, only perform updates for non-null flips */
    UpdateHybridInfo();
    UpdateVW2Auto();
    AdaptG2WSatNoise();
  }
  if (iMultiAlgCurrent == 4) {
    PostFlipRSAPS();
  }
}

#ifdef __cplusplus 
}
#endif

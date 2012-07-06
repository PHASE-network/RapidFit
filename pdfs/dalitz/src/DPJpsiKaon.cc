
#include "DPJpsiKaon.hh"
#include "DPBWResonanceShape.hh"
#include "DPLassShape.hh"
#include "DPBarrierFactor.hh"
#include "DPBarrierL0.hh"
#include "DPBarrierL1.hh"
#include "DPBarrierL2.hh"
#include "DPBarrierL3.hh"
#include "DPHelpers.hh"

#include <iostream>

DPJpsiKaon::DPJpsiKaon(int fLB, int fLR, double fmB, double mR, 
                               double gammaR, double m1, double m2, 
                               double RB, double RR, double fmJpsi,
                               int spin,std::string mShape,
                               double a, double r):
A0(1,0),
Aplus(1,0),
Aminus(1,0),
spinKaon(spin)
{
  this->LB=fLB;
  this->LR=fLR;
  this->mB=fmB;
  this->mR=mR;
  this->m1=m1;
  this->m2=m2;
  mJpsi=fmJpsi;
  if ( mShape == "LASS" )
  {
    massShape = new DPLassShape(mR, gammaR, LR, m1, m2, RR, a, r);
  }
  else
  {
    massShape = new DPBWResonanceShape(mR, gammaR, LR, m1, m2, RR);
  }
  switch (LB)
  {
    case 0: barrierB=new DPBarrierL0(RB);
            break;
    case 1: barrierB=new DPBarrierL1(RB);
            break;
    case 2: barrierB=new DPBarrierL2(RB);
            break;
    case 3: barrierB=new DPBarrierL3(RB);
            break;
    default: std::cout<<"WARNING DPJpsiKaon (LB): Do not know which barrier factor to use.  Using L=0 and you should check what are you doing.\n";
             barrierB=new DPBarrierL0(RB);
             break;
  }
  switch (LR)
  {
    case 0: barrierR=new DPBarrierL0(RR);
            break;
    case 1: barrierR=new DPBarrierL1(RR);
            break;
    case 2: barrierR=new DPBarrierL2(RR);
            break;
    case 3: barrierR=new DPBarrierL3(RR);
            break;
    default: std::cout<<"WARNING DPJpsiKaon (LR): Do not know which barrier factor to use.  Using L=0 and you should check what are you doing.\n";
             barrierR=new DPBarrierL0(RR);
             break;
  }
  switch(spin)
  {
    case 0: wigner=new DPWignerFunctionJ0();
            break;
    case 1: wigner=new DPWignerFunctionJ1();
            break;
    case 2: wigner=new DPWignerFunctionJ2();
            break;
  }
}

DPJpsiKaon::~DPJpsiKaon()
{
  if( wigner != NULL ) delete wigner;
}

DPJpsiKaon::DPJpsiKaon( const DPJpsiKaon& input ) : DPComponent( input ),
        mJpsi(input.mJpsi), m1(input.m1), m2(input.m2), pR0(input.pR0), A0(input.A0), Aplus(input.Aplus),
        Aminus(input.Aminus), spinKaon(input.spinKaon), wigner(NULL), wignerPsi(input.wignerPsi)
{    
        if( input.wigner != NULL )
        {
                switch(spinKaon)
                {
                        case 0: wigner=new DPWignerFunctionJ0();
                        break;
                        case 1: wigner=new DPWignerFunctionJ1();
                        break;
                        case 2: wigner=new DPWignerFunctionJ2();
                        break;
                }
        }
}

TComplex DPJpsiKaon::amplitude(double m23, double cosTheta1, 
                             double cosTheta2, double phi, 
                             int twoLambda, int twoLambdaPsi)
{
  TComplex result(0,0);
  // Muon helicity
  if ( twoLambda != 2 && twoLambda != -2 ) // Not right option
  {
    return result;
  }
  
  // Psi helicity
  if ( twoLambdaPsi != 2 && twoLambdaPsi != -2 && twoLambdaPsi != 0 ) // Not right option
  {
    return result;
  }

  // 
  if ( spinKaon==0 && twoLambdaPsi != 0 )
  {
    return result;
  }
  
  double pB = DPHelpers::daughterMomentum(this->mB, this->mJpsi, m23);
  double pR = DPHelpers::daughterMomentum(m23, this->m1, this->m2);

  double orbitalFactor = TMath::Power(pB/this->mB, this->LB)*
                         TMath::Power(pR/m23, this->LR);

  double barrierFactor = barrierB->barrier( DPHelpers::daughterMomentum(this->mB,
                                            this->mJpsi, this->mR), pB)*
                 barrierR->barrier(DPHelpers::daughterMomentum(this->mR,
                                   this->m1, this->m2), pR);

  TComplex massFactor = this->massShape->massShape(m23);

  // Angular part
  TComplex angular(0,0);
  angular=wignerPsi.function(cosTheta1,twoLambdaPsi/2,twoLambda/2)*
          wigner->function(cosTheta2,twoLambdaPsi/2,0)*
          TComplex::Exp(-0.5*twoLambdaPsi*TComplex::I()*phi);
  switch (twoLambdaPsi)
  {
    case 0: angular*=A0;
            break;
    case 2: angular*=Aplus;
            break;
    case -2: angular*=Aminus;
            break;
  }

  result = massFactor*barrierFactor*orbitalFactor*angular;

  return result;
}

void DPJpsiKaon::setHelicityAmplitudes(double magA0, double magAplus,
                     double magAminus, double phaseA0, double phaseAplus,
                     double phaseAminus)
{
  A0=TComplex(magA0*TMath::Cos(phaseA0),magA0*TMath::Sin(phaseA0));
  Aplus=TComplex(magAplus*TMath::Cos(phaseAplus),magAplus*TMath::Sin(phaseAplus));
  Aminus=TComplex(magAminus*TMath::Cos(phaseAminus),magAminus*TMath::Sin(phaseAminus));
}

void DPJpsiKaon::setResonanceParameters(double mass, double sigma)
{
        massShape->setResonanceParameters( mass, sigma );
}
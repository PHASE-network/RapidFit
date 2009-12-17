// $Id: Bs2JpsiPhiMassSignal.h,v 1.1 2009/11/10 10:35:49 gcowan Exp $
/** @class Bs2JpsiPhiMassSignal Bs2JpsiPhiMassSignal.h
 *
 *  RapidFit PDF for Bs2JpsiPhi long lived background
 *
 *  @author Greig A Cowan greig.alan.cowan@cern.ch
 *  @date 2009-10-04
 */

#ifndef Bs2JpsiPhiMassSignal_H
#define Bs2JpsiPhiMassSignal_H

#include "BasePDF.h"

class Bs2JpsiPhiMassSignal : public BasePDF
{
	public:
		Bs2JpsiPhiMassSignal();
		~Bs2JpsiPhiMassSignal();

		//Calculate the PDF value
		virtual double Evaluate(DataPoint*);

	protected:
		//Calculate the PDF normalisation
		virtual double Normalisation(DataPoint*, PhaseSpaceBoundary*);

	private:
		void MakePrototypes();

		// Physics parameters
		string f_sig_m1Name;	// fraction
		string sigma_m1Name;	// width 1
		string sigma_m2Name;	// width 2 
		string m_BsName;	// Bs mass

		// Observables
		string recoMassName;	// reconstructed Bs mass
};

#endif
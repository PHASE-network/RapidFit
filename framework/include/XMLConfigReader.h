/**
        @class XMLConfigReader

        Opens an xml config file and uses it to create RapidFit data objects

        @author Benjamin M Wynne bwynne@cern.ch
	@date 2009-10-02
*/

#ifndef XML_CONFIG_READER_H
#define XML_CONFIG_READER_H

#include <vector>
#include <string>
#include "XMLTag.h"
#include "ParameterSet.h"
#include "PDFWithData.h"
#include "FitFunctionConfiguration.h"
#include "MinimiserConfiguration.h"
#include "OutputConfiguration.h"
#include "DataSetConfiguration.h"
#include "ConstraintFunction.h"
#include "ScanParam.h"

using namespace std;

class XMLConfigReader
{
	public:
		XMLConfigReader();
		XMLConfigReader(string);
		~XMLConfigReader();

		ParameterSet * GetFitParameters();
		MinimiserConfiguration * GetMinimiserConfiguration();
		FitFunctionConfiguration * GetFitFunctionConfiguration();
		OutputConfiguration * GetOutputConfiguration();
		vector< PDFWithData* > GetPDFsAndData();
		vector< ConstraintFunction* > GetConstraints();
		vector<PhaseSpaceBoundary*> GetPhaseSpaceBoundaries();
		vector<vector<IPrecalculator*> > GetPrecalculators();
		int GetNumberRepeats();
		bool IsLoaded();

		unsigned int GetSeed();	//  Return the Random Seed
		void SetSeed( unsigned int new_seed );	//  Set Seed returned by XMLFile

	private:
		ParameterSet * GetParameterSet( XMLTag* );
		PhysicsParameter * GetPhysicsParameter( XMLTag*, string& );
		PhaseSpaceBoundary * GetPhaseSpaceBoundary( XMLTag* );
		IConstraint * GetConstraint( XMLTag*, string& );
		PDFWithData * GetPDFWithData( XMLTag*, XMLTag* );
		IPDF * GetNamedPDF( XMLTag* );
		IPDF * GetSumPDF( XMLTag*, PhaseSpaceBoundary* );
		IPDF * GetNormalisedSumPDF( XMLTag*, PhaseSpaceBoundary* );
		IPDF * GetProdPDF( XMLTag*, PhaseSpaceBoundary* );
		IPDF * GetPDF( XMLTag*, PhaseSpaceBoundary* );
		FitFunctionConfiguration * MakeFitFunction( XMLTag* );
		MinimiserConfiguration * MakeMinimiser( XMLTag* );
		pair< string, string > MakeContourPlot( XMLTag* );
		OutputConfiguration * MakeOutputConfiguration( XMLTag* );
		IPrecalculator * MakePrecalculator( XMLTag*, PhaseSpaceBoundary* );
		DataSetConfiguration * MakeDataSetConfiguration( XMLTag*, PhaseSpaceBoundary* );
		ConstraintFunction * GetConstraintFunction( XMLTag* );
		ExternalConstraint * GetExternalConstraint( XMLTag* );
		ScanParam * GetScanParam( XMLTag * InputTag );
		pair<ScanParam*, ScanParam*> Get2DScanParam( XMLTag * InputTag );

		vector< XMLTag* > children;
		bool isLoaded;

		vector<int> seed;	//  Random Seed
};

#endif

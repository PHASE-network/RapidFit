// $Id: LongLivedBkg_3Dangular.cpp,v 1.2 2009/11/13 15:31:51 gcowan Exp $
/** @class LongLivedBkg_3Dangular LongLivedBkg_3Dangular.cpp
 *
 *  PDF for  long lived background with 3D histogram angular description. A 3D root Histogram required provided by user and input in the xml file. The transversity angles in the order (x,y,z) --> (cosPsi, cosTheta, phi)
 *
 *  @author Ailsa Sparkes
 *  @date 2011-05-30
 */

#include "LongLivedBkg_3Dangular.h"
#include "Mathematics.h"
#include <iostream>
#include <fstream>
#include "math.h"
#include "TROOT.h"
#include "TFile.h"
#include "TH3D.h"
#include "TAxis.h"
#include "TH1.h"

PDF_CREATOR( LongLivedBkg_3Dangular );

//.....................................................
//Constructor
LongLivedBkg_3Dangular::LongLivedBkg_3Dangular( PDFConfigurator* config ) :

	// Physics parameters
	f_LL1Name		( config->getName("f_LL1")  )
	, tauLL1Name		( config->getName("tau_LL1") )
	, tauLL2Name		( config->getName("tau_LL2") )
	//Detector parameters
	, timeResLL1FracName( config->getName("timeResLL1Frac") )
	, sigmaLL1Name		( config->getName("sigma_LL1") )
	, sigmaLL2Name		( config->getName("sigma_LL2") )
	// Observables
	, timeName			( config->getName("time") )
	, cosThetaName		( config->getName("cosTheta") )
	, phiName			( config->getName("phi") )
	, cosPsiName		( config->getName("cosPsi") )
	, cthetakName 		( config->getName("helcosthetaK") )
	, cthetalName		( config->getName("helcosthetaL") )
	, phihName			( config->getName("helphi") )
	, timeconstName		( config->getName("time") )
, tauLL1(), tauLL2(), f_LL1(), sigmaLL(), sigmaLL1(), sigmaLL2(), timeResLL1Frac(), tlow(), thigh(), time(), cos1(),
	cos2(), phi(), histo(), xaxis(), yaxis(), zaxis(), nxbins(), nybins(), nzbins(), xmin(), xmax(), ymin(),
	ymax(), zmin(), zmax(), deltax(), deltay(), deltaz(), total_num_entries(), useFlatAngularDistribution(true),
	_useHelicityBasis(false)
{

	cout << "Constructing PDF: LongLivedBkg_3Dangular  " << endl ;

	//Configure
	_useHelicityBasis = config->isTrue( "UseHelicityBasis" ) ;

	//Make prototypes
	MakePrototypes();

	//Find name of histogram needed to define 3-D angular distribution
	string fileName = config->getConfigurationValue( "AngularDistributionHistogram" ) ;

	//Initialise depending upon whether configuration parameter was found
	if( fileName == "" )
	{
		cout << "   No AngularDistributionHistogram found: using flat background " << endl ;
		useFlatAngularDistribution = true ;
	}
	else
	{
		cout << "   AngularDistributionHistogram requested: " << fileName << endl ;
		useFlatAngularDistribution = false ;

		//File location
		ifstream input_file;
		input_file.open( fileName.c_str(), ifstream::in );
		input_file.close();

		bool local_fail = input_file.fail();

		if( !getenv("RAPIDFITROOT") && local_fail )
		{
			cerr << "\n" << endl;
			//cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
			cerr << "$RAPIDFITROOT NOT DEFINED, PLEASE DEFINE IT SO I CAN USE ACCEPTANCE DATA" << endl;
			//cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
			cerr << "\n" << endl;
			exit(-987);
		}

		string fullFileName;

		if( getenv("RAPIDFITROOT") )
		{
			string path( getenv("RAPIDFITROOT") ) ;

			cout << "RAPIDFITROOT defined as: " << path << endl;

			fullFileName = path+"/pdfs/configdata/"+fileName ;

			input_file.open( fullFileName.c_str(), ifstream::in );
			input_file.close();
		}
		bool elsewhere_fail = input_file.fail();

		if( elsewhere_fail && local_fail )
		{
			cerr << "\n\tFileName:\t" << fullFileName << "\t NOT FOUND PLEASE CHECK YOUR RAPIDFITROOT" << endl;
			cerr << "\t\tAlternativley make sure your XML points to the correct file, or that the file is in the current working directory\n" << endl;
			exit(-89);
		}

		if( fullFileName.empty() || !local_fail )
		{
			fullFileName = fileName;
		}

		//Read in histo
		TFile* f =  TFile::Open(fullFileName.c_str());
		if( _useHelicityBasis ) {
			histo = (TH3D*) f->Get("histoHel"); //(fileName.c_str())));
		}
		else {
			histo = (TH3D*) f->Get("histo"); //(fileName.c_str())));
		}

		xaxis = histo->GetXaxis();
		cout << "X axis Name: " << xaxis->GetName() << "\tTitle: " << xaxis->GetTitle() << endl;
		xmin = xaxis->GetXmin();
		xmax = xaxis->GetXmax();
		nxbins = histo->GetNbinsX();
		cout << "X axis Min: " << xmin << "\tMax: " << xmax << "\tBins: " << nxbins << endl;
		deltax = (xmax-xmin)/nxbins;

		yaxis = histo->GetYaxis();
		cout << "Y axis Name: " << yaxis->GetName() << "\tTitle: " << yaxis->GetTitle() << endl;
		ymin = yaxis->GetXmin();
		ymax = yaxis->GetXmax();
		nybins = histo->GetNbinsY();
		cout << "Y axis Min: " << ymin << "\tMax: " << ymax << "\tBins: " << nybins << endl;
		deltay = (ymax-ymin)/nybins;

		zaxis = histo->GetZaxis();
		cout << "Z axis Name: " << zaxis->GetName() << "\tTitle: " << zaxis->GetTitle() << endl;
		zmin = zaxis->GetXmin();
		zmax = zaxis->GetXmax();
		nzbins = histo->GetNbinsZ();
		cout << "Z axis Min: " << zmin << "\tMax: " << zmax << "\tBins: " << nzbins << endl;
		deltaz = (zmax-zmin)/nzbins;

		//method for Checking whether histogram is sensible

		total_num_entries = histo->GetEntries();
		int total_num_bins = nxbins * nybins * nzbins;
		int sum = 0;

		vector<int> zero_bins;

		//loop over each bin in histogram and print out how many zero bins there are
		for (int i=1; i < nxbins+1; ++i)
		{
			for (int j=1; j < nybins+1; ++j)
			{
				for (int k=1; k < nzbins+1; ++k)
				{

					double bin_content = histo->GetBinContent(i,j,k);
					//cout << "Bin content: " << bin_content << endl;
					if(bin_content<=0)
					{
						zero_bins.push_back(1);
					}
					//cout << " Zero bins " << zero_bins.size() << endl;}
					else if (bin_content>0)
					{
						sum += (int) bin_content;
					}
			}
		}
	}

	int average_bin_content = sum / total_num_bins;

	cout << "\n\t\t" << "****" << "For total number of bins " << total_num_bins << " there are " << zero_bins.size() << " bins containing zero events " << "****" << endl;
	cout <<  "\t\t\t" << "***" << "Average number of entries of non-zero bins: " << average_bin_content << "***" << endl;
	//cout << endl;
	cout << endl;

	// Check.  This order works for both bases since phi is always the third one.
	if ((xmax-xmin) < 2. || (ymax-ymin) < 2. || (zmax-zmin) < 2.*Mathematics::Pi() )
	{
		cout << "In LongLivedBkg_3Dangular::LongLivedBkg_3Dangular: The full angular range is not used in this histogram - the PDF does not support this case" << endl;
		exit(1);
	}

	//cout << "Finishing processing histo" << endl;
}
}

//................................................................
//Destructor
LongLivedBkg_3Dangular::~LongLivedBkg_3Dangular()
{
}



//..................................................................
//Make the data point and parameter set
void LongLivedBkg_3Dangular::MakePrototypes()
{
	//Make the DataPoint prototype
	allObservables.push_back( timeName );
	//allObservables.push_back( cosThetaName );
	//allObservables.push_back( phiName );
	//allObservables.push_back( cosPsiName );
	if( _useHelicityBasis ) {
		allObservables.push_back( cthetakName );
		allObservables.push_back( cthetalName );
		allObservables.push_back( phihName );
	}
	else {
		allObservables.push_back( cosThetaName );
		allObservables.push_back( phiName );
		allObservables.push_back( cosPsiName );
	}

	//Make the parameter set
	vector<string> parameterNames;
	parameterNames.push_back( f_LL1Name );
	parameterNames.push_back( tauLL1Name );
	parameterNames.push_back( tauLL2Name );
	parameterNames.push_back( timeResLL1FracName );
	parameterNames.push_back( sigmaLL1Name );
	parameterNames.push_back( sigmaLL2Name );

	allParameters = ParameterSet(parameterNames);
}


//.................................................................
bool LongLivedBkg_3Dangular::SetPhysicsParameters( ParameterSet * NewParameterSet )
{
	bool isOK = allParameters.SetPhysicsParameters(NewParameterSet);
	f_LL1       = allParameters.GetPhysicsParameter( f_LL1Name )->GetValue();
	tauLL1      = allParameters.GetPhysicsParameter( tauLL1Name )->GetValue();
	tauLL2      = allParameters.GetPhysicsParameter( tauLL2Name )->GetValue();
	timeResLL1Frac = allParameters.GetPhysicsParameter( timeResLL1FracName )->GetValue();
	sigmaLL1    = allParameters.GetPhysicsParameter( sigmaLL1Name )->GetValue();
	sigmaLL2    = allParameters.GetPhysicsParameter( sigmaLL2Name )->GetValue();

	return isOK;
}

//..............................................................
//Main method to build the PDF return value
double LongLivedBkg_3Dangular::Evaluate(DataPoint * measurement)
{
	// Observable
	time = measurement->GetObservable( timeName )->GetValue();
	if( _useHelicityBasis ) {
		cos1   = measurement->GetObservable( cthetakName )->GetValue();
		cos2   = measurement->GetObservable( cthetalName )->GetValue();
		phi    = measurement->GetObservable( phihName )->GetValue();  // Pi offset is difference between angle calculator and "Our Paper"
	}
	else {
		cos1   = measurement->GetObservable( cosPsiName )->GetValue();
		cos2   = measurement->GetObservable( cosThetaName )->GetValue();
		phi    = measurement->GetObservable( phiName )->GetValue();
	}

	double returnValue = 0;
	double val1=-1., val2=-1.;
	//Deal with propertime resolution
	if( timeResLL1Frac >= 0.9999 )
	{
		// Set the member variable for time resolution to the first value and calculate
		sigmaLL = sigmaLL1;
		returnValue =  this->buildPDFnumerator(measurement) ;
	}
	else
	{
		// Set the member variable for time resolution to the first value and calculate
		sigmaLL = sigmaLL1;
		val1 = this->buildPDFnumerator(measurement);
		// Set the member variable for time resolution to the second value and calculate
		sigmaLL = sigmaLL2;
		val2 = this->buildPDFnumerator(measurement);
		returnValue = (timeResLL1Frac*val1 + (1. - timeResLL1Frac)*val2) ;
	}

	if (returnValue <= 0)
	{
		cout << "PDF returns zero!" << endl;
	}

	return returnValue;
}


//.............................................................
// Core calculation of PDF value
double LongLivedBkg_3Dangular::buildPDFnumerator(DataPoint * measurement)
{
	_datapoint = measurement;
	// Sum of two exponentials, using the time resolution functions

	double returnValue = 0.;

	tlow = measurement->GetPhaseSpaceBoundary()->GetConstraint( timeName )->GetMinimum();
	thigh = measurement->GetPhaseSpaceBoundary()->GetConstraint( timeName )->GetMaximum();

	double val1=-1., val2=-1., norm1=-1., norm2=-1.;
	if( f_LL1 >= 0.9999 ) {
		if( tauLL1 <= 0 ) {
			cout << " In LongLivedBkg_3Dangular() you gave a negative or zero lifetime for tauLL1 " << endl ;
			exit(1) ;
		}
		val1 = Mathematics::Exp(time, 1./tauLL1, sigmaLL);
		norm1= Mathematics::ExpInt(tlow, thigh, 1./tauLL1, sigmaLL);
		returnValue = val1 /norm1 ;
	}
	else {
		if( (tauLL1 <= 0) ||  (tauLL2 <= 0) ) {
			cout << " In LongLivedBkg_3Dangular() you gave a negative or zero lifetime for tauLL1/2 " << endl ;
			exit(1) ;
		}
		val1 = Mathematics::Exp(time, 1./tauLL1, sigmaLL);
		norm1= Mathematics::ExpInt(tlow, thigh, 1./tauLL1, sigmaLL);
		val2 = Mathematics::Exp(time, 1./tauLL2, sigmaLL);
		norm2= Mathematics::ExpInt(tlow, thigh, 1./tauLL2, sigmaLL);
		returnValue = f_LL1 * val1/norm1 + (1. - f_LL1) * val2/norm2;
	}

	returnValue *= angularFactor();

	return returnValue;
}


//..............................................................
// Normlisation
double LongLivedBkg_3Dangular::Normalisation(PhaseSpaceBoundary * boundary)
{

	return 1. ;

	/*
	   IConstraint * timeBound = boundary->GetConstraint( timeconstName );
	   if ( timeBound->GetUnit() == "NameNotFoundError" )
	   {
	   cerr << "Bound on time not provided" << endl;
	   return -1.;
	   }
	   else
	   {
	   tlow = timeBound->GetMinimum();
	   thigh = timeBound->GetMaximum();
	   }

	   double returnValue = 0;

	   if( timeResLL1Frac >= 0.9999 )
	   {
	// Set the member variable for time resolution to the first value and calculate
	sigmaLL = sigmaLL1;
	returnValue = buildPDFdenominator();
	}
	else
	{
	// Set the member variable for time resolution to the first value and calculate
	sigmaLL = sigmaLL1;
	double val1 = buildPDFdenominator();
	// Set the member variable for time resolution to the second value and calculate
	sigmaLL = sigmaLL2;
	double val2 = buildPDFdenominator();
	returnValue =  timeResLL1Frac*val1 + (1. - timeResLL1Frac)*val2;
	}

	return returnValue ;
	*/
}

//.............................................................
//
double LongLivedBkg_3Dangular::buildPDFdenominator()
{
	return 1. ;
	/*
	// Sum of two exponentials, using the time resolution functions

	double returnValue = 0;

	if( f_LL1 >= 0.9999 ) {
	if( tauLL1 <= 0 ) {
	cout << " In LongLivedBkg_3Dangular() you gave a negative or zero lifetime for tauLL1 " << endl ;
	exit(1) ;
	}
	returnValue = Mathematics::ExpInt(tlow, thigh, 1./tauLL1, sigmaLL);

	}
	else {
	if( (tauLL1 <= 0) ||  (tauLL2 <= 0) ) {
	cout << " In LongLivedBkg_3Dangular() you gave a negative or zero lifetime for tauLL1/2 " << endl ;
	exit(1) ;
	}
	double val1 = Mathematics::ExpInt(tlow, thigh, 1./tauLL1, sigmaLL);
	double val2 = Mathematics::ExpInt(tlow, thigh, 1./tauLL2, sigmaLL);
	returnValue = f_LL1 * val1 + (1. - f_LL1) * val2;
	}

	//This PDF only works for full angular phase space= 8pi factor included in the factors in the Evaluate() method - so no angular normalisation term.
	return returnValue ;
	*/

}


//................................................................
//Angular distribution function
double LongLivedBkg_3Dangular::angularFactor( )
{
	if( useFlatAngularDistribution ) return 0.125 * Mathematics::_Over_PI();

	double returnValue=0.;

	int globalbin=-1;
	int xbin=-1, ybin=-1, zbin=-1;
	double num_entries_bin=-1.;

	Observable* cos1Obs = NULL;

	if( !_useHelicityBasis ) cos1Obs = _datapoint->GetObservable( cosPsiName );
	else cos1Obs = _datapoint->GetObservable( cthetakName );

	if( cos1Obs->GetBkgBinNumber() > -1 )
	{
		return cos1Obs->GetBkgAcceptance();
	}
	else
	{
		Observable* cos2Obs = NULL;
		Observable* phiObs = NULL;
		if( _useHelicityBasis ) {
			cos2Obs   = _datapoint->GetObservable( cthetalName );
			phiObs    = _datapoint->GetObservable( phihName );  // Pi offset is difference between angle calculator and "Our Paper"
		}
		else {
			cos2Obs   = _datapoint->GetObservable( cosThetaName );
			phiObs    = _datapoint->GetObservable( phiName );
		}

		//Find global bin number for values of angles, find number of entries per bin, divide by volume per bin and normalise with total number of entries in the histogram
		xbin = xaxis->FindFixBin( cos1 ); if( xbin > nxbins ) xbin = nxbins;
		ybin = yaxis->FindFixBin( cos2 ); if( ybin > nybins ) ybin = nybins;
		zbin = zaxis->FindFixBin( phi ); if( zbin > nzbins ) zbin = nzbins;

		globalbin = histo->GetBin( xbin, ybin, zbin );
		num_entries_bin = histo->GetBinContent(globalbin);

		//Angular factor normalized with phase space of histogram and total number of entries in the histogram
		returnValue = num_entries_bin / (deltax * deltay * deltaz) / total_num_entries ;

		cos1Obs->SetBkgBinNumber( xbin ); cos1Obs->SetBkgAcceptance( returnValue );
		cos2Obs->SetBkgBinNumber( ybin ); cos2Obs->SetBkgAcceptance( returnValue );
		phiObs->SetBkgBinNumber( zbin ); phiObs->SetBkgAcceptance( returnValue );
	}

	return returnValue;
}


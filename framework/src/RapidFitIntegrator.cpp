/**
  @class RapidFitIntegrator

  A numerical integrator to be used for projecting the PDF, and as a fallback if the PDF does not provide its own normalisation
  This class uses two integrator classes provided by root: AdaptiveIntegratorMultiDim and GaussLegendreIntegrator.
  Both of these assume the function to be integrated is well behaved.

  @author Benjamin M Wynne bwynne@cern.ch
  @date 2009-10-8
 */

//	ROOT Headers
#include "RVersion.h"
//	RapidFit Headers
#include "RapidFitIntegrator.h"
#include "StringProcessing.h"
#include "StatisticsFunctions.h"
#include "ClassLookUp.h"
#include "NormalisedSumPDF.h"
#include "DebugClass.h"
#include "Threading.h"
//	System Headers
#include <iostream>
#include <iomanip>
#include <cmath>
#include <stdlib.h>
#include <float.h>
#include <pthread.h>
#include <exception>
#include <pthread.h>

#ifdef __RAPIDFIT_USE_GSL
//	GSL for MC integration
#include <gsl/gsl_qrng.h>
//	GSLMCIntegrator from ROOT
#include "Math/GSLMCIntegrator.h"
#endif

pthread_mutex_t multi_mutex;
pthread_mutex_t multi_mutex2;
pthread_mutex_t multi_mutex3;

pthread_mutex_t gsl_mutex;

//#define DOUBLE_TOLERANCE DBL_MIN
#define DOUBLE_TOLERANCE 1E-6

using namespace::std;

//1% tolerance on integral value
//const double INTEGRAL_PRECISION_THRESHOLD = 0.01;

//Constructor with correct argument
RapidFitIntegrator::RapidFitIntegrator( IPDF * InputFunction, bool ForceNumerical, bool UsePseudoRandomIntegration ) :
	ratioOfIntegrals(-1.), fastIntegrator(NULL), functionToWrap(InputFunction), multiDimensionIntegrator(NULL), oneDimensionIntegrator(NULL),
	functionCanIntegrate(false), haveTestedIntegral(false), num_threads(4),
	RapidFitIntegratorNumerical( ForceNumerical ), obs_check(false), checked_list(), debug(new DebugClass(false) ), pseudoRandomIntegration( UsePseudoRandomIntegration )
{
	multiDimensionIntegrator = new AdaptiveIntegratorMultiDim();
#if ROOT_VERSION_CODE > ROOT_VERSION(5,28,0)
	multiDimensionIntegrator->SetAbsTolerance( 1E-9 );      //      Absolute error for things such as plots
	multiDimensionIntegrator->SetRelTolerance( 1E-9 );
	multiDimensionIntegrator->SetMaxPts( 1000000 );		//	These are the defaults, and it's unlikely you will be able to realistically push the integral without using "double double"'s
#endif

	ROOT::Math::IntegrationOneDim::Type type = ROOT::Math::IntegrationOneDim::kGAUSS;
	oneDimensionIntegrator = new IntegratorOneDim(type);
}

RapidFitIntegrator::RapidFitIntegrator( const RapidFitIntegrator& input ) : ratioOfIntegrals( input.ratioOfIntegrals ),
	fastIntegrator( NULL ), functionToWrap( input.functionToWrap ), multiDimensionIntegrator( NULL ), oneDimensionIntegrator( NULL ),
	pseudoRandomIntegration(input.pseudoRandomIntegration), functionCanIntegrate( input.functionCanIntegrate ), haveTestedIntegral( true ),
	RapidFitIntegratorNumerical( input.RapidFitIntegratorNumerical ), obs_check( input.obs_check ), checked_list( input.checked_list ),
	debug((input.debug==NULL)?NULL:new DebugClass(*input.debug)), num_threads(input.num_threads)
{
	//	We don't own the PDF so no need to duplicate it as we have to be told which one to use
	//if( input.functionToWrap != NULL ) functionToWrap = ClassLookUp::CopyPDF( input.functionToWrap );

	//	Only Construct the Integrators if they are required
	if( input.fastIntegrator != NULL ) fastIntegrator = new FoamIntegrator( *(input.fastIntegrator) );
	if( input.multiDimensionIntegrator != NULL )
	{
		multiDimensionIntegrator = new AdaptiveIntegratorMultiDim();// *(input.multiDimensionIntegrator) );//new AdaptiveIntegratorMultiDim();
		//      These functions only exist with ROOT > 5.27 I think, at least they exist in 5.28/29
#if ROOT_VERSION_CODE > ROOT_VERSION(5,28,0)
		multiDimensionIntegrator->SetAbsTolerance( 1E-9 );      //      Absolute error for things such as plots
		multiDimensionIntegrator->SetRelTolerance( 1E-9 );
		multiDimensionIntegrator->SetMaxPts( 1000000 );
#endif
	}
	if( input.oneDimensionIntegrator != NULL )
	{
		ROOT::Math::IntegrationOneDim::Type type = ROOT::Math::IntegrationOneDim::kGAUSS;
		(void) type;	//	For the global state of ROOT
		oneDimensionIntegrator = new IntegratorOneDim( );//*(input.oneDimensionIntegrator) );
	}
}

bool RapidFitIntegrator::GetUseGSLIntegrator() const
{
	return pseudoRandomIntegration;
}

void RapidFitIntegrator::SetUseGSLIntegrator( bool input )
{
	pseudoRandomIntegration = input;
	if( debug != NULL )
	{
		if( debug->DebugThisClass( "RapidFitIntegrator" ) )
		{
			if( input ) cout << "Requesting GSL." << endl;
		}
	}
}

//	Don't want the projections to be insanely accurate
void RapidFitIntegrator::ProjectionSettings()
{
	//	These functions only exist with ROOT > 5.27 I think, at least they exist in 5.28/29
#if ROOT_VERSION_CODE > ROOT_VERSION(5,28,0)
	multiDimensionIntegrator->SetAbsTolerance( 1E-4 );	//	Absolute error for things such as plots
	multiDimensionIntegrator->SetRelTolerance( 1E-4 );
	multiDimensionIntegrator->SetMaxPts( 10000 );
#endif
}

RapidFitIntegrator::~RapidFitIntegrator()
{
	if( multiDimensionIntegrator != NULL ) delete multiDimensionIntegrator;
	//if( oneDimensionIntegrator != NULL ) delete oneDimensionIntegrator;
	if( fastIntegrator != NULL ) delete fastIntegrator;
	if( debug != NULL ) delete debug;
}

//Return the integral over all observables
double RapidFitIntegrator::Integral( DataPoint * NewDataPoint, PhaseSpaceBoundary * NewBoundary )
{
	NewDataPoint->SetPhaseSpaceBoundary( NewBoundary );
	if( functionToWrap == NULL )
	{
		cerr << "WHAT ARE YOU DOING TO ME, YOUR TRYING TO INTEGRATE OVER A NULL OBJECT!!!" << endl;
		exit(-69);
	}

	double PDF_test_result = 0.;

	//cout << "R:here1" << endl;

	try
	{
		//	cout << "R:here1b" << endl;
		//	cout << functionToWrap << endl;
		PDF_test_result = functionToWrap->Integral( NewDataPoint, NewBoundary );
		//	cout << "R:here1b2" << endl;
	}
	catch(...)
	{
		cerr << "RapidFitIntegrator: PDF " << functionToWrap->GetLabel() << " has failed the basic test of not crashing, check your PhaseSpace and/or return -1 for Numerical Integration" << endl;
		throw(1280);
	}

	//cout << "PDF: " << PDF_test_result << endl;

	functionCanIntegrate = !functionToWrap->GetNumericalNormalisation();

	if( functionCanIntegrate == false )
	{
		//	PDF doesn't know how to Normalise no need to check PDF!
		haveTestedIntegral = true;
	}

	//cout << "R:here" << endl;

	bool cacheEnabled = functionToWrap->GetCachingEnabled();

	//Test the integration method the function has provided
	if( haveTestedIntegral )
	{
		//If the function has been tested already, use the result
		if( functionCanIntegrate && !RapidFitIntegratorNumerical )
		{
			return PDF_test_result;
		}
		else
		{
			double return_value = -5.;
			if( functionToWrap->CacheValid( NewDataPoint, NewBoundary ) )
			{
				return_value = PDF_test_result;
			}
			else
			{
				//Make a list of observables not to integrate
				vector<string> dontIntegrate = this->DontNumericallyIntegrateList( NewDataPoint );

				return_value = this->NumericallyIntegrateDataPoint( NewDataPoint, NewBoundary, dontIntegrate );

				if( cacheEnabled )
				{
					functionToWrap->SetCache( return_value, NewDataPoint, NewBoundary );
				}
			}
			return return_value;
		}
	}
	else
	{
		//cout << "R:here2" << endl;
		double returnVal = TestIntegral( NewDataPoint, NewBoundary );
		return returnVal;
	}
	return -1;
}

vector<string> RapidFitIntegrator::DontNumericallyIntegrateList( const DataPoint* NewDataPoint, vector<string> input )
{
	//Make a list of observables not to integrate
	vector<string> dontIntegrate;

	//	If the Observables haven't been checked, check them.
	if( !obs_check )
	{
		vector<string> PDF_params = functionToWrap->GetPrototypeDataPoint();
		vector<string> DataPoint_params;
		if( NewDataPoint != NULL ) DataPoint_params = NewDataPoint->GetAllNames();
		vector<string>::iterator data_i = DataPoint_params.begin();

		vector<string> not_found_list;
		vector<string> not_integrable = functionToWrap->GetDoNotIntegrateList();
		for( ; data_i != DataPoint_params.end(); ++data_i )
		{
			if( StringProcessing::VectorContains( &PDF_params, &(*data_i) ) == -1 )
			{
				not_found_list.push_back( *data_i );
			}
		}
		checked_list = StringProcessing::CombineUniques( not_found_list, not_integrable );
		obs_check = true;
	}

	//	Observables have been checked, set the dontIntegrate list to be the same as the checked list
	dontIntegrate = StringProcessing::CombineUniques( checked_list, input );

	return dontIntegrate;
}

double RapidFitIntegrator::TestIntegral( DataPoint * NewDataPoint, PhaseSpaceBoundary * NewBoundary )
{
	NewDataPoint->SetPhaseSpaceBoundary( NewBoundary );
	//Make a list of observables not to integrate
	vector<string> dontIntegrate = functionToWrap->GetDoNotIntegrateList();

	double testIntegral = 0.;
	try
	{
		testIntegral = functionToWrap->Integral( NewDataPoint, NewBoundary );
	}
	catch(...)
	{
		cerr << "RapidFitIntegrator: PDF " << functionToWrap->GetLabel() << " Failed to Integrate Correctly" << endl;
		throw(-762);
	}
	double numericalIntegral = 0.;

	int NumberCombinations = NewBoundary->GetNumberCombinations();

	if( NumberCombinations == 1 || NumberCombinations == 0 )
	{
		numericalIntegral = this->NumericallyIntegrateDataPoint( NewDataPoint, NewBoundary, dontIntegrate );
	}
	else
	{
		numericalIntegral = this->NumericallyIntegratePhaseSpace( NewBoundary, dontIntegrate );
	}

	//Check if the function has an integrate method
	if( testIntegral > 0.0 )
	{
		//Check if the function's integrate method agrees with the numerical integral
		//Trust the function's integration
		ratioOfIntegrals = numericalIntegral/testIntegral;
		functionCanIntegrate = true;
		functionToWrap->SetCache( testIntegral, NewDataPoint, NewBoundary );

		haveTestedIntegral = true;
		return testIntegral;
	}
	haveTestedIntegral = true;
	return this->NumericallyIntegrateDataPoint( NewDataPoint, NewBoundary, dontIntegrate );
}

double RapidFitIntegrator::GetRatioOfIntegrals()
{
	return ratioOfIntegrals;
}

IPDF * RapidFitIntegrator::GetPDF() const
{
	return functionToWrap;
}

void RapidFitIntegrator::SetPDF( IPDF* input )
{
	functionToWrap = input;
}

double RapidFitIntegrator::OneDimentionIntegral( IPDF* functionToWrap, IntegratorOneDim * oneDimensionIntegrator, const DataPoint * NewDataPoint, const PhaseSpaceBoundary * NewBoundary,
		ComponentRef* componentIndex, vector<string> doIntegrate, vector<string> dontIntegrate, bool haveTestedIntegral, DebugClass* debug )
{
	(void) haveTestedIntegral;
	IntegratorFunction* quickFunction = new IntegratorFunction( functionToWrap, NewDataPoint, doIntegrate, dontIntegrate, NewBoundary, componentIndex );
	if( debug != NULL ) quickFunction->SetDebug( debug );
	//Find the observable range to integrate over
	IConstraint * newConstraint = NULL;
	try
	{
		newConstraint = NewBoundary->GetConstraint( doIntegrate[0] );
	}
	catch(...)
	{
		cerr << "RapidFitIntegrator: Could NOT find required Constraint " << doIntegrate[0] << " in Data PhaseSpaceBoundary" << endl;
		cerr << "RapidFitIntegrator: Please Fix this by Adding the Constraint to your PhaseSpace for PDF: " << functionToWrap->GetLabel() << endl;
		cerr << endl;
		exit(-8737);
	}
	//IConstraint * newConstraint = NewBoundary->GetConstraint( doIntegrate[0] );
	double minimum = newConstraint->GetMinimum();
	double maximum = newConstraint->GetMaximum();

	//cout << "1D Integration" << endl;
	//Do a 1D integration
	oneDimensionIntegrator->SetFunction(*quickFunction);

	//cout << "Performing 1DInt" << endl;
	double output = oneDimensionIntegrator->Integral( minimum, maximum );
	//cout << output << endl;

	delete quickFunction;

	return output;
}

double RapidFitIntegrator::PseudoRandomNumberIntegral( IPDF* functionToWrap, const DataPoint * NewDataPoint, const PhaseSpaceBoundary * NewBoundary,
		ComponentRef* componentIndex, vector<string> doIntegrate, vector<string> dontIntegrate )
{
#ifdef __RAPIDFIT_USE_GSL

	//Make arrays of the observable ranges to integrate over
	double* minima = new double[ doIntegrate.size() ];
	double* maxima = new double[ doIntegrate.size() ];

	for (unsigned int observableIndex = 0; observableIndex < doIntegrate.size(); ++observableIndex )
	{
		IConstraint * newConstraint = NULL;
		try
		{
			newConstraint = NewBoundary->GetConstraint( doIntegrate[observableIndex] );
		}
		catch(...)
		{
			cerr << "RapidFitIntegrator: Could NOT find required Constraint " << doIntegrate[observableIndex] << " in Data PhaseSpaceBoundary" << endl;
			cerr << "RapidFitIntegrator: Please Fix this by Adding the Constraint to your PhaseSpace for PDF: " << functionToWrap->GetLabel() << endl;
			cerr << endl;
			exit(-8737);
		}
		minima[observableIndex] = (double)newConstraint->GetMinimum();
		maxima[observableIndex] = (double)newConstraint->GetMaximum();
	}

	//unsigned int npoint = 10000;
	unsigned int npoint = 100000;
	vector<double> * integrationPoints = new std::vector<double>[doIntegrate.size()];

	gsl_qrng * q = gsl_qrng_alloc (gsl_qrng_sobol, int(doIntegrate.size()));
	for (unsigned int i = 0; i < npoint; i++)
	{
		double v[doIntegrate.size()];
		gsl_qrng_get(q, v);
		for ( unsigned int j = 0; j < doIntegrate.size(); j++)
		{
			integrationPoints[j].push_back(v[j]);
		}
	}
	gsl_qrng_free(q);

	vector<double> minima_v, maxima_v;
	for( unsigned int i=0; i< doIntegrate.size(); ++i )
	{
		minima_v.push_back( minima[i] );
		maxima_v.push_back( maxima[i] );
	}
	IntegratorFunction* quickFunction = new IntegratorFunction( functionToWrap, NewDataPoint, doIntegrate, dontIntegrate, NewBoundary, componentIndex, minima_v, maxima_v );

	double result = 0.;
	for (unsigned int i = 0; i < integrationPoints[0].size(); ++i)
	{
		double* point = new double[ doIntegrate.size() ];
		for ( unsigned int j = 0; j < doIntegrate.size(); j++)
		{
			//cout << doIntegrate[j] << " " << maxima[j] << " " << minima[j] << " " << integrationPoints[j][i] << endl;
			point[j] = integrationPoints[j][i]*(maxima[j]-minima[j])+minima[j];
		}
		result += quickFunction->DoEval( point );
		delete[] point;
	}
	double factor=1.;
	for( unsigned int i=0; i< (unsigned)doIntegrate.size(); ++i )
	{
		factor *= maxima[i]-minima[i];
	}
	result /= ( (double)integrationPoints[0].size() / factor );

	//cout << "GSL :D" << endl;

	delete[] minima; delete[] maxima;
	delete[] integrationPoints;
	delete quickFunction;
	return result;
#else
	(void) functionToWrap; (void) NewDataPoint; (void) NewBoundary; (void) componentIndex; (void) doIntegrate; (void) dontIntegrate;
	return -1.;
#endif
}


double RapidFitIntegrator::PseudoRandomNumberIntegralThreaded( IPDF* functionToWrap, const DataPoint * NewDataPoint, const PhaseSpaceBoundary * NewBoundary,
		ComponentRef* componentIndex, vector<string> doIntegrate, vector<string> dontIntegrate, unsigned int num_threads )
{
#ifdef __RAPIDFIT_USE_GSL

	//Make arrays of the observable ranges to integrate over
	double* minima = new double[ doIntegrate.size() ];
	double* maxima = new double[ doIntegrate.size() ];

	for (unsigned int observableIndex = 0; observableIndex < doIntegrate.size(); ++observableIndex )
	{
		IConstraint * newConstraint = NULL;
		try
		{
			newConstraint = NewBoundary->GetConstraint( doIntegrate[observableIndex] );
		}
		catch(...)
		{
			cerr << "RapidFitIntegrator: Could NOT find required Constraint " << doIntegrate[observableIndex] << " in Data PhaseSpaceBoundary" << endl;
			cerr << "RapidFitIntegrator: Please Fix this by Adding the Constraint to your PhaseSpace for PDF: " << functionToWrap->GetLabel() << endl;
			cerr << endl;
			exit(-8737);
		}
		minima[observableIndex] = (double)newConstraint->GetMinimum();
		maxima[observableIndex] = (double)newConstraint->GetMaximum();
	}

	unsigned int npoint = 1000000;
	std::vector<double> * integrationPoints = new std::vector<double>[doIntegrate.size()];

	//pthread_mutex_lock( &gsl_mutex );
	//cout << "Allocating GSL Integration Tool" << endl;
	gsl_qrng * q = NULL;
	try
	{
		q = gsl_qrng_alloc( gsl_qrng_sobol, (unsigned)doIntegrate.size() );
	}
	catch(...)
	{
		cout << "Can't Allocate Integration Tool for GSL Integral." << endl;
		cout << "Using: " <<functionToWrap->GetLabel() << " Dim: " << doIntegrate.size() << endl;
		exit(-742);
	}

	if( q == NULL )
	{
		cout << "Can't Allocate Integration Tool for GSL Integral." << endl;
		cout << "Using: " <<functionToWrap->GetLabel() << " Dim: " << doIntegrate.size() << endl;
		exit(-741);
	}

	for (unsigned int i = 0; i < npoint; i++)
	{
		double v[doIntegrate.size()];
		gsl_qrng_get( q, v );
		for( unsigned int j = 0; j < (unsigned)doIntegrate.size(); j++)
		{
			integrationPoints[j].push_back(v[j]);
		}
	}
	gsl_qrng_free(q);
	//cout << "Freed GSL Integration Tool" << endl;
	//pthread_mutex_unlock( &gsl_mutex );

	vector<double> minima_v, maxima_v;
	for( unsigned int i=0; i< doIntegrate.size(); ++i )
	{
		minima_v.push_back( minima[i] );
		maxima_v.push_back( maxima[i] );
	}
	cout << "Constructing Functions" << endl;
	IntegratorFunction* quickFunction = new IntegratorFunction( functionToWrap, NewDataPoint, doIntegrate, dontIntegrate, NewBoundary, componentIndex, minima_v, maxima_v );

	vector<double*> doEval_points;
	for (unsigned int i = 0; i < integrationPoints[0].size(); ++i)
	{
		double* point = new double[ doIntegrate.size() ];
		for ( unsigned int j = 0; j < doIntegrate.size(); j++)
		{
			//cout << doIntegrate[j] << " " << maxima[j] << " " << minima[j] << " " << integrationPoints[j][i] << endl;
			point[j] = integrationPoints[j][i]*(maxima[j]-minima[j])+minima[j];
		}
		doEval_points.push_back( point );
		//result += quickFunction->DoEval( point );
		//delete[] point;
	}
	//result /= double(integrationPoints[0].size());

	//      Construct Thread Objects
	pthread_t* Thread = new pthread_t[ num_threads ];
	pthread_attr_t attrib;

	//	Construct Integrator Functions (1 per theread)
	vector<IntegratorFunction*> evalFunctions;
	for( unsigned int i=0; i< num_threads; ++i ) evalFunctions.push_back( new IntegratorFunction(
				ClassLookUp::CopyPDF( functionToWrap ), NewDataPoint, doIntegrate, dontIntegrate, NewBoundary, componentIndex, minima_v, maxima_v ) );

	//	Split Points to evaluate between threads
	vector<vector<double*> > eval_perThread = Threading::divideDataNormalise( doEval_points, num_threads );

	//	Construct 'structs' to be passed to each thread
	struct Normalise_Thread* fit_thread_data = new Normalise_Thread[ num_threads ];
	for( unsigned int i=0; i< num_threads; ++i )
	{
		fit_thread_data[i].function = evalFunctions[i];
		fit_thread_data[i].normalise_points = eval_perThread[i];
	}

	//	Construct Threads
	//      Threads HAVE to be joinable
	//      We CANNOT _AND_SHOULD_NOT_ ***EVER*** return information to Minuit without the results from ALL threads successfully returned
	//      Not all pthread implementations are required to obey this as default when constructing the thread _SO_BE_EXPLICIT_
	pthread_attr_init(&attrib);
	pthread_attr_setdetachstate(&attrib, PTHREAD_CREATE_JOINABLE);

	//cout << "Creating Threads" << endl;
	//      Create the Threads and set them to be joinable
	for( unsigned int threadnum=0; threadnum< num_threads; ++threadnum )
	{
		int status = pthread_create(&Thread[threadnum], &attrib, RapidFitIntegrator::ThreadWork, (void *) &fit_thread_data[threadnum] );
		if( status )
		{
			cerr << "ERROR:\tfrom pthread_create()\t" << status << "\t...Exiting\n" << endl;
			exit(-1);
		}
	}

	//	Join Threads and Start Running
	//cout << "Joining Threads!!" << endl;
	//      Join the Threads
	for( unsigned int threadnum=0; threadnum< num_threads; ++threadnum )
	{
		int status = pthread_join( Thread[threadnum], NULL);
		if( status )
		{
			cerr << "Error Joining a Thread:\t" << threadnum << "\t:\t" << status << "\t...Exiting\n" << endl;
		}
	}

	//      Do some cleaning Up
	pthread_attr_destroy(&attrib);

	//	Calculate the total Integral
	double result=0.;
	for( unsigned int threadnum=0; threadnum< num_threads; ++threadnum )
	{
		for( unsigned int point_num=0; point_num< fit_thread_data[threadnum].dataPoint_Result.size(); ++point_num )
		{
			result+= fit_thread_data[threadnum].dataPoint_Result[ point_num ];
		}
		fit_thread_data[threadnum].dataPoint_Result.clear();
	}
        double factor=1.;
        for( unsigned int i=0; i< (unsigned)doIntegrate.size(); ++i )
        {
                factor *= maxima[i]-minima[i];
        }
        result /= ( (double)integrationPoints[0].size() / factor );

	//cout << "Hello :D" << endl;

	delete[] minima; delete[] maxima;
	delete[] integrationPoints;
	while( !evalFunctions.empty() )
	{
		if( evalFunctions.back() != NULL ) delete evalFunctions.back();
		evalFunctions.pop_back();
	}
	while( !doEval_points.empty() )
	{
		if( doEval_points.back() != NULL ) delete doEval_points.back();
		doEval_points.pop_back();
	}
	delete[] fit_thread_data;
	delete[] Thread;
	delete quickFunction;
	return result;
#else
	(void) functionToWrap; (void) NewDataPoint; (void) NewBoundary; (void) componentIndex; (void) doIntegrate; (void) dontIntegrate; (void) num_threads;
	return -1.;
#endif
}

void* RapidFitIntegrator::ThreadWork( void* inputData )
{
	struct Normalise_Thread* thread_object = (struct Normalise_Thread*) inputData;
	for( unsigned int i=0; i< (unsigned)thread_object->normalise_points.size(); ++i )
	{
		thread_object->dataPoint_Result.push_back( thread_object->function->DoEval( thread_object->normalise_points[i] ) );
	}
	//      Finished evaluating this thread
	pthread_exit( NULL );
}

double RapidFitIntegrator::MultiDimentionIntegral( IPDF* functionToWrap, AdaptiveIntegratorMultiDim* multiDimensionIntegrator, const DataPoint * NewDataPoint, const PhaseSpaceBoundary * NewBoundary,
		ComponentRef* componentIndex, vector<string> doIntegrate, vector<string> dontIntegrate, bool haveTestedIntegral, DebugClass* debug )
{
	(void) haveTestedIntegral;
	//Make arrays of the observable ranges to integrate over
	double* minima = new double[ doIntegrate.size() ];
	double* maxima = new double[ doIntegrate.size() ];

	for (unsigned int observableIndex = 0; observableIndex < doIntegrate.size(); ++observableIndex )
	{
		IConstraint * newConstraint = NULL;
		try
		{
			newConstraint = NewBoundary->GetConstraint( doIntegrate[observableIndex] );
		}
		catch(...)
		{
			cerr << "RapidFitIntegrator: Could NOT find required Constraint " << doIntegrate[observableIndex] << " in Data PhaseSpaceBoundary" << endl;
			cerr << "RapidFitIntegrator: Please Fix this by Adding the Constraint to your PhaseSpace for PDF: " << functionToWrap->GetLabel() << endl;
			cerr << endl;
			exit(-8737);
		}
		minima[observableIndex] = (double)newConstraint->GetMinimum();
		maxima[observableIndex] = (double)newConstraint->GetMaximum();
	}

	//Do a 2-15D integration

	vector<double> minima_v, maxima_v;
	for( unsigned int i=0; i< doIntegrate.size(); ++i )
	{
		minima_v.push_back( minima[i] );
		maxima_v.push_back( maxima[i] );
	}

	IntegratorFunction* quickFunction = new IntegratorFunction( functionToWrap, NewDataPoint, doIntegrate, dontIntegrate, NewBoundary, componentIndex, minima_v, maxima_v );
	if( debug != NULL ) quickFunction->SetDebug( debug );

	multiDimensionIntegrator->SetFunction( *quickFunction );

	double output =  multiDimensionIntegrator->Integral( minima, maxima );

	delete[] minima; delete[] maxima;
	delete quickFunction;

	return output;
}

//	Integrate over all possible discrete combinations within the phase-space
double RapidFitIntegrator::NumericallyIntegratePhaseSpace( PhaseSpaceBoundary* NewBoundary, vector<string> DontIntegrateThese, ComponentRef* componentIndex )
{
	return this->DoNumericalIntegral( NULL, NewBoundary, DontIntegrateThese, componentIndex, false );
}

//	Integrate over the given phase space using this given DataPoint. i.e. ONLY 1 UNIQUE discrete combination
double RapidFitIntegrator::NumericallyIntegrateDataPoint( DataPoint* NewDataPoint, PhaseSpaceBoundary* NewBoundary, vector<string> DontIntegrateThese, ComponentRef* componentIndex )
{
	return this->DoNumericalIntegral( NewDataPoint, NewBoundary, DontIntegrateThese, componentIndex, true );
}

//Actually perform the numerical integration
double RapidFitIntegrator::DoNumericalIntegral( const DataPoint * NewDataPoint, PhaseSpaceBoundary * NewBoundary, const vector<string> DontIntegrateThese, ComponentRef* componentIndex, const bool IntegrateDataPoint )
{
	//Make lists of observables to integrate and not to integrate
	vector<string> doIntegrate, dontIntegrate;
	StatisticsFunctions::DoDontIntegrateLists( functionToWrap, NewBoundary, &DontIntegrateThese, doIntegrate, dontIntegrate );

	dontIntegrate = StringProcessing::CombineUniques( dontIntegrate, DontIntegrateThese );
	dontIntegrate = this->DontNumericallyIntegrateList( NewDataPoint, dontIntegrate );

	vector<DataPoint*> DiscreteIntegrals;

	vector<string> required = functionToWrap->GetPrototypeDataPoint();
	bool isFixed=true;
	for( unsigned int i=0; i< required.size(); ++i )
	{
		isFixed = isFixed && NewBoundary->GetConstraint( required[i] )->IsDiscrete();
	}
	if( isFixed )
	{
		if( NewDataPoint != NULL )
		{
			DataPoint* thisDataPoint = new DataPoint( *NewDataPoint );
			thisDataPoint->SetPhaseSpaceBoundary( NewBoundary );
			double returnVal = functionToWrap->Evaluate( thisDataPoint );
			delete thisDataPoint;
			return returnVal;
		}
		else
		{
			DiscreteIntegrals = NewBoundary->GetDiscreteCombinations();
			double returnVal=0.;
			for( unsigned int i=0; i< DiscreteIntegrals.size(); ++i )
			{
				returnVal+=functionToWrap->Evaluate( DiscreteIntegrals[i] );
			}
			while( !DiscreteIntegrals.empty() )
			{
				if( DiscreteIntegrals.back() != NULL ) delete DiscreteIntegrals.back();
				DiscreteIntegrals.pop_back();
			}
			return returnVal;
		}
	}

	if( IntegrateDataPoint )
	{
		DiscreteIntegrals.push_back( new DataPoint(*NewDataPoint) );
		DiscreteIntegrals.back()->SetPhaseSpaceBoundary( NewBoundary );
	}
	else
	{
		DiscreteIntegrals = NewBoundary->GetDiscreteCombinations();
	}

	double output_val = 0.;

	//If there are no observables left to integrate over, just evaluate the function
	if( doIntegrate.empty() || doIntegrate.size() == 0 )
	{
		for( vector<DataPoint*>::iterator dataPoint_i = DiscreteIntegrals.begin(); dataPoint_i != DiscreteIntegrals.end(); ++dataPoint_i )
		{
			output_val += functionToWrap->Integral( *dataPoint_i, NewBoundary );
		}
	}
	else
	{
		for( vector<DataPoint*>::iterator dataPoint_i = DiscreteIntegrals.begin(); dataPoint_i != DiscreteIntegrals.end(); ++dataPoint_i )
		{
			double numericalIntegral = 0.;
			//Chose the one dimensional or multi-dimensional method
			if( doIntegrate.size() == 1 )
			{
				numericalIntegral += this->OneDimentionIntegral( functionToWrap, oneDimensionIntegrator, *dataPoint_i, NewBoundary, componentIndex, doIntegrate, dontIntegrate, debug );
				//cout << "ret: " << numericalIntegral << endl;
			}
			else
			{
				if( !pseudoRandomIntegration )
				{
					numericalIntegral += this->MultiDimentionIntegral( functionToWrap, multiDimensionIntegrator, *dataPoint_i, NewBoundary, componentIndex, doIntegrate, dontIntegrate, debug );
				}
				else
				{
					if( debug != NULL )
					{
						if( debug->DebugThisClass( "RapidFitIntegrator" ) )
						{
							cout << "RapidFitIntegrator: Using GSL PseudoRandomNumber :D" << endl;
						}
					}
					numericalIntegral += this->PseudoRandomNumberIntegral( functionToWrap, *dataPoint_i, NewBoundary, componentIndex, doIntegrate, dontIntegrate );
					if( numericalIntegral < 0 )
					{
						cout << "Calculated a -ve Integral: " << numericalIntegral << ". Did you Compile with the GSL options Enabled with 'make gsl'?" << endl;
						cout << endl;	exit(-2356);
					}
				}
			}

			if( !haveTestedIntegral && !functionToWrap->GetNumericalNormalisation() )
			{
				double testIntegral = functionToWrap->Integral( *dataPoint_i, NewBoundary );
				cout << "Integration Test: numerical : analytical  " << setw(7) << numericalIntegral << " : " << testIntegral;
				string description = NewBoundary->DiscreteDescription( *dataPoint_i );
				description = description.substr(0, description.size()-2);
				cout << "  "  << description << "  " << functionToWrap->GetLabel() << endl;
			}

			output_val += numericalIntegral;
		}
		//cout << "output: " << output_val << endl;
	}

	while( !DiscreteIntegrals.empty() )
	{
		if( DiscreteIntegrals.back() != NULL ) delete DiscreteIntegrals.back();
		DiscreteIntegrals.pop_back();
	}

	//cout << "ret2: " << output_val << endl;
	return output_val;
}

//Return the integral over all observables except one
double RapidFitIntegrator::ProjectObservable( DataPoint* NewDataPoint, PhaseSpaceBoundary * NewBoundary, string ProjectThis, ComponentRef* Component )
{
	//Make the list of observables not to integrate
	vector<string> dontIntegrate = functionToWrap->GetDoNotIntegrateList();
	double value = -1.;

	vector<string> allIntegrable = functionToWrap->GetPrototypeDataPoint();

	vector<string> testedIntegrable;

	for( unsigned int i=0; i< allIntegrable.size(); ++i )
	{
		if( StringProcessing::VectorContains( &dontIntegrate, &(allIntegrable[i]) ) == -1 )
		{
			testedIntegrable.push_back( allIntegrable[i] );
		}
	}

	dontIntegrate.push_back(ProjectThis);

	if( testedIntegrable.size() == 1 )
	{
		if( testedIntegrable[0] == ProjectThis )
		{
			value = functionToWrap->EvaluateComponent( NewDataPoint, Component );
		}
		else
		{
			cerr << "This PDF only knows how to Integrate: " << testedIntegrable[0] << " CAN NOT INTEGRATE: " << ProjectThis << endl << endl;
			throw(-342);
		}
	}
	else
	{
		value = this->NumericallyIntegrateDataPoint( NewDataPoint, NewBoundary, dontIntegrate, Component );
	}
	return value;
}

void RapidFitIntegrator::ForceTestStatus( bool input )
{
	haveTestedIntegral = input;
}

void RapidFitIntegrator::SetDebug( DebugClass* input_debug )
{
	if( debug != NULL ) delete debug;
	debug = new DebugClass( *input_debug );
	if( functionToWrap != NULL ) functionToWrap->SetDebug( input_debug );
}


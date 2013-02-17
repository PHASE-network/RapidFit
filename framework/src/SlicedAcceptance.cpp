/**
  @class SlicedAcceptance

  A class for holding a sliced propertime acceptance

  @author Pete Clarke
  @data 2011-06-07
  */


#include "SlicedAcceptance.h"
#include "StringProcessing.h"

#include <stdio.h>
#include <vector>
#include <string>

using namespace::std;

AcceptanceSlice::AcceptanceSlice( const AcceptanceSlice& input ) :
	_tlow(input._tlow), _thigh(input._thigh), _height(input._height)
{
}

//............................................
// Constructor for flat acceptance
SlicedAcceptance::SlicedAcceptance( double tl, double th ) :
	slices(), nullSlice( new AcceptanceSlice(0.,0.,0.) ), tlow(tl), thigh(th), beta(0)
{

	//Reality checks
	if( tl > th )
	{
		cout << "SlicedAcceptance::SlicedAcceptance :  tl > th  - exiting" << endl;
		exit(1);
	}

	//Create single slice with acceptance=1.0
	slices.push_back( new AcceptanceSlice( tlow, thigh, 1.0 ) );

	//....done.....

}

SlicedAcceptance::SlicedAcceptance( const SlicedAcceptance& input ) :
	slices(), nullSlice( new AcceptanceSlice(0.,0.,0.) ), tlow( input.tlow ), thigh( input.thigh ), beta( input.beta )
{
	for( unsigned int i=0; i< input.slices.size(); ++i )
	{
		if( (input.slices[i]) != NULL ) slices.push_back( new AcceptanceSlice( *(input.slices[i]) ) );
		else slices.push_back( NULL );
	}
}

SlicedAcceptance::~SlicedAcceptance()
{
	if( nullSlice != NULL ) delete nullSlice;
	while( !slices.empty() )
	{
		if( slices.back() != NULL ) delete slices.back();
		slices.pop_back();
	}
}

//............................................
// Constructor for simple upper time acceptance only
SlicedAcceptance::SlicedAcceptance( double tl, double th, double b ) :
	slices(), nullSlice(new AcceptanceSlice(0.,0.,0.)), tlow(tl), thigh(th), beta(b)
{
	//Reality checks
	if( tlow > thigh )
	{
		cout << "SlicedAcceptance::SlicedAcceptance :  tl > th  - exiting" << endl;
		exit(1);
	}
	if( beta < 0. )
	{
		cout << "SlicedAcceptance::SlicedAcceptance :  beta < 0  - exiting" << endl;
		exit(1);
	}

	// The upper time acceptance formula is (1 - beta*time) so determine highest and lowest acceptance
	double accUp = 1.0 - beta*tlow;
	double accLow = 1.0 - beta*thigh;


	//Create N more, equispaced
	int N = 15 ;
	double dh = (accUp-accLow) / (double)N;
	double dt = (thigh - tlow) / (double)N;

	//First slice which goes underneath the whole slope at the accLow level +dh/2
	slices.push_back( new AcceptanceSlice( tlow, thigh, (accLow+(dh/2.0)) ) );

	for( int is=1; is <= (N-1) ; is++ )    //The N-1 is important as the N one is included as the +dh/2 in first slice
	{
		double this_th = tlow + is*dt;
		slices.push_back( new AcceptanceSlice( tlow, this_th, dh ) );
	}

	//....done.....
}


//............................................
// Constructor for simple 2010 version of lower time acceptance only
SlicedAcceptance::SlicedAcceptance( string s ) :
	slices(), nullSlice(new AcceptanceSlice(0.,0.,0.)), tlow(), thigh(), beta()
{
	(void)s;
	int N = 31;

	double ts[31] = {0, 0.05,  0.1, 0.15, 0.2, 0.25, 0.3, 0.34, 0.38, 0.42, 0.46, 0.5, 0.54, 0.58, 0.62, 0.66,
		0.7,  0.75,  0.8, 0.85 , 0.9, 0.95, 1.0, 1.05, 1.1,  1.2, 1.3, 1.4,1.6 , 1.8, 2};

	double ac[31] = {0.000122069 , 0.00436829 , 0.022288 , 0.0633692 , 0.132094 , 0.225401 , 0.321979 , 0.409769 ,
		0.494438 , 0.570185 , 0.637997 , 0.695043 , 0.744576 , 0.784799 , 0.816805 , 0.844606 , 0.869917 ,
		0.891761 , 0.910512 , 0.926418 , 0.937439 , 0.947105 , 0.953599 , 0.960726 , 0.968278 , 0.976149 ,
		0.980624 , 0.98685 , 0.991765 , 0.995575 , 1 };


	for( int is=0; is < N ; is++ )
	{
		double this_tlow = ts[is];
		double this_thigh = 14.0;
		double height = is > 0 ? ac[is] - ac[is-1] : ac[is];
		slices.push_back( new AcceptanceSlice( this_tlow, this_thigh, height ) );
	}

	//....done.....
}

//............................................
// Constructor for accpetance from a file
SlicedAcceptance::SlicedAcceptance( string type, string fileName ) :
	slices(), nullSlice(new AcceptanceSlice(0.,0.,0.)), tlow(), thigh(), beta()
{
	(void)type;
	if( type != "File" ) {   }//do nothing for now

	string fullFileName = StringProcessing::FindFileName( fileName );

	cout << "Opening: " << fullFileName << endl;

	ifstream in;
	in.open(fullFileName.c_str());
	if( in.fail() )
	{
		cout << "SlicedAcceptance::SlicedAcceptance : failed to open acceptance file  '  " << fullFileName  << "  '  " << endl;
		exit(1);
	}

	vector<double> lowEdge; vector<double> hiEdge; vector<double> binContent;
	int ok = true;
	while (ok)
	{
		lowEdge.push_back(stream(in));  hiEdge.push_back(stream(in)); binContent.push_back(stream(in) );
		if (in.eof()  == true || in.good() == false) ok =false;
	}
	in.close();

	for( unsigned int is=0; is < lowEdge.size() ; ++is )
	{
		double this_tlow = lowEdge[is];
		double this_thigh = hiEdge[is];
		double height = binContent[is];
		//cout << " Adding slice " << tlow << " /  " << thigh << " /  " << height << endl ;
		slices.push_back( new AcceptanceSlice( this_tlow, this_thigh, height ) );
	}
	cout << "Time Acc Slices: " << lowEdge.size() << endl;
	if( lowEdge.size() == 1 )
	{
		cout << "SlicedAcceptance: SERIOUS ERROR" << endl;
		exit(0);
	}
	//....done.....

}

//............................................
// Return numerator for evaluate
double SlicedAcceptance::getValue( const double t ) const
{
	double returnValue = 0;
	for( unsigned int is = 0; is < slices.size() ; ++is )
	{
		if( (t>=slices[is]->tlow()) && (t<slices[is]->thigh()) ) returnValue += slices[is]->height();
	}
	if( t == slices.back()->thigh() ) returnValue += slices.back()->height();
	return returnValue;
}

double SlicedAcceptance::getValue( const Observable* time, const double timeOffset ) const
{
	//return this->getValue( time->GetValue() - timeOffset );

	if( time->GetBinNumber() != -1 )
	{
		//#pragma GCC diagnostic ignored "-Wfloat-equal"
		if( time->GetOffSet() == timeOffset ) return time->GetAcceptance();
		//#pragma GCC diagnostic pop
		else
		{
			time->SetBinNumber( -1 );
			time->SetOffSet( -999. );
		}
	}

	double t = time->GetValue() - timeOffset;
	double returnValue = 0;
	unsigned int is = 0;
	for( ; is < slices.size(); ++is )
	{
		if( (t >= slices[is]->tlow() ) && ( t < slices[is]->thigh() ) )
		{
			returnValue += slices[is]->height();
		}
	}
	if( t == slices.back()->thigh() )
	{
		returnValue += slices.back()->height();
	}

	time->SetBinNumber( (int)is );
	time->SetAcceptance( returnValue );
	time->SetOffSet( timeOffset );
	return returnValue;
}

//............................................
// Return the number of slices
unsigned int SlicedAcceptance::numberOfSlices() const
{
	return (unsigned)slices.size();
}

//............................................
// Return a slice
AcceptanceSlice * SlicedAcceptance::getSlice( const unsigned int s ) const
{
	if( s>=(unsigned)slices.size() ) return nullSlice;
	return slices[s];
}

//............................................
//Helpers	Only Used in Constructor
double SlicedAcceptance::stream(ifstream& thisStream)
{
	double tmpVal;
	thisStream >> tmpVal;
	return tmpVal;
}


//
/**
  @class MemoryDataSet

  A data set which simply stores a vector of pointers to datapoint objects

  @author Benjamin M Wynne bwynne@cern.ch
  @date 2009-10-02
  */

//	RapidFit Headers
#include "MemoryDataSet.h"
#include "IConstraint.h"
#include "ObservableDiscreteConstraint.h"
#include "StringProcessing.h"
#include "StatisticsFunctions.h"
//	System Headers
#include <iostream>
#include <vector>
#include <algorithm>
#include <math.h>

#define DOUBLE_TOLERANCE_DATA 1E-6

using namespace::std;

//Constructor with correct argument
MemoryDataSet::MemoryDataSet( PhaseSpaceBoundary * NewBoundary ) : allData(), dataBoundary( new PhaseSpaceBoundary(*NewBoundary) ), allSubSets(), WeightName(""), useWeights(false)
{
	for( unsigned int i=0; i< (unsigned)dataBoundary->GetNumberCombinations(); ++i )
	{
		allSubSets.push_back( -1 );
	}
}

//Destructor
MemoryDataSet::~MemoryDataSet()
{
	delete dataBoundary;
	while( !allData.empty() )
	{
		if( allData.back() != NULL ) delete allData.back();
		allData.pop_back();
	}
}

void MemoryDataSet::ReserveDataSpace( int numberOfPoints )
{
	allData.reserve( unsigned(numberOfPoints) );
}

//Add a data point to the set
bool MemoryDataSet::AddDataPoint( DataPoint* NewDataPoint )
{
	if( dataBoundary->IsPointInBoundary(NewDataPoint) )
	{
		allData.push_back( NewDataPoint );
		allData.back()->SetPhaseSpaceBoundary( dataBoundary );
		return true;
	}
	else
	{
		delete NewDataPoint;
		//cerr << "Data point is not within data set boundary" << endl;
		return false;
	}
}

//Retrieve the data point with the given index
DataPoint * MemoryDataSet::GetDataPoint( int Index ) const
{
	if ( Index < int(allData.size()) )
	{
		allData[unsigned(Index)]->SetPhaseSpaceBoundary( dataBoundary );
		return allData[unsigned(Index)];
	}
	else
	{
		cerr << "Index (" << Index << ") out of range in DataSet" << endl;
	}
	return NULL;
}

//Get the number of data points in the set
int MemoryDataSet::GetDataNumber( DataPoint* templateDataPoint, bool silence ) const
{
	if( templateDataPoint == NULL )	return int(allData.size());
	else
	{
		try
		{
			int number = allSubSets[ dataBoundary->GetDiscreteIndex( templateDataPoint, silence ) ];

			if( number != -1 ) return number;
			else
			{

				int counter = 0;
				vector<string> allDiscrete = dataBoundary->GetDiscreteNames();

				PhaseSpaceBoundary* temp_Boundary = new PhaseSpaceBoundary( *dataBoundary );

				for( vector<string>::iterator disc_i = allDiscrete.begin(); disc_i != allDiscrete.end(); ++disc_i )
				{
					double val = templateDataPoint->GetObservable( *disc_i, silence )->GetValue();
					ObservableDiscreteConstraint* thisConstraint = (ObservableDiscreteConstraint*) temp_Boundary->GetConstraint( *disc_i );
					vector<double> vec_val( 1, val );
					thisConstraint->SetValues( vec_val );
				}

				for( vector<DataPoint*>::const_iterator point_i = allData.begin(); point_i != allData.end(); ++point_i )
				{
					if( temp_Boundary->IsPointInBoundary( *point_i, silence ) )	++counter;
				}
				delete temp_Boundary;
				allSubSets[ dataBoundary->GetDiscreteIndex( templateDataPoint ) ] = counter;
				return counter;
			}
		}
		catch(...)
		{
			vector<ObservableRef> missing;
			vector<string> observables = templateDataPoint->GetAllNames();
			vector<string> allDiscrete = dataBoundary->GetDiscreteNames();

			for( vector<string>::iterator disc_i = allDiscrete.begin(); disc_i != allDiscrete.end(); ++disc_i )
			{
				if( StringProcessing::VectorContains( &observables, &(*disc_i) ) ==  -1 )
				{
					missing.push_back( *disc_i );
					//cout << string(missing.back()) << endl;
				}
			}

			vector<vector<double> > possible_values;

			for( vector<ObservableRef>::iterator missing_i = missing.begin(); missing_i != missing.end(); ++missing_i )
			{
				possible_values.push_back( dataBoundary->GetConstraint( *missing_i )->GetValues() );
			}

			int total_count=0;

			vector<vector<double> > possible_datapoints = StatisticsFunctions::Combinatorix( possible_values );

			for( unsigned int i=0; i< possible_datapoints.size(); ++i )
			{
				DataPoint* newPoint = new DataPoint( *templateDataPoint );
				for( unsigned int j=0; j< possible_datapoints[i].size(); ++j )
				{
					Observable* this_obs = new Observable( string(missing[j]), possible_datapoints[i][j], 0., "unitless" );
					//cout << string(missing[i]) << "\t" << possible_datapoints[i][j] << endl;
					newPoint->AddObservable( string(missing[j]), this_obs );
					delete this_obs;
				}
				//cout << string(missing[j]) << endl;
				total_count+=this->GetDataNumber( newPoint );
				delete newPoint;
			}

			return total_count;
		}
	}
}

//Get the data bound
PhaseSpaceBoundary * MemoryDataSet::GetBoundary() const
{
	return dataBoundary;
}

//Empty the data set
void MemoryDataSet::Clear()
{
	allData.clear();
}

void MemoryDataSet::SortBy( string parameter )
{

	cout << "Sorting" << endl;
	if( allData.size() > 0 )
	{
		vector<pair<DataPoint*,ObservableRef> > allData_sort;

		for( vector<DataPoint*>::iterator data_i = allData.begin(); data_i != allData.end(); ++data_i )
		{
			allData_sort.push_back( make_pair( *data_i, ObservableRef( parameter ) ) );
		}

		//cout << "hello" << endl;
		sort( allData_sort.begin(), allData_sort.end(), DataPoint() );
		cout << "sorted" << endl;
		//	Sort the data in memory

		while( !allData.empty() ) allData.pop_back();

		for( vector<pair<DataPoint*,ObservableRef> >::iterator sort_i = allData_sort.begin(); sort_i != allData_sort.end(); ++sort_i )
		{
			allData.push_back( sort_i->first );
		}

		cout << allData.size() << endl;
	}
	cout << "Sorted" << endl;
}

vector<DataPoint*> MemoryDataSet::GetDiscreteSubSet( vector<string> discreteParam, vector<double> discreteVal )
{
	vector<DataPoint*> returnable_subset;
	if( discreteParam.size() != discreteVal.size() )
	{
		cerr << "\n\n\t\tBadly Defined definition of a subset, returning 0 events!\n\n" << endl;
		return returnable_subset;
	}

	for( unsigned int i=0; i< allData.size(); ++i )
	{
		DataPoint* data_i = allData[i];
		vector<ObservableRef*> temp_ref;
		for( unsigned int j=0; j< discreteParam.size(); ++j )
		{
			temp_ref.push_back( new ObservableRef( discreteParam[j] ) );
		}

		bool decision = true;
		for( unsigned int j=0; j< discreteParam.size(); ++j )
		{
			if( !( fabs( data_i->GetObservable( *(temp_ref[j]) )->GetValue() - discreteVal[j] ) < DOUBLE_TOLERANCE_DATA ) )
			{
				decision = false;
			}
		}

		if( decision ) returnable_subset.push_back( data_i );

		while( !temp_ref.empty() )
		{
			if( temp_ref.back() != NULL ) delete temp_ref.back();
			temp_ref.pop_back();
		}
	}

	return returnable_subset;
}

int MemoryDataSet::Yield()
{
	return this->GetDataNumber();
}

string MemoryDataSet::GetWeightName() const
{
	return WeightName;
}

bool MemoryDataSet::GetWeightsWereUsed() const
{
	return useWeights;
}

void MemoryDataSet::UseEventWeights( const string Name )
{
	WeightName = Name;
	useWeights = true;
	for( unsigned int i=0; i< allData.size(); ++i )
	{
		allData[i]->SetEventWeight( allData[i]->GetObservable( WeightName )->GetValue() );
	}
}

void MemoryDataSet::NormaliseWeights()
{
	if( useWeights )
	{
		double alpha=0.;
		double sum_Val=0.;
		double sum_Val2=0.;
		for( unsigned int i=0; i< allData.size(); ++i )
		{
			double thisVal=allData[i]->GetEventWeight();
			sum_Val += thisVal;
			sum_Val2 += thisVal*thisVal;
		}
		alpha=sum_Val/sum_Val2;
		for( unsigned int i=0; i< allData.size(); ++i )
		{
			allData[i]->SetEventWeight( allData[i]->GetObservable( WeightName )->GetValue() * alpha );
		}
	}
}

void MemoryDataSet::Print() const
{
	if( this->GetWeightsWereUsed() )
	{
		double total=0.;
		double avr=0.;
		double avr_sq=0.;
		double val=0.;
		ObservableRef weightRef( WeightName );
		for( unsigned int i=0; i< allData.size(); ++i )
		{
			val=allData[i]->GetObservable( WeightName )->GetValue();
			avr+=val;
			avr_sq+=val*val;
		}
		total=avr;
		avr/=(double)allData.size(); avr_sq/=(double)allData.size();
		double err = sqrt( avr_sq - avr*avr );
		cout << "DataSet contains a total of:     " << total << " ± " << err << "     SIGNAL events.(" << allData.size() << " total). In " << this->GetBoundary()->GetNumberCombinations() << " Discrete DataSets." << endl;
	}
	else
	{
		cout << "DataSet contains a total of:     " << allData.size() << "     events. In " << this->GetBoundary()->GetNumberCombinations() << " Discrete DataSets." << endl;
	}
}


/**
 @class AngularAcceptance
 
 A class for holding angular acceptance information
 
 @author Pete Clarke
 @data 2012-03-29
 */


#include "AngularAcceptance.h"

#include "TChain.h"


//............................................
// Constructor for accpetance from a file
AngularAcceptance::AngularAcceptance( string fileName, bool useHelicityBasis ) :
 _af1(1), _af2(1), _af3(1), _af4(0), _af5(0), _af6(0), _af7(1), _af8(0), _af9(0), _af10(0) 
{
	
	//Initialise depending upon whether configuration parameter was found
	if( fileName == "" )
	{
		useFlatAngularAcceptance = true ;
	}
	else
	{
		useFlatAngularAcceptance = false ;
		
		// Get the acceptance histogram
	
		string fullFileName = this->openFile( fileName ) ;
		
		TFile* f =  TFile::Open(fullFileName.c_str());
		
		if( useHelicityBasis ) {
			histo = (TH3D*) f->Get("helacc"); //(fileName.c_str())));
			cout << "AngularAcceptance::  Using heleicity basis" << endl ;
		}
		else {
			histo = (TH3D*) f->Get("tracc"); //(fileName.c_str())));
			cout << "AngularAcceptance::  Using transversity basis" << endl ;
		}
		
		if( histo == NULL ) histo = (TH3D*) f->Get("acc");

		if( histo == NULL )
		{
			cerr << "Cannot Open a Valid NTuple" << endl;
			exit(0);
		}

		this->processHistogram() ;
		
			
		// Get the 10 angular factors	
		
		TChain* decayTree ;
		decayTree = new TChain("tree");
		decayTree->Add(fullFileName.c_str());	
		
		vector<double> *pvect = new vector<double>() ;
		decayTree->SetBranchAddress("weights", &pvect ) ;
		decayTree->GetEntry(0);

		_af1=(*pvect)[0], _af2=(*pvect)[1], _af3=(*pvect)[2], _af4=(*pvect)[3], _af5=(*pvect)[4], 
		_af6=(*pvect)[5], _af7=(*pvect)[6], _af8=(*pvect)[7], _af9=(*pvect)[8], _af10=(*pvect)[9] ;
		
		//for( int ii=0; ii <10; ii++) {
		//	cout << "AcceptanceWeight "<<ii+1<< " = "  << (*pvect)[ii] << endl ;
		//}
	}
	
}



//............................................
// Return numerator for evaluate
double AngularAcceptance::getValue( double cosPsi, double cosTheta, double phi ) const
{

	if( useFlatAngularAcceptance ) return 1. ;
	
	double returnValue=0.;
		
	int globalbin=-1;
	int xbin=-1, ybin=-1, zbin=-1;
	double num_entries_bin=-1.;
	
	//Find global bin number for values of angles, find number of entries per bin, divide by volume per bin and normalise with total number of entries in the histogram
	xbin = xaxis->FindFixBin( cosPsi ); if( xbin > nxbins ) xbin = nxbins;
	ybin = yaxis->FindFixBin( cosTheta ); if( ybin > nybins ) ybin = nybins;
	zbin = zaxis->FindFixBin( phi ); if( zbin > nzbins ) zbin = nzbins;
			
	globalbin = histo->GetBin( xbin, ybin, zbin );
	num_entries_bin = histo->GetBinContent(globalbin);
			
	returnValue = num_entries_bin; /// (deltax * deltay * deltaz) / total_num_entries ;
	
	return returnValue / average_bin_content ;
}


//............................................
// Open the input file containing the acceptance
string AngularAcceptance::openFile( string fileName ) {
	
	ifstream input_file2;
	input_file2.open( fileName.c_str(), ifstream::in );
	input_file2.close();
	bool local_fail2 = input_file2.fail();
	if( !getenv("RAPIDFITROOT") && local_fail2 )
	{
		cerr << "\n" << endl;
		//cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
		cerr << "$RAPIDFITROOT NOT DEFINED, PLEASE DEFINE IT SO I CAN FIND ACCEPTANCE FILE" << endl;
		//cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
		cerr << "\n" << endl;
		exit(-987);
	}

	string fullFileName;	
	
	if( getenv("RAPIDFITROOT") )
	{
		string path( getenv("RAPIDFITROOT") ) ;
		
		//cout << "RAPIDFITROOT defined as: " << path << endl;
		fullFileName = path+"/pdfs/configdata/"+fileName ;
		
		input_file2.open( fullFileName.c_str(), ifstream::in );
		input_file2.close();
	}
	bool elsewhere_fail = input_file2.fail();
	
	if( elsewhere_fail && local_fail2 )
	{
		cerr << "AngularAcceptance::AngularAcceptance: \n\tFileName:\t" << fullFileName << "\t NOT FOUND PLEASE CHECK YOUR RAPIDFITROOT" << endl;
		cerr << "\t\tAlternativley make sure your XML points to the correct file, or that the file is in the current working directory\n" <<
		endl;
		exit(-89);
	}
	
	
	if( fullFileName.empty() || !local_fail2 )
	{
		fullFileName = fileName;
	}

	return fullFileName;
}



//............................................
// Open the input file containing the acceptance
void AngularAcceptance::processHistogram() {
	
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
	double sum = 0;
	
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
				else if (bin_content>0)                                        {
					sum += bin_content;
				}
			}
		}
	}
	
	average_bin_content = sum / total_num_bins;
	
	cout << "For total number of bins " << total_num_bins << " there are " << zero_bins.size() << " bins containing zero events "  << endl;
	cout << "Average number of entries of non-zero bins: " << average_bin_content <<  endl;
	
	// Check.  This order works for both bases since phi is always the third one.
	if ((xmax-xmin) < 2. || (ymax-ymin) < 2. || (zmax-zmin) < (2.*TMath::Pi()-0.01) )
	{
		cout << "In LongLivedBkg_3Dangular::LongLivedBkg_3Dangular: The full angular range is not used in this histogram - the PDF does not support this case" << endl;
		exit(1);
	}
	
	cout << "Finishing processing angular acceptance histo" << endl;

}



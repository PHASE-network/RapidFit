/*!
 * @class BasePDF_Framework
 *
 * @brief This Particular Framework makes use of the 
 *
 * @author Robert Currie rcurrie@cern.ch
 *
 */

#pragma once
#ifndef BASE_FRAMEWORK_PDF_H
#define BASE_FRAMEWORK_PDF_H

	///	ROOT Headers
#include "TRandom.h"
#include "TRandom3.h"
	///	RapidFit Headers
#include "IPDF_Framework.h"
#include "PhaseSpaceBoundary.h"
#include "RapidFitIntegrator.h"
	/*#include "ObservableRef.h"
#include "PDFConfigurator.h"
#include "ComponentRef.h"
#include "PhaseSpaceBoundary.h"
#include "ParameterSet.h"*/
	///	System Headers
#include <vector>
#include <cmath>
#include <pthread.h>

using namespace::std;


class BasePDF_Framework : public virtual IPDF_Framework
{
	public:
		/*!
		 * @brief Explicitly use this Copy Constructor
		 *
		 *
		 * Derived PDF SHOULD write their own copy constructor if they include pointers to objects
		 *
		 * @param Input   Input BasePDF cast of input IPDF
		 */
		BasePDF_Framework(const BasePDF_Framework& Input);

		/*!
		 * @brief   Interface Function: Default Destructor
		 *
		 * Derived class should only write their own if they contain objects initialized with 'new'
		 */
		~BasePDF_Framework();

		/*!
		 * @brief   Return a pointer to the internal object used for numerically Integrating this PDF
		 *
		 * @warning This is deleted along with PDF
		 *
		 * @return  reference to internal RapidFitIntegrator instance
		 */
		RapidFitIntegrator* GetPDFIntegrator() const;

		void SetUpIntegrator( const RapidFitIntegratorConfig* thisConfig );

		/*!
		 * @brief Get the Name of the PDF
		 *
		 * @return Returns the name of the PDF as a string
		 */
		string GetName() const;

		/*!
		 * @brief Get the Label of this PDF
		 *
		 * @return Return the Label of the PDF which describes the PDF better than the Name
		 */
		string GetLabel() const;

		/*!
		 * @brief Set the Label of this PDF
		 *
		 * @param Label    This is a description of this PDF (automatically generated by ClassLookUp but user configurable
		 *
		 * @return Void
		 */
		void SetLabel( string Label );

		/*!
		 * Interface Function:
		 * Can the PDF be safely copied through it's copy constructor?
		 */
		bool IsCopyConstructorSafe() const;

		/*!
		 * @brief Give the PDF a pointer to the template of it's copy constructor object
		 *
		 * In theory a Copy Constructor can be written for IPDF which wraps back around to this but that breaks the concept of an Interface
		 *
		 * @param Input  This is the CopyPDF_t which wraps the Copy Constructor for this PDF
		 *
		 * @return Void
		 */
		void SetCopyConstructor( CopyPDF_t* Input ) const;

		/*!
		 * @breif Get the pointer to the PDF copy constructor
		 *
		 * @return Returns the CopyPDF_t instance which references the Copy Constructor of this class
		 */
		CopyPDF_t* GetCopyConstructor() const;

		/*!
		 * @brief Return the required XML for this PDF
		 *
		 * @return Returns the name of the PDF in PDF tags
		 */
		string XML() const;

		/*!
		 * @brief Set the internal PDFConfigurator Object
		 *
		 * Set the Internal Configurator Object to be this input.
		 * This is called in the correct constructor for the class and this object is correctly duplicated for all copied instances.
		 *
		 * @warning THIS WILL NOT EFFECT THE WAY THE PDF IS CONFIGURED IT IS JUST FOR READING THE ACTUAL CONFIGURATION!!!!!!!
		 *
		 * @param config  This is the configuration you wish to replace the internal object with. This will NOT chane the way the PDF is configured
		 *
		 * @return Void
		 */
		void SetConfigurator( PDFConfigurator* config );

		/*!
		 * @brief Get a pointer to the PDF Configurator
		 *
		 * @return Returns a pointer ot the Configurator assigned to this PDF
		 */
		PDFConfigurator* GetConfigurator() const;

		pthread_mutex_t* DebugMutex() const;

		void SetDebugMutex( pthread_mutex_t* Input, bool =true );

		void Print() const;

		/*!
		 * @brief Set the Name of the PDF
		 *
		 * @warning Use With CAUTION!
		 *
		 * @return Name   This is the new Name we want to give to this PDF
		 *
		 * @return Void
		 */
		void SetName( string Name );

		void ChangePhaseSpace( PhaseSpaceBoundary * InputBoundary );

	protected:

		/*!
		 * Default  Constructor
		 */
		BasePDF_Framework( IPDF* thisPDF );

		bool IsDebuggingON();

		/*!
		 * Interface Function:
		 * Set if the PDF be safely copied through it's copy constructor?
		 */
		void SetCopyConstructorSafe( bool = true );

	private:

		pthread_mutex_t* debug_mutex;   /*!     This is the pthread mutex object which is to be used for thread-locking sections of code in the PDFs that is not thread safe, e.g. using streamers*/

		bool can_remove_mutex;          /*!     This is internal to let the PDF know if it's safe to remove the shared mutex    */

		/*!
		 * Don't Copy the PDF this way!
		 */
		BasePDF_Framework& operator=(const BasePDF_Framework&);

		/*!
		 * This is the Name of the PDF, it's name is tied in with the name of the C wrappers for the on disk elemends of the pdfs
		 * Change this at your own risk!
		 */
		string PDFName;

		/*!
		 * This is a label describing the PDF, useful for knowing the structure of this PDF
		 */
		string PDFLabel;

		/*!
		 * Pointer to the 'on disk' copy constructor for this class
		 * This means that the PDF only needs to look up it's own 'on disk' elements once
		 */
		mutable CopyPDF_t* copy_object;

		PDFConfigurator* thisConfig;	/*!	PDFConfigurator containing the Configurator which knowsn how this PDF was configured	*/

		RapidFitIntegrator* myIntegrator;

		bool debuggingON;

		bool CopyConstructorIsSafe;

};

#endif


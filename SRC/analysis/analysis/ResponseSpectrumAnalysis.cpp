/* ****************************************************************** **
**    OpenSees - Open System for Earthquake Engineering Simulation    **
**          Pacific Earthquake Engineering Research Center            **
**                                                                    **
**                                                                    **
** (C) Copyright 1999, The Regents of the University of California    **
** All Rights Reserved.                                               **
**                                                                    **
** Commercial use of this program without express permission of the   **
** University of California, Berkeley, is strictly prohibited.  See   **
** file 'COPYRIGHT'  in main directory for information on usage and   **
** redistribution,  and for a DISCLAIMER OF ALL WARRANTIES.           **
**                                                                    **
** Developed by:                                                      **
**   Frank McKenna (fmckenna@ce.berkeley.edu)                         **
**   Gregory L. Fenves (fenves@ce.berkeley.edu)                       **
**   Filip C. Filippou (filippou@ce.berkeley.edu)                     **
**                                                                    **
** ****************************************************************** */

// $Revision: 1.0 $
// $Date: 2020-11-27 18:07:11 $
// $Source: /usr/local/cvs/OpenSees/SRC/analysis/analysis/ResponseSpectrumAnalysis.h,v $

// Written: Massimo Petracca (ASDEA Software) 
// Created: Fri Nov 27 18:07:11: 2020
// Revision: A
//
// Description: This file contains the class definition for ResponseSpectrumAnalysis.
//
// What: "@(#) ResponseSpectrumAnalysis.h, revA"

#include <ResponseSpectrumAnalysis.h>
#include <AnalysisModel.h>
#include <Domain.h>
#include <DomainModalProperties.h>
#include <TimeSeries.h>
#include <elementAPI.h>
#include <Node.h>
#include <NodeIter.h>
#include <vector>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

int
OPS_ResponseSpectrumAnalysis(void)
{
	// responseSpectrum $tsTag $dir <-scale $scale>

	// some kudos
	static bool first_done = false;
	if (!first_done) {
		opserr << "Using ResponseSpectrumAnalysis - Developed by: Massimo Petracca, Guido Camata, ASDEA Software Technology\n";
		first_done = true;
	}

	// get analysis model
	AnalysisModel* theAnalysisModel = *OPS_GetAnalysisModel();
	if (theAnalysisModel == nullptr) {
		opserr << "ResponseSpectrumAnalysis Error: no AnalysisModel available.\n";
		return -1;
	}
	if (theAnalysisModel->getDomainPtr() == nullptr) {
		opserr << "ResponseSpectrumAnalysis Error: no Domain available.\n";
		return -1;
	}

	// init default arguments
	TimeSeries* ts = nullptr;
	int dir = 1;
	double scale = 1.0;

	// make sure eigenvalue and modal properties have been called before
	DomainModalProperties modal_props;
	if (theAnalysisModel->getDomainPtr()->getModalProperties(modal_props) < 0) {
		opserr << "ResponseSpectrumAnalysis - eigen and modalProperties have not been called" << endln;
		return -1;
	}
	int ndf = modal_props.totalMass().Size();

	// parse
	int nargs = OPS_GetNumRemainingInputArgs();
	if (nargs < 2) {
		opserr << "ResponseSpectrumAnalysis $tsTag $dir <-scale $scale> <-damp $damp>\n"
			"Error: at least 2 arguments should be provided.\n";
		return -1;
	}
	int numData = 1;
	int tstag;
	if (OPS_GetInt(&numData, &tstag) < 0) {
		opserr << "ResponseSpectrumAnalysis Error: Failed to get timeSeries tag.\n";
		return -1;
	}
	ts = OPS_getTimeSeries(tstag);
	if (ts == nullptr) {
		opserr << "ResponseSpectrumAnalysis Error: Failed to get timeSeries with tag = " << tstag << ".\n";
		return -1;
	}
	if (OPS_GetInt(&numData, &dir) < 0) {
		opserr << "ResponseSpectrumAnalysis Error: Failed to get direction.\n";
		return -1;
	}
	if (dir < 1 || dir > ndf) {
		opserr << "ResponseSpectrumAnalysis Error: provided direction (" << dir << ") should be in the range 1-" << ndf << ".\n";
		return -1;
	}

	// parse optional data
	nargs = OPS_GetNumRemainingInputArgs();
	int loc = 0;
	int mode_id = 0;
	bool single_mode = false;
	while (loc < nargs) {
		const char* value = OPS_GetString();
		if (strcmp(value, "-scale") == 0) {
			if (loc < nargs - 1) {
				if (OPS_GetDouble(&numData, &scale) < 0) {
					opserr << "ResponseSpectrumAnalysis Error: Failed to get scale factor.\n";
					return -1;
				}
				++loc;
			}
			else {
				opserr << "ResponseSpectrumAnalysis Error: scale factor requested but not provided.\n";
				return -1;
			}
		}
		else if (strcmp(value, "-mode") == 0) {
			if (loc < nargs - 1) {
				if (OPS_GetInt(&numData, &mode_id) < 0) {
					opserr << "ResponseSpectrumAnalysis Error: Failed to get the mode_id.\n";
					return -1;
				}
				--mode_id; // make it 0-based
				single_mode = true;
				++loc;
			}
			else {
				opserr << "ResponseSpectrumAnalysis Error: mode_id requested but not provided.\n";
				return -1;
			}
		}
		++loc;
	}

	// ok, create the response spectrum analysis and run it here... 
	// no need to store it
	ResponseSpectrumAnalysis rsa(theAnalysisModel, ts, dir, scale);
	int result;
	if (single_mode)
		result = rsa.analyze(mode_id);
	else
		result = rsa.analyze();

	return result;
}

ResponseSpectrumAnalysis::ResponseSpectrumAnalysis(
	AnalysisModel* theModel,
	TimeSeries* theFunction,
	int theDirection,
	double scale
)
	: m_model(theModel)
	, m_function(theFunction)
	, m_direction(theDirection)
	, m_scale(scale)
	, m_current_mode(0)
{

}

ResponseSpectrumAnalysis::~ResponseSpectrumAnalysis()
{
}

int ResponseSpectrumAnalysis::analyze()
{
	// get the domain
	Domain* domain = m_model->getDomainPtr();

	// get the modal properties
	DomainModalProperties mp;
	if (domain->getModalProperties(mp) < 0) {
		opserr << "ResponseSpectrumAnalysis::analyze() - failed to get modal properties" << endln;
		return -1;
	}

	// size info
	int num_eigen = domain->getEigenvalues().Size();

	// check consistency
	int error_code;
	error_code = check();
	if (error_code < 0) return error_code;

	// loop over all required eigen-modes, compute the modal displacement
	// and save the results.
	// we just compute modal displacements without doing any (SRSS, CQC, etc..)
	// modal combination otherwise derived results cannot be computed.
	// for each mode, this analysis produces a new analysis step.
	// modal combination of displacements (or any derived results)
	// it's up to the user.
	for (m_current_mode = 0; m_current_mode < num_eigen; ++m_current_mode)
	{
		// init the new step
		error_code = beginMode();
		if (error_code < 0) return error_code;

		// compute modal acceleration for this mode using the 
		// provided response spectrum function (time series)
		error_code = solveMode();
		if (error_code < 0) return error_code;

		// done with the current step.
		// here the modal displacements will be recorded (and all other results
		// if requested via recorders...)
		error_code = endMode();
		if (error_code < 0) return error_code;
	}

	return 0;
}

int ResponseSpectrumAnalysis::analyze(int mode_id)
{
	// get the domain
	Domain* domain = m_model->getDomainPtr();

	// get the modal properties
	//const DomainModalProperties& mp = domain->getModalProperties();
	DomainModalProperties mp;
	if (domain->getModalProperties(mp) < 0) {
		opserr << "ResponseSpectrumAnalysis::analyze(mode_id) - failed to get modal properties" << endln;
		return -1;
	}

	// size info
	int num_eigen = domain->getEigenvalues().Size();
	if (mode_id < 0 || mode_id >= num_eigen) {
		opserr << "ResponseSpectrumAnalysis::analyze(mode_id) - The provided mode_id (" << mode_id + 1 << ") is out of range (1, " << num_eigen << ")\n";
		return -1;
	}
	m_current_mode = mode_id;

	// check consistency
	int error_code;
	error_code = check();
	if (error_code < 0) return error_code;

	// init the new step
	error_code = beginMode();
	if (error_code < 0) return error_code;

	// compute modal acceleration for this mode using the 
	// provided response spectrum function (time series)
	error_code = solveMode();
	if (error_code < 0) return error_code;

	// done with the current step.
	// here the modal displacements will be recorded (and all other results
	// if requested via recorders...)
	error_code = endMode();
	if (error_code < 0) return error_code;

	return 0;
}

int ResponseSpectrumAnalysis::check()
{
	// get the domain
	Domain* domain = m_model->getDomainPtr();

	// get the modal properties
	//const DomainModalProperties& mp = domain->getModalProperties();
	DomainModalProperties mp;
	if (domain->getModalProperties(mp) < 0) {
		opserr << "ResponseSpectrumAnalysis::check() - failed to get modal properties" << endln;
		return -1;
	}
	
	// number of eigen-modes
	int num_eigen = domain->getEigenvalues().Size();
	if (num_eigen < 1) {
		opserr << "ResponseSpectrumAnalysis::check() - No Eigenvalue provided.\n";
		return -1;
	}

	// check consistency
	auto check_eigen = [&mp, domain]() -> bool {
		const Vector& ev = domain->getEigenvalues();
		if (ev.Size() != mp.eigenvalues().Size())
			return false;
		double tol = std::max(1.0e-15, 1.0e-12 * ev.Norm());
		for (int i = 0; i < ev.Size(); ++i) {
			double a = ev(i);
			double b = mp.eigenvalues()(i);
			if (std::abs(a - b) > tol)
				return false;
		}
		return true;
	};
	if (!check_eigen()) {
		opserr << "ResponseSpectrumAnalysis::check() - Eigenvalues stored in DomainModalProperties are not equal to the eigenvalues in the model.\n"
			"Make sure to call the 'modalProperties' command\n"
			"after the 'eigen' command, and right before the 'responseSpectrum' command.\n";
		return -1;
	}

	return 0;
}

int ResponseSpectrumAnalysis::beginMode()
{
	// new step... (do nothing)
	if (m_model->analysisStep() < 0) {
		opserr << "ResponseSpectrumAnalysis::analyze() - the AnalysisModel failed"
			" at mode " << m_current_mode << "\n";
		return -1;
	}

	return 0;
}

int ResponseSpectrumAnalysis::endMode()
{
	// update domain
	if (m_model->updateDomain() < 0) {
		opserr << "ResponseSpectrumAnalysis::analyze() - the AnalysisModel failed in updateDomain"
			" at mode " << m_current_mode << "\n";
		return -1;
	}

	// commit domain
	if (m_model->commitDomain() < 0) {
		opserr << "ResponseSpectrumAnalysis::analyze() - the AnalysisModel failed in commitDomain"
			" at mode " << m_current_mode << "\n";
		return -1;
	}

	return 0;
}

int ResponseSpectrumAnalysis::solveMode()
{
	// get the domain
	Domain* domain = m_model->getDomainPtr();

	// get the modal properties
	//const DomainModalProperties& mp = domain->getModalProperties();
	DomainModalProperties mp;
	if (domain->getModalProperties(mp) < 0) {
		opserr << "ResponseSpectrumAnalysis::solveMode() - failed to get modal properties" << endln;
		return -1;
	}
	
	// size info
	int ndf = mp.totalMass().Size();

	// excited DOF.
	// Note: now we assume that a RS acts along one of the global directions, so we need
	// to consider only the associated column of the modal participation factors.
	// in future versions we can implement a general direction vector.
	int exdof = m_direction - 1; // make it 0-based

	// compute modal acceleration for this mode using the 
	// provided response spectrum function (time series)
	double lambda = mp.eigenvalues()(m_current_mode);
	double omega = std::sqrt(lambda);
	double freq = omega / 2.0 / M_PI;
	double period = 1.0 / freq;
	double mga = m_function->getFactor(period);
	double Vscale = mp.eigenVectorScaleFactors()(m_current_mode);
	double MPF = mp.modalParticipationFactors()(m_current_mode, exdof);

	// loop over all nodes and compute the modal displacements
	Node* node;
	NodeIter& theNodes = domain->getNodes();
	while ((node = theNodes()) != 0) {

		// get the nodal eigenvector, according to the ndf of modal properties
		// and its scaling
		const Matrix& node_evec = node->getEigenvectors();
		int node_ndf = node_evec.noRows();

		// for each DOF...
		for (int i = 0; i < std::min(node_ndf, ndf); ++i) {
			// exclude any dof >= node_ndf (if ndf > node_ndf... in case node is U-only)
			// exclude any dof >= ndf (if node_ndf > ndf... in case the node has more DOFs than U and R)

			// we also need to exclude pressure dofs... easy in 3D because node_ndf is 4,
			// but in 2D it is 3, as in U-R...
			if (ndf == 6 && node_ndf == 4 && i == 3)
				continue;

			// eigenvector
			double V = node_evec(i, m_current_mode) * Vscale;

			// compute modal displacements for the i-th DOF
			double u_modal = V * MPF * mga / lambda;

			// save this displacement at the i-th dof as new trial displacement
			node->setTrialDisp(u_modal, i);
		}

	}

	return 0;
}


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
                                                                        
// Original implementation: José Abell (UANDES), Massimo Petracca (ASDEA)
//
// ASDPlasticMaterial
//
// Fully general templated material class for plasticity modeling

#include "LinearHardeningScalar_EV.h"
#include "Vector.h"

VoigtScalar LinearHardeningScalar_EV::derivative = 0.0;

LinearHardeningScalar_EV::LinearHardeningScalar_EV( VoigtScalar H_) : EvolvingVariable(0.0), H(H_)
{}

LinearHardeningScalar_EV::LinearHardeningScalar_EV( VoigtScalar H_, VoigtScalar k0) : EvolvingVariable(k0), H(H_)
{}

const VoigtScalar& LinearHardeningScalar_EV::getDerivative(const VoigtVector &depsilon,
        const VoigtVector &m,
        const VoigtVector& stress) const
{
    using namespace ASDPlasticMaterialGlobals;

    //Compute the derivative.
    derivative = H * sqrt((2 * m.dot(m)) / 3);
    return derivative ;
}


int LinearHardeningScalar_EV::sendSelf(int commitTag, Channel &theChannel)
{
    //Shove all data into single vector for sending
    // static Vector data(3);
    // const VoigtScalar &a = this->getVariableConstReference();
    // const VoigtScalar &a_committed = this->getVariableConstReference();

    // data(0) = H;
    // data(1) = a;
    // data(2) = a_committed;

    // if (theChannel.sendVector(0, commitTag, data) != 0)
    // {
    //     cerr << "LinearHardeningScalar_EV::sendSelf() - Failed sending data" << endl;
    //     return -1;
    // }

    return 0;
}

int LinearHardeningScalar_EV::recvSelf(int commitTag, Channel &theChannel, FEM_ObjectBroker &theBroker)
{
    // static Vector data(3);
    // if (theChannel.recvVector(0, commitTag, data) != 0)
    // {
    //     cerr << "LinearHardeningScalar_EV::recvSelf() - Failed recieving data" << endl;
    //     return -1;
    // }

    // //Extract data from vector
    // VoigtScalar tmp_a;
    // VoigtScalar tmp_a_committed;
    // H = data(0);
    // tmp_a = data(1);
    // tmp_a_committed = data(2);

    // this->setVar(tmp_a);
    // this->setCommittedVar(tmp_a_committed);

    return 0;
}
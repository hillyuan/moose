//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#ifndef STRAINGRADDISPDERIVATIVES_H
#define STRAINGRADDISPDERIVATIVES_H

#include "Material.h"
#include "DerivativeMaterialInterface.h"

class RankTwoTensor;
class StrainGradDispDerivatives;

template <>
InputParameters validParams<StrainGradDispDerivatives>();

class StrainGradDispDerivatives : public DerivativeMaterialInterface<Material>
{
public:
  StrainGradDispDerivatives(const InputParameters & parameters);

protected:
  virtual void computeQpProperties();

  unsigned int _nvar;
  unsigned int _gdim;

  std::vector<MaterialProperty<RankTwoTensor> *> _dstrain;
};

#endif // STRAINGRADDISPDERIVATIVES_H

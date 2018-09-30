//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#ifndef MOOSEUNITAPP_H
#define MOOSEUNITAPP_H

#include "MooseApp.h"

class MooseUnitApp;

template <>
InputParameters validParams<MooseUnitApp>();

class MooseUnitApp : public MooseApp
{
public:
  MooseUnitApp(const InputParameters & parameters);
  virtual ~MooseUnitApp();

  static void registerAll(Factory & f, ActionFactory & af, Syntax & s);
};

#endif /* MOOSEUNITAPP_H */

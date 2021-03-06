//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MortarConstraint.h"

// MOOSE includes
#include "Assembly.h"
#include "FEProblem.h"
#include "MooseVariable.h"
#include "NearestNodeLocator.h"
#include "PenetrationLocator.h"

#include "libmesh/quadrature.h"

template <>
InputParameters
validParams<MortarConstraint>()
{
  InputParameters params = validParams<Constraint>();
  params.addRequiredParam<std::string>("interface", "The name of the interface.");
  params.addRequiredParam<VariableName>("master_variable", "Variable on master surface");
  params.addParam<VariableName>("slave_variable", "Variable on master surface");
  return params;
}

MortarConstraint::MortarConstraint(const InputParameters & parameters)
  : Constraint(parameters),
    CoupleableMooseVariableDependencyIntermediateInterface(this, true),
    MooseVariableInterface<Real>(this,
                                 true,
                                 "variable",
                                 Moose::VarKindType::VAR_NONLINEAR,
                                 Moose::VarFieldType::VAR_FIELD_STANDARD),
    _fe_problem(*getCheckedPointerParam<FEProblemBase *>("_fe_problem_base")),
    _dim(_mesh.dimension()),

    _q_point(_assembly.qPoints()),
    _qrule(_assembly.qRule()),
    _JxW(_assembly.JxW()),
    _coord(_assembly.coordTransformation()),
    _current_elem(_assembly.elem()),

    _master_var(_subproblem.getStandardVariable(_tid, getParam<VariableName>("master_variable"))),
    _slave_var(
        isParamValid("slave_variable")
            ? _subproblem.getStandardVariable(_tid, getParam<VariableName>("slave_variable"))
            : _subproblem.getStandardVariable(_tid, getParam<VariableName>("master_variable"))),
    _lambda(_var.sln()),

    _iface(*_mesh.getMortarInterfaceByName(getParam<std::string>("interface"))),
    _master_penetration_locator(getMortarPenetrationLocator(
        _iface._master, _iface._slave, Moose::Master, Order(_master_var.order()))),
    _slave_penetration_locator(getMortarPenetrationLocator(
        _iface._master, _iface._slave, Moose::Slave, Order(_slave_var.order()))),

    _test_master(_master_var.phi()),
    _grad_test_master(_master_var.gradPhi()),
    _phi_master(_master_var.phi()),

    _test_slave(_slave_var.phi()),
    _grad_test_slave(_slave_var.gradPhi()),
    _phi_slave(_slave_var.phi())
{
}

void
MortarConstraint::reinit()
{
  unsigned int nqp = _qrule->n_points();

  _u_master.resize(nqp);
  _grad_u_master.resize(nqp);
  _phys_points_master.resize(nqp);
  _u_slave.resize(nqp);
  _grad_u_slave.resize(nqp);
  _phys_points_slave.resize(nqp);
  _test = _assembly.getFE(_var.feType(), _dim - 1)->get_phi(); // yes we need to do a copy here
  _JxW_lm = _assembly.getFE(_var.feType(), _dim - 1)
                ->get_JxW(); // another copy here to preserve the right JxW

  for (_qp = 0; _qp < nqp; _qp++)
  {
    const Node * current_node = _mesh.getQuadratureNode(_current_elem, 0, _qp);

    PenetrationInfo * master_pinfo =
        _master_penetration_locator._penetration_info[current_node->id()];
    PenetrationInfo * slave_pinfo =
        _slave_penetration_locator._penetration_info[current_node->id()];

    if (master_pinfo && slave_pinfo)
    {
      const Elem * master_side =
          master_pinfo->_elem->build_side_ptr(master_pinfo->_side_num, true).release();

      std::vector<std::vector<Real>> & master_side_phi = master_pinfo->_side_phi;
      std::vector<std::vector<RealGradient>> & master_side_grad_phi = master_pinfo->_side_grad_phi;
      mooseAssert(master_side_phi.size() == master_side_grad_phi.size(),
                  "phi and grad phi size are different");
      _u_master[_qp] = _master_var.getValue(master_side, master_side_phi);
      _grad_u_master[_qp] = _master_var.getGradient(master_side, master_side_grad_phi);
      _phys_points_master[_qp] = master_pinfo->_closest_point;
      _elem_master = master_pinfo->_elem;
      delete master_side;

      const Elem * slave_side =
          slave_pinfo->_elem->build_side_ptr(slave_pinfo->_side_num, true).release();
      std::vector<std::vector<Real>> & slave_side_phi = slave_pinfo->_side_phi;
      std::vector<std::vector<RealGradient>> & slave_side_grad_phi = slave_pinfo->_side_grad_phi;
      mooseAssert(slave_side_phi.size() == slave_side_grad_phi.size(),
                  "phi and grad phi size are different");
      _u_slave[_qp] = _slave_var.getValue(slave_side, slave_side_phi);
      _grad_u_slave[_qp] = _slave_var.getGradient(slave_side, slave_side_grad_phi);
      _phys_points_slave[_qp] = slave_pinfo->_closest_point;
      _elem_slave = slave_pinfo->_elem;
      delete slave_side;
    }
  }
}

void
MortarConstraint::reinitSide(Moose::ConstraintType res_type)
{
  switch (res_type)
  {
    case Moose::Master:
      _assembly.setCurrentSubdomainID(_elem_master->subdomain_id());
      _assembly.reinit(_elem_master);
      _master_var.prepare();
      _assembly.prepare();
      _assembly.reinitAtPhysical(_elem_master, _phys_points_master);
      break;

    case Moose::Slave:
      _assembly.setCurrentSubdomainID(_elem_slave->subdomain_id());
      _assembly.reinit(_elem_slave);
      _slave_var.prepare();
      _assembly.prepare();
      _assembly.reinitAtPhysical(_elem_slave, _phys_points_slave);
      break;
  }
}

void
MortarConstraint::computeResidual()
{
  DenseVector<Number> & re = _assembly.residualBlock(_var.number());
  for (_qp = 0; _qp < _qrule->n_points(); _qp++)
    for (_i = 0; _i < _test.size(); _i++)
      re(_i) += _JxW_lm[_qp] * _coord[_qp] * computeQpResidual();
}

void
MortarConstraint::computeResidualSide(Moose::ConstraintType side)
{
  switch (side)
  {
    case Moose::Master:
    {
      DenseVector<Number> & re_master = _assembly.residualBlock(_master_var.number());
      for (_qp = 0; _qp < _qrule->n_points(); _qp++)
      {
        for (_i = 0; _i < _test_master.size(); _i++)
          re_master(_i) += _JxW_lm[_qp] * computeQpResidualSide(Moose::Master);
      }
    }
    break;

    case Moose::Slave:
    {
      DenseVector<Number> & re_slave = _assembly.residualBlock(_slave_var.number());
      for (_qp = 0; _qp < _qrule->n_points(); _qp++)
      {
        for (_i = 0; _i < _test_slave.size(); _i++)
          re_slave(_i) += _JxW_lm[_qp] * _coord[_qp] * computeQpResidualSide(Moose::Slave);
      }
    }
    break;
  }
}

void
MortarConstraint::computeJacobian()
{
  _phi = _assembly.getFE(_var.feType(), _dim - 1)->get_phi(); // yes we need to do a copy here
  std::vector<std::vector<Real>> phi_master;
  std::vector<std::vector<Real>> phi_slave;

  DenseMatrix<Number> & Kee = _assembly.jacobianBlock(_var.number(), _var.number());

  for (_qp = 0; _qp < _qrule->n_points(); _qp++)
    for (_i = 0; _i < _test.size(); _i++)
      for (_j = 0; _j < _phi.size(); _j++)
        Kee(_i, _j) += _JxW_lm[_qp] * _coord[_qp] * computeQpJacobian();
}

void
MortarConstraint::computeJacobianSide(Moose::ConstraintType side)
{
  switch (side)
  {
    case Moose::Master:
    {
      DenseMatrix<Number> & Ken_master =
          _assembly.jacobianBlock(_var.number(), _master_var.number());
      DenseMatrix<Number> & Kne_master =
          _assembly.jacobianBlock(_master_var.number(), _var.number());

      for (_qp = 0; _qp < _qrule->n_points(); _qp++)
        for (_i = 0; _i < _test_master.size(); _i++)
        {
          for (_j = 0; _j < _phi.size(); _j++)
          {
            Ken_master(_j, _i) +=
                _JxW_lm[_qp] * _coord[_qp] * computeQpJacobianSide(Moose::MasterMaster);
            Kne_master(_i, _j) +=
                _JxW_lm[_qp] * _coord[_qp] * computeQpJacobianSide(Moose::SlaveMaster);
          }
        }
    }
    break;

    case Moose::Slave:
    {
      DenseMatrix<Number> & Ken_slave = _assembly.jacobianBlock(_var.number(), _slave_var.number());
      DenseMatrix<Number> & Kne_slave = _assembly.jacobianBlock(_slave_var.number(), _var.number());
      for (_qp = 0; _qp < _qrule->n_points(); _qp++)
        for (_i = 0; _i < _test_slave.size(); _i++)
        {
          for (_j = 0; _j < _phi.size(); _j++)
          {
            Ken_slave(_j, _i) +=
                _JxW_lm[_qp] * _coord[_qp] * computeQpJacobianSide(Moose::MasterSlave);
            Kne_slave(_i, _j) +=
                _JxW_lm[_qp] * _coord[_qp] * computeQpJacobianSide(Moose::SlaveSlave);
          }
        }
    }
    break;
  }
}

Real
MortarConstraint::computeQpJacobian()
{
  return 0.;
}

Real MortarConstraint::computeQpJacobianSide(Moose::ConstraintJacobianType /*side_type*/)
{
  return 0.;
}

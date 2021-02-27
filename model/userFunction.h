/*
  @copyright Steve Keen 2020
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef USERFUNCTION_H
#define USERFUNCTION_H
#include <memory>                   // for allocator, shared_ptr
#include <string>                   // for string, operator+, char_traits
#include <vector>                   // for vector
#include "TCL_obj_stl.h"            // for TCL_objp
#include "cairo.h"                  // for cairo_t
#include "callableFunction.h"       // for CallableFunction
#include "classdesc_access.h"       // for CLASSDESC_ACCESS
#include "item.h"                   // for BoundingBox, Item, ItemT
#include "operation.h"              // for Operation, NamedOp, Operation::Type
#include "operationType.h"          // for OperationType, OperationType::use...
#include "typeName_epilogue.h"      // for typeName
#include "unitsExpressionWalker.h"  // for UnitsExpressionWalker
#include "variableType.h"           // for Units, operator<<

namespace  minsky
{
  class UserFunction: public ItemT<UserFunction, Operation<OperationType::userFunction>>, public NamedOp, public CallableFunction
  {
    struct Impl;
    std::shared_ptr<Impl> impl;
    void updateBB() override {bb.update(*this);}
    CLASSDESC_ACCESS(UserFunction);
  public:
    static int nextId;
    std::vector<std::string> argNames;
    std::vector<double> argVals;
    std::string expression;
    UserFunction(): UserFunction("uf"+std::to_string(nextId++)+"(x,y)") {}
    UserFunction(const std::string& name, const std::string& expression="");
    std::vector<std::string> symbolNames() const;
    void compile();
    double evaluate(double x, double y);

    /// evaluate function on arbitrary number of arguments (exprtk support)
    double operator()(const std::vector<double>& p) override;

    Units units(bool check=false) const override;
    void displayTooltip(cairo_t* cr, const std::string& tt) const override
    {Item::displayTooltip(cr,tt.empty()? expression: tt+" "+expression);}

    using NamedOp::description;
    std::string description(const std::string&) override;
    /// function name, shorn of argument decorators
    std::string name() const override;

    // required by the compiler
    static UserFunction* create(Type t) 
    {return (t==OperationType::userFunction)? new UserFunction: nullptr;}
    
  };

  // static UnitExpressionWalker that is initialised to the time unit
  extern UnitsExpressionWalker timeUnit;

}
#include "userFunction.cd"
#include "userFunction.xcd"
#endif

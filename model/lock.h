/*
  @copyright Steve Keen 2021
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

#ifndef LOCK_H
#define LOCK_H

#include <string>           // for operator+, operator==
#include "TCL_obj_stl.h"    // for TCL_objp
#include "cairo.h"          // for cairo_t
#include "item.h"           // for ItemT
#include "operationType.h"  // for operator<<
#include "ravelState.h"     // for RavelState
#include "variableType.h"   // for Units, operator<<

namespace minsky
{
  class Ravel;
  class Lock: public ItemT<Lock>
  {
  public:
    Lock();
    ravel::RavelState lockedState;

    bool locked() const {return !lockedState.empty();}
    void toggleLocked();

    void draw(cairo_t* context) const override;
    Units units(bool) const override;
    /// Ravel this is connected to. nullptr if not connected to a Ravel
    Ravel* ravelInput() const;
  };

  /// set the lock icon resources
  /// @param locked SVG file showing a locked icon
  /// @param unlocked SVG file showing an unlocked icon
  void setLockIcons(const std::string& locked, const std::string& unlocked);
}

#include "lock.cd"
#endif

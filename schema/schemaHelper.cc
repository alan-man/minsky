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

#include "schemaHelper.h"
#include <ctype.h>    // for isspace
#include <algorithm>  // for replace
#include <iosfwd>     // for std
#include <utility>    // for move
#include "a85.h"      // for from_a85, size_for_a85, size_for_bin, to_a85
#include "zStream.h"  // for DeflateZStream, InflateZStream

#include <capiRenderer.h>
#include "lassoBox.h"
#include "selection.h"
#include "SVGItem.h"
#include "minsky_epilogue.h"
using namespace std;

namespace minsky
{
  classdesc::pack_t decode(const classdesc::CDATA& data)
  {
    string trimmed; //trim whitespace
    for (auto c: data)
      if (!isspace(c)) trimmed+=c;
    
    vector<unsigned char> zbuf(a85::size_for_bin(trimmed.size()));
    // reverse transformation required to avoid the escape sequence ']]>'
    replace(trimmed.begin(),trimmed.end(),'~',']'); 
    a85::from_a85(trimmed.data(), trimmed.size(),zbuf.data());

    InflateZStream zs(zbuf);
    zs.inflate();
    return move(zs.output);
  }


  classdesc::CDATA encode(const classdesc::pack_t& buf)
  {
    vector<unsigned char> zbuf(buf.size());
    DeflateZStream zs(buf, zbuf);
    zs.deflate();
    
    vector<char> cbuf(a85::size_for_a85(zs.total_out,false));
    a85::to_a85(&zbuf[0],zs.total_out, &cbuf[0], false);
    // this ensures that the escape sequence ']]>' never appears in the data
    replace(cbuf.begin(),cbuf.end(),']','~');
    return CDATA(cbuf.begin(),cbuf.end());
  }
}

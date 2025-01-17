/*
  @copyright Steve Keen 2018
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

#include "ravelWrap.h"
#include "selection.h"
#include "dimension.h"
#include "minskyTensorOps.h"
#include "minsky.h"
#include "pango.h"
#include "minsky_epilogue.h"

#include <string>
#include <cmath>
using namespace std;

#include "cairoRenderer.h"

namespace minsky
{
  unsigned RavelLockGroup::nextColour=1;

  namespace
  {
    typedef ravel::Op::ReductionOp ReductionOp;
    typedef ravel::HandleSort::Order HandleSort;
    
    inline double sqr(double x) {return x*x;} 
  }

  Ravel::Ravel()
  {
    if (!*this)
      {
        tooltip="https://ravelation.hpcoders.com.au";
        detailedText=lastError();
      }
  }

namespace
{
  struct CairoRenderer: public ravel::CairoRenderer
  {
    ecolab::Pango m_pango;
    
    static ecolab::Pango& pango(CAPIRenderer* r) {return static_cast<CairoRenderer*>(r)->m_pango;}

    static void s_showText(CAPIRenderer* c, const char* s)
    {
      pango(c).setText(s);
      pango(c).show();
    }
    static void s_setTextExtents(CAPIRenderer* c, const char* s)
    {pango(c).setText(s);}
    static double s_textWidth(CAPIRenderer* c) {return pango(c).width();}
    static double s_textHeight(CAPIRenderer* c) {return pango(c).height();}

    CairoRenderer(cairo_t* cairo): ravel::CairoRenderer(cairo), m_pango(cairo) {
      showText=s_showText;
      setTextExtents=s_setTextExtents;
      textWidth=s_textWidth;
      textHeight=s_textHeight;
    }
    
  };
}
  
  void Ravel::draw(cairo_t* cairo) const
  {
    double  z=zoomFactor(), r=1.1*z*radius();
    m_ports[0]->moveTo(x()+1.1*r, y());
    m_ports[1]->moveTo(x()-1.1*r, y());
    if (mouseFocus)
      {
        drawPorts(cairo);
        displayTooltip(cairo,tooltip.empty()? explanation: tooltip);
        // Resize handles always visible on mousefocus. For ticket 92.
        drawResizeHandles(cairo);
      }
    cairo_rectangle(cairo,-r,-r,2*r,2*r);
    cairo_rectangle(cairo,-1.1*r,-1.1*r,2.2*r,2.2*r);
    cairo_stroke_preserve(cairo);
    if (onBorder || lockGroup)
      { // shadow the border when mouse is over it
        cairo::CairoSave cs(cairo);
        cairo::Colour c{1,1,1,0};
        if (lockGroup)
          c=palette[ lockGroup->colour() % paletteSz ];
        c.r*=0.5; c.g*=0.5; c.b*=0.5;
        c.a=onBorder? 0.5:0.3;
        cairo_set_source_rgba(cairo,c.r,c.g,c.b,c.a);
        cairo_set_fill_rule(cairo,CAIRO_FILL_RULE_EVEN_ODD);
        cairo_fill_preserve(cairo);
      }
    
    cairo_clip(cairo);

    {
      cairo::CairoSave cs(cairo);
      cairo_rectangle(cairo,-r,-r,2*r,2*r);
      cairo_clip(cairo);
      cairo_scale(cairo,z,z);
      CairoRenderer cr(cairo);
      render(cr);
    }        
    if (selected) drawSelected(cairo);
  }

  void Ravel::resize(const LassoBox& b)
  {
    rescale(0.5*std::max(fabs(b.x0-b.x1),fabs(b.y0-b.y1))/(1.21*zoomFactor()));
    moveTo(0.5*(b.x0+b.x1), 0.5*(b.y0+b.y1));
    bb.update(*this);
  }

  bool Ravel::inItem(float xx, float yy) const
  {
    float r=1.1*zoomFactor()*radius();
    return std::abs(xx-x())<=r && std::abs(yy-y())<=r;
  }
  
  void Ravel::onMouseDown(float xx, float yy)
  {
    double invZ=1/zoomFactor();
    ravel::Ravel::onMouseDown((xx-x())*invZ,(yy-y())*invZ);
  }
  
  void Ravel::onMouseUp(float xx, float yy)
  {
    double invZ=1/zoomFactor();
    ravel::Ravel::onMouseUp((xx-x())*invZ,(yy-y())*invZ);
    broadcastStateToLockGroup();
  }
  bool Ravel::onMouseMotion(float xx, float yy)
  {
    double invZ=1/zoomFactor();
    return ravel::Ravel::onMouseMotion((xx-x())*invZ,(yy-y())*invZ);
  }
  
  bool Ravel::onMouseOver(float xx, float yy)
  {
    double invZ=1/zoomFactor();
    return ravel::Ravel::onMouseOver((xx-x())*invZ,(yy-y())*invZ);
  }

  Hypercube Ravel::hypercube() const
  {
    auto outHandles=outputHandleIds();
    Hypercube hc;
    auto& xv=hc.xvectors;
    for (auto h: outHandles)
      {
        auto labels=sliceLabels(h);
        xv.emplace_back(handleDescription(h));
        auto dim=axisDimensions.find(xv.back().name);
        if (dim!=axisDimensions.end())
          xv.back().dimension=dim->second;
        else
          {
            auto dim=cminsky().dimensions.find(xv.back().name);
            if (dim!=cminsky().dimensions.end())
              xv.back().dimension=dim->second;
          }
        // else otherwise dimension is a string (default type)
        for (auto& i: labels)
          xv.back().push_back(i);
      }
    return hc;
  }
  
  void Ravel::populateHypercube(const Hypercube& hc)
  {
    auto state=initState.empty()? getState(): initState;
    bool redistribute=!initState.empty();
    initState.clear();
    clear();
    for (auto& i: hc.xvectors)
      {
        vector<string> ss;
        for (auto& j: i) ss.push_back(str(j,i.dimension.units));
        addHandle(i.name, ss);
        size_t h=numHandles()-1;
        ravel::Ravel::displayFilterCaliper(h,false);
      }
    if (state.empty())
      setRank(hc.rank());
    else
      {
        applyState(state);
        if (redistribute) redistributeHandles();
      }
#ifndef NDEBUG
    if (state.empty())
      {
        auto d=hc.dims();
        assert(d.size()==rank());
        auto outputHandles=outputHandleIds();
        for (size_t i=0; i<d.size(); ++i)
          assert(d[i]==numSliceLabels(outputHandles[i]));
      }
#endif
  }

  
  unsigned Ravel::maxRank() const
  {
    return numHandles();
  }

  void Ravel::setRank(unsigned rank)
  {
    vector<size_t> ids;
    for (size_t i=0; i<rank; ++i) ids.push_back(i);
    setOutputHandleIds(ids);
  }
  
  void Ravel::adjustSlicer(int n)
  {
    ravel::Ravel::adjustSlicer(n);
    broadcastStateToLockGroup();
  }

  bool Ravel::onKeyPress(int keySym, const std::string& utf8, int state)
  {
    switch (keySym)
      {
      case 0xff52: case 0xff53: //Right, Up
        adjustSlicer(1);
        break;
      case 0xff51: case 0xff54: //Left, Down
        adjustSlicer(-1);
        break;
      default:
        return false;
      }
    if (state>0) minsky().reset(); //reset if any modifier key pressed
    return true;
  }

  
  bool Ravel::displayFilterCaliper() const
  {
    int h=selectedHandle();
    if (h>=0)
      {
        auto state=getHandleState(h);
        return state.displayFilterCaliper;
      }
    return false;
  }
    
  bool Ravel::setDisplayFilterCaliper(bool x)
  {
    int h=selectedHandle();
    if (h>=0)
      ravel::Ravel::displayFilterCaliper(h,x);
    return x;
  }

  vector<string> Ravel::allSliceLabels() const
  {
    return ravel::Ravel::allSliceLabels(selectedHandle(),ravel::HandleSort::forward);
  }
  
  vector<string> Ravel::allSliceLabelsAxis(int axis) const
  {
    return ravel::Ravel::allSliceLabels(axis,ravel::HandleSort::forward);
  }  

  vector<string> Ravel::pickedSliceLabels() const
  {return sliceLabels(selectedHandle());}

  void Ravel::pickSliceLabels(int axis, const vector<string>& pick) 
  {
    if (axis>=0 && axis<int(numHandles()))
      {
        // stash previous handle sort order
        auto state=getHandleState(axis);
        if (state.order!=ravel::HandleSort::custom)
          previousOrder=state.order;

        if (pick.size()>=numAllSliceLabels(axis))
          {
            // if all labels are selected, revert ordering to previous
            setHandleSortOrder(previousOrder, axis);
            return;
          }
        
        auto allLabels=ravel::Ravel::allSliceLabels(axis, ravel::HandleSort::none);
        map<string,size_t> idxMap; // map index positions
        for (size_t i=0; i<allLabels.size(); ++i)
          idxMap[allLabels[i]]=i;
        vector<size_t> customOrder;
        for (auto& i: pick)
          {
            auto j=idxMap.find(i);
            if (j!=idxMap.end())
              customOrder.push_back(j->second);
          }
        assert(!customOrder.empty());
        applyCustomPermutation(axis,customOrder);
      }
  }

  Dimension Ravel::dimension(int handle) const
  {
    Dimension dim;
    auto dimitr=cminsky().dimensions.find(handleDescription(handle));
    if (dimitr!=cminsky().dimensions.end())
      dim=dimitr->second;
    return dim;
  }
  
  ravel::HandleSort::Order Ravel::sortOrder() const
  {
    int h=selectedHandle();
    if (h>=0)
      {
        auto state=getHandleState(h);
        return state.order;
      }
    return ravel::HandleSort::none;
  }
 
  ravel::HandleSort::Order Ravel::setSortOrder(ravel::HandleSort::Order x)
  {
    setHandleSortOrder(x, selectedHandle());
    return x;
  }

  ravel::HandleSort::Order Ravel::setHandleSortOrder(ravel::HandleSort::Order order, int handle)
  {
    if (handle>=0)
      {
        Dimension dim=dimension(handle);
        orderLabels(handle,order,ravel::toEnum<ravel::HandleSort::OrderType>(dim.type),dim.units);
      }
    return order;
  }

  bool Ravel::handleSortableByValue() const
  {
    if (rank()!=1) return false;
    auto ids=outputHandleIds();
    return size_t(selectedHandle())==ids[0];
  }

  void Ravel::sortByValue(ravel::HandleSort::Order dir)
  {
    if (rank()!=1) return;
    setHandleSortOrder(ravel::HandleSort::none, outputHandleIds()[0]);
    try {minsky().reset();} catch (...) {throw runtime_error("Cannot sort handle at the moment");}
    auto vv=m_ports[0]->getVariableValue();
    if (!vv)
      throw runtime_error("Cannot sort handle at the moment");
    vector<size_t> permutation;
    for (size_t i=0; i<vv->hypercube().xvectors[0].size(); ++i)
      if (std::isfinite(vv->atHCIndex(i)))
        permutation.push_back(i);
    switch (dir)
      {
      case ravel::HandleSort::forward:
        sort(permutation.begin(), permutation.end(), [&](size_t i, size_t j)
        {return vv->atHCIndex(i)<vv->atHCIndex(j);});
        break;
      case ravel::HandleSort::reverse:
        sort(permutation.begin(), permutation.end(), [&](size_t i, size_t j)
        {return vv->atHCIndex(i)>vv->atHCIndex(j);});
        break;
      default:
        break;
      }
    applyCustomPermutation(outputHandleIds()[0], permutation);
  }
  
  
  string Ravel::description() const
  {
    return handleDescription(selectedHandle());
  }
  
  void Ravel::setDescription(const string& description)
  {
    setHandleDescription(selectedHandle(),description);
  }

  Dimension::Type Ravel::dimensionType() const
  {
    auto descr=description();
    auto i=axisDimensions.find(descr);
    if (i!=axisDimensions.end())
      return i->second.type;
    {
      auto i=cminsky().dimensions.find(descr);
      if (i!=cminsky().dimensions.end())
        return i->second.type;
    }
    return Dimension::string;
  }
  
  std::string Ravel::dimensionUnitsFormat() const
  {
    auto descr=description();
    if (descr.empty()) return "";
    auto i=axisDimensions.find(descr);
    if (i!=axisDimensions.end())
      return i->second.units;
    i=cminsky().dimensions.find(descr);
    if (i!=cminsky().dimensions.end())
      return i->second.units;
    return "";
  }
      
  /// @throw if type does not match global dimension type
  void Ravel::setDimension(Dimension::Type type,const std::string& units)
  {
    auto descr=description();
    if (descr.empty()) return;
    auto i=cminsky().dimensions.find(descr);
    Dimension d{type,units};
    if (i!=cminsky().dimensions.end())
      {
        if (type!=i->second.type)
          throw error("type mismatch with global dimension");
      }
    else
      {
        minsky().dimensions[descr]=d;
        minsky().imposeDimensions();
      }
    axisDimensions[descr]=d;
  }

  
  void Ravel::exportAsCSV(const string& filename) const
  {
    if (!m_ports.empty())
      if (auto vv=m_ports[0]->getVariableValue())
        {
          vv->exportAsCSV(filename, ravel::Ravel::description());
          return;
        }

    // if no variable value attached, create one
    VariableValue v(VariableType::flow);
    TensorsFromPort tp(make_shared<EvalCommon>());
    tp.ev->update(ValueVector::flowVars.data(), ValueVector::flowVars.size(), ValueVector::stockVars.data());
    v=*tensorOpFactory.create(itemPtrFromThis(), tp);
    // TODO: add some comment lines, such as source of data
    v.exportAsCSV(filename, ravel::Ravel::description());
  }

  Units Ravel::units(bool check) const
  {
    Units inputUnits=m_ports[1]->units(check);
    if (inputUnits.empty()) return inputUnits;
    size_t multiplier=1;
    // at this stage, gross up exponents by the handle size of each
    // reduced by product handles
    for (size_t h=0; h<numHandles(); ++h)
      {
        auto state=getHandleState(h);
        if (state.collapsed && state.reductionOp==ravel::Op::prod)
          multiplier*=numSliceLabels(h);
      }
    if (multiplier>1)
      for (auto& u: inputUnits)
        u.second*=multiplier;
    return inputUnits;
  }

  void Ravel::applyState(const ravel::RavelState& state)
 {
   auto r=radius();
   setRavelState(state);
   if (state.radius!=r) // only need to update bounding box if radius changes
     updateBoundingBox();
 }

  
  void Ravel::displayDelayedTooltip(float xx, float yy)
  {
    if (rank()==0)
      explanation="load CSV data from\ncontext menu";
    else
      {
        explanation=explain(xx-x(),yy-y());
        // line break every 5 words
        int spCnt=0;
        for (auto& c: explanation)
          if (isspace(c) && ++spCnt % 5 == 0)
            c='\n';
      }
  }
    
  void Ravel::leaveLockGroup()
  {
    if (lockGroup)
      lockGroup->removeFromGroup(*this);
    lockGroup.reset();
  }

  void Ravel::broadcastStateToLockGroup() const
  {
    if (lockGroup) lockGroup->broadcast(*this);
  }

  void RavelLockGroup::initialBroadcast()
  {
    if (!m_ravels.empty())
      if (auto r=m_ravels.front().lock())
        broadcast(*r);
  }

  void RavelLockGroup::broadcast(const Ravel& ravel)
  {
    vector<shared_ptr<Ravel>> lockedRavels;
    size_t ravelIdx=m_ravels.size();
    for (auto& i: m_ravels)
      {
        lockedRavels.push_back(i.lock());
        if (lockedRavels.back().get()==&ravel)
          ravelIdx=lockedRavels.size()-1;
      }
    if (ravelIdx==m_ravels.size()) return; // not in lock group
    
    const auto sourceState=ravel.getState();

    if (handleLockInfo.empty()) // default is all handles are lock
      {
        for (auto& i: m_ravels)
          if (auto r=i.lock())
            r->applyState(sourceState);
        return;
      }

    // reorder source handle states according to handleLockInfo
    vector<const ravel::HandleState*> sourceHandleStates;
    set<string> handlesAdded;
    for (auto& i: handleLockInfo)
      {
        auto hs=find_if(sourceState.handleStates.begin(), sourceState.handleStates.end(),
                        [&](const ravel::HandleState& hs){return hs.description==i.handleNames[ravelIdx];});
        if (hs!=sourceState.handleStates.end())
          {
            sourceHandleStates.emplace_back(&*hs);
            if (!handlesAdded.insert(hs->description).second)
              throw runtime_error("Multiple locks found on handle "+hs->description);
          }
        else
          sourceHandleStates.emplace_back(nullptr);
      }

    assert(sourceHandleStates.size()==handleLockInfo.size());
    
    for (size_t ri=0; ri<m_ravels.size(); ++ri)
      if (auto r=m_ravels[ri].lock())
        {
          if (r.get()==&ravel) continue;
          auto state=r->getState();
          set<string> outputHandles(state.outputHandles.begin(), state.outputHandles.end());
          for (size_t i=0; i<handleLockInfo.size(); ++i)
            {
              if (!sourceHandleStates[i]) continue;
              auto& sourceHandleState=*sourceHandleStates[i];
              
              auto& hlInfo=handleLockInfo[i];
              auto handleState=find_if(state.handleStates.begin(), state.handleStates.end(),
                                            [&](ravel::HandleState& s){return s.description==hlInfo.handleNames[ri];});
              if (handleState!=state.handleStates.end())
                {
                  if (hlInfo.slicer)
                    {
                      handleState->sliceLabel=sourceHandleState.sliceLabel;
                      if (find(sourceState.outputHandles.begin(), sourceState.outputHandles.end(), sourceHandleState.description)!=sourceState.outputHandles.end())
                        // is an output handle
                        outputHandles.insert(handleState->description);
                      else
                        outputHandles.erase(handleState->description);
                    }
                  if (hlInfo.orientation)
                    {
                      handleState->x=sourceHandleState.x;
                      handleState->y=sourceHandleState.y;
                      handleState->collapsed=sourceHandleState.collapsed;
                      handleState->reductionOp=sourceHandleState.reductionOp;
                    }
                  if (hlInfo.calipers)
                    {
                      handleState->displayFilterCaliper=sourceHandleState.displayFilterCaliper;
                      handleState->minLabel=sourceHandleState.minLabel;
                      handleState->maxLabel=sourceHandleState.maxLabel;
                    }
                  if (hlInfo.order)
                    {
                      handleState->order=sourceHandleState.order;
                      handleState->customOrder=sourceHandleState.customOrder;
                    }
                }
            }
          r->applyState(state);
        }
  }

  void RavelLockGroup::validateLockHandleInfo()
  {
    vector<set<string>> checkHandleNames(m_ravels.size());
    for (auto& hl: handleLockInfo)
      {
        if (hl.handleNames.size()!=m_ravels.size())
          throw runtime_error("Insufficient data on line");
        for (size_t i=0; i<hl.handleNames.size(); ++i)
          {
            auto& nm=hl.handleNames[i];
            // non-breaking space 0xa0 not treated as space by isspace
            if (find_if(nm.begin(), nm.end(), [](char i){return !isspace(i)&&i!=0xa0;})==nm.end())
              continue; // disregard wholly white space strings
            if (!checkHandleNames[i].insert(nm).second) // check for duplicated handle names in a column
              throw runtime_error("duplicate handle name "+nm);
          }
      }
  }

  
  vector<string> RavelLockGroup::allLockHandles() const
  {
    set<string> handles;
    for (auto& rr: m_ravels)
      if (auto r=rr.lock())
        {
          auto state=r->getState();
          for (auto& h: state.handleStates)
            handles.insert(h.description);
        }
    return {handles.begin(), handles.end()};
  }
  
  std::vector<std::string> RavelLockGroup::ravelNames() const
  {
    std::vector<std::string> r;
    int cnt=0;
    for (auto& i: m_ravels)
      if (auto rr=i.lock())
        r.emplace_back(rr->tooltip.empty()? to_string(cnt++): rr->tooltip);
      else
        r.emplace_back("<invalid>");
    return r;
  }

  std::vector<std::string> RavelLockGroup::handleNames(size_t ravel_idx) const
  {
    if (auto rr=m_ravels[ravel_idx].lock())
      return rr->handleNames();
    return {};
  }
  
  void RavelLockGroup::setLockHandles(const std::vector<std::string>& handles)
  {
    handleLockInfo.clear();
    std::vector<std::set<std::string>> handleNames;
    for (auto& rp: m_ravels)
      if (auto r=rp.lock())
        {
          auto names=r->handleNames();
          handleNames.emplace_back(names.begin(), names.end());
        }
      else
        handleNames.emplace_back();
    
    for (auto& h: handles)
      {
        handleLockInfo.emplace_back();
        for (size_t i=0; i<m_ravels.size(); ++i)
          handleLockInfo.back().handleNames.push_back(handleNames[i].count(h)? h: "");
      }
  }

  void RavelLockGroup::addRavel(const std::weak_ptr<Ravel>& ravel)
  {
    m_ravels.push_back(ravel);
    for (auto& i: handleLockInfo)
      i.handleNames.resize(m_ravels.size());
    addHandleInfo(m_ravels.back());
  }
  
  void RavelLockGroup::addHandleInfo(const std::weak_ptr<Ravel>& ravel)
  {
    auto ravelIdx=&ravel-&m_ravels[0];
    assert(ravelIdx>=0);
    if (ravelIdx<0 || size_t(ravelIdx)>=m_ravels.size()) return;
    if (auto r=ravel.lock())
      {
        auto names=r->handleNames();
        set<string> handleNames(names.begin(), names.end());
        if (names.size()!=handleNames.size())
          r->throw_error("Ambiguous handle names");
        // add corresponding handles to what's already there
        for (auto& hli: handleLockInfo)
          {
            assert(hli.handleNames.size()==m_ravels.size());
            set<string> lockNames(hli.handleNames.begin(), hli.handleNames.end());
            for (auto& l: lockNames)
              {
                auto hn=handleNames.find(l);
                if (hn!=handleNames.end())
                  {
                    hli.handleNames[ravelIdx]=l;
                    handleNames.erase(hn);
                  }
              }
          }
        // now add in any extras
        for (auto& h: handleNames)
          {
            handleLockInfo.emplace_back();
            handleLockInfo.back().handleNames.resize(m_ravels.size());
            handleLockInfo.back().handleNames[ravelIdx]=h;
          }
      }
  }

  void RavelLockGroup::removeFromGroup(const Ravel& ravel)
  {
    auto found=find_if(m_ravels.begin(), m_ravels.end(),
                    [&](const weak_ptr<Ravel>& i){
                      if (auto r=i.lock())
                        return r.get()==&ravel;
                      return false;
                    });
    if (found==m_ravels.end()) return;
    for (auto& i: handleLockInfo)
      i.handleNames.erase(i.handleNames.begin()+(found-m_ravels.begin()));
    m_ravels.erase(found);
    if (m_ravels.size()==1)
      if (auto r=m_ravels[0].lock())
        r->lockGroup.reset(); // this may delete this, so should be last
  }
 
}

  

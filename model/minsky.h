/*
  @copyright Steve Keen 2012
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

#ifndef MINSKY_H
#define MINSKY_H

#include "intrusiveMap.h"
#include "bookmark.h"
#include "selection.h"
#include "godleyIcon.h"
#include "operation.h"
#include "evalOp.h"
#include "wire.h"
#include "plotWidget.h"
#include "version.h"
#include "variable.h"
#include "equations.h"
#include "latexMarkup.h"
#include "variableValue.h"
#include "canvas.h"
#include "lock.h"
#include "panopticon.h"
#include "fontDisplay.h"
#include "variableTab.h"
#include "parameterTab.h"
#include "plotTab.h"
#include "godleyTab.h"
#include "dimension.h"
#include "rungeKutta.h"
#include "saver.h"

#include <vector>
#include <string>
#include <set>
#include <deque>

#include <ecolab.h>
#include <xml_pack_base.h>
#include <xml_unpack_base.h>
using namespace ecolab;
using namespace classdesc;

namespace minsky
{
  using namespace std;
  using classdesc::shared_ptr;
  using namespace civita;
  
  struct CallableFunction;

  class SaveThread;
  
  // handle the display of rendered equations on the screen
  class EquationDisplay: public CairoSurface
  {
    Minsky& m;
    double m_width=0, m_height=0;
    bool redraw(int x0, int y0, int width, int height) override;
    CLASSDESC_ACCESS(EquationDisplay);
  public:
    float offsx=0, offsy=0; // pan controls
    double width() const {return m_width;}
    double height() const {return m_height;}
    EquationDisplay(Minsky& m): m(m) {}
    EquationDisplay& operator=(const EquationDisplay& x) {CairoSurface::operator=(x); return *this;}
    void requestRedraw() {if (surface.get()) surface->requestRedraw();}
  };

  // a place to put working variables of the Minsky class that needn't
  // be serialised.
  struct MinskyExclude
  {
    shared_ptr<ofstream> outputDataFile;
    unique_ptr<BackgroundSaver> autoSaver;

    enum StateFlags {is_edited=1, reset_needed=2, fullEqnDisplay_needed=4};
    int flags=reset_needed;
    
    std::vector<int> flagStack;

    std::map<std::string, std::shared_ptr<CallableFunction>> userFunctions;
    
    
    // make copy operations just dummies, as assignment of Minsky's
    // doesn't need to change this
    MinskyExclude(): historyPtr(0) {}
    MinskyExclude(const MinskyExclude&): historyPtr(0) {}
    MinskyExclude& operator=(const MinskyExclude&) {return *this;}
    
  protected:
    /// save history of model for undo
    /* 
       TODO: it should be sufficient to add move semantics to pack_t,
       but for some reason copy semantics are required, requiring the
       use of shared_ptr
     */
    std::deque<classdesc::pack_t> history;
    std::size_t historyPtr;

  };

  enum ItemType {wire, op, var, group, godley, plot};

  class Minsky: public Exclude<MinskyExclude>, public RungeKutta
  {
    CLASSDESC_ACCESS(Minsky);

    /// returns a diagnostic about an item that is infinite or
    /// NaN. Either a variable name, or and operator type.
    std::string diagnoseNonFinite() const;

    /// write current state of all variables to the log file
    void logVariables() const;

    Exclude<boost::posix_time::ptime> lastRedraw;

    
  public:
    EquationDisplay equationDisplay;
    Panopticon panopticon{canvas};
    FontDisplay fontSampler;
    ParameterTab parameterTab;
    VariableTab variableTab;
    PlotTab plotTab;
    GodleyTab godleyTab;
        // Allow multiple equity columns.
    bool multipleEquities=false;    

    /// reflects whether the model has been changed since last save
    bool edited() const {return flags & is_edited;}
    /// true if reset needs to be called prior to numerical integration
    bool reset_flag() const {return flags & reset_needed;}
    /// indicate model has been changed since last saved
    void markEdited() {
      flags |= is_edited | reset_needed | fullEqnDisplay_needed;
      canvas.model.updateTimestamp();
    }

    /// @{ push and pop state of the flags
    void pushFlags() {flagStack.push_back(flags);}
    void popFlags() {
      if (!flagStack.empty()) {
        flags=flagStack.back();
        flagStack.pop_back();
      }
    }
    /// @}

    void step();  ///< step the equations (by n steps, default 1)

    bool resetIfFlagged() override {
      if (reset_flag())
        reset();
      return reset_flag();
    }
    
    VariableValues variableValues;
    Dimensions dimensions;
    Conversions conversions;
    /// fills in dimensions table with all loaded ravel axes
    void populateMissingDimensions();

    void populateMissingDimensionsFromVariable(const VariableValue&);
    
    void setGodleyIconResource(const string& s)
    {GodleyIcon::svgRenderer.setResource(s);}
    void setGroupIconResource(const string& s)
    {Group::svgRenderer.setResource(s);}
    void setLockIconResource(const string& locked, const string& unlocked) {
      Lock::lockedIcon.setResource(locked);
      Lock::unlockedIcon.setResource(unlocked);
    }
    
    /// @return available matching columns from other Godley tables
    /// @param currTable - this table, not included in the matching process
    //  @param ac type of column we wish matches for
    std::set<string> matchingTableColumns(const GodleyIcon& currTable, GodleyAssetClass::AssetClass ac);

    /// find any duplicate column, and use it as a source column for balanceDuplicateColumns
    void importDuplicateColumn(GodleyTable& srcTable, int srcCol);
    /// makes all duplicated columns consistent with \a srcTable, \a srcCol
    void balanceDuplicateColumns(const GodleyIcon& srcTable, int srcCol);

    // reset m_edited as the GodleyIcon constructor calls markEdited
    Minsky(): equationDisplay(*this) {
      lastRedraw=boost::posix_time::microsec_clock::local_time();
      model->iHeight(std::numeric_limits<float>::max());
      model->iWidth(std::numeric_limits<float>::max());
      model->self=model;
    }

    GroupPtr model{new Group};
    Canvas canvas{model};

    void clearAllMaps();

    /// returns reference to variable defining (ie input wired) for valueId
    VariablePtr definingVar(const std::string& valueId) const;

    static void saveGroupAsFile(const Group&, const string& fileName);
    void saveCanvasItemAsFile(const string& fileName) const
    {if (auto g=dynamic_cast<Group*>(canvas.item.get())) saveGroupAsFile(*g,fileName);}

    void initGodleys();

    /// erase items in current selection, put copy into clipboard
    void cut();
    /// copy items in current selection into clipboard
    void copy() const;
    /// paste clipboard as a new group or ungrouped items on the canvas. canvas.itemFocus is set to
    /// refer to the new group or items.
    void paste();
    void saveSelectionAsFile(const string& fileName) const {saveGroupAsFile(canvas.selection,fileName);}
    
    /// @{ override to provide clipboard handling functionality
    virtual void putClipboard(const string&) const {}
    virtual std::string getClipboard() const {return "";}
    /// @}

    void insertGroupFromFile(const char* file);

    void makeVariablesConsistent();

    void imposeDimensions();

    // runs over all ports and variables removing those not in use
    void garbageCollect();

    /// checks for presence of illegal cycles in network. Returns true
    /// if there are some
    bool cycleCheck() const;

    /// opens the log file, and writes out a header line describing
    /// names of all variables
    void openLogFile(const string&);
    /// closes log file
    void closeLogFile() {outputDataFile.reset();}
    std::set<string> logVarList;
    
    /// construct the equations based on input data
    /// @throws ecolab::error if the data is inconsistent
    void constructEquations();
    /// performs dimension analysis, throws if there is a problem
    void dimensionalAnalysis() const;
    /// removes units markup from all variables in model
    void deleteAllUnits();
    
    /// consistency check of the equation order. Should return
    /// true. Outputs the operation number of the invalidly ordered
    /// operation.
    bool checkEquationOrder() const;

    
    void reset(); ///<resets the variables back to their initial values

    /// save to a file
    void save(const std::string& filename);
    /// load from a file
    void load(const std::string& filename);

    void exportSchema(const char* filename, int schemaLevel=1) const; //NOLINT

    /// indicate operation item has error, if visible, otherwise contining group
    void displayErrorItem(const Item& op) const;

    /// return the AEGIS assigned version number
    static const char* minskyVersion;
    std::string ecolabVersion() const {return VERSION;}
    std::string ravelVersion() const {return ravel::Ravel::version();}

    std::string fileVersion; ///< Minsky version file was saved under
    
    unsigned maxHistory{100}; ///< maximum no. of history states to save
    int maxWaitMS=100; ///< maximum  wait in millisecond between redrawing canvas during simulation

    /// clear history
    void clearHistory() {history.clear(); historyPtr=0;}
    /// called periodically to ensure history up to date
    void checkPushHistory() {if (historyPtr==history.size()) pushHistory();}

    /// push current model state onto history if it differs from previous
    bool pushHistory();

    /// flag to indicate whether a TCL should be pushed onto the
    /// history stack, or logged in a recording. This is used to avoid
    /// movements being added to recordings and undo history
    bool doPushHistory=true;

    /// restore model to state \a changes ago 
    void undo(int changes=1);

    /// set a Tk image to render equations to
    void renderEquationsToImage(const char* image);

    /// Converts variable(s) named by \a name into a variable of type \a type.
    /// @throw if conversion is disallowed
    void convertVarType(const std::string& name, VariableType::Type type);

    /// add integral to current canvas item (which must be variable
    /// convertible to an integral variable
    void addIntegral();
    
    /// returns true if any variable of name \a name has a wired input
    bool inputWired(const std::string& name) const {return definingVar(name).get();}

    /// render canvas to a postscript file
    void renderCanvasToPS(const char* filename) {canvas.renderToPS(filename);}
    /// render canvas to a PDF file
    void renderCanvasToPDF(const char* filename) {canvas.renderToPDF(filename);}
    /// render canvas to an SVG file
    void renderCanvasToSVG(const char* filename) {canvas.renderToSVG(filename);}
    /// render canvas to a PNG image file
    void renderCanvasToPNG(const char* filename) {canvas.renderToPNG(filename);}
    /// render canvas to a EMF image file (Windows only)
    void renderCanvasToEMF(const char* filename) {canvas.renderToEMF(filename);}
    
    /// render all plots 
    void renderAllPlotsAsSVG(const string& prefix) const;
    /// export all plots
    void exportAllPlotsAsCSV(const string& prefix) const;

    /// set DE mode on all godley tables
    void setAllDEmode(bool);
    /// set std library RNG seed
    void srand(int seed) {::srand(seed);}

    // godley table display values preferences
    bool displayValues=false;
    GodleyTable::DisplayStyle displayStyle=GodleyTable::sign;

    /// set display value mode on all godley table editor modes
    void setGodleyDisplayValue(bool displayValues, GodleyTable::DisplayStyle displayStyle);

    /// import a Vensim file
    void importVensim(const std::string&);
    
    /// set/clear busy cursor in GUI
    virtual void setBusyCursor() {}
    virtual void clearBusyCursor() {}

    /// display a message in a popup box on the GUI
    virtual void message(const std::string&) {}
    /// request all Godley table windows to redraw
    virtual void redrawAllGodleyTables() {}

    /// run callback attached to \a item
    virtual void runItemDeletedCallback(const Item&) {}
    
    /// check whether to proceed or abort, given a request to allocate
    /// \a bytes of memory. Implemented in MinskyTCL
    virtual bool checkMemAllocation(std::size_t bytes) const {return true;}

    /// initialises auto saving
    /// empty \a file to turn off autosave
    void setAutoSaveFile(const std::string& file);
    
  };

  /// global minsky object
  Minsky& minsky();
  /// const version to help in const correctness
  inline const Minsky& cminsky() {return minsky();}
  /// RAII set the minsky object to a different one for the current scope.
  struct LocalMinsky
  {
    LocalMinsky(Minsky& m);
    ~LocalMinsky();
  };



}

#ifdef _CLASSDESC
#pragma omit pack minsky::MinskyExclude
#pragma omit unpack minsky::MinskyExclude
#pragma omit TCL_obj minsky::MinskyExclude
#pragma omit xml_pack minsky::MinskyExclude
#pragma omit xml_unpack minsky::MinskyExclude
#pragma omit xsd_generate minsky::MinskyExclude

#pragma omit xml_pack minsky::Integral
#pragma omit xml_unpack minsky::Integral

#pragma omit pack minsky::MinskyMatrix
#pragma omit unpack minsky::MinskyMatrix
#pragma omit xml_pack minsky::MinskyMatrix
#pragma omit xml_unpack minsky::MinskyMatrix
#pragma omit xsd_generate minsky::MinskyMatrix
#endif

#include "minsky.cd"
#include "minsky.xcd"
#endif

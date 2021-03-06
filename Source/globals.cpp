/*
  ==============================================================================

    globals.cpp
    Created: 14 Dec 2015 1:54:11pm
    Author:  falindrith

  ==============================================================================
*/

#include "globals.h"
#include "MainComponent.h"
#include "AttributeSearch.h"

static ApplicationCommandManager* _manager;
static Rig* _rig;
static StatusBar* _status;
static Recorder* _recorder;
static GlobalSettings* _globalSettings;
static map<int, DeviceSet> _bins;

SearchResult::SearchResult() {
  _snapshot = nullptr;
}

SearchResult::SearchResult(const SearchResult & other) :
  _scene(other._scene), _editHistory(other._editHistory), _objFuncVal(other._objFuncVal),
  _sampleNo(other._sampleNo), _cluster(other._cluster)
{
  _snapshot = new Snapshot(*(other._snapshot));
}

SearchResult::~SearchResult() {
  if (_snapshot != nullptr)
    delete _snapshot;
}

ApplicationCommandManager* getApplicationCommandManager() {
  if (_manager == nullptr) {
    _manager = new ApplicationCommandManager();
  }

  return _manager;
}

void cleanUpGlobals() {
  if (_manager != nullptr)
    delete _manager;

  if (_rig != nullptr)
    delete _rig;

  if (_status != nullptr)
    delete _status;

  if (_recorder != nullptr)
    delete _recorder;

  if (_globalSettings != nullptr)
    delete _globalSettings;
}

bool isDeviceParamLocked(string id, string param)
{
  string metadataId = param + "_lock";
  Device* d = getRig()->getDevice(id);

  if (d == nullptr)
    return true;

  if (!d->paramExists(param))
    return true;

  if (d->getMetadata(metadataId) == "y")
    return true;
  else
    return false;
}

void lockDeviceParam(string id, string param)
{
  Device* d = getRig()->getDevice(id);

  if (d == nullptr)
    return;

  d->setMetadata(param + "_lock", "y");
  getRecorder()->log(ACTION, "Pinned " + id + " parameter " + param);
}

void unlockDeviceParam(string id, string param)
{
  Device* d = getRig()->getDevice(id);

  if (d == nullptr)
    return;

  d->setMetadata(param + "_lock", "n");
  getRecorder()->log(ACTION, "Unpinned " + id + " parameter " + param);
}

void toggleDeviceParamLock(string id, string param)
{
  Device* d = getRig()->getDevice(id);

  if (d->getMetadata(param + "_lock") == "y") {
    unlockDeviceParam(id, param);
  }
  else {
    lockDeviceParam(id, param);
  }
}

void setSessionName()
{
  time_t t = time(0);   // get time now
  struct tm * now = localtime(&t);

  char buffer[80];
  strftime(buffer, 80, "%Y-%m-%d-%H%M", now);

  getGlobalSettings()->_sessionName = "search-" + string(buffer);
}

float clamp(float val, float min, float max)
{
  return (val > max) ? max : ((val < min) ? min : val);
}

void GlobalSettings::dumpDiagnosticData()
{
  if (_exportTraces) {
    ofstream file;

    string filename = _logRootDir + "/traces/" + _sessionName + ".csv";
    
    file.open(filename, ios::trunc);

    // export format:
    // time, Thread id, sample id, function val, acceptance chance, generating edit name, feature vector
    for (auto& kvp : _samples) {
      for (auto& d : kvp.second) {
        double time = chrono::duration<float>(d._timeStamp - _searchStartTime).count();
        
        file << time << "," << kvp.first << "," << d._sampleId << "," << d._f << "," << d._a << "," << d._editName << "," << d._accepted << ",";
        for (int i = 0; i < d._scene.size(); i++) {
          file << d._scene[i];
          if (i <= (d._scene.size() - 2)) {
            file << ",";
          }
          else {
            file << "\n";
          }
        }
      }
    }

    file.close();

    // write metadata file
    filename += ".meta";
    file.open(filename, ios::trunc);
    file << _sessionSearchSettings;

    // write out search specific settings
    file << "Starting Edit Depth: " << _startChainLength << "\n";
    file << "Edit Step Size: " << _editStepSize << "\n";
    file << "Max MCMC Iterations: " << _maxMCMCIters << "\n";
    file << "Max Displayed Results: " << _maxReturnedScenes << "\n";
    file << "Required Distance from Other Results: " << _jndThreshold << "\n";
    file << "Search Failure Limit: " << _searchFailureLimit << "\n";
    file << "Search Threads: " << _searchThreads << "\n";
    file << "Temperature: " << _T << "\n";

    file.close();

    if (_autoRunTraceGraph) {
      // actually just go generate a report now
      string cmd = "python C:/Users/eshimizu/Documents/AttributesInterface/dataviz/tsne2.py " + _logRootDir + "/traces/" + _sessionName + " 30";
      system(cmd.c_str());
    }
  }

  _clusterCounter = 0;
}

void GlobalSettings::loadDiagnosticData(string filename)
{
  ifstream file(filename);
  string line;

  _loadedTraces.clear();

  getStatusBar()->setStatusMessage("Loading traces...");
  int traceID = 1;

  if (file.is_open()) {
    while (getline(file, line)) {
      stringstream lineStream(line);
      string cell;

      vector<double> sceneVals;
      string tooltip;
      DebugData sample;

      int i = 0;
      while (getline(lineStream, cell, ',')) {
        if (i == 1) {
          sample._threadId = stoi(cell);
        }
        else if (i == 2) {
          sample._sampleId = stoi(cell);
        }
        else if (i == 3) {
          sample._f = stod(cell);
        }
        else if (i == 4) {
          sample._a = stod(cell);
        }
        else if (i == 5) {
          sample._editName = cell;
        }
        else if (i == 6) {
          sample._accepted = (stoi(cell) == 1) ? true : false;
        }
        else if (i > 6) {
          sceneVals.push_back(stod(cell));
        }
        
        i++;
      }

      // create scene vector
      sample._scene.resize(sceneVals.size());
      for (i = 0; i < sceneVals.size(); i++) {
        sample._scene[i] = sceneVals[i];
      }

      // sort debug data
      if (sample._threadId == -1) {
        _loadedTraces[-1] = { sample };
      }
      else {
        if (sample._editName == "TERMINAL") {
          // new trace id needed, skip adding the terminal element, it's a duplicate
          traceID++;
        }
        else {
          _loadedTraces[traceID].push_back(sample);
        }
      }
    }
  }

  getStatusBar()->setStatusMessage("Loaded " + String(_loadedTraces.size()) + " traces.");
}

unsigned int GlobalSettings::getSampleID()
{
  unsigned int ret = _clusterCounter;
  _clusterCounter++;
  return ret;
}

void GlobalSettings::updateCache()
{
  ArnoldAnimationPatch* p = getAnimationPatch();

  if (p == nullptr) {
    getRecorder()->log(RENDER, "Render failed. Could not find ArnoldAnimationPatch.");
    return;
  }

  // Get the image dimensions
  int width = 100 * 2; //getGlobalSettings()->_renderWidth;
  int height = 100 * 2; //getGlobalSettings()->_renderHeight;
  p->setDims(width, height);
  p->setSamples(getGlobalSettings()->_stageRenderSamples);

  Image cache = Image(Image::ARGB, width, height, true);
  uint8* bufptr = Image::BitmapData(cache, Image::BitmapData::readWrite).getPixelPointer(0, 0);
  p->renderSingleFrameToBuffer(getRig()->getDeviceRaw(), bufptr, width, height);

  setCache(cache);
}

void GlobalSettings::setCache(Image img)
{
  _renderCache = img;
  _cacheUpdated = true;
}

void GlobalSettings::invalidateCache()
{
  _cacheUpdated = false;
}

void GlobalSettings::clearEdits()
{
  for (auto& e : _edits) {
    delete e;
  }
  _edits.clear();
}

void GlobalSettings::generateDefaultConstraints()
{
  // create a constraint for each system
  auto systems = getRig()->getMetadataValues("system");
  auto& constraints = getGlobalSettings()->_constraints;

  for (auto& s : systems) {
    string query = "$system=" + s;
    constraints[query] = ConsistencyConstraint(query, LOCAL, { INTENSITY, RED, GREEN, BLUE, HUE, SAT });
  }
}

void GlobalSettings::exportSettings()
{
  // there are plenty of things in the globals object but here we'll
  // only save things that are actually user-editable
  // note that user-editable is defined as "has a branch somewhere in SettingsEditor.cpp"
  // not not "actually shows up in the settings panel

  JSONNode settings;
  settings.push_back(JSONNode("version", ProjectInfo::versionString));

  settings.push_back(JSONNode("thumbnailRenderSamples", _thumbnailRenderSamples));
  //settings.push_back(JSONNode("stageRenderSamples", _stageRenderSamples));
  settings.push_back(JSONNode("searchDerivDelta", _searchDerivDelta));
  settings.push_back(JSONNode("renderWidth", _renderWidth));
  settings.push_back(JSONNode("renderHeight", _renderHeight));
  settings.push_back(JSONNode("thumbnailScale", _thumbnailRenderScale));
  settings.push_back(JSONNode("startChainLength", _startChainLength));
  settings.push_back(JSONNode("clusterDistThreshold", _clusterDistThreshold));
  settings.push_back(JSONNode("editStepSize", _editStepSize));
  settings.push_back(JSONNode("maxMCMCIters", _maxMCMCIters));
  settings.push_back(JSONNode("jndThreshold", _jndThreshold));
  settings.push_back(JSONNode("clusterElemsPerRow", _clusterElemsPerRow));
  settings.push_back(JSONNode("maxReturnedScenes", _maxReturnedScenes));
  settings.push_back(JSONNode("T", _T));
  settings.push_back(JSONNode("meanShiftBandwidth", _meanShiftBandwidth));
  settings.push_back(JSONNode("meanShiftEps", _meanShiftEps));
  settings.push_back(JSONNode("searchThreads", _searchThreads));
  settings.push_back(JSONNode("numPrimaryClusters", _numPrimaryClusters));
  settings.push_back(JSONNode("numSecondaryClusters", _numSecondaryClusters));
  settings.push_back(JSONNode("spectralBandwidth", _spectralBandwidth));
  settings.push_back(JSONNode("maxGradIters", _maxGradIters));
  settings.push_back(JSONNode("primaryDivisiveThreshold", _primaryDivisiveThreshold));
  settings.push_back(JSONNode("secondaryDivisibeThreshold", _secondaryDivisiveThreshold));
  settings.push_back(JSONNode("evWeight", _evWeight));
  settings.push_back(JSONNode("resampleTime", _resampleTime));
  settings.push_back(JSONNode("resampleThreads", _resampleThreads));
  settings.push_back(JSONNode("maskTolerance", _maskTolerance));
  settings.push_back(JSONNode("repulsionConeK", _repulsionConeK));
  settings.push_back(JSONNode("repulsionCostK", _repulsionCostK));
  settings.push_back(JSONNode("numPairs", _numPairs));
  settings.push_back(JSONNode("viewJndThreshold", _viewJndThreshold));
  settings.push_back(JSONNode("thresholdDecayRate", _thresholdDecayRate));
  settings.push_back(JSONNode("bigBucketSize", _bigBucketSize));

  if (getAnimationPatch() != nullptr) {
    CachingArnoldInterface* renderer = dynamic_cast<CachingArnoldInterface*>(getAnimationPatch()->getArnoldInterface());
    if (renderer)
      settings.push_back(JSONNode("exposure", renderer->getExposure()));
  }

  settings.push_back(JSONNode("exportTraces", _exportTraces));
  settings.push_back(JSONNode("grayscaleMode", _grayscaleMode));
  settings.push_back(JSONNode("autoRunTraceGraph", _autoRunTraceGraph));
  settings.push_back(JSONNode("useFGMask", _useFGMask));
  settings.push_back(JSONNode("reduceRepeatEdits", _reduceRepeatEdits));
  settings.push_back(JSONNode("uniformEditWeights", _uniformEditWeights));
  settings.push_back(JSONNode("randomInit", _randomInit));
  settings.push_back(JSONNode("continuousSort", _continuousSort));
  settings.push_back(JSONNode("useSearchStyles", _useSearchStyles));
  settings.push_back(JSONNode("recalculateWeights", _recalculateWeights));
  settings.push_back(JSONNode("autoCluster", _autoCluster));
  settings.push_back(JSONNode("unconstrained", _unconstrained));
  settings.push_back(JSONNode("pxIntensDist", _pxIntensDist));
  settings.push_back(JSONNode("noPinWiggle", _noPinWiggle));

  settings.push_back(JSONNode("primaryClusterMethod", _primaryClusterMethod));
  settings.push_back(JSONNode("primaryClusterMetric", _primaryClusterMetric));
  settings.push_back(JSONNode("primaryFocusArea", _primaryFocusArea));
  settings.push_back(JSONNode("secondaryClusterMethod", _secondaryClusterMethod));
  settings.push_back(JSONNode("secondaryClusterMetric", _secondaryClusterMetric));
  settings.push_back(JSONNode("secondaryFocusArea", _secondaryFocusArea));
  settings.push_back(JSONNode("clusterDisplay", _clusterDisplay));
  settings.push_back(JSONNode("searchMode", _searchMode));
  settings.push_back(JSONNode("editSelectMode", _editSelectMode));
  settings.push_back(JSONNode("searchDistMetric", _searchDistMetric));
  settings.push_back(JSONNode("searchDispMetric", _searchDispMetric));

  settings.push_back(JSONNode("logDirectory", _logRootDir));
  settings.push_back(JSONNode("imageDirectory", _imageAttrLoc.getFullPathName().toStdString()));

  File settingsFile = File::getCurrentWorkingDirectory().getChildFile("settings.json");

  ofstream file;
  file.open(settingsFile.getFullPathName().toStdString(), ios::trunc);
  file << settings.write_formatted();
}

void GlobalSettings::loadSettings()
{
  File settingsFile = File::getCurrentWorkingDirectory().getChildFile("settings.json");

  if (settingsFile.existsAsFile()) {
    ifstream file;
    file.open(settingsFile.getFullPathName().toStdString(), ios::in | ios::binary | ios::ate);

    // "+ 1" for the ending
    streamoff size = file.tellg();
    char* memblock = new char[(unsigned int)size + 1];

    file.seekg(0, ios::beg);

    file.read(memblock, size);
    file.close();

    // It's not guaranteed that the following memory after memblock is blank.
    // C-style string needs an end.
    memblock[size] = '\0';

    JSONNode n = libjson::parse(memblock);

    // iterate through the node and get the data
    for (auto it = n.begin(); it != n.end(); it++) {
      string name = it->name();

      if (name == "thumbnailRenderSamples")
        _thumbnailRenderSamples = it->as_int();
      else if (name == "searchDerivDelta")
        _searchDerivDelta = it->as_float();
      else if (name == "renderWidth")
        _renderWidth = it->as_int();
      else if (name == "renderHeight")
        _renderHeight = it->as_int();
      else if (name == "thumbnailScale")
        _thumbnailRenderScale = it->as_float();
      else if (name == "startChainLength")
        _startChainLength = it->as_int();
      else if (name == "clusterDistThreshold")
        _clusterDistThreshold = it->as_float();
      else if (name == "editStepSize")
        _editStepSize = it->as_float();
      else if (name == "maxMCMCIters")
        _maxMCMCIters = it->as_int();
      else if (name == "jndThreshold")
        _jndThreshold = it->as_float();
      else if (name == "clusterElemsPerRow")
        _clusterElemsPerRow = it->as_int();
      else if (name == "maxReturnedScenes")
        _maxReturnedScenes = it->as_int();
      else if (name == "T")
        _T = it->as_float();
      else if (name == "meanShiftBandwidth")
        _meanShiftBandwidth = it->as_float();
      else if (name == "meanShiftEps")
        _meanShiftEps = it->as_float();
      else if (name == "numPrimaryClusters")
        _numPrimaryClusters = it->as_int();
      else if (name == "numSecondaryClusters")
        _numSecondaryClusters = it->as_int();
      else if (name == "spectralBandwidth")
        _spectralBandwidth = it->as_float();
      else if (name == "maxGradIters")
        _maxGradIters = it->as_int();
      else if (name == "primaryDivisiveThreshold")
        _primaryDivisiveThreshold = (float)it->as_float();
      else if (name == "secondaryDivisiveThreshold")
        _secondaryDivisiveThreshold = (float)it->as_float();
      else if (name == "evWeight")
        _evWeight = (float)it->as_float();
      else if (name == "resampleTime")
        _resampleTime = it->as_int();
      else if (name == "resampleThreads")
        _resampleThreads = it->as_int();
      else if (name == "maskTolerance")
        _maskTolerance = it->as_float();
      else if (name == "repulsionConeK")
        _repulsionConeK = it->as_float();
      else if (name == "repulsionCostK")
        _repulsionCostK = it->as_float();
      else if (name == "numPairs")
        _numPairs = it->as_int();
      else if (name == "viewJndThreshold")
        _viewJndThreshold = it->as_float();
      else if (name == "thresholdDecayRate")
        _thresholdDecayRate = it->as_float();
      else if (name == "bigBucketSize")
        _bigBucketSize = it->as_float();
      else if (name == "exposure") {
        // exposure is actually stored in the lumiverse config
        //CachingArnoldInterface* renderer = dynamic_cast<CachingArnoldInterface*>(getAnimationPatch()->getArnoldInterface());
        //if (renderer)
        //  renderer->setExposure(it->as_float());
      }
      else if (name == "exportTraces")
        _exportTraces = it->as_bool();
      else if (name == "grayscaleMode")
        _grayscaleMode = it->as_bool();
      else if (name == "autoRunTraceGraph")
        _autoRunTraceGraph = it->as_bool();
      else if (name == "useFGMask")
        _useFGMask = it->as_bool();
      else if (name == "reduceRepeatEdits")
        _reduceRepeatEdits = it->as_bool();
      else if (name == "uniformEditWeights")
        _uniformEditWeights = it->as_bool();
      else if (name == "randomInit")
        _randomInit = it->as_bool();
      else if (name == "continuousSort")
        _continuousSort = it->as_bool();
      else if (name == "useSearchStyles")
        _useSearchStyles = it->as_bool();
      else if (name == "recalculateWeights")
        _recalculateWeights = it->as_bool();
      else if (name == "autoCluster")
        _autoCluster = it->as_bool();
      else if (name == "unconstrained")
        _unconstrained = it->as_bool();
      else if (name == "pxIntensDist")
        _pxIntensDist = it->as_bool();
      else if (name == "noPinWiggle")
        _noPinWiggle = it->as_bool();
      else if (name == "primaryClusterMethod")
        _primaryClusterMethod = (ClusterMethod)it->as_int();
      else if (name == "primaryClusterMetric")
        _primaryClusterMetric = (DistanceMetric)it->as_int();
      else if (name == "primaryFocusArea")
        _primaryFocusArea = (FocusArea)it->as_int();
      else if (name == "secondaryClusterMethod")
        _secondaryClusterMethod = (ClusterMethod)it->as_int();
      else if (name == "secondaryClusterMetric")
        _secondaryClusterMetric = (DistanceMetric)it->as_int();
      else if (name == "secondaryFocusArea")
        _secondaryFocusArea = (FocusArea)it->as_int();
      else if (name == "clusterDisplay")
        _clusterDisplay = (ClusterDisplayMode)it->as_int();
      else if (name == "searchMode")
        _searchMode = (SearchMode)it->as_int();
      else if (name == "editSelectMode")
        _editSelectMode = (EditSelectMode)it->as_int();
      else if (name == "searchDistMetric")
        _searchDistMetric = (DistanceMetric)it->as_int();
      else if (name == "searchDispMetric")
        _searchDispMetric = (DistanceMetric)it->as_int();
      else if (name == "logDirectory")
        _logRootDir = string(it->as_string());
      else if (name == "imageDirectory")
        _imageAttrLoc = File(it->as_string());
    }

    delete memblock;
  }
}

Rig* getRig() {
  if (_rig == nullptr) {
    _rig = new Rig();
  }

  return _rig;
}

DocumentWindow* getAppTopLevelWindow()
{
  for (int i = TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
    if (DocumentWindow* win = dynamic_cast<DocumentWindow*> (TopLevelWindow::getTopLevelWindow(i))) {
      return win;
    }

  return nullptr;
}

DocumentWindow* getAppMainContentWindow()
{
  for (int i = TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
    if (DocumentWindow* win = dynamic_cast<DocumentWindow*> (TopLevelWindow::getTopLevelWindow(i))) {
      if (dynamic_cast<MainContentComponent*>(win->getContentComponent()) != nullptr)
        return win;
    }

  return nullptr;
}

ArnoldAnimationPatch * getAnimationPatch()
{
  // Find the patch, we search for the first ArnoldAnimationPatch we can find.
  Rig* rig = getRig();

  ArnoldAnimationPatch* p = nullptr;
  for (const auto& kvp : rig->getPatches()) {
    if (kvp.second->getType() == "ArnoldAnimationPatch") {
      p = (ArnoldAnimationPatch*)kvp.second;
    }
  }

  return p;
}

StatusBar* getStatusBar() {
  if (_status == nullptr) {
    _status = new StatusBar();
  }

  return _status;
}

GlobalSettings * getGlobalSettings()
{
  if (_globalSettings == nullptr) {
    _globalSettings = new GlobalSettings();

    // check for existence of settings file and load if available
    _globalSettings->loadSettings();
  }

  return _globalSettings;
}

Recorder* getRecorder() {
  if (_recorder == nullptr)
    _recorder = new  Recorder();

  return _recorder;
}

GlobalSettings::GlobalSettings()
{
  _thumbnailRenderSamples = -1;
  _stageRenderSamples = 1;
  _searchDerivDelta = 1e-4;
  _renderWidth = 0;
  _renderHeight = 0;
  _thumbnailRenderScale = 0.25;
  _startChainLength = 30;
  _clusterDistThreshold = 0.30;
  _editStepSize = 0.25;
  _maxMCMCIters = 10;
  _numDisplayClusters = 1;
  _jndThreshold = 20;
  _viewJndThreshold = 0.045;
  _currentSortMode = "Attribute Default";
  _clusterElemsPerRow = 6;
  _maxReturnedScenes = 100;
  _showThumbnailImg = false;
  _T = 1;
  _exportTraces = false;
  _logRootDir = ".";
  _clusterCounter = 0;
  _meanShiftEps = 1e-4;
  _meanShiftBandwidth = 7;
  _grayscaleMode = false;
  _searchFailureLimit = 3;
  _searchThreads = thread::hardware_concurrency() / 2;
  _autoRunTraceGraph = false;
  _standardMCMCIters = (int) 5e3;
  _numPrimaryClusters = 6;
  _primaryClusterMethod = DIVISIVE; // "K-Means";
  _primaryClusterMetric = DIRPPAVGLAB; // "Per-Pixel Average Lab Difference";
  _primaryFocusArea = ALL_IMAGE; // "All";
  _numSecondaryClusters = 4;
  _secondaryClusterMethod = DIVISIVE; // "K-Means";
  _secondaryClusterMetric = DIRPPAVGLAB; // "Per-Pixel Average Lab Difference";
  _secondaryFocusArea = ALL_IMAGE; // "All";
  _spectralBandwidth = 2;
  _useFGMask = false;
  _primaryDivisiveThreshold = 7.5;
  _secondaryDivisiveThreshold = 5;
	_clusterDisplay = COLUMNS;
	_searchMode = GIBBS_SAMPLING;
	_maxGradIters = 200;
  _reduceRepeatEdits = true;
  _autoTimeout = 30;
  _evWeight = 0;
  _uniformEditWeights = false;
  _randomInit = false;
  _resampleTime = 30;
  _resampleThreads = thread::hardware_concurrency();
  _maskTolerance = 5;
  _editSelectMode = SIMPLE_BANDIT;
  _continuousSort = false;
  _useSearchStyles = false;
  _searchFrontierSize = 4;
  _repulsionConeK = 0.5;
  _repulsionCostK = 0.5;
  _numPairs = 100;
  _searchDistMetric = L2PARAM;
  _searchDispMetric = PPAVGLAB;
  _thresholdDecayRate = 0.1;
  _activeIdea = nullptr;
  _bigBucketSize = 0.75;
  _recalculateWeights = false;
  _autoCluster = true;
  _unconstrained = false;
  _pxIntensDist = false;
  _noPinWiggle = false;

  _imageAttrLoc = File::getCurrentWorkingDirectory().getChildFile("image_attributes");
  _tempDir = File::getCurrentWorkingDirectory().getChildFile("tmp");
  _paletteAppDir = File::getCurrentWorkingDirectory().getChildFile("PaletteExtraction");

  if (_searchThreads <= 0)
    _searchThreads = 1;
}

GlobalSettings::~GlobalSettings()
{
  for (auto& e : _edits) {
    delete e;
  }
}

Eigen::Vector3d rgbToLab(double r, double g, double b)
{
  Eigen::Vector3d xyz = ColorUtils::convRGBtoXYZ(r, g, b, sRGB);
  return ColorUtils::convXYZtoLab(xyz, refWhites[D65] / 100.0);
}

double avgLabMaskedImgDiff(Image & a, Image & b, Image & mask)
{
  // dimensions must match
  if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight())
    return -1;

  double sum = 0;
  int count = 0;

  for (int y = 0; y < a.getWidth(); y++) {
    for (int x = 0; x < a.getWidth(); x++) {
      // check if in masked set
      if (mask.getPixelAt(x, y).getRed() > 0) {
        count++;

        auto px = a.getPixelAt(x, y);
        Eigen::Vector3d Lab1 = rgbToLab(px.getRed() / 255.0, px.getGreen() / 255.0, px.getBlue() / 255.0);

        auto px2 = b.getPixelAt(x, y);
        Eigen::Vector3d Lab2 = rgbToLab(px2.getRed() / 255.0, px2.getGreen() / 255.0, px2.getBlue() / 255.0);

        sum += (Lab1 - Lab2).norm();
      }
    }
  }

  return sum / count;
}

Image renderImage(Snapshot * s, int width, int height)
{
  auto p = getAnimationPatch();

  if (p == nullptr) {
    return Image(Image::ARGB, width, height, true);
  }

  // with caching we can render at full and then scale down
  Image img = Image(Image::ARGB, width, height, true);
  uint8* bufptr = Image::BitmapData(img, Image::BitmapData::readWrite).getPixelPointer(0, 0);
  p->setDims(width, height);
  p->setSamples(getGlobalSettings()->_thumbnailRenderSamples);

  getAnimationPatch()->renderSingleFrameToBuffer(s->getDevices(), bufptr, width, height);

  return img;
}

set<string> getUnlockedSystems()
{
  map<string, bool> systemIsUnlocked;
  DeviceSet all = getRig()->getAllDevices();

  for (auto d : all.getDevices()) {
    if (!d->metadataExists("system"))
      continue;

    string sys = d->getMetadata("system");
    bool locked = isDeviceParamLocked(d->getId(), "intensity");

    if (systemIsUnlocked.count(sys) == 0) {
      systemIsUnlocked[sys] = false;
    }

    systemIsUnlocked[sys] |= !locked;
  }

  set<string> sets;
  for (auto s : systemIsUnlocked) {
    if (s.second) {
      sets.insert(s.first);
    }
  }

  return sets;
}

void computeLightSensitivity()
{
  int imgWidth = 100;
  int imgHeight = 100;

  getGlobalSettings()->_sensitivity.clear();

  // process: set each light individually to 50% intensity
  // calculate average per-pixel difference in image brightness
  auto devices = getRig()->getAllDevices();
  Snapshot* tmp = new Snapshot(getRig());
  auto tmpData = tmp->getRigData();

  for (auto active : devices.getIds()) {
    // process moving lights separately
    if (tmpData[active]->getFocusPaletteNames().size() > 0)
      continue;

    // render image at 50% with just that device
    for (auto id : devices.getIds()) {
      if (id == active) {
        tmpData[id]->getIntensity()->setValAsPercent(0.5);

        if (tmpData[id]->paramExists("color")) {
          tmpData[id]->setColorRGBRaw("color", 1, 1, 1);
        }
      }
      else {
        tmpData[id]->setIntensity(0);
      }
    }

    sensCache cache;

    // render
    Image base = renderImage(tmp, imgWidth, imgHeight);
    cache.i50 = base;

    // adjust to 51%
    tmpData[active]->getIntensity()->setValAsPercent(0.51f);

    // render
    Image brighter = renderImage(tmp, imgWidth, imgHeight);
    cache.i51 = brighter;

    // render a 100% image
    tmpData[active]->getIntensity()->setValAsPercent(1.0f);
    Image brightest = renderImage(tmp, imgWidth, imgHeight);
    cache.i100 = brightest;
    cache.maxBr = 0;
    Histogram1D bhist(100);

    // calculate avg per-pixel brightness difference
    double diff = 0;
    int ct = 0;
    float sum = 0;

    for (int y = 0; y < base.getHeight(); y++) {
      for (int x = 0; x < base.getWidth(); x++) {
        diff += brighter.getPixelAt(x, y).getBrightness() - base.getPixelAt(x, y).getBrightness();

        float brightpx = brightest.getPixelAt(x, y).getBrightness();

        if (brightpx > 0.02) {
          ct++;
          sum += brightpx;
          bhist.addValToBin(brightpx);
        }
        if (brightpx > cache.maxBr) {
          cache.maxBr = brightpx;
        }
      }
    }

    // average of all non-zero pixels
    cache.avgVal = sum / ct;
    cache.numAboveAvg = 0;
    cache.numMaxBr = 0;
    cache.data["85pct"] = bhist.percentile(85);
    cache.data["85pct_ct"] = 0;
    cache.data["95pct"] = bhist.percentile(95);
    cache.data["95pct_ct"] = 0;

    for (int y = 0; y < base.getHeight(); y++) {
      for (int x = 0; x < base.getWidth(); x++) {
        if (brightest.getPixelAt(x, y).getBrightness() > cache.avgVal)
          cache.numAboveAvg++;

        if (brightest.getPixelAt(x, y).getBrightness() > cache.maxBr * 0.9) {
          cache.numMaxBr++;
        }

        if (brightest.getPixelAt(x, y).getBrightness() > cache.data["85pct"]) {
          cache.data["85pct_ct"] += 1;
        }
        if (brightest.getPixelAt(x, y).getBrightness() > cache.data["95pct"]) {
          cache.data["95pct_ct"] += 1;
        }
      }
    }

    getGlobalSettings()->_sensitivityCache[active] = cache;
    //getGlobalSettings()->_sensitivity[active] = diff / (base.getHeight() * base.getWidth()) * 100;
  }

  // moving lights
  for (auto active : devices.getIds()) {
    // we already processed static lights
    if (tmpData[active]->getFocusPaletteNames().size() == 0)
      continue;

    // render image at 50% with just that device
    for (auto id : devices.getIds()) {
      if (id == active) {
        tmpData[id]->getIntensity()->setValAsPercent(0.5);

        if (tmpData[id]->paramExists("color")) {
          tmpData[id]->setColorRGBRaw("color", 1, 1, 1);
        }
      }
      else {
        tmpData[id]->setIntensity(0);
      }
    }

    auto angles = tmpData[active]->getFocusPaletteNames();
    for (auto& a : angles) {
      // set the focus palette
      tmpData[active]->setFocusPalette(a);
      tmpData[active]->getIntensity()->setValAsPercent(0.5);

      // proceed as normal
      sensCache cache;

      // render
      Image base = renderImage(tmp, imgWidth, imgHeight);
      cache.i50 = base;

      // adjust to 51%
      tmpData[active]->getIntensity()->setValAsPercent(0.51f);

      // render
      Image brighter = renderImage(tmp, imgWidth, imgHeight);
      cache.i51 = brighter;

      // render a 100% image
      tmpData[active]->getIntensity()->setValAsPercent(1.0f);
      Image brightest = renderImage(tmp, imgWidth, imgHeight);
      cache.i100 = brightest;
      cache.maxBr = 0;
      Histogram1D bhist(100);

      // calculate avg per-pixel brightness difference
      double diff = 0;
      int ct = 0;
      float sum = 0;

      for (int y = 0; y < base.getHeight(); y++) {
        for (int x = 0; x < base.getWidth(); x++) {
          diff += brighter.getPixelAt(x, y).getBrightness() - base.getPixelAt(x, y).getBrightness();

          float brightpx = brightest.getPixelAt(x, y).getBrightness();

          if (brightpx > 0.02) {
            ct++;
            sum += brightpx;
            bhist.addValToBin(brightpx);
          }
          if (brightpx > cache.maxBr) {
            cache.maxBr = brightpx;
          }
        }
      }

      // average of all non-zero pixels
      cache.avgVal = sum / ct;
      cache.numAboveAvg = 0;
      cache.numMaxBr = 0;
      cache.data["85pct"] = bhist.percentile(85);
      cache.data["85pct_ct"] = 0;
      cache.data["95pct"] = bhist.percentile(95);
      cache.data["95pct_ct"] = 0;

      for (int y = 0; y < base.getHeight(); y++) {
        for (int x = 0; x < base.getWidth(); x++) {
          if (brightest.getPixelAt(x, y).getBrightness() > cache.avgVal)
            cache.numAboveAvg++;

          if (brightest.getPixelAt(x, y).getBrightness() > cache.maxBr * 0.9) {
            cache.numMaxBr++;
          }

          if (brightest.getPixelAt(x, y).getBrightness() > cache.data["85pct"]) {
            cache.data["85pct_ct"] += 1;
          }
          if (brightest.getPixelAt(x, y).getBrightness() > cache.data["95pct"]) {
            cache.data["95pct_ct"] += 1;
          }
        }
      }

      getGlobalSettings()->_mlSensCache[active][a] = cache;
      //getGlobalSettings()->_sensitivity[active] = diff / (base.getHeight() * base.getWidth()) * 100;
    }
  }

  // systems
  // basically the same except we adjust multiple affected devices
  auto systems = getRig()->getMetadataValues("system");
  for (auto system : systems) {
    DeviceSet active = getRig()->select("$system=" + system);

    // render image at 50% with selected devices
    for (auto id : devices.getIds()) {
      if (active.contains(id)) {
        tmpData[id]->getIntensity()->setValAsPercent(0.5);

        if (tmpData[id]->paramExists("color")) {
          tmpData[id]->setColorRGBRaw("color", 1, 1, 1);
        }
      }
      else {
        tmpData[id]->setIntensity(0);
      }
    }

    // render
    Image base = renderImage(tmp, imgWidth, imgHeight);

    // adjust to 51%
    for (auto id : active.getIds()) {
      tmpData[id]->getIntensity()->setValAsPercent(0.51f);
    }

    // render
    Image brighter = renderImage(tmp, imgWidth, imgHeight);

    // calculate avg per-pixel brightness difference
    double diff = 0;
    for (int y = 0; y < base.getHeight(); y++) {
      for (int x = 0; x < base.getWidth(); x++) {
        diff += brighter.getPixelAt(x, y).getBrightness() - base.getPixelAt(x, y).getBrightness();
      }
    }

    getGlobalSettings()->_systemSensitivity[system] = diff / (base.getHeight() * base.getWidth()) * 100;
  }

  delete tmp;
}

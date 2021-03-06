/*
  ==============================================================================

    GibbsSchedule.cpp
    Created: 2 Nov 2016 10:56:01am
    Author:  falindrith

  ==============================================================================
*/

#include "GibbsSchedule.h"
#include "HistogramAttribute.h"
#include "closure_over_uneven_buckets.h"
#include "gibbs_with_gaussian_mixture.h"

Sampler::Sampler(DeviceSet affectedDevices, juce::Rectangle<float> region, set<string> intensPins, set<string> colorPins) :
  _devices(affectedDevices), _region(region), _intensPins(intensPins), _colorPins(colorPins)
{
}

juce::Rectangle<float> Sampler::getRegion()
{
  return _region;
}

void Sampler::computeSystemSensitivity()
{
  int imgWidth = 100;
  int imgHeight = 100;
  juce::Rectangle<int> cropRegion = juce::Rectangle<int>((int)(_region.getX() * imgWidth), (int)(_region.getY() * imgHeight),
    (int)(_region.getWidth() * imgWidth), (int)(_region.getHeight() * imgHeight));

  _systemSensitivity.clear();
  DeviceSet allDevices = getRig()->getAllDevices();
  Snapshot* tmp = new Snapshot(getRig());
  auto tmpData = tmp->getRigData();

  // systems
  auto systems = getRig()->getMetadataValues("system");
  for (auto system : systems) {
    DeviceSet fullSystem = getRig()->select("$system=" + system);
    DeviceSet active(getRig());

    // gather devices
    for (auto id : fullSystem.getIds()) {
      if (_devices.contains(id)) {
        active = active.add(id);
      }
    }

    // render image at 50% with selected devices
    for (auto id : allDevices.getIds()) {
      if (active.contains(id) && _intensPins.count(id) == 0) {
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
    Image base = renderImage(tmp, imgWidth, imgHeight).getClippedImage(cropRegion);

    // adjust to 51%
    for (auto id : active.getIds()) {
      tmpData[id]->getIntensity()->setValAsPercent(0.51f);
    }

    // render
    Image brighter = renderImage(tmp, imgWidth, imgHeight).getClippedImage(cropRegion);

    // calculate avg per-pixel brightness difference
    double diff = 0;
    for (int y = 0; y < cropRegion.getHeight(); y++) {
      for (int x = 0; x < cropRegion.getWidth(); x++) {
        diff += brighter.getPixelAt(x, y).getBrightness() - base.getPixelAt(x, y).getBrightness();
      }
    }

    _systemSensitivity[system] = (diff / (base.getHeight() * base.getWidth())) * 100;
  }
}

string Sampler::getAffectedDevices()
{
  string devices = "";

  bool first = true;
  for (auto id : _devices.getIds()) {
    if (first) {
      devices += id;
      first = false;
    }
    else {
      devices += ", " + id;
    }
  }

  return devices;
}

// =============================================================================

ColorSampler::ColorSampler(DeviceSet affectedDevices, juce::Rectangle<float> region,
  set<string> intensPins, set<string> colorPins,
  vector<Eigen::Vector3d> colors, vector<float> weights) :
  Sampler(affectedDevices, region, intensPins, colorPins), _colors(colors), _weights(weights),
  _srcColor(3, { 0, 0.05f, 0, 0.2f, 0, 0.2f })
{
  normalizeWeights();
}

ColorSampler::~ColorSampler()
{
}

void ColorSampler::sample(Snapshot * state)
{
  if (getGlobalSettings()->_unconstrained) {
    // the color sampler will sample by individual lights (lol)
    vector<float> results;
    vector<float> bins;
    bins.resize(_colors.size());
    vector<string> ids;
    auto stateData = state->getRigData();

    // ok so here we'll want to do some pre-filling based on what's pinned
    // process goes like this
    // - find light's closest color if pinned, add intensity (%) to bin
    // - If not pinned, add light's intensity to system total
    int i = 0;
    for (auto id : stateData) {
      if (_devices.contains(id.first)) {
        if (!stateData[id.first]->paramExists("color"))
          continue;

        if (_colorPins.count(id.first) == 0) {
          // unconstrained
        }
        else {
          // pinned, find closest color
          int closestColorIndex = getClosestColorIndex(stateData[id.first]->getColor()->getHSV());

          // add to color bin, note that this is not actually part of the
          // local system set because it's pinned
          bins[closestColorIndex] += stateData[id.first]->getIntensity()->asPercent();
        }
      }

      // ensure proper size for results, even if the value is 0
      results.push_back(stateData[id.first]->getIntensity()->asPercent());
      ids.push_back(id.first);
      i++;
    }

    vector<int> colorIds;
    colorIds.resize(results.size());

    ClosureOverUnevenBuckets(results, _weights, colorIds, bins);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0, 1);
    float rnd = dist(gen);

    // fill in the results if not pinned
    for (int j = 0; j < results.size(); j++) {
      Eigen::Vector3d color = _colors[colorIds[j]];

      // perterb the color a little bit 
      normal_distribution<double> hueDist(0, 0.02);
      normal_distribution<double> satDist(0, 0.1);

      color[0] += hueDist(gen);
      color[1] += satDist(gen);
      color[2] += satDist(gen);

      // apply color to each light in the system
      string id = ids[j];
      if (_colorPins.count(id) == 0 && stateData[id]->paramExists("color")) {
        stateData[id]->getColor()->setHSV(color[0] * 360.0, color[1], color[2]);
      }
    }
  }
  else {
    // the color sampler will sample by system
    vector<float> results;
    vector<float> bins;
    bins.resize(_colors.size());
    vector<DeviceSet> systemMap;
    auto stateData = state->getRigData();

    // ok so here we'll want to do some pre-filling based on what's pinned
    // process goes like this
    // - find light's closest color if pinned, add intensity (%) to bin
    // - If not pinned, add light's intensity to system total

    auto systems = getRig()->getMetadataValues("system");
    int i = 0;
    for (auto s : systems) {
      DeviceSet localSys(getRig());
      DeviceSet globalSys = getRig()->select("$system=" + s);

      float totalIntens = 0;
      for (auto id : globalSys.getIds()) {
        if (_devices.contains(id)) {
          if (!stateData[id]->paramExists("color"))
            continue;

          if (_colorPins.count(id) == 0) {
            // unconstrained
            localSys = localSys.add(id);
            totalIntens += stateData[id]->getIntensity()->asPercent();
          }
          else {
            // pinned, find closest color
            int closestColorIndex = getClosestColorIndex(stateData[id]->getColor()->getHSV());

            // add to color bin, note that this is not actually part of the
            // local system set because it's pinned
            bins[closestColorIndex] += stateData[id]->getIntensity()->asPercent();
          }
        }
      }

      // ensure proper size for results, even if the value is 0
      results.push_back(totalIntens);
      systemMap.push_back(localSys);
      i++;
    }

    vector<int> colorIds;
    colorIds.resize(results.size());

    // here we do a bit of re-weighting of the color buckets.
    vector<float> sampleWeights;
    sampleWeights.resize(_weights.size());
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0, 1);
    float rnd = dist(gen);
    int idx = 0;

    // pick a color to be the "big bucket"
    for (; idx < _weights.size(); idx++) {
      if (rnd < _weights[idx])
        break;

      rnd -= _weights[idx];
    }

    // reassign weights. the bucket indicated by i gets n%, everything else
    // proportional to the original weight
    float bigWeight = getGlobalSettings()->_bigBucketSize;

    if (bigWeight < _weights[idx])
      bigWeight = _weights[idx];

    sampleWeights[idx] = bigWeight;
    float rem = 1 - _weights[idx];

    float smallWeight = 1 - bigWeight;
    for (int j = 0; j < _weights.size(); j++) {
      if (idx == j)
        continue;

      sampleWeights[j] = (_weights[j] / rem) * smallWeight;
    }

    // do the sampling
    if (getGlobalSettings()->_recalculateWeights) {
      ClosureOverUnevenBuckets(results, sampleWeights, colorIds, bins);
    }
    else {
      ClosureOverUnevenBuckets(results, _weights, colorIds, bins);
    }

    // fill in the results if not pinned
    for (int j = 0; j < results.size(); j++) {
      Eigen::Vector3d color = _colors[colorIds[j]];

      // perterb the color a little bit 
      normal_distribution<double> hueDist(0, 0.02);
      normal_distribution<double> satDist(0, 0.1);

      color[0] += hueDist(gen);
      color[1] += satDist(gen);
      color[2] += satDist(gen);

      // apply color to each light in the system
      for (string id : systemMap[j].getIds()) {
        if (_colorPins.count(id) == 0 && stateData[id]->paramExists("color")) {
          stateData[id]->getColor()->setHSV(color[0] * 360.0, color[1], color[2]);
        }
      }
    }

    // adjust the saturation constrained devices
    for (int i = 0; i < _constraints._satTargets.size(); i++) {
      DeviceSet s = _constraints._satTargets[i];

      for (auto id : s.getIds()) {
        if (_devices.contains(id)) {
          // clamp saturation
          auto hsv = stateData[id]->getColor()->getHSV();

          float sat = Lumiverse::clamp(hsv[1], _constraints._satMin[i], _constraints._satMax[i]);
          stateData[id]->setColorHSV("color", hsv[0], sat, hsv[2]);
        }
      }
    }
  }
}

double ColorSampler::score(Snapshot * /* state */, Image & img, bool masked)
{
  // compute histogram from scaled region
  juce::Rectangle<int> region = juce::Rectangle<int>((int)(_region.getX() * img.getWidth()), (int)(_region.getY() * img.getHeight()),
    (int)(_region.getWidth() * img.getWidth()), (int)(_region.getHeight() * img.getHeight()));
  Image clipped = img.getClippedImage(region);
  Image clippedMask = getGlobalSettings()->_fgMask.rescaled(img.getWidth(), img.getHeight()).getClippedImage(region);

  // scale, 200 max dim
  float scale = (clipped.getWidth() > clipped.getHeight()) ? 200.0f / clipped.getWidth() : 200.0f / clipped.getHeight();
  clipped = clipped.rescaled((int)(scale * clipped.getWidth()), (int)(scale * clipped.getHeight()));
  clippedMask = clippedMask.rescaled(clipped.getWidth(), clipped.getHeight());

  // do the thing
  SparseHistogram stage(3, _srcColor.getBounds());

  for (int y = 0; y < clipped.getHeight(); y++) {
    for (int x = 0; x < clipped.getWidth(); x++) {
      if (masked && getGlobalSettings()->_useFGMask) {
        if (clippedMask.getPixelAt(x, y).getBrightness() > 0) {
          Colour px = clipped.getPixelAt(x, y);
          vector<float> pts;
          pts.resize(3);
          pts[0] = px.getFloatRed();
          pts[1] = px.getFloatGreen();
          pts[2] = px.getFloatBlue();
          stage.add(pts);
        }
      }
      else {
        // add everything
        Colour px = clipped.getPixelAt(x, y);
        vector<float> pts;
        pts.resize(3);
        pts[0] = px.getFloatRed();
        pts[1] = px.getFloatGreen();
        pts[2] = px.getFloatBlue();
        stage.add(pts);
      }
    }
  }

  if (stage.getTotalWeight() == 0)
    return 0;

  // compute the dist
  return _srcColor.EMD(stage);
}

void ColorSampler::setColorHistogram(SparseHistogram c)
{
  _srcColor = c;
}

string ColorSampler::info()
{
  String colors = "[";
  String weights = "[";
  for (int i = 0; i < _colors.size(); i++) {
    auto c = _colors[i];
    colors << i << String(": (") << c[0] << String(",") << c[1] << String(",") << c[2] << String(")");
    weights << i << ": " << _weights[i];
    
    if (i != _colors.size() - 1) {
      weights << String(" ");
      colors << String(" ");
    }
  }
  colors << "]";
  weights << "]";

  String info;
  info << "Colors: " << colors << "\nWeights: " << weights << "\nDevices: " << getAffectedDevices();
  return info.toStdString();
}

void ColorSampler::normalizeWeights()
{
  // compute sum
  double sum = 0;
  for (auto w : _weights) {
    sum += w;
  }

  // divide
  for (int i = 0; i < _weights.size(); i++) {
    _weights[i] /= (float)sum;
  }
}

int ColorSampler::getClosestColorIndex(Eigen::Vector3d color)
{
  int closest = 0;
  double closestDist = DBL_MAX;
  
  // normalization factor. Lumiverse returns hue in [0-360]
  color[0] /= 360.0;

  for (int i = 0; i < _colors.size(); i++) {
    double dist = (color - _colors[i]).norm();
    if (dist < closestDist) {
      closest = 0;
      closestDist = dist;
    }
  }
  
  return closest;
}

// =============================================================================
PinSampler::PinSampler(DeviceSet affectedDevice, juce::Rectangle<float> region, set<string> intensPins, set <string> colorPins) :
  Sampler(affectedDevice, region, intensPins, colorPins)
{

}

PinSampler::~PinSampler()
{
}

void PinSampler::sample(Snapshot * state)
{
  // if the setting disabling this sampler has been set, we just return without doing anything
  if (getGlobalSettings()->_noPinWiggle)
    return;

  // the pin sampler takes the pinned values and 'wiggles' them a bit, showing small
  // variations of the current state of the device parameter
  std::random_device rd;
  std::mt19937 gen(rd());
  normal_distribution<double> hueDist(0, 2);
  normal_distribution<double> satDist(0, 0.05);
  normal_distribution<double> intensDist(0, 0.05);

  // do this on a system-level, not device-level
  auto systems = getRig()->getMetadataValues("system");
  vector<DeviceSet> localIntens, localColor;

  for (auto system : systems) {
    DeviceSet globalSys = getRig()->select("$system=" + system);
    DeviceSet localI(getRig());
    DeviceSet localC(getRig());

    for (auto id : globalSys.getIds()) {
      if (_intensPins.count(id) > 0) {
        localI = localI.add(id);
      }
      if (_colorPins.count(id) > 0) {
        localC = localC.add(id);
      }
    }

    localIntens.push_back(localI);
    localColor.push_back(localC);
  }

  auto stateData = state->getRigData();

  // intensity
  for (auto ds : localIntens) {
    double delta = intensDist(gen);

    for (auto id : ds.getIds()) {
      if (stateData[id]->paramExists("intensity")) {
        double newIntens = stateData[id]->getIntensity()->asPercent() + delta;
        stateData[id]->getIntensity()->setValAsPercent((float)newIntens);
      }
    }
  }

  // color
  for (auto ds : localColor) {
    double hueDelta = hueDist(gen);
    double satDelta = satDist(gen);

    for (auto id : ds.getIds()) {
      if (stateData[id]->paramExists("color")) {
        Eigen::Vector3d hsv = stateData[id]->getColor()->getHSV();
        hsv[0] += hueDelta;
        hsv[1] += satDelta;

        stateData[id]->getColor()->setHSV(hsv[0], hsv[1], hsv[2]);
      }
    }
  }
}

string PinSampler::info()
{
  return "Devices: " + getAffectedDevices();
}

// =============================================================================

IntensitySampler::IntensitySampler(DeviceSet affectedDevices, juce::Rectangle<float> region,
  set<string> intensPins, set<string> colorPins, int k, float bm, float m) :
  Sampler(affectedDevices, region, intensPins, colorPins), _k(k), _brightMean(bm),
  _mean(m), _srcBrightness(1, { 0, 0.1f }), _preProcessComplete(false)
{
  computeSystemSensitivity();
  std::random_device rd;
  _rng = mt19937(rd());
}

IntensitySampler::~IntensitySampler()
{
}

void IntensitySampler::sample(Snapshot * state)
{
  if (getGlobalSettings()->_pxIntensDist) {
    // the color sampler will sample by system
    vector<float> results;
    vector<int> constraint;
    vector<float> sens;
    vector<DeviceSet> systemMap;
    auto stateData = state->getRigData();

    auto systems = getRig()->getMetadataValues("system");

    for (auto system : systems) {
      DeviceSet globalSys = getRig()->select("$system=" + system);
      DeviceSet localSys(getRig());
      float avgIntens = 0;
      int ct = 0;

      bool noDevicesInLocal = true;

      for (auto id : globalSys.getIds()) {
        if (_devices.contains(id)) {
          avgIntens += stateData[id]->getIntensity()->asPercent();
          ct++;
          noDevicesInLocal = false;

          if (_intensPins.count(id) == 0) {
            localSys = localSys.add(id);
          }
          // some other effect if pinned
        }
      }

      // skip if there's nothing actually in the local set
      // from this system
      if (noDevicesInLocal) {
        continue;
      }

      // calculate avg intens
      results.push_back(avgIntens / ct);

      sens.push_back((float)(_systemSensitivity[system]));
      systemMap.push_back(localSys);

      if (localSys.size() > 0) {
        constraint.push_back(0);
      }
      else {
        // pinned
        constraint.push_back(3);
      }
    }

    // sample
    std::random_device rd;
    std::mt19937 gen(rd());
    uniform_int_distribution<int> wr(0, _concept.getWidth());
    uniform_int_distribution<int> hr(0, _concept.getHeight());

    // this sampling method selects a random pixel, then assigns the system to that pixel value
    for (int i = 0; i < constraint.size(); i++) {
      if (constraint[i] == 3)
        continue;

      float br = _concept.getPixelAt(wr(gen), hr(gen)).getBrightness();

      // apply color to each light in the system
      for (string id : systemMap[i].getIds()) {
        if (_intensPins.count(id) == 0 && stateData[id]->paramExists("intensity")) {
          stateData[id]->getIntensity()->setValAsPercent(br);
        }
      }
    }
  }
  else if (getGlobalSettings()->_unconstrained) {
    vector<float> results;
    vector<int> constraint;
    vector<float> sens;
    vector<string> ids;
    auto stateData = state->getRigData();

    for (auto id : stateData) {
      if (_devices.contains(id.first)) {
        sens.push_back(getGlobalSettings()->_sensitivity[id.first]);
        ids.push_back(id.first);
        results.push_back(id.second->getIntensity()->asPercent());

        if (_intensPins.count(id.first) == 0) {
          constraint.push_back(0);
        }
        else {
          constraint.push_back(3);
        }
      }
    }

    // sample
    GibbsSamplingGaussianMixturePrior(results, constraint, sens, (int)results.size(), _k, _brightMean, _mean, 0);

    // apply to snapshot
    for (int j = 0; j < results.size(); j++) {
      float intens = results[j];

      // apply color to each light in the system
      string id = ids[j];
      if (_intensPins.count(id) == 0 && stateData[id]->paramExists("intensity")) {
        stateData[id]->getIntensity()->setValAsPercent(intens);
      }
    }
  }
  else {
    // the sampler will sample by system
    auto stateData = state->getRigData();

    // for moving lights, select a focus palette for them to use at random, if unpinned
    for (auto fp : _availableFocusPalettes) {
      if (_focusPins.count(fp.first) == 0) {
        // unpinned
        uniform_int_distribution<int> dist(0, fp.second.size() - 1);
        int idx = dist(_rng);
        stateData[fp.first]->setFocusPalette(fp.second[idx]);
      }
    }

    // and that should actually be it for focus palettes. From this point on in the sampler,
    // the light functions as a normal single position lighting fixture. Since the focus palette
    // also updates the metadata for the fixture in question, the sampler should have no problem
    // properly categorizing and handling that light.

    // results is written to, so we'll need some local memory
    vector<float> results;
    results.resize(_results.size());

    // we've precomputed most things in the constructor, but we need to get the average intensity for each group
    // as well as turn off excluded devices (if any)
    for (int i = 0; i < results.size(); i++) {
      DeviceSet sysDevices = _systemMap[i];
      float avg = 0;

      for (auto id : sysDevices.getIds()) {
        avg += stateData[id]->getIntensity()->asPercent();
      }

      results[i] = avg / sysDevices.size();
    }

    for (auto id : _excludedOff.getIds()) {
      stateData[id]->setIntensity(0);
    }

    // check if key lights are to be used exclusively
    int k = _k;
    if (_constraints._keyLightsAreExclusive) {
      k = _keyLightCount;  // should be equal to the number of 1 constraints in the constraint vector
    }

    // randomize order of results vector? shouldn't have to, unconstrained values get shuffled anyway

    // sample
    uniform_real_distribution<float> rnd(0, 1);
    if (_constraints._relative.size() > 0) {
      GibbsSamplingGaussianMixturePrior(results, _constraint, _sens, (int)results.size(), k, _brightMean, _mean, rnd(_rng), _rel);
    }
    else {
      GibbsSamplingGaussianMixturePrior(results, _constraint, _sens, (int)results.size(), k, _brightMean, _mean, rnd(_rng));
    }

    // apply to snapshot
    for (int j = 0; j < results.size(); j++) {
      if (_constraint[j] == 4)
        continue;

      float intens = results[j];
      // apply color to each light in the system
      for (string id : _systemMap[j].getIds()) {
        if (_intensPins.count(id) == 0 && stateData[id]->paramExists("intensity")) {
          stateData[id]->getIntensity()->setValAsPercent(intens);
        }
      }
    }

    // enforce relative constraints on a per-area basis
    for (auto c : _constraints._relative) {
      auto areas = getRig()->getMetadataValues("area");
      string source = c.first;
      string target = c.second.first;
      float ratio = c.second.second;

      for (auto area : areas) {
        // for each area, find the specified system
        DeviceSet sd = getRig()->select("$area=" + area + "[$system=" + source + "]");
        DeviceSet td = getRig()->select("$area=" + area + "[$system=" + target + "]");

        // if there are multiple devices in this area we basically take the first one we find
        // and assume that everything is the same. If the source set is empty we continue.
        if (sd.size() == 0)
          continue;

        // make sure source devices are actually in the selected devices
        string srcId = "";
        for (auto id : sd.getIds()) {
          if (_devices.contains(id)) {
            srcId = id;
            break;
          }
        }

        if (srcId == "")
          continue;

        // update devices that are in the local set
        float val = stateData[srcId]->getIntensity()->asPercent() * ratio;
        for (auto id : td.getIds()) {
          if (_devices.contains(id))
            stateData[id]->getIntensity()->setValAsPercent(val);
        }
      }
    }
  }
}

double IntensitySampler::score(Snapshot * /*state*/, Image& img, bool masked)
{
  // compute histogram from scaled region
  juce::Rectangle<int> region = juce::Rectangle<int>((int)(_region.getX() * img.getWidth()), (int)(_region.getY() * img.getHeight()),
    (int)(_region.getWidth() * img.getWidth()), (int)(_region.getHeight() * img.getHeight()));
  Image clipped = img.getClippedImage(region);
  Image clippedMask = getGlobalSettings()->_fgMask.rescaled(img.getWidth(), img.getHeight()).getClippedImage(region);

  // scale, 200 max dim
  float scale = (clipped.getWidth() > clipped.getHeight()) ? 200.0f / clipped.getWidth() : 200.0f / clipped.getHeight();
  clipped = clipped.rescaled((int)(scale * clipped.getWidth()), (int)(scale * clipped.getHeight()));
  clippedMask = clippedMask.rescaled(clipped.getWidth(), clipped.getHeight());

  // do the thing
  SparseHistogram stage(1, _srcBrightness.getBounds());

  for (int y = 0; y < clipped.getHeight(); y++) {
    for (int x = 0; x < clipped.getWidth(); x++) {
      if (masked && getGlobalSettings()->_useFGMask) {
        if (clippedMask.getPixelAt(x, y).getBrightness() > 0) {
          Colour px = clipped.getPixelAt(x, y);
          vector<float> pts;
          pts.push_back(px.getBrightness());
          stage.add(pts);
        }
      }
      else {
        // add everything
        Colour px = clipped.getPixelAt(x, y);
        vector<float> pts;
        pts.push_back(px.getBrightness());
        stage.add(pts);
      }
    }
  }

  // mask may not be present in region
  if (stage.getTotalWeight() == 0)
    return 0;

  // compute the dist
  return _srcBrightness.EMD(stage);
}
void IntensitySampler::setBrightnessHistogram(SparseHistogram b)
{
  _srcBrightness = b;
}

void IntensitySampler::setFocusPalettes(map<string, vector<string>> fp, set<string> pp)
{
  _availableFocusPalettes = fp;
  _focusPins = pp;
}

string IntensitySampler::info()
{
  String info;
  info << "Average: " << _mean << ", Bright: " << _brightMean << ", Bright Lights: " << _k << "\n";
  info << "Devices: " << getAffectedDevices();
  return info.toStdString();
}

void IntensitySampler::preProcess()
{
  if (_preProcessComplete)
    return;

  // compute common data. this function creates the constraint vector,
  // results vector, stores data about the systems involved, and handles constraints.
  // Since this data does not change from run to run, we should only
  // need to run this once.
  // note that the results vector gets filled in during every search, but we only need to compute
  // average intensity, and since we already have the groups, it's easy
  set<string> systemSet = getRig()->getMetadataValues("system");
  _keyLightCount = 0;
  _excludedOff = DeviceSet(getRig());

  // determine if there are systems to use for relative constraints
  map<string, string> relTarget;
  for (auto c : _constraints._relative) {
    // reverses the map
    relTarget[c.second.first] = c.first;
  }

  vector<string> systems;
  for (auto s : systemSet) {
    systems.push_back(s);
  }

  // data structure for relative constraints
  // [src index] : ([target index],[factor])
  vector<string> sysNames;

  // we now need to divide the lights up into a few different groups.
  // With constraints, we need the following:
  // - List of unconstrained lights
  // - List of Key Lights
  // - List of pinned lights
  // - List of excluded lights and what to do with them (ignore or turn off)
  // - List of targeted lights for a particular system
  // Relative constraints cause some problems with the systems here and are included in
  // the sampler in an attempt to have them actually part of the sampling process. They will
  // be attached to the first sub-system found and will estimate their value off of that.
  for (auto system : systems) {
    DeviceSet globalSys = getRig()->select("$system=" + system);
    DeviceSet localSys(getRig());

    bool relAdded = false;
    bool noDevicesInLocal = true;

    for (auto id : globalSys.getIds()) {
      if (_devices.contains(id)) {
        noDevicesInLocal = false;

        if (_intensPins.count(id) == 0) {
          localSys = localSys.add(id);
        }
        // some other effect if pinned
      }
    }

    // skip if there's nothing actually in the local set
    // from this system
    if (noDevicesInLocal) {
      continue;
    }

    if (localSys.size() > 0) {
      // if this is a relative constraint target we do something different
      if (relTarget.count(system) > 0) {
        _constraint.push_back(4);
        _results.push_back(0);
        _systemMap.push_back(localSys);
        _sens.push_back(_systemSensitivity[system]);
        sysNames.push_back(system);
      }
      else {
        // now divide the local system into the required groups if any constraints exist
        DeviceSet unconstrained(getRig());
        DeviceSet key(getRig());
        DeviceSet excludedOff(getRig());
        DeviceSet excludedIgnore(getRig());

        for (auto id : localSys.getIds()) {
          if (_constraints._keyLights.contains(id)) {
            key = key.add(id);
          }
          else if (_constraints._excludeIgnore.contains(id)) {
            excludedIgnore = excludedIgnore.add(id);
          }
          else if (_constraints._excludeTurnOff.contains(id)) {
            excludedOff = excludedOff.add(id);
          }
          else {
            unconstrained = unconstrained.add(id);
          }
        }

        // key lights
        if (key.size() > 0) {
          _constraint.push_back(1);
          _results.push_back(0);
          _sens.push_back((float)(_systemSensitivity[system]));
          _systemMap.push_back(key);
          sysNames.push_back(system);
          _keyLightCount++;
        }

        // unconstrained
        if (unconstrained.size() > 0) {
          _constraint.push_back(0);
          _results.push_back(0);
          // this is an estimate, since we're dividing up this system according to constraints now
          _sens.push_back((float)(_systemSensitivity[system]));
          _systemMap.push_back(unconstrained);
          sysNames.push_back(system);
        }

        // excluded off
        if (excludedOff.size() > 0) {
          _excludedOff = _excludedOff.add(excludedOff);
        }

        // excluded on
        if (excludedIgnore.size() > 0) {
          // functionally, these are pinned lights
          _constraint.push_back(3);
          _results.push_back(0);
          _sens.push_back((float)(_systemSensitivity[system]));
          _systemMap.push_back(excludedIgnore);
          sysNames.push_back(system);
        }
      }
    }
    else {
      // pinned
      _constraint.push_back(3);
      _results.push_back(0);
      _sens.push_back((float)(_systemSensitivity[system]));
      _systemMap.push_back(localSys);
      sysNames.push_back(system);
    }
  }

  // link up dependent light references
  for (int i = 0; i < _results.size(); i++) {
    if (_constraint[i] == 4) {
      // find the index of the FIRST entry in results that has the source system name
      int j;
      for (j = 0; j < _results.size(); j++) {
        if (sysNames[j] == relTarget[sysNames[i]])
          break;
      }

      // nothing actually found
      if (j == _results.size())
        continue;

      // src index maps to target index and factor
      _rel[j] = pair<int, float>(i, _constraints._relative[relTarget[sysNames[i]]].second);
    }
  }

  // check if key lights are to be used exclusively
  int k = _k;
  if (_constraints._keyLightsAreExclusive) {
    k = _keyLightCount;  // should be equal to the number of 1 constraints in the constraint vector
  }

  _preProcessComplete = true;
}

MonochromeSampler::MonochromeSampler(DeviceSet affectedDevices, juce::Rectangle<float> region,
  set<string> intensPins, set<string> colorPins, Colour color) :
  Sampler(affectedDevices, region, intensPins, colorPins), _target(color)
{
}

MonochromeSampler::~MonochromeSampler()
{
}

void MonochromeSampler::sample(Snapshot * state)
{
  // this one is pretty easy
  // we just pick a color close to the target, and adjust the satration a bit
  // the color sampler will sample by system
  vector<DeviceSet> systemMap;
  auto stateData = state->getRigData();

  auto systems = getRig()->getMetadataValues("system");
  for (auto system : systems) {
    DeviceSet globalSys = getRig()->select("$system=" + system);
    DeviceSet localSys(getRig());

    for (auto id : globalSys.getIds()) {
      if (_devices.contains(id)) {
        if (_colorPins.count(id) == 0) {
          localSys = localSys.add(id);
        }
      }
    }

    systemMap.push_back(localSys);
  }

  // sample
  std::random_device rd;
  std::mt19937 gen(rd());
  normal_distribution<float> hueDist(0, 0.01f);
  normal_distribution<float> satDist(0, 0.2f);
  float hue = _target.getHue() + hueDist(gen);
  float sat = _target.getSaturation() + satDist(gen);

  // apply to snapshot
  for (int j = 0; j < systemMap.size(); j++) {

    // apply color to each light in the system
    for (string id : systemMap[j].getIds()) {
      if (_colorPins.count(id) == 0 && stateData[id]->paramExists("color")) {
        stateData[id]->getColor()->setHSV(hue * 360, sat, _target.getBrightness());
      }
    }
  }
}

double MonochromeSampler::score(Snapshot * /*state*/, Image & img, bool masked)
{
  // compute histogram from scaled region
  juce::Rectangle<int> region = juce::Rectangle<int>((int)(_region.getX() * img.getWidth()), (int)(_region.getY() * img.getHeight()),
    (int)(_region.getWidth() * img.getWidth()), (int)(_region.getHeight() * img.getHeight()));
  Image clipped = img.getClippedImage(region);
  Image clippedMask = getGlobalSettings()->_fgMask.rescaled(img.getWidth(), img.getHeight()).getClippedImage(region);

  // scale, 200 max dim
  float scale = (clipped.getWidth() > clipped.getHeight()) ? 200.0f / clipped.getWidth() : 200.0f / clipped.getHeight();
  clipped = clipped.rescaled((int)(scale * clipped.getWidth()), (int)(scale * clipped.getHeight()));
  clippedMask = clippedMask.rescaled(clipped.getWidth(), clipped.getHeight());

  Eigen::Vector3f t;
  _target.getHSB(t[0], t[1], t[2]);

  float sum = 0;
  // per-pixel average difference from target
  for (int y = 0; y < clipped.getHeight(); y++) {
    for (int x = 0; x < clipped.getWidth(); x++) {
      if (masked && getGlobalSettings()->_useFGMask) {
        if (clippedMask.getPixelAt(x, y).getBrightness() > 0) {
          Eigen::Vector3f px;
          img.getPixelAt(x, y).getHSB(px[0], px[1], px[2]);

          sum += (t - px).norm();
        }
      }
      else {
        Eigen::Vector3f px;
        img.getPixelAt(x, y).getHSB(px[0], px[1], px[2]);

        sum += (t - px).norm();
      }
    }
  }

  return sum / (clipped.getHeight() * clipped.getWidth());
}

string MonochromeSampler::info()
{
  String info;
  info << "Target Color: " << _target.toString();
  info << "\nDevices: " << getAffectedDevices();
  return info.toStdString();
}

TheatricalSampler::TheatricalSampler(DeviceSet affectedDevices, juce::Rectangle<float> region,
  set<string> intensPins, set<string> colorPins,
  vector<Eigen::Vector3d> colors, vector<float> weights) :
  ColorSampler(affectedDevices, region, intensPins, colorPins, colors, weights),
  front(getRig()) 
{
  DeviceSet other(getRig());

  // separate front devices
  for (auto id : _devices.getIds()) {
    string system = getRig()->getDevice(id)->getMetadata("system");
    if (system == "front" || system == "front left" || system == "front right" || system == "key" || system == "fill") {
      front = front.add(id);
    }
    else {
      other = other.add(id);
    }
  }
  
  // update internal affected devices
  _devices = other;
}

TheatricalSampler::~TheatricalSampler()
{
}

void TheatricalSampler::sample(Snapshot * state)
{
  // this sampler handles the front lights, everything else is handled by the parent class
  auto data = state->getRigData();
  vector<DeviceSet> systemMap;

  vector<string> systems = { "front", "front left", "front right", "key", "fill" };

  for (auto system : systems) {
    DeviceSet globalSys = getRig()->select("$system=" + system);
    DeviceSet localSys(getRig());

    for (auto id : globalSys.getIds()) {
      if (front.contains(id)) {
        if (_colorPins.count(id) == 0) {
          localSys = localSys.add(id);
        }
      }
    }

    if (localSys.size() > 0)
      systemMap.push_back(localSys);
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  uniform_int_distribution<int> ctb(6500, 12000);
  uniform_int_distribution<int> ctw(1900, 6500);
  uniform_int_distribution<int> all(1900, 12000);
  vector<Eigen::Vector3d> colors;

  if (systemMap.size() == 1) {
    colors.push_back(cctToRgb(all(gen)));
  }
  else {
    colors.push_back(cctToRgb(ctb(gen)));
    colors.push_back(cctToRgb(ctw(gen)));

    for (int j = 0; j < ((int)systemMap.size()) - j; j++) {
      colors.push_back(cctToRgb(all(gen)));
    }
  }

  // assign to systems in random order
  shuffle(systemMap.begin(), systemMap.end(), gen);

  // apply to snapshot
  for (int i = 0; i < systemMap.size(); i++) {
    Eigen::Vector3d color = colors[i];

    // apply color to each light in the system
    for (string id : systemMap[i].getIds()) {
      if (_colorPins.count(id) == 0 && data[id]->paramExists("color")) {
        data[id]->getColor()->setRGBRaw(color[0], color[1], color[2]);
      }
    }
  }

  // sample remaining colors
  ColorSampler::sample(state);
}

Eigen::Vector3d TheatricalSampler::cctToRgb(int cct)
{
  // based on http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/ 
  float temp = cct / 100.0f;

  // red
  float red = 0;
  if (temp <= 66) {
    red = 255;
  }
  else {
    red = temp - 60;
    red = 329.698727446f * pow(red, -0.1332047592f);
    red = Lumiverse::clamp(red, 0, 255.f);
  }

  float green;
  if (temp <= 66) {
    green = temp;
    green = 99.4708025861f * log(green) - 161.1195681661f;
  }
  else {
    green = temp - 60;
    green = 288.1221695283f * pow(green, -0.0755148492f);
  }
  green = Lumiverse::clamp(green, 0, 255.f);

  float blue;
  if (temp >= 66) {
    blue = 255;
  }
  else {
    if (temp <= 19)
      blue = 0;
    else {
      blue = temp - 10;
      blue = 138.5177312231f * log(blue) - 305.0447927307f;
    }
  }

  return Eigen::Vector3d(red / 255.0f, green / 255.0f, blue / 255.0f);
}


// =============================================================================

GibbsSchedule::GibbsSchedule()
{
}

GibbsSchedule::~GibbsSchedule()
{
  deleteSamplers();
}

void GibbsSchedule::addSampler(Sampler * s)
{
  // want to insert the sampler after the smallest region
  // that contains it
  // assert: before operation, list is in order (i.e. if a sampler
  // contains another sampler, it will show up before that sampler)
  vector<Sampler*>::iterator insertBefore = _samplers.end();
  for (vector<Sampler*>::iterator it = _samplers.begin(); it != _samplers.end(); ) {
    if ((*it)->getRegion().contains(s->getRegion())) {
      insertBefore = ++it;
    }
    else if (s->getRegion().contains((*it)->getRegion())) {
      insertBefore = it++;

      // as soon as this contains something else, exit to place it there
      break;
    }
    // if it intersects instead, insert after element (latest takes precidence)
    else if (s->getRegion().intersects((*it)->getRegion())) {
      insertBefore = ++it;
    }
    else {
      it++;
    }
  }
  
  _samplers.insert(insertBefore, s);
}

void GibbsSchedule::deleteSamplers()
{
  for (auto s : _samplers)
    delete s;
  _samplers.clear();
}

void GibbsSchedule::score(shared_ptr<SearchResult> result, Snapshot* state)
{
  return;

  //int width = getGlobalSettings()->_renderWidth;
  //int height = getGlobalSettings()->_renderHeight;
  //Image render = renderImage(state, (int)(getGlobalSettings()->_thumbnailRenderScale * width),
  //  (int)(getGlobalSettings()->_thumbnailRenderScale * height));

  //for (auto s : _samplers) {
  //  if (s->_name == "Pinned")
  //    continue;

  //  result->_extraFuncs[s->_name] = s->score(state, render, false);
  //  result->_extraFuncs[s->_name + " - foreground"] = s->score(state, render, true);
  //}
}

Snapshot * GibbsSchedule::sample(Snapshot * state)
{
  // TODO: implement the sampling for real. Right now we simply sample things
  // iteratively. At some point we'll probably want to sample intensity then color
  // so the color sampler can actually do things? it may get complicated
  Snapshot* newState = new Snapshot(*state);

  for (auto s : _samplers) {
    // state carries over to each new sampler
    // may have to check for conflicts before running at some point
    s->sample(newState);
  }

  return newState;
}

void GibbsSchedule::log()
{
  string log;
  for (auto s : _samplers) {
    log += s->_name + "\n" + s->info() + "\n";
  }
  getRecorder()->log(SYSTEM, log);
}

void GibbsSchedule::moveIntensityUp()
{
  vector<Sampler*> intens;
  vector<Sampler*> other;

  for (auto s : _samplers) {
    if (s->getType() == "intensity") {
      intens.push_back(s);
    }
    else {
      other.push_back(s);
    }
  }

  // recombine
  _samplers.clear();
  for (auto i : intens) {
    _samplers.push_back(i);
  }
  for (auto o : other) {
    _samplers.push_back(o);
  }
}

void GibbsSchedule::addConstraints(ConstraintData cd)
{
  for (auto s : _samplers) {
    s->_constraints = cd;
  }
}

void GibbsSchedule::init()
{
  for (auto s : _samplers) {
    if (s->getType() == "intensity") {
      IntensitySampler* is = dynamic_cast<IntensitySampler*>(s);
      is->preProcess();
    }
  }
}


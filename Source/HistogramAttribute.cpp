/*
  ==============================================================================

    HistogramAttribute.cpp
    Created: 21 Apr 2016 12:07:38pm
    Author:  falindrith

  ==============================================================================
*/

#include "HistogramAttribute.h"

Histogram1D::Histogram1D(int numBins) {
  _count = 0;

  if (numBins < 1)
    _numBins = 1;
  else
    _numBins = numBins;
}

Histogram1D::Histogram1D(const Histogram1D & other) :
  _histData(other._histData),
  _count(other._count),
  _numBins(other._numBins)
{
}

Histogram1D::Histogram1D(map<unsigned int, unsigned int> data, unsigned int count, int numBins) :
  _histData(data), _count(count), _numBins(numBins)
{
}

Histogram1D::~Histogram1D()
{
}

void Histogram1D::addValToBin(double x)
{
  int bin = (int)(x * _numBins);
  _histData[bin] += 1;
  _count++;
}

void Histogram1D::addValToBin(uint8 x)
{
  addValToBin(x / 255.0);
}

unsigned int Histogram1D::getBin(unsigned int id)
{
  return _histData[id];
}

unsigned int Histogram1D::getNthBin(unsigned int n)
{
  unsigned int i = 0;
  for (const auto& d : _histData) {
    if (i == n) {
      return d.first;
    }

    i++;
  }

  // returns 0 if nothing is found
  return 0;
}

void Histogram1D::addToBin(unsigned int amt, unsigned int id)
{
  _histData[id] += amt;
  _count += amt;
}

void Histogram1D::removeFromBin(unsigned int amt, unsigned int id)
{
  // don't do a thing if the bin isn't actually here
  if (_histData.count(id) == 0 || _histData[id] == 0)
    return;

  _histData[id] -= amt;
  _count -= amt;
}

double Histogram1D::diff(Histogram1D & other)
{
  // histograms must have the same number of bins to be compared
  if (_histData.size() != other._histData.size())
    return -1;

  double sum = 0;
  for (auto& bin : _histData) {
    sum += sqrt(pow((double)bin.second - (double)(other._histData[bin.first]), 2));
  }

  return sum;
}

Histogram1D Histogram1D::consolidate(int targetNumBins)
{
  // distance threshold
  int maxDist = 1;
  

  map<unsigned int, unsigned int> newData = _histData;

  while (newData.size() > targetNumBins) {
    for (auto& b : newData) {
      // if the bin is below the threshold
      //if (b.second / (float)_count < t) {
        // find closest larger bin and merge
        int start = b.first;

        int max = -1;
        for (int i = 1; i <= maxDist; i++) {
          // do a search starting at the position of the current bin
          if (newData.count(start - i) > 0) {
            // check if bin is larger than current bin
            if (newData[start - i] >= newData[start]) {
              if (max == -1 || newData[max] < newData[start - i]) {
                max = start - i;
              }
            }
          }

          // same thing for forward
          if (newData.count(start + i) > 0) {
            if (newData[start + i] >= newData[start]) {
              if (max == -1 || newData[max] < newData[start + i]) {
                max = start + i;
              }
            }
          }

          if (max != -1) {
            newData[max] += newData[start];
            newData[start] = 0;
            break;
          }
        }
      //}
    }

    // delete bins that are 0
    for (int i = 0; i <= _numBins; i++) {
      if (newData[i] == 0)
        newData.erase(i);
    }

    // increment threshold
    //t += 0.01f;
    maxDist += 1;
  }

  return Histogram1D(newData, _count, _numBins);
}

double Histogram1D::avg()
{
  double interval = 1.0 / _numBins;
  double sum = 0;

  for (const auto& vals : _histData) {
    double binVal = interval * vals.first;
    sum += binVal * vals.second;
  }

  return sum / (double)_count;
}

double Histogram1D::variance()
{
  double a = avg();
  
  double sum = 0;
  double interval = 1.0 / _numBins;

  for (const auto& vals : _histData) {
    // bin deviation
    double dev = (vals.first * interval) - a;
    dev *= dev;

    // count the number of elements in the bin, add to running total
    sum += dev * vals.second;
  }

  // return mean of sum
  return sum / _count;
}

double Histogram1D::stdev()
{
  return sqrt(variance());
}

double Histogram1D::percentile(float pct)
{
  float target = pct / 100.0f;

  // defined as the first value such that n% of all values are <= than that value
  unsigned int total = 0;
  double interval = 1.0 / _numBins;
  unsigned int highest = 0;

  for (const auto& vals : _histData) {
    total += vals.second;
    if ((float)total / _count <= target)
      highest = vals.first;
    else
      break;
  }

  return highest * interval;
}

double Histogram1D::percentGreaterThan(float pct)
{
  pct /= 100.0;
  unsigned int firstBin = (unsigned int) (pct * _numBins);
  unsigned int total = 0;

  for (int i = firstBin; i < _numBins; i++) {
    total += _histData[i];
  }

  return total / (double)_count;
}

double Histogram1D::percentLessThan(float pct)
{
  return 1 - percentGreaterThan(pct);
}

// ============================================================================

Histogram3D::Histogram3D(int n) :
  _x(n), _y(n), _z(n), _xmin(0), _xmax(1), _ymin(0), _ymax(1), _zmin(0), _zmax(0)
{
  _count = 0;
  _histData.resize(_x * _y * _z);
}

Histogram3D::Histogram3D(int x, int y, int z) :
  _x(x), _y(y), _z(z), _xmin(0), _xmax(1), _ymin(0), _ymax(1), _zmin(0), _zmax(0)
{
  _count = 0;
  _histData.resize(_x * _y * _z);
}

Histogram3D::Histogram3D(int x, int y, int z, double xmin, double xmax, double ymin, double ymax, double zmin, double zmax) :
  _x(x), _y(y), _z(z), _xmin(xmin), _xmax(xmax), _ymin(ymin), _ymax(ymax), _zmin(zmin), _zmax(zmax)
{
  _count = 0;
  _histData.resize(_x * _y * _z);
}

Histogram3D::Histogram3D(const Histogram3D & other) :
  _x(other._x), _y(other._y), _z(other._z), _xmin(other._xmin), _xmax(other._xmax),
  _ymin(other._ymin), _ymax(other._ymax), _zmin(other._zmin), _zmax(other._zmax)
{
  _count = other._count;
  _histData.resize(_x * _y * _z);
  
  memcpy(&_histData.front(), &other._histData.front(), sizeof(unsigned int) * _x * _y * _z);
}

Histogram3D & Histogram3D::operator=(const Histogram3D & other)
{
  _x = other._x;
  _y = other._y;
  _z = other._z;
  _xmin = other._xmin;
  _xmax = other._xmax;
  _ymin = other._ymin;
  _ymax = other._ymax;
  _zmin = other._zmin;
  _zmax = other._zmax;
  _count = other._count;

  _histData.resize(_x * _y * _z);

  memcpy(&_histData.front(), &other._histData.front(), sizeof(unsigned int) * _x * _y * _z);
  return *this;
}

Histogram3D::~Histogram3D()
{
}

void Histogram3D::addValToBin(double x, double y, double z)
{
  // scale x, y, z to proper values and find what bin they're in
  int xbin = (int) clamp(((x - _xmin) / (_xmax - _xmin)) * _x, 0, _x - 1);
  int ybin = (int) clamp(((y - _ymin) / (_ymax - _ymin)) * _y, 0, _y - 1);
  int zbin = (int) clamp(((z - _zmin) / (_zmax - _zmin)) * _z, 0, _z - 1);

  addToBin(1, xbin, ybin, zbin);
}

void Histogram3D::addToBin(int amt, int x, int y, int z)
{
  _histData[getIndex(x, y, z)] += amt;
  _count += amt;
}

void Histogram3D::removeFromBin(int amt, int x, int y, int z)
{
  _histData[getIndex(x, y, z)] -= amt;
  _count -= amt;
}

unsigned int Histogram3D::getBin(int x, int y, int z)
{
  return _histData[getIndex(x, y, z)];
}

string Histogram3D::toString()
{
  stringstream ss;

  for (int z = 0; z < _z; z++) {
    ss << "z = " << z << "\n";
    for (int y = 0; y < _y; y++) {
      bool first = true;
      for (int x = 0; x < _x; x++) {
        if (!first) {
          ss << "\t" << _histData[getIndex(x, y, z)];
        }
        else {
          ss << _histData[getIndex(x, y, z)];
          first = false;
        }
      }
      ss << "\n";
    }
    ss << "\n";
  }

  return ss.str();
}

double Histogram3D::L2dist(Histogram3D & other)
{
  if (other._x != _x || other._y != _y || other._z != _z) {
    // invalid dimensions
    Lumiverse::Logger::log(ERR, "Invalid dimensions provided for histogram difference.");
    return -1;
  }

  double err = 0;
  for (int i = 0; i < _x * _y * _z; i++) {
    err += pow((double)_histData[i] - (double)other._histData[i], 2.0);
  }

  return sqrt(err);
}

double Histogram3D::emd(Histogram3D & other, vector<vector<double>>& gd)
{
  return _emd(normalized(), other.normalized(), gd);
}

vector<double> Histogram3D::normalized()
{
  vector<double> norm;
  norm.resize(_x * _y * _z);

  for (int i = 0; i < _x * _y * _z; i++) {
    if (_count != 0) {
      norm[i] = _histData[i] / (double)_count;
    }
  }

  return norm;
}

vector<vector<double>> Histogram3D::getGroundDistances()
{
  // L1 distance (manhattan distance) between each bin since
  // this is a fixed size histogram for now
  vector<vector<double>> gd;
  gd.resize(_x * _y * _z);

  // fun nested loop time 
  for (int i = 0; i < _z; i++) {
    for (int j = 0; j < _y; j++) {
      for (int k = 0; k < _x; k++) {
        vector<double> distances;
        distances.resize(_x * _y * _z);
        Eigen::Vector3d binVal = getBinVal(k, j, i);

        // ok now go through it again
        for (int i2 = 0; i2 < _z; i2++) {
          for (int j2 = 0; j2 < _y; j2++) {
            for (int k2 = 0; k2 < _x; k2++) {
              // calculate l2 dist in Lab space
              distances[getIndex(i2, j2, k2)] = (getBinVal(k2, j2, i2) - binVal).norm();
            }
          }
        }

        gd[getIndex(i, j, k)] = distances;
      }
    }
  }

  return gd;
}

int Histogram3D::getIndex(int x, int y, int z)
{
  return x + y * _x + z * _x * _y;
}

Eigen::Vector3d Histogram3D::getBinVal(int x, int y, int z)
{
  // we'll return the midpoint of the bin 
  Eigen::Vector3d binVal;
  binVal[0] = (x + 0.5) * ((_xmax - _xmin) / _x) + _xmin;
  binVal[1] = (y + 0.5) * ((_ymax - _ymin) / _y) + _ymin;
  binVal[2] = (z + 0.5) * ((_zmax - _zmin) / _z) + _zmin;

  return binVal;
}

// ============================================================================

HistogramFeature::HistogramFeature()
{
}

HistogramFeature::HistogramFeature(float L, float a, float b, float x, float y)
{
  _data.L = L;
  _data.a = a;
  _data.b = b;
  _data.x = x;
  _data.y = y;
}

bool HistogramFeature::operator==(const HistogramFeature & other) const
{
  return (other._data.L == _data.L &&
    other._data.a == _data.a &&
    other._data.b == _data.b &&
    other._data.x == _data.x &&
    other._data.y == _data.y
  );
}

Sparse5DHistogram::Sparse5DHistogram(vector<float> bounds, float lambda) : _bounds(bounds), _lambda(lambda)
{
  _totalWeight = 0;
}

Sparse5DHistogram::~Sparse5DHistogram()
{
}

void Sparse5DHistogram::add(float l, float a, float b, float x, float y)
{
  addToBin(1, l, a, b, x, y);
}

void Sparse5DHistogram::addToBin(float amt, float l, float a, float b, float x, float y)
{
  _histData[closestBin(l, a, b, x, y)] += amt;
  _totalWeight += amt;
}

void Sparse5DHistogram::removeFromBin(float amt, float l, float a, float b, float x, float y)
{
  _histData[closestBin(l, a, b, x, y)] -= amt;
  _totalWeight -= amt;
}

float Sparse5DHistogram::getBin(float l, float a, float b, float x, float y)
{
  return _histData[closestBin(l, a, b, x, y)];
}

float Sparse5DHistogram::EMD(Sparse5DHistogram & other)
{
  vector<feature_t> f1, f2;
  vector<float> weights1 = normalizedWeights(f1);
  vector<float> weights2 = other.normalizedWeights(f2);
  
  signature_t s1;
  s1.n = f1.size();
  s1.Features = &f1[0];
  s1.Weights = &weights1[0];

  signature_t s2;
  s2.n = f2.size();
  s2.Features = &f2[0];
  s2.Weights = &weights2[0];

  function<float(feature_t*, feature_t*)> dist = [this](feature_t* f1, feature_t* f2) {
    return sqrt(pow(f1->L - f2->L, 2) + pow(f1->a - f2->a, 2) + pow(f1->b - f2->b, 2) + _lambda * (pow(f1->x - f2->x, 2) + pow(f1->y - f2->y, 2)));
  };

  return emd(&s1, &s2, dist, NULL, NULL);
}

vector<float> Sparse5DHistogram::weights(vector<feature_t>& out)
{
  out.clear();
  vector<float> weights;
  for (auto& f : _histData) {
    weights.push_back(f.second);
    out.push_back(f.first._data);
  }

  return weights;
}

vector<float> Sparse5DHistogram::normalizedWeights(vector<feature_t>& out)
{
  out.clear();
  vector<float> nrmWeights;
  for (auto& f : _histData) {
    nrmWeights.push_back(f.second / _totalWeight);
    out.push_back(f.first._data);
  }

  return nrmWeights;
}

void Sparse5DHistogram::setLambda(double lambda)
{
  _lambda = lambda;
}

HistogramFeature Sparse5DHistogram::closestBin(float l, float a, float b, float x, float y)
{
  // bins are aligned in multiples of bin size starting at bin base
  // ex: L base = 5, L size = 10. Bin centers at 5, 15, 25... ranges at [0,10),[10,20)...
  float lcenter = closest(l, 0);
  float acenter = closest(a, 2);
  float bcenter = closest(b, 4);
  float xcenter = closest(x, 6);
  float ycenter = closest(y, 8);

  return HistogramFeature(lcenter, acenter, bcenter, xcenter, ycenter);
}

float Sparse5DHistogram::closest(float val, int boundsIndex)
{
  float bin = (int)((abs(val) - _bounds[boundsIndex]) / _bounds[boundsIndex + 1]) *
    _bounds[boundsIndex + 1] + _bounds[boundsIndex] + (_bounds[boundsIndex + 1] / 2);

  if (bin == 0)
    return 0;
  else
    return (val < 0) ? -bin : bin;
}

// ============================================================================

HistogramAttribute::HistogramAttribute(string name) : AttributeControllerBase(name)
{
}

HistogramAttribute::HistogramAttribute(string name, int w, int h) :
  AttributeControllerBase(name, w, h)
{
}

HistogramAttribute::~HistogramAttribute()
{
}

Histogram1D HistogramAttribute::getGrayscaleHist(Image& canonical, int numBins)
{
  Histogram1D gs(numBins);

  for (int x = 0; x < _canonicalWidth; x++) {
    for (int y = 0; y < _canonicalHeight; y++) {
      auto color = canonical.getPixelAt(x, y);
      gs.addValToBin(color.getBrightness());
    }
  }

  return gs;
}

Histogram1D HistogramAttribute::getPerceptualGrayscaleHist(Image& canonical, int numBins)
{
  Histogram1D gs(numBins);

  for (int x = 0; x < _canonicalWidth; x++) {
    for (int y = 0; y < _canonicalHeight; y++) {
      auto color = canonical.getPixelAt(x, y);
      gs.addValToBin(color.getPerceivedBrightness());
    }
  }

  return gs;
}

Histogram1D HistogramAttribute::getChannelHist(Image& canonical, int numBins, int channel)
{
  Histogram1D gs(numBins);

  for (int x = 0; x < _canonicalWidth; x++) {
    for (int y = 0; y < _canonicalHeight; y++) {
      auto color = canonical.getPixelAt(x, y);

      if (channel == 0) {
        gs.addValToBin(color.getRed());
      }
      if (channel == 1) {
        gs.addValToBin(color.getGreen());
      }
      if (channel == 2) {
        gs.addValToBin(color.getBlue());
      }
    }
  }

  return gs;
}

Histogram1D HistogramAttribute::getHueHist(Image& canonical, int numBins)
{
  Histogram1D gs(numBins);

  for (int x = 0; x < _canonicalWidth; x++) {
    for (int y = 0; y < _canonicalHeight; y++) {
      auto color = canonical.getPixelAt(x, y);
      gs.addValToBin(color.getHue());
    }
  }

  return gs;
}

Histogram1D HistogramAttribute::getSatHist(Image& canonical, int numBins)
{
  Histogram1D gs(numBins);

  for (int x = 0; x < _canonicalWidth; x++) {
    for (int y = 0; y < _canonicalHeight; y++) {
      auto color = canonical.getPixelAt(x, y);
      gs.addValToBin(color.getSaturation());
    }
  }

  return gs;
}

Eigen::Vector3d HistogramAttribute::getAverageColor(Image i)
{
  Eigen::Vector3d color(0,0,0);

  for (int y = 0; y < i.getHeight(); y++) {
    for (int x = 0; x < i.getWidth(); x++) {
      auto c = i.getPixelAt(x, y);
      color[0] += c.getRed() / 255.0;
      color[1] += c.getGreen() / 255.0;
      color[2] += c.getBlue() / 255.0;
    }
  }

  return color / (i.getWidth() * i.getHeight());
}

double HistogramAttribute::getAverageHue(Image i)
{
  double hue = 0;

  for (int y = 0; y < i.getHeight(); y++) {
    for (int x = 0; x < i.getWidth(); x++) {
      auto c = i.getPixelAt(x, y);
      hue += c.getHue();
    }
  }

  return hue / (i.getHeight() * i.getWidth());
}

Histogram3D HistogramAttribute::getLabHist(Image& canonical, int n)
{
  return getLabHist(canonical, n, n, n);
}

Histogram3D HistogramAttribute::getLabHist(Image& canonical, int x, int y, int z)
{
  Histogram3D lab(x, y, z, 0, 100, -100, 100, -100, 100);

  for (int y2 = 0; y2 < canonical.getHeight(); y2++) {
    for (int x2 = 0; x2 < canonical.getWidth(); x2++) {
      auto color = canonical.getPixelAt(x2, y2);
      Eigen::Vector3d labColor = RGBtoLab(color.getRed() / 255.0, color.getGreen() / 255.0, color.getBlue() / 255.0);

      lab.addValToBin(labColor[0], labColor[1], labColor[2]);
    }
  }

  return lab;
}

Sparse5DHistogram HistogramAttribute::getLabxyHist(Image & canonical, int n)
{
  return getLabxyHist(canonical, n, n, n, n, n);
}

Sparse5DHistogram HistogramAttribute::getLabxyHist(Image & canonical, int l, int a, int b, int x, int y)
{
  Sparse5DHistogram hist({0, 10, -5, 20, -5, 20, 0, 0.2f, 0, 0.2f}, 1);

  for (int y2 = 0; y2 < canonical.getHeight(); y2++) {
    for (int x2 = 0; x2 < canonical.getWidth(); x2++) {
      auto color = canonical.getPixelAt(x2, y2);
      Eigen::Vector3d labColor = RGBtoLab(color.getRed() / 255.0, color.getGreen() / 255.0, color.getBlue() / 255.0);
      float xnrm = x2 / (float)canonical.getWidth();
      float ynrm = y2 / (float)canonical.getHeight();

      hist.add(labColor[0], labColor[1], labColor[2], xnrm, ynrm);
    }
  }

  return hist;
}

Eigen::Vector3d HistogramAttribute::RGBtoLab(double r, double g, double b)
{
  Eigen::Vector3d xyz = ColorUtils::convRGBtoXYZ(r, g, b, sRGB);
  return ColorUtils::convXYZtoLab(xyz, refWhites[D65] / 100.0);
}
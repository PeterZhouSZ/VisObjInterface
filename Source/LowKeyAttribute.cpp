/*
  ==============================================================================

    LowKeyAttribute.cpp
    Created: 30 Mar 2016 5:15:56pm
    Author:  falindrith

  ==============================================================================
*/

#include "LowKeyAttribute.h"

LowKeyAttribute::LowKeyAttribute(string area) : _area(area), AttributeControllerBase("Low Key: " + area)
{
  _autoLockParams.insert(HUE);
  _autoLockParams.insert(SAT);
  _autoLockParams.insert(VALUE);
  _autoLockParams.insert(POLAR);
  _autoLockParams.insert(AZIMUTH);
  _autoLockParams.insert(SOFT);
}

LowKeyAttribute::~LowKeyAttribute()
{
}

double LowKeyAttribute::evaluateScene(Snapshot * s, Image& /* img */)
{
  auto& rigData = s->getRigData();

  // find max value
  double max = 0;
  double sum = 0;
  for (auto& id : _devices) {
    double intens = rigData[id]->getIntensity()->getVal();
    sum += intens;
    if (intens > max)
      max = intens;
  }

  // get ratio of max brightness light to all other light brightness
  sum -= max;

  return (max - sum) / 200;
}

void LowKeyAttribute::preProcess()
{
  _devices.clear();
  auto devices = getRig()->select("$area=" + _area).getDevices();
  for (const auto& d : devices) {
    _devices.push_back(d->getId());
  }
}

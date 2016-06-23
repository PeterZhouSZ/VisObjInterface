/*
  ==============================================================================

    TopLevelCluster.cpp
    Created: 21 Jun 2016 1:40:40pm
    Author:  eshimizu

  ==============================================================================
*/

#include "TopLevelCluster.h"
#include "Clustering.h"

TopLevelCluster::TopLevelCluster()
{
  _rep = nullptr;
  _viewer = new Viewport();
  _contents = new SearchResultList();
  _viewer->setViewedComponent(_contents);
  addAndMakeVisible(_viewer);

  _statsUpdated = false;
}

TopLevelCluster::~TopLevelCluster()
{
  delete _viewer;
}

void TopLevelCluster::resized()
{
  // proportionally the top element will take up 33%
  auto lbounds = getLocalBounds();

  if (_rep != nullptr) {
    _rep->setBounds(lbounds.removeFromTop(lbounds.getHeight() * 0.33));
  }

  _contents->setWidth(lbounds.getWidth() - _viewer->getScrollBarThickness());
  _viewer->setBounds(lbounds);
}

void TopLevelCluster::paint(Graphics & g)
{
  // borders likely need to be drawn eventually
}

void TopLevelCluster::addToCluster(shared_ptr<SearchResultContainer> r)
{
  _contents->addResult(r);
  resized();
  _statsUpdated = false;
}

int TopLevelCluster::numElements()
{
  return _contents->size();
}

int TopLevelCluster::getClusterId()
{
  return _id;
}

void TopLevelCluster::setClusterId(int id)
{
  _id = id;
}

Array<shared_ptr<SearchResultContainer> > TopLevelCluster::getChildElements()
{
  Array<shared_ptr<SearchResultContainer> > res;

  for (int i = 0; i < _contents->size(); i++)
    res.add((*_contents)[i]);

  return res;
}

Array<shared_ptr<SearchResultContainer>> TopLevelCluster::getAllChildElements()
{
  Array<shared_ptr<SearchResultContainer> > res;
  for (int i = 0; i < _contents->size(); i++) {
    if ((*_contents)[i]->isClusterCenter()) {
      res.addArray((*_contents)[i]->getResults());
    }
    else {
      res.add((*_contents)[i]);
    }
  }

  return res;
}

void TopLevelCluster::setRepresentativeResult()
{
  double minVal = DBL_MAX;
  shared_ptr<SearchResultContainer> best = nullptr;

  for (int i = 0; i < _contents->size(); i++) {
    SearchResult* e = (*_contents)[i]->getSearchResult();

    if (e->_objFuncVal < minVal) {
      minVal = e->_objFuncVal;
      best = (*_contents)[i];
    }
  }

  // copy best
  if (_rep != nullptr) {
    _rep = nullptr;
  }

  _rep = shared_ptr<SearchResultContainer>(new SearchResultContainer(new SearchResult(*best->getSearchResult())));
  _rep->setImage(best->getImage());
  _scene = _rep->getSearchResult()->_scene;
  _features = best->getFeatures();
  addAndMakeVisible(_rep.get());
}

shared_ptr<SearchResultContainer> TopLevelCluster::getRepresentativeResult()
{
  return _rep;
}

shared_ptr<SearchResultContainer> TopLevelCluster::constructResultContainer()
{
  auto src = shared_ptr<SearchResultContainer>(new SearchResultContainer(new SearchResult()));
  src->getSearchResult()->_scene = _scene;
  src->setFeatures(_features);

  return src;
}

void TopLevelCluster::sort(AttributeSorter * s)
{
  _contents->sort(s);
}

void TopLevelCluster::cluster()
{
  // here we'll want to take everything from contents and cluster them
  // we will need to convert from top level clusters to regular clusters however
  // remove everything from contents
  auto results = _contents->removeAllResults();

  // cluster results using secondary clustering settings
  ClusterMethod mode = getGlobalSettings()->_secondaryClusterMethod;
  bool invert = (getGlobalSettings()->_secondaryFocusArea == BACKGROUND) ? true : false;
  bool overrideMask = (getGlobalSettings()->_secondaryFocusArea == ALL_IMAGE) ? true : false;
  DistanceMetric dm = getGlobalSettings()->_secondaryClusterMetric;

  // run the clustering
  Array<shared_ptr<TopLevelCluster> > centers;

  distFuncType f = [invert, overrideMask, dm](SearchResultContainer* x, SearchResultContainer* y) {
    return x->dist(y, dm, overrideMask, invert);
  };

  switch (mode) {
  case KMEANS:
    centers = Clustering::kmeansClustering(results, getGlobalSettings()->_numSecondaryClusters, f);
    break;
  case MEAN_SHIFT:
    centers = Clustering::meanShiftClustering(results, getGlobalSettings()->_meanShiftBandwidth);
    break;
  case SPECTRAL:
    centers = Clustering::spectralClustering(results, getGlobalSettings()->_numSecondaryClusters, f);
    break;
  case DIVISIVE:
    centers = Clustering::divisiveKMeansClustering(results, getGlobalSettings()->_numSecondaryClusters, f);
    break;
  default:
    break;
  }

  // take the cluster centers representative items, and create new elements
  for (auto& c : centers) {
    c->setRepresentativeResult();
    auto subcenter = c->getRepresentativeResult();
    
    // add items contained in the top level item to the subcenter
    for (int i = 0; i < c->numElements(); i++) {
      subcenter->addToCluster(c->getElement(i));
    }

    _contents->addResult(subcenter);
  }

  resized();
}

shared_ptr<SearchResultContainer> TopLevelCluster::getElement(int i)
{
  return (*_contents)[i];
}

void TopLevelCluster::clear()
{
  _contents->removeAllResults();
  resized();
  _statsUpdated = false;
}

void TopLevelCluster::calculateStats(function<double(SearchResultContainer*, SearchResultContainer*)> f)
{
  if (_statsUpdated)
    return;

  // get the elements
  auto elems = getAllChildElements();
  
  // calculate pairwise distances 
  _distMatrix.resize(elems.size(), elems.size());
  _distMatrix.setZero();

  double max = 0;
  int x; 
  int y;
  for (int i = 0; i < elems.size(); i++) {
    _distMatrix(i, i) = 0;
    for (int j = i + 1; j < elems.size(); j++) {
      double dist = f(elems[i].get(), elems[j].get());
      _distMatrix(i, j) = dist;
      _distMatrix(j, i) = dist;

      if (dist > max) {
        max = dist;
        x = i;
        y = j;
      }
    }
  }

  // save max distance and relevant elements
  _diameter = max;
  _x = elems[x];
  _y = elems[y];
  _statsUpdated = true;
}

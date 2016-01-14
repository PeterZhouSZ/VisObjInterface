/*
  ==============================================================================

    AttributeSearchResult.h
    Created: 7 Jan 2016 4:59:12pm
    Author:  falindrith

  ==============================================================================
*/

#ifndef ATTRIBUTESEARCHRESULT_H_INCLUDED
#define ATTRIBUTESEARCHRESULT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "globals.h"
#include "AttributeSearch.h"

class AttributeSearchCluster;

//==============================================================================
/*
*/
class AttributeSearchResult : public Component, public SettableTooltipClient
{
public:
  AttributeSearchResult(SearchResult* result);
  ~AttributeSearchResult();

  void paint (Graphics&);
  void resized();

  // Sets the image for this component.
  void setImage(Image img);

  SearchResult* getSearchResult() { return _result; }

  // Clicking does things.
  virtual void mouseDown(const MouseEvent& event);

  void setClusterElements(Array<AttributeSearchResult*> elems);
  void addClusterElement(AttributeSearchResult* elem);

private:
  // Search result object from the attribute search
  SearchResult* _result;

  // rendered image 
  Image _render;

  // If this is empty, this object does not spawn a popup window element when clicked.
  Array<AttributeSearchResult*> _clusterElems;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AttributeSearchResult)
};


//==============================================================================
class AttributeSearchCluster : public Component
{
public:
  AttributeSearchCluster(Array<AttributeSearchResult*> elems);
  ~AttributeSearchCluster();

  void paint(Graphics& g);
  void resized();

  void setWidth(int width);

private:
  Array<AttributeSearchResult*> _elems;

  int _elemsPerRow = 2;
};

#endif  // ATTRIBUTESEARCHRESULT_H_INCLUDED
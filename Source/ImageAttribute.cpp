/*
  ==============================================================================

    ImageAttribute.cpp
    Created: 1 Aug 2016 5:24:38pm
    Author:  eshimizu

  ==============================================================================
*/

#include "ImageAttribute.h"

ImageDrawer::ImageDrawer(Image i) : _img(i) {
}

ImageDrawer::~ImageDrawer()
{
}

void ImageDrawer::resized()
{
}

void ImageDrawer::paint(Graphics & g)
{
  g.drawImageWithin(_img, 0, 0, getWidth(), getHeight(), RectanglePlacement::centred);
}

ImageAttribute::ImageAttribute(string name, string filepath, int n) : HistogramAttribute(name),
  _sourceHist(1), _n(n)
{
  File img(filepath);
  FileInputStream in(img);

  if (in.openedOk()) {
    // load image
    PNGImageFormat pngReader;
    _originalImg = pngReader.decodeImage(in);
    _sourceImg = _originalImg.rescaled(_canonicalWidth, _canonicalHeight);

    getRecorder()->log(SYSTEM, "Loaded image for attribute " + name);
  }
  else {
    getRecorder()->log(SYSTEM, "Failed to load image for attribute " + name);
  }

  _showImgButton.setButtonText("Show Image");
  _showImgButton.setName("Show Image");
  _showImgButton.addListener(this);
  addAndMakeVisible(_showImgButton);
}

ImageAttribute::ImageAttribute(string name, Image img, int n) : HistogramAttribute(name),
  _sourceHist(1), _n(n)
{
  _originalImg = img;
  _sourceImg = img.rescaled(_canonicalWidth, _canonicalHeight);

  _showImgButton.setButtonText("Show Image");
  _showImgButton.setName("Show Image");
  _showImgButton.addListener(this);
  addAndMakeVisible(_showImgButton);
}

ImageAttribute::~ImageAttribute()
{
}

double ImageAttribute::evaluateScene(Snapshot * s, Image& img)
{
  if (s->getRigData().size() == 0)
    return 0;

  if (img.getWidth() != _canonicalWidth || img.getHeight() != _canonicalHeight) {
    // in the event that the image was actually generated by an attribute with a different
    // size (which would be a bit weird) regenerate it here.
    img = generateImage(s);
  }

  LabxyHistogram currentHist = getLabxyHist(img, _n, _n, _n, 1, 1);

  double diff = currentHist.EMD(_sourceHist, _metric);

  return (500 - diff) / 5;
}

void ImageAttribute::preProcess()
{
  _sourceHist = getLabxyHist(_sourceImg, _n, _n, _n, 1, 1);

  _metric = _sourceHist.getGroundDistances();

  // sanity check
  double selfDist = _sourceHist.EMD(_sourceHist, _metric);
}

void ImageAttribute::resized()
{
  HistogramAttribute::resized();

  auto lbounds = getLocalBounds();
  auto top = lbounds.removeFromTop(24);

  top.removeFromRight(80);
  _showImgButton.setBounds(top.removeFromRight(80).reduced(2));
}

void ImageAttribute::buttonClicked(Button * b)
{
  HistogramAttribute::buttonClicked(b);

  if (b->getName() == "Show Image") {
    Viewport* vp = new Viewport();
    ImageDrawer* id = new ImageDrawer(_originalImg);
    id->setSize(500, 500);
    vp->setViewedComponent(id, true);
    vp->setSize(id->getWidth(), id->getHeight());

    CallOutBox::launchAsynchronously(vp, _showImgButton.getScreenBounds(), nullptr);
  }
}

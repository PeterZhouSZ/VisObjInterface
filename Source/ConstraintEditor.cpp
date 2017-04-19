/*
  ==============================================================================

    ConstraintEditor.cpp
    Created: 20 Jul 2016 5:26:24pm
    Author:  eshimizu

  ==============================================================================
*/

#include "ConstraintEditor.h"
#include "MainComponent.h"

ConstraintComponent::ConstraintComponent(ConstraintType t, int id, Component* parent) :
  _t(t), _id(id), _deleteButton("x"), _parent(parent)
{
  _deleteButton.addListener(this);
  addAndMakeVisible(_deleteButton);
}

ConstraintComponent::~ConstraintComponent()
{
}

void ConstraintComponent::paint(Graphics & g)
{
  g.fillAll(Colour(0xff333333));

  // paints the type of the component
  auto b = getLocalBounds();
  
  g.setColour(Colours::white);
  g.setFont(16);

  String text = "Type: ";
  
  switch (_t) {
  case KEY:
    text += "Key Light";
    break;
  case EXCLUDE:
    text += "Exclude Lights";
    break;
  case RELATIVE_BRIGHTNESS:
    text += "Relative Brightness";
    break;
  case SATURATION_RANGE:
    text += "Saturation Range";
    break;
  }

  g.drawFittedText(text, b.removeFromTop(30).reduced(1), Justification::centredLeft, 1);
}

void ConstraintComponent::resized()
{
  auto b = getLocalBounds();

  _deleteButton.setBounds(b.removeFromTop(30).removeFromRight(30).reduced(5));
}

ConstraintType ConstraintComponent::getType()
{
  return _t;
}

int ConstraintComponent::getId()
{
  return _id;
}

void ConstraintComponent::buttonClicked(Button * b)
{
  if (b->getName() == "x") {
    ((ConstraintEditor*)_parent)->deleteConstraint(_id);
    // immediately exit since the component no longer exists;
    return;
  }
}

PopupMenu ConstraintComponent::getSelectorMenu(map<int, string>& cmdOut)
{
  PopupMenu all;

  // get all areas
  auto areaSet = getRig()->getMetadataValues("area");
  int i = 1;

  all.addItem(i, "Selected Lights");
  i++;
  all.addItem(i, "All Lights");
  i++;
  all.addItem(i, "Custom Query...");
  i++;

  PopupMenu area;

  // populate areas
  for (auto a : getRig()->getMetadataValues("area")) {
    PopupMenu perAreaSys;

    // check presence of systems
    DeviceSet devices = getRig()->select("$area=" + a);
    set<string> sys = devices.getAllMetadataForKey("system");

    // populate per-area system
    for (auto s : sys) {
      perAreaSys.addItem(i, s);
      cmdOut[i] = "$area=" + a + "[$system=" + s + "]";
      i++;
    }

    area.addSubMenu(a, perAreaSys, true, nullptr, false, i);
    cmdOut[i] = "$area=" + a;

    i++;
  }

  all.addSubMenu("Area", area);

  int sysStart = i;

  // get systems
  auto systemSet = getRig()->getMetadataValues("system");

  vector<string> systems;
  for (auto s : systemSet)
    systems.push_back(s);

  PopupMenu system;
  for (int j = 0; j < systems.size(); j++) {
    system.addItem(sysStart + j, systems[j]);
    cmdOut[sysStart + j] = "$system=" + systems[j];
  }

  all.addSubMenu("System", system);

  return all;
}

void ConstraintComponent::showDeviceSelectMenu(Button * b, DeviceSet& d)
{
  map<int, string> cmdMap;
  PopupMenu m = getSelectorMenu(cmdMap);

  int result = m.showAt(b);
  
  if (result == 0)
    return;

  if (result == 1) {
    // use current selection
    MainContentComponent* mc = dynamic_cast<MainContentComponent*>(getAppMainContentWindow()->getContentComponent());
    auto current = mc->getSelectedDeviceIds();

    d = DeviceSet(getRig());
    for (auto id : current) {
      d = d.add(id.toStdString());
    }
  }
  else if (result == 2) {
    // get all devices
    d = getRig()->getAllDevices();
  }
  else if (result == 3) {
    // custom query
    AlertWindow w("Custom Query",
      "Enter a Lumiverse Selection Query.",
      AlertWindow::QuestionIcon);

    w.addTextEditor("query", "", "Query");

    w.addButton("Select", 1, KeyPress(KeyPress::returnKey, 0, 0));
    w.addButton("Cancel", 0, KeyPress(KeyPress::escapeKey, 0, 0));

    if (w.runModalLoop() != 0) // is they picked 'go'
    {
      // this is the item they selected
      auto query = w.getTextEditorContents("query");
      d = getRig()->select(query.toStdString());
    }
  }
  else {
    // preset query
    d = getRig()->select(cmdMap[result]);
  }
}

String ConstraintComponent::deviceSetToString(DeviceSet & d)
{
  String sel = "Selected: ";
  auto ids = d.getIds();

  if (ids.size() == 0) {
    return "[No Devices Selected]";
  }
  else {
    for (int i = 0; i < ids.size(); i++) {
      sel += ids[i];

      if (i != ids.size() - 1)
        sel += ", ";
    }
  }

  return sel;
}

KeyConstraint::KeyConstraint(int id, Component* parent) : ConstraintComponent(KEY, id, parent)
{
  _exclusive = false;
  _exclusiveButton.setName("exclusive");
  _exclusiveButton.setButtonText("Exclusive");
  _exclusiveButton.setToggleState(_exclusive, dontSendNotification);
  _exclusiveButton.addListener(this);
  _exclusiveButton.setColour(ToggleButton::ColourIds::textColourId, Colours::white);
  addAndMakeVisible(_exclusiveButton);

  _deviceSelector.setName("select");
  _deviceSelector.setButtonText("[No Devices Selected]");
  _deviceSelector.addListener(this);
  addAndMakeVisible(_deviceSelector);
}

KeyConstraint::~KeyConstraint()
{
}

void KeyConstraint::paint(Graphics & g)
{
  // paint things
  ConstraintComponent::paint(g);
}

void KeyConstraint::resized()
{
  ConstraintComponent::resized();

  auto b = getLocalBounds();
  b.removeFromTop(30);

  _deviceSelector.setBounds(b.removeFromTop(30).reduced(2));
  _exclusiveButton.setBounds(b.removeFromTop(30).reduced(2));
}

void KeyConstraint::buttonClicked(Button * b)
{
  // process parent events first
  if (b->getName() == "x") {
    ConstraintComponent::buttonClicked(b);
    return;
  }

  if (b->getName() == "exclusive") {
    _exclusive = b->getToggleState();
  }
  else if (b->getName() == "select") {
    // bring up the selection menu and do the selection. Since this gets replicated
    // across devices, the function to do this is in the base class
    showDeviceSelectMenu(b, _affected);
    updateButtonText();
  }
}

void KeyConstraint::updateButtonText()
{
  _deviceSelector.setButtonText(deviceSetToString(_affected));
}

ExcludeConstraint::ExcludeConstraint(int id, Component* parent) : ConstraintComponent(EXCLUDE, id, parent) {
  _deviceSelector.setName("select");
  _deviceSelector.setButtonText("[No Devices Selected]");
  _deviceSelector.addListener(this);
  addAndMakeVisible(_deviceSelector);

  _mode.addItem("Ignore Lights", 1);
  _mode.addItem("Turn Lights Off", 2);
  _mode.setSelectedId(1, dontSendNotification);

  // turnOff = false => Ignore Lights
  _turnOff = false;

  _mode.addListener(this);
  addAndMakeVisible(_mode);
}

ExcludeConstraint::~ExcludeConstraint()
{
}

void ExcludeConstraint::paint(Graphics & g)
{
  ConstraintComponent::paint(g);

  auto b = getLocalBounds();
  b.removeFromTop(60);

  auto mode = b.removeFromTop(30);

  g.setColour(Colours::white);
  g.setFont(14);
  g.drawFittedText("Exclusion Mode", mode.removeFromLeft(100), Justification::centred, 1);
}

void ExcludeConstraint::resized()
{
  ConstraintComponent::resized();

  auto b = getLocalBounds();
  b.removeFromTop(30);

  _deviceSelector.setBounds(b.removeFromTop(30).reduced(2));
  
  auto mode = b.removeFromTop(30);
  mode.removeFromLeft(100);
  _mode.setBounds(mode.reduced(2));
}

void ExcludeConstraint::buttonClicked(Button * b)
{
  // process parent events first
  if (b->getName() == "x") {
    ConstraintComponent::buttonClicked(b);
    return;
  }

  if (b->getName() == "select") {
    // bring up the selection menu and do the selection. Since this gets replicated
    // across devices, the function to do this is in the base class
    showDeviceSelectMenu(b, _affected);
    updateButtonText();
  }
}

void ExcludeConstraint::comboBoxChanged(ComboBox * b)
{
  int selectedId = b->getSelectedId();

  if (selectedId == 1) {
    // ignore
    _turnOff = false;
  }
  else if (selectedId == 2) {
    _turnOff = true;
  }
}

void ExcludeConstraint::updateButtonText()
{
  _deviceSelector.setButtonText(deviceSetToString(_affected));
}

RelativeConstraint::RelativeConstraint(int id, Component* parent) :
  ConstraintComponent(RELATIVE_BRIGHTNESS, id, parent)
{
  _sourceSelector.setName("source");
  _sourceSelector.setButtonText("[No Devices Selected]");
  _sourceSelector.addListener(this);
  addAndMakeVisible(_sourceSelector);
  
  _targetSelector.setName("target");
  _targetSelector.setButtonText("[No Devices Selected]");
  _targetSelector.addListener(this);
  addAndMakeVisible(_targetSelector);

  _ratioSlider.setName("ratio");
  _ratioSlider.setRange(0, 2, 0.01);
  _ratioSlider.setValue(1);
  _ratioSlider.setSliderStyle(Slider::SliderStyle::LinearHorizontal);
  _ratioSlider.setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxRight, false, 80, 26);
  addAndMakeVisible(_ratioSlider);
}

RelativeConstraint::~RelativeConstraint()
{
}

void RelativeConstraint::paint(Graphics & g)
{
  ConstraintComponent::paint(g);

  g.setColour(Colours::white);
  g.setFont(14);

  auto b = getLocalBounds();
  b.removeFromTop(30);

  auto row1 = b.removeFromTop(30).reduced(2);
  g.drawFittedText("Source", row1.removeFromLeft(80), Justification::centred, 1);

  auto row2 = b.removeFromTop(30).reduced(2);
  g.drawFittedText("Target", row2.removeFromLeft(80), Justification::centred, 1);

  auto row3 = b.removeFromTop(30).reduced(2);
  g.drawFittedText("Ratio", row3.removeFromLeft(80), Justification::centred, 1);
}

void RelativeConstraint::resized()
{
  ConstraintComponent::resized();

  auto b = getLocalBounds();
  b.removeFromTop(30);

  auto row1 = b.removeFromTop(30).reduced(2);
  row1.removeFromLeft(80);
  _sourceSelector.setBounds(row1);
  
  auto row2 = b.removeFromTop(30).reduced(2);
  row2.removeFromLeft(80);
  _targetSelector.setBounds(row2);

  auto row3 = b.removeFromTop(30).reduced(2);
  row3.removeFromLeft(80);
  _ratioSlider.setBounds(row3);
}

void RelativeConstraint::buttonClicked(Button * b)
{
  // process parent events first
  if (b->getName() == "x") {
    ConstraintComponent::buttonClicked(b);
    return;
  }

  if (b->getName() == "source") {
    showDeviceSelectMenu(b, _source);
    updateButtonText();
  }
  else if (b->getName() == "target") {
    showDeviceSelectMenu(b, _target);
    updateButtonText();
  }
}

void RelativeConstraint::sliderValueChanged(Slider * s)
{
  // only one slider is attached to this so
  _ratio = s->getValue();
}

void RelativeConstraint::updateButtonText()
{
  _sourceSelector.setButtonText(deviceSetToString(_source));
  _targetSelector.setButtonText(deviceSetToString(_target));
}

SaturationConstraint::SaturationConstraint(int id, Component* parent) :
  ConstraintComponent(SATURATION_RANGE, id, parent)
{
  _deviceSelector.setName("select");
  _deviceSelector.setButtonText("[No Devices Selected]");
  _deviceSelector.addListener(this);
  addAndMakeVisible(_deviceSelector);

  _satSlider.setName("ratio");
  _satSlider.setRange(0, 1, 0.01);
  _satSlider.setMinAndMaxValues(0, 1);
  _satSlider.setSliderStyle(Slider::SliderStyle::TwoValueHorizontal);
  _satSlider.setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, false, 0, 0);
  _satSlider.setPopupDisplayEnabled(true, nullptr);
  addAndMakeVisible(_satSlider);
}

SaturationConstraint::~SaturationConstraint()
{
}

void SaturationConstraint::paint(Graphics & g)
{
  ConstraintComponent::paint(g);
  auto b = getLocalBounds();
  b.removeFromTop(60);

  auto row1 = b.removeFromTop(30).reduced(2);
  g.setColour(Colours::white);
  g.setFont(14);
  g.drawFittedText("Saturation", row1.removeFromLeft(80), Justification::centred, 1);
}

void SaturationConstraint::resized()
{
  ConstraintComponent::resized();
  auto b = getLocalBounds();
  b.removeFromTop(30);

  _deviceSelector.setBounds(b.removeFromTop(30).reduced(2));

  auto row1 = b.removeFromTop(30).reduced(2);
  row1.removeFromLeft(80);
  _satSlider.setBounds(row1);
}

void SaturationConstraint::buttonClicked(Button * b)
{
  // process parent events first
  if (b->getName() == "x") {
    ConstraintComponent::buttonClicked(b);
    return;
  }

  if (b->getName() == "select") {
    // bring up the selection menu and do the selection. Since this gets replicated
    // across devices, the function to do this is in the base class
    showDeviceSelectMenu(b, _affected);
    updateButtonText();
  }
}

void SaturationConstraint::sliderValueChanged(Slider * s)
{
  _min = s->getMinValue();
  _max = s->getMaxValue();
}

void SaturationConstraint::updateButtonText()
{
  _deviceSelector.setButtonText(deviceSetToString(_affected));
}

ConstraintContainer::ConstraintContainer() {
}

ConstraintContainer::~ConstraintContainer()
{
  deleteAllConstraints();
}

void ConstraintContainer::resized()
{
  // compute height
  setBounds(0, 0, getWidth(), _constraints.size() * 150);
  auto bounds = getLocalBounds();

  for (auto c : _constraints) {
    c->setBounds(bounds.removeFromTop(150));
  }
}

void ConstraintContainer::paint(Graphics & g)
{
}

void ConstraintContainer::addConstraint(ConstraintComponent * c)
{
  _constraints.add(c);
  addAndMakeVisible(c);
  resized();
}

void ConstraintContainer::deleteConstraint(int id)
{
  // find it
  int idx;
  for (idx = 0; idx < _constraints.size(); idx++) {
    if (id == _constraints[idx]->getId())
      break;
  }

  // delete it
  delete _constraints[idx];
  _constraints.remove(idx);

  resized();
}

void ConstraintContainer::deleteAllConstraints()
{
  for (auto c : _constraints) {
    delete c;
  }

  _constraints.clear();
  resized();
}

void ConstraintContainer::setWidth(int width)
{
  setBounds(0, 0, width, getHeight());
}

ConstraintData ConstraintContainer::getConstraintData()
{
  ConstraintData cd;
  cd._keyLightsAreExclusive = false;

  for (auto c : _constraints) {
    ConstraintType t = c->getType();

    // it could be a switch but I'm not a huge fan
    if (t == KEY) {
      // all key lights get placed into the same device set. If any of the
      // key light constraints are exclusive, the entire set is
      // please just use one key light constraint thanks
      KeyConstraint* kc = (KeyConstraint*)c;

      cd._keyLights = cd._keyLights.add(kc->_affected);
      cd._keyLightsAreExclusive |= kc->_exclusive;
    }
    else if (t == EXCLUDE) {
      // Exclude gets placed into two different sets depending on if they
      // are ignored or actively turned off
      ExcludeConstraint* ec = (ExcludeConstraint*)c;

      if (ec->_turnOff) {
        cd._excludeTurnOff = cd._excludeTurnOff.add(ec->_affected);
      }
      else {
        cd._excludeIgnore = cd._excludeIgnore.add(ec->_affected);
      }
    }
    else if (t == RELATIVE_BRIGHTNESS) {
      // just start appending stuff
      RelativeConstraint* rc = (RelativeConstraint*)c;

      cd._relativeSources.push_back(rc->_source);
      cd._relativeTargets.push_back(rc->_target);
      cd._relativeRatios.push_back(rc->_ratio);
    }
    else if (t == SATURATION_RANGE) {
      // also start appending things
      SaturationConstraint* sc = (SaturationConstraint*)c;

      cd._satTargets.push_back(sc->_affected);
      cd._satMin.push_back(sc->_min);
      cd._satMax.push_back(sc->_max);
    }
  }

  return cd;
}

ConstraintEditor::ConstraintEditor() : _id(0)
{
  _addButton.setName("add");
  _addButton.setButtonText("Add Constraint");
  _addButton.addListener(this);
  addAndMakeVisible(_addButton);

  _deleteAllButton.setName("deleteAll");
  _deleteAllButton.setButtonText("Delete All");
  _deleteAllButton.addListener(this);
  addAndMakeVisible(_deleteAllButton);
  
  _cc = new ConstraintContainer();
  _vp = new Viewport();

  _vp->setViewedComponent(_cc);
  addAndMakeVisible(_vp);
}

ConstraintEditor::~ConstraintEditor()
{
  deleteAllConstraints();
  delete _cc;
  delete _vp;
}

void ConstraintEditor::paint(Graphics & g)
{
}

void ConstraintEditor::resized()
{
  auto b = getLocalBounds();

  auto lower = b.removeFromBottom(30);
  _addButton.setBounds(lower.removeFromRight(120).reduced(2));
  _deleteAllButton.setBounds(lower.removeFromRight(120).reduced(2));

  _vp->setBounds(b);
  _cc->setWidth(getWidth() - _vp->getScrollBarThickness());
}

void ConstraintEditor::addConstraint(ConstraintType t)
{
  ConstraintComponent* c;

  switch (t) {
  case KEY:
    c = (ConstraintComponent*)(new KeyConstraint(_id, (Component*)this));
    break;
  case EXCLUDE:
    c = (ConstraintComponent*)(new ExcludeConstraint(_id, (Component*)this));
    break;
  case RELATIVE_BRIGHTNESS:
    c = (ConstraintComponent*)(new RelativeConstraint(_id, (Component*)this));
    break;
  case SATURATION_RANGE:
    c = (ConstraintComponent*)(new SaturationConstraint(_id, (Component*)this));
    break;
  default:
    break;
  }

  c->setBounds(0, 0, getLocalBounds().getWidth(), 150);
  _cc->addConstraint(c);

  _id++;
}

void ConstraintEditor::deleteConstraint(int id)
{
  _cc->deleteConstraint(id);
}

void ConstraintEditor::deleteAllConstraints()
{
  _cc->deleteAllConstraints();
}

ConstraintData ConstraintEditor::getConstraintData()
{
  // time to collect everything into a single structure to send to the samplers
  return _cc->getConstraintData();
}

void ConstraintEditor::buttonClicked(Button * b)
{
  if (b->getName() == "add") {
    // TODO: Use popup menu instead of hardcoded single type
    PopupMenu options;
    options.addItem(1, "Key Light");
    options.addItem(2, "Exclude Lights");
    options.addItem(3, "Relative Brightness");
    options.addItem(4, "Saturation Range");

    int res = options.showAt(b);

    if (res == 0)
      return;

    addConstraint((ConstraintType)(res - 1));
  }
  if (b->getName() == "deleteAll") {
    // no warning, yolo
    deleteAllConstraints();
  }
}
/*
  ==============================================================================

    ParamControls.cpp
    Created: 15 Dec 2015 5:07:12pm
    Author:  falindrith

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"
#include "ParamControls.h"
#include "MainComponent.h"
#include "hsvPicker.h"
#include <sstream>

//==============================================================================
// Properties
FloatPropertySlider::FloatPropertySlider(string id, string param, LumiverseFloat* val) :
  SliderPropertyComponent(param, val->getMin(), val->getMax(), 0.001)
{
  _id = id;
  _param = param;

  if (_param == "intensity") {
    slider.setValue(val->asPercent());
    slider.setRange(0, 100, 0.01);
    slider.setTextValueSuffix("%");
  }
  else {
    slider.setValue(val->getVal());
  }

  slider.setTextBoxIsEditable(false);
}

FloatPropertySlider::~FloatPropertySlider()
{
}

void FloatPropertySlider::paint(Graphics & g)
{
  LookAndFeel& lf = getLookAndFeel();

  if (isDeviceParamLocked(_id, _param)) {
    g.setColour(Colour(0xFFFF3838));
  }
  else {
    g.setColour(this->findColour(PropertyComponent::backgroundColourId));
  }

  g.fillRect(0, 0, getWidth(), getHeight() - 1);

  lf.drawPropertyComponentLabel(g, getWidth(), getHeight(), *this);
}

void FloatPropertySlider::mouseDown(const MouseEvent & event)
{
  if (event.mods.isRightButtonDown()) {
    if (isDeviceParamLocked(_id, _param)) {
      unlockDeviceParam(_id, _param);
    }
    else {
      lockDeviceParam(_id, _param);
    }

    getApplicationCommandManager()->invokeDirectly(command::REFRESH_PARAMS, false);
  }
}

void FloatPropertySlider::setValue(double newValue)
{
  if (_param == "intensity") {
    getRig()->getDevice(_id)->getParam<LumiverseFloat>(_param)->setValAsPercent((float) newValue / 100.0f);
  }
  else {
    getRig()->getDevice(_id)->setParam(_param, (float)newValue);
  }

  getGlobalSettings()->invalidateCache();
  getApplicationCommandManager()->invokeDirectly(command::REFRESH_ATTR, true);
}

double FloatPropertySlider::getValue() const
{
  LumiverseFloat* val = getRig()->getDevice(_id)->getParam<LumiverseFloat>(_param);

  if (_param == "intensity") {
    return val->asPercent() * 100;
  }
  else {
    return val->getVal();
  }
}

void FloatPropertySlider::sliderDragStarted(Slider * /* s */)
{
  stringstream ss;
  ss << _id << ":" << _param << " change start at " << getValue();
  getRecorder()->log(ACTION, ss.str());
}

void FloatPropertySlider::sliderDragEnded(Slider * /* s */)
{
  stringstream ss;
  ss << _id << ":" << _param << " value changed to " << getValue();
  getStatusBar()->setStatusMessage(ss.str());
  getRecorder()->log(ACTION, ss.str());
  getRig()->updateOnce();
}

OrientationPropertySlider::OrientationPropertySlider(string id, string param, LumiverseOrientation * val) :
  SliderPropertyComponent(param, val->getMin(), val->getMax(), 0.001)
{
  _id = id;
  _param = param;
  slider.setValue(val->getVal());
}

OrientationPropertySlider::~OrientationPropertySlider()
{
}

void OrientationPropertySlider::paint(Graphics & g)
{
  LookAndFeel& lf = getLookAndFeel();

  if (isDeviceParamLocked(_id, _param)) {
    g.setColour(Colour(0xFFFF3838));
  }
  else {
    g.setColour(this->findColour(PropertyComponent::backgroundColourId));
  }

  g.fillRect(0, 0, getWidth(), getHeight() - 1);

  lf.drawPropertyComponentLabel(g, getWidth(), getHeight(), *this);
}

void OrientationPropertySlider::mouseDown(const MouseEvent & event)
{
  if (event.mods.isRightButtonDown()) {
    if (isDeviceParamLocked(_id, _param)) {
      unlockDeviceParam(_id, _param);
    }
    else {
      lockDeviceParam(_id, _param);
    }

    getApplicationCommandManager()->invokeDirectly(command::REFRESH_PARAMS, false);
  }
}

void OrientationPropertySlider::setValue(double newValue)
{
  getRig()->getDevice(_id)->setParam(_param, (float)newValue);
  getGlobalSettings()->invalidateCache();
  getApplicationCommandManager()->invokeDirectly(command::REFRESH_ATTR, true);
}

double OrientationPropertySlider::getValue() const
{
  LumiverseOrientation* val = getRig()->getDevice(_id)->getParam<LumiverseOrientation>(_param);
  return val->getVal();
}

void OrientationPropertySlider::sliderDragStarted(Slider * /* s */)
{
  stringstream ss;
  ss << _id << ":" << _param << " change start at " << getValue();
  getRecorder()->log(ACTION, ss.str());
}

void OrientationPropertySlider::sliderDragEnded(Slider * /* s */)
{
  stringstream ss;
  ss << _id << ":" << _param << " value changed to " << getValue();
  getStatusBar()->setStatusMessage(ss.str());
  getRecorder()->log(ACTION, ss.str());
}

ColorPickerButton::ColorPickerButton(string id, string param, LumiverseColor * val) :
  _id(id), _param(param), _val(val)
{
	_button = new ColoredTextButton("Set Color");
	addAndMakeVisible(_button);
	_button->setTriggeredOnMouseDown(false);
	_button->addListener(this);

	Eigen::Vector3d c;
	c[0] = _val->getColorChannel("Red");
	c[1] = _val->getColorChannel("Green");
	c[2] = _val->getColorChannel("Blue");
	c *= 255;

	_button->setColor(Colour((uint8) c[0], (uint8) c[1], (uint8) c[2]));
}

ColorPickerButton::~ColorPickerButton()
{
	delete _button;
}

void ColorPickerButton::paint(Graphics & g)
{
  if (isDeviceParamLocked(_id, _param)) {
    g.setColour(Colour(0xFFFF3838));
  }
}

void ColorPickerButton::changeListenerCallback(ChangeBroadcaster * source)
{
  HSVPicker* cs = dynamic_cast<HSVPicker*>(source);
  if (!isDeviceParamLocked(_id, _param) && cs != nullptr) {
    Colour current = cs->getCurrentColour();
    _val->setColorChannel("Red", current.getFloatRed());
    _val->setColorChannel("Green", current.getFloatGreen());
    _val->setColorChannel("Blue", current.getFloatBlue());

    stringstream ss;
    ss << _id << ":" << _param << " value changed to " << _val->asString();
    getStatusBar()->setStatusMessage(ss.str());
    getRecorder()->log(ACTION, ss.str());

    getGlobalSettings()->invalidateCache();

    MainContentComponent* mc = dynamic_cast<MainContentComponent*>(getAppMainContentWindow()->getContentComponent());

    if (mc != nullptr) {
      mc->refreshParams();
      mc->refreshAttr();
    }

    getRig()->updateOnce();
  }
}

void ColorPickerButton::changeId(string newId)
{
  _id = newId;
  _val = getRig()->getDevice(_id)->getColor();

  refresh();
}

void ColorPickerButton::resized()
{
  _button->setBounds(getLocalBounds().reduced(2));
}

void ColorPickerButton::buttonClicked(Button* /* b */)
{
  Eigen::Vector3d c;
  c[0] = _val->getColorChannel("Red");
  c[1] = _val->getColorChannel("Green");
  c[2] = _val->getColorChannel("Blue");
  c *= 255;

  HSVPicker* cs = new HSVPicker();
  cs->setName(_id + " " + _param);
  cs->setCurrentColour(Colour((uint8)c[0], (uint8)c[1], (uint8)c[2]));
  cs->setSize(300, 400);
  cs->addChangeListener(this);
  CallOutBox::launchAsynchronously(cs, this->getScreenBounds(), nullptr);
}

void ColorPickerButton::refresh()
{
	Eigen::Vector3d c;
	c[0] = _val->getColorChannel("Red");
	c[1] = _val->getColorChannel("Green");
	c[2] = _val->getColorChannel("Blue");
	c *= 255;

	_button->setColor(Colour((uint8) c[0], (uint8) c[1], (uint8) c[2]));

	_button->repaint();
}

PaletteButton::PaletteButton(string id, vector<string> palettes) :
  _id(id), _d(getRig()->getDevice(id)), _palettes(palettes)
{
  setName(id + "_fp");
  setText();
  addListener(this);
}

void PaletteButton::buttonClicked(Button * b)
{
  PopupMenu p;
  
  int i = 1;
  for (auto s : _palettes) {
    p.addItem(i, s);
    i++;
  }

  int res = p.show();
  if (res > 0) {
    string selected = _palettes[res - 1];
    _d->setFocusPalette(selected);
    
    getRecorder()->log(ACTION, _id + " set to focus palette " + selected);
    setText();

    getApplicationCommandManager()->invokeDirectly(command::ARNOLD_RENDER, true);
  }
}

void PaletteButton::updatePalettes(string id, vector<string> palettes)
{
  _id = id;
  _d = getRig()->getDevice(id);
  _palettes = palettes;
  setText();
}

void PaletteButton::setText()
{
  FocusPalette* fp = _d->closestPalette();
  if (fp != nullptr) {
    setButtonText(fp->_name + " : (" + String(fp->_pan).toStdString() + "," + String(fp->_tilt).toStdString() + ")");
  }
  else {
    setButtonText("[No Focus Palette]");
  }
}


//==============================================================================
ParamControls::ParamControls() : _groupColor("Group Color", true)
{
  // In your constructor, you should add any child components, and
  // initialise any special settings that your component needs.

  addAndMakeVisible(_table);
  _table.setModel(this);

  _table.getHeader().addColumn("ID", 1, 100, 30, -1, TableHeaderComponent::ColumnPropertyFlags::notSortable);
  _table.getHeader().addColumn("Intensity", 2, 100, TableHeaderComponent::ColumnPropertyFlags::notSortable);
  _table.getHeader().addColumn("IL", 3, 20, TableHeaderComponent::ColumnPropertyFlags::notSortable | TableHeaderComponent::ColumnPropertyFlags::notResizable);
  _table.getHeader().addColumn("Color", 4, 100, TableHeaderComponent::ColumnPropertyFlags::notSortable);
  _table.getHeader().addColumn("CL", 5, 20, TableHeaderComponent::ColumnPropertyFlags::notSortable | TableHeaderComponent::ColumnPropertyFlags::notResizable);
  _table.getHeader().addColumn("Position", 6, 100, TableHeaderComponent::ColumnPropertyFlags::notSortable);
  _table.getHeader().addColumn("PL", 7, 20, TableHeaderComponent::ColumnPropertyFlags::notResizable | TableHeaderComponent::ColumnPropertyFlags::notResizable);

  _table.setMultipleSelectionEnabled(true);
  _table.setColour(ListBox::ColourIds::backgroundColourId, Colour(0xff333333));

  _groupIntens.addListener(this);
  _groupIntens.setName("Group Intensity");
  _groupIntens.setSliderStyle(Slider::SliderStyle::LinearBar);
  _groupIntens.setRange(0, 100, 0.01);
  _groupIntens.setColour(Slider::ColourIds::backgroundColourId, Colour(0xff929292));
  _groupIntens.setTextBoxIsEditable(false);
  addAndMakeVisible(_groupIntens);

  _groupColor.addListener(this);
  addAndMakeVisible(_groupColor);

  _qsAdd.addListener(this);
  _qsAdd.setName("qsAdd");
  _qsAdd.setButtonText("Add");
  addAndMakeVisible(_qsAdd);

  _qsSubtract.addListener(this);
  _qsSubtract.setName("qsSubtract");
  _qsSubtract.setButtonText("Subtract");
  addAndMakeVisible(_qsSubtract);

  _qsNone.addListener(this);
  _qsNone.setName("qsNone");
  _qsNone.setButtonText("None");
  addAndMakeVisible(_qsNone);

  _qsAll.addListener(this);
  _qsAll.setName("qsAll");
  _qsAll.setButtonText("All");
  addAndMakeVisible(_qsAll);

  _invert.addListener(this);
  _invert.setName("invert");
  _invert.setButtonText("Invert");
  addAndMakeVisible(_invert);

  _render.addListener(this);
  _render.setName("render");
  _render.setButtonText("Render");
  addAndMakeVisible(_render);

  _qsOnly.addListener(this);
  _qsOnly.setName("qsOnly");
  _qsOnly.setButtonText("Only");
  addAndMakeVisible(_qsOnly);
}

ParamControls::~ParamControls()
{
}

void ParamControls::paint (Graphics& g)
{
  auto b = getLocalBounds();

  auto top = b.removeFromTop(100);

  g.fillAll(Colour(0xff333333));

  g.setColour(Colours::white);
  g.drawFittedText("Selected: " + String(_selected.size()), top.removeFromTop(26).reduced(5), Justification::left, 2);

  g.drawFittedText("Intensity", top.removeFromTop(26).removeFromLeft(80).reduced(5), Justification::left, 1);

  g.drawFittedText("Color", top.removeFromTop(26).removeFromLeft(80).reduced(5), Justification::left, 1);

  auto bot = b.removeFromBottom(26);
  g.drawFittedText("Quick Select", bot.removeFromLeft(80).reduced(5), Justification::centredLeft, 1);
}

void ParamControls::resized()
{
  auto b = getLocalBounds();

  b.removeFromTop(26);
  auto intens = b.removeFromTop(26);
  intens.removeFromLeft(80);
  _groupIntens.setBounds(intens.reduced(2));

  auto color = b.removeFromTop(26);
  color.removeFromLeft(80);
  _groupColor.setBounds(color.reduced(2));

  auto bot = b.removeFromBottom(26);
  bot.removeFromLeft(80);

  _qsOnly.setBounds(bot.removeFromLeft(60).reduced(2));
  _qsAdd.setBounds(bot.removeFromLeft(60).reduced(2));
  _qsSubtract.setBounds(bot.removeFromLeft(60).reduced(2));
  _qsAll.setBounds(bot.removeFromLeft(60).reduced(2));
  _qsNone.setBounds(bot.removeFromLeft(60).reduced(2));
  _invert.setBounds(bot.removeFromLeft(60).reduced(2));
  _render.setBounds(bot.removeFromRight(60).reduced(2));

  _table.setBounds(b);
}

void ParamControls::initProperties()
{
  _ids.clear();
  Rig* rig = getRig();
  auto devices = rig->getAllDevices();

  for (const auto d : devices.getDevices()) {
    _ids.add(d->getId());
  }

  _ids.sort(true);
}

void ParamControls::refreshParams() {
  _table.updateContent();
  _table.repaint();
  repaint();
}

int ParamControls::getNumRows()
{
  return _ids.size();
}

void ParamControls::paintRowBackground(Graphics & g, int /* rowNumber */, int /* width */, int /* height */, bool rowIsSelected)
{
  if (rowIsSelected)
    g.fillAll(Colours::lightblue);
  else
    g.fillAll(Colour(0xff333333));
}

void ParamControls::paintCell(Graphics & g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
  if (rowIsSelected) {
    g.setColour(Colours::black);
  }
  else {
    g.setColour(Colours::white);
  }

  g.setFont(14);

  if (columnId == 1) {
    String text(_ids[rowNumber]);
    g.drawText(text, 2, 0, width - 4, height, Justification::centredLeft, true);
  }
  else if (columnId == 2) {
    g.setColour(Colour(0xff929292));
    g.fillRect(0, 0, width, height);
  }
  else if (columnId == 3) {
    if (isDeviceParamLocked(_ids[rowNumber].toStdString(), "intensity")) {
      g.setColour(Colours::white);
      g.fillRect(2, 2, width - 2, height - 2);
    }
    g.setColour(Colours::white);
    g.drawRect(2, 2, width - 2, height - 2, 1);
  }
  else if (columnId == 5) {
    if (isDeviceParamLocked(_ids[rowNumber].toStdString(), "color")) {
      g.setColour(Colours::white);
      g.fillRect(2, 2, width - 2, height - 2);
    }
    g.setColour(Colours::white);
    g.drawRect(2, 2, width - 2, height - 2, 1);
  }
  else if (columnId == 7) {
    // same as other locks, but for position param
    if (isDeviceParamLocked(_ids[rowNumber].toStdString(), "pan")) {
      g.setColour(Colours::white);
      g.fillRect(2, 2, width - 2, height - 2);
    }
    g.setColour(Colours::white);
    g.drawRect(2, 2, width - 2, height - 2, 1);
  }
}

Component * ParamControls::refreshComponentForCell(int rowNumber, int columnId, bool /* isRowSelected */, Component * existingComponentToUpdate)
{
  if (columnId == 2) {
    Slider* intensSlider = static_cast<Slider*>(existingComponentToUpdate);

    if (intensSlider == nullptr) {
      intensSlider = new Slider();
      intensSlider->setRange(0, 100, 0.01);
      intensSlider->setSliderStyle(Slider::SliderStyle::LinearBar);
      intensSlider->setColour(Slider::ColourIds::backgroundColourId, Colour(0xa0929292));
      intensSlider->setTextBoxIsEditable(false);
      intensSlider->addListener(this);
    }

    intensSlider->setName(_ids[rowNumber]);
    intensSlider->setValue(getRig()->getDevice(_ids[rowNumber].toStdString())->getIntensity()->asPercent() * 100, dontSendNotification);
    return intensSlider;
  }
  if (columnId == 4) {
    ColorPickerButton* button = static_cast<ColorPickerButton*>(existingComponentToUpdate);

    string id = _ids[rowNumber].toStdString();
    LumiverseColor* targetVal = getRig()->getDevice(id)->getColor();

    if (targetVal == nullptr) {
      if (existingComponentToUpdate != nullptr)
        delete existingComponentToUpdate;

      return nullptr;
    }

    if (button == nullptr) {
      button = new ColorPickerButton(id, "color", targetVal);
    }

    button->changeId(id);
    return button;
  }
  if (columnId == 6) {
    PaletteButton* button = static_cast<PaletteButton*>(existingComponentToUpdate);

    string id = _ids[rowNumber].toStdString();
    auto fps = getRig()->getDevice(id)->getFocusPaletteNames();

    if (fps.size() == 0) {
      if (existingComponentToUpdate != nullptr)
        delete existingComponentToUpdate;

      return nullptr;
    }

    if (button == nullptr) {
      button = new PaletteButton(id, fps);
    }

    button->updatePalettes(id, fps);
    return button;
  }

  return nullptr;
}

void ParamControls::selectedRowsChanged(int lastRowSelected)
{
  _selected.clear();

  for (int i = 0; i < _table.getNumSelectedRows(); i++) {
    _selected.add(_ids[_table.getSelectedRow(i)]);
  }

  if (lastRowSelected != -1) {
    // pull most recently selected device's intensity and color
    string recentId = _ids[lastRowSelected].toStdString();
    _recentIntens = getRig()->getDevice(recentId)->getIntensity()->asPercent();

    LumiverseColor* recentColor = getRig()->getDevice(recentId)->getColor();
    if (recentColor != nullptr) {
      Eigen::Vector3d c;
      c[0] = recentColor->getColorChannel("Red");
      c[1] = recentColor->getColorChannel("Green");
      c[2] = recentColor->getColorChannel("Blue");
      c *= 255;
      _recentColor = Colour((uint8)c[0], (uint8)c[1], (uint8)c[2]);
    }
    else {
      _recentColor = Colours::black;
    }

    _groupColor.setColor(_recentColor);
    _groupIntens.setValue(_recentIntens * 100, dontSendNotification);
  }

  getRecorder()->log(ACTION, "Selected rows changed");

  repaint();
}

void ParamControls::cellClicked(int rowNumber, int columnId, const MouseEvent & /*e*/)
{
  if (columnId == 3) {
    toggleDeviceParamLock(_ids[rowNumber].toStdString(), "intensity");
    _table.updateContent();
    repaint();
  }
  else if (columnId == 5) {
    toggleDeviceParamLock(_ids[rowNumber].toStdString(), "color");
    _table.updateContent();
    repaint();
  }
  else if (columnId == 7) {
    // lock the position things
    toggleDeviceParamLock(_ids[rowNumber].toStdString(), "pan");
    _table.updateContent();
    repaint();
  }
}

void ParamControls::sliderValueChanged(Slider * s)
{
  if (s->getName() == "Group Intensity") {
    float val = (float)s->getValue() / 100.0f;
    for (auto& id : _selected) {
      getRig()->getDevice(id.toStdString())->getIntensity()->setValAsPercent(val);
    }

    refreshParams();
  }
  else {
    getRig()->getDevice(s->getName().toStdString())->getIntensity()->setValAsPercent((float)s->getValue() / 100.f);
  }

  stringstream ss;
  ss << "Slider " << s->getName() << " value changed to " << s->getValue();
  getRecorder()->log(actionType::ACTION, ss.str());

  getGlobalSettings()->invalidateCache();
  getApplicationCommandManager()->invokeDirectly(command::REFRESH_ATTR, true);
  getRig()->updateOnce();
}

void ParamControls::buttonClicked(Button * b)
{
  if (b->getName() == "Group Color") {
    HSVPicker* cs = new HSVPicker();
    cs->setName("Group Color");
    cs->setCurrentColour(_recentColor);
    cs->setSize(300, 400);
    cs->addChangeListener(this);
    CallOutBox::launchAsynchronously(cs, _groupColor.getScreenBounds(), nullptr);
  }
  else if (b->getName() == "qsNone") {
    _table.deselectAllRows();
    _table.updateContent();
    _table.repaint();
    getRecorder()->log(ACTION, "All devices deselected");
  }
  else if (b->getName() == "qsAll") {
    _table.deselectAllRows();
    _table.selectRangeOfRows(0, _ids.size() - 1);
    _table.updateContent();
    _table.repaint();
    getRecorder()->log(ACTION, "All devices selected");
  }
  else if (b->getName() == "invert") {
    for (int i = 0; i < _table.getNumRows(); i++) {
      if (_table.isRowSelected(i))
        _table.deselectRow(i);
      else
        _table.selectRow(i, false, false);
    }
    getRecorder()->log(ACTION, "Selection inverted");
  }
  else if (b->getName() == "qsOnly") {
    map<int, string> cmdMap;
    PopupMenu all = getSelectorMenu(cmdMap);

    int result = all.showAt(&_qsOnly);

    if (result == 0)
      return;

    _table.deselectAllRows();
    auto selected = getRig()->select(cmdMap[result]);
    set<Device*> devices = selected.getDevices();
    for (auto d : devices) {
      _table.selectRow(_ids.indexOf(String(d->getId())), true, false);
    }

    _table.updateContent();
    _table.repaint();
    
    getRecorder()->log(ACTION, "Selected Devices: " + selected.info());
  }
  else if (b->getName() == "qsAdd") {
    map<int, string> cmdMap;
    PopupMenu all = getSelectorMenu(cmdMap);

    int result = all.showAt(&_qsAdd);

    if (result == 0)
      return;

    auto selected = getRig()->select(cmdMap[result]);
    set<Device*> devices = selected.getDevices();
    for (auto d : devices) {
      _table.selectRow(_ids.indexOf(String(d->getId())), true, false);
    }

    _table.updateContent();
    _table.repaint();

    getRecorder()->log(ACTION, "Added Devices: " + selected.info());
  }
  else if (b->getName() == "qsSubtract") {
    map<int, string> cmdMap;
    PopupMenu all = getSelectorMenu(cmdMap);

    int result = all.showAt(&_qsSubtract);

    if (result == 0)
      return;

    auto selected = getRig()->select(cmdMap[result]);
    set<Device*> devices = selected.getDevices();
    for (auto d : devices) {
      _table.deselectRow(_ids.indexOf(String(d->getId())));
    }

    _table.updateContent();
    _table.repaint();

    getRecorder()->log(ACTION, "Removed Devices: " + selected.info());
  }
  else if (b->getName() == "render") {
    getApplicationCommandManager()->invokeDirectly(ARNOLD_RENDER, true);
  }
}

void ParamControls::changeListenerCallback(ChangeBroadcaster * source)
{
  HSVPicker* cs = dynamic_cast<HSVPicker*>(source);
  if (cs != nullptr) {
    _recentColor = cs->getCurrentColour();

    for (auto& id : _selected) {
      LumiverseColor* val = getRig()->getDevice(id.toStdString())->getColor();

      if (val != nullptr) {
        val->setColorChannel("Red", _recentColor.getFloatRed());
        val->setColorChannel("Green", _recentColor.getFloatGreen());
        val->setColorChannel("Blue", _recentColor.getFloatBlue());
      }
    }

    _groupColor.setColor(_recentColor);
    getGlobalSettings()->invalidateCache();

    getRecorder()->log(ACTION, "Group color set to " + _recentColor.toString().toStdString());

    MainContentComponent* mc = dynamic_cast<MainContentComponent*>(getAppMainContentWindow()->getContentComponent());

    if (mc != nullptr) {
      mc->refreshParams();
      mc->refreshAttr();
    }
  }
}

void ParamControls::lockSelected(vector<string> params)
{
  for (auto& p : params) {
    for (auto& id : _selected) {
      lockDeviceParam(id.toStdString(), p);
    }
  }
  
  _table.repaint();
}

void ParamControls::unlockSelected(vector<string> params) {
  for (auto& p : params) {
    for (auto& id : _selected) {
      unlockDeviceParam(id.toStdString(), p);
    }
  }
  
  _table.repaint();
}

StringArray ParamControls::getSelectedIds()
{
  return _selected;
}

void ParamControls::setSelectedIds(DeviceSet selection)
{
  _table.deselectAllRows();
  set<Device*> devices = selection.getDevices();
  for (auto d : devices) {
    _table.selectRow(_ids.indexOf(String(d->getId())), true, false);
  }

  _table.updateContent();
  _table.repaint();
}

PopupMenu ParamControls::getSelectorMenu(map<int, string>& cmdOut)
{
  PopupMenu all;

  // get all areas
  auto areaSet = getRig()->getMetadataValues("area");
  int i = 1;

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


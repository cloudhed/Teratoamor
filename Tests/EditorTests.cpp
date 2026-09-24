#include "PluginEditor.h"
#include "ui/DraggableValueLabel.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>
#include <set>
#include <windows.h>

namespace
{
void require (bool condition, const juce::String& message)
{
    if (! condition) throw std::runtime_error (message.toStdString());
}
void pump (int milliseconds = 90)
{
    const auto end = juce::Time::getMillisecondCounter() + juce::uint32 (milliseconds);
    do
    {
        MSG message;
        while (PeekMessage (&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage (&message);
            DispatchMessage (&message);
        }
        juce::Thread::sleep (2);
    } while (juce::Time::getMillisecondCounter() < end);
}
template <typename T> T* find (juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        if (auto* result = dynamic_cast<T*> (&c)) return result;
    for (auto* child : c.getChildren())
        if (auto* result = find<T> (*child, id)) return result;
    return nullptr;
}
void checkBounds (juce::Component& c)
{
    for (auto* child : c.getChildren())
    {
        if (! child->isVisible()) continue;
        require (child->getWidth() > 0 && child->getHeight() > 0, "Empty control: " + child->getName());
        require (c.getLocalBounds().expanded (2).contains (child->getBoundsInParent()), "Out of bounds: " + child->getName());
        if (dynamic_cast<juce::TextButton*> (child))
            require (child->getHeight() >= 24, "Unreadably short button: " + child->getComponentID());
        if (auto* slider = dynamic_cast<juce::Slider*> (child))
            if (slider->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
                require (slider->getHeight() >= 58, "Too little space for knob: " + slider->getComponentID());
        checkBounds (*child);
    }
}
bool visibleInSnapshot (const juce::Component& c)
{
    // isShowing() additionally requires a desktop peer. These renders deliberately
    // have no window, so check visibility through the complete parent chain.
    for (auto* ancestor = &c; ancestor != nullptr; ancestor = ancestor->getParentComponent())
        if (! ancestor->isVisible()) return false;
    return true;
}

void checkValueDragging (juce::Slider& slider)
{
    auto* label = find<teratoamor::ui::DraggableValueLabel> (slider, "draggableValue");
    require (label != nullptr, "Missing draggable value: " + slider.getComponentID());
    if (! slider.isEnabled()) return; // Linked Element envelopes are covered separately.
    const double original = slider.getValue();
    struct Gestures final : juce::Slider::Listener
    {
        void sliderValueChanged (juce::Slider*) override {}
        void sliderDragStarted (juce::Slider*) override { ++starts; }
        void sliderDragEnded (juce::Slider*) override { ++ends; }
        int starts = 0, ends = 0;
    } gestures;
    slider.addListener (&gestures);
    const auto event = [label] (float y, bool fine = false, int clicks = 1)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 10, y },
                                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                    | (fine ? juce::ModifierKeys::shiftModifier : 0)),
                                1, 0, 0, 0, 0, label, label, juce::Time::getCurrentTime(),
                                { 10, 10 }, juce::Time::getCurrentTime(), clicks, y != 10);
    };
    slider.setValue (slider.proportionOfLengthToValue (0.5), juce::sendNotificationSync);
    const auto middle = slider.getValue();
    label->mouseDown (event (10));
    label->mouseDrag (event (-30));
    const auto coarse = slider.getValue();
    label->mouseUp (event (-30));
    require (coarse > middle, "Value did not increase on upward drag");
    require (! label->isBeingEdited(), "Drag opened the text editor");
    require (gestures.starts == 1 && gestures.ends == 1, "Drag gesture was not balanced");

    slider.setValue (middle, juce::sendNotificationSync);
    label->mouseDown (event (10, true));
    label->mouseDrag (event (-30, true));
    label->mouseUp (event (-30, true));
    require (slider.getValue() >= middle && slider.getValue() < coarse, "Shift did not reduce sensitivity");

    label->mouseDown (event (10));
    label->mouseDrag (event (10000));
    require (slider.getValue() == slider.getMinimum(), "Downward drag did not clamp to minimum");
    label->mouseDrag (event (-10000));
    label->mouseUp (event (-10000));
    require (slider.getValue() == slider.getMaximum(), "Upward drag did not clamp to maximum");

    // First click must wait, allowing a second click to reset rather than edit.
    label->mouseDown (event (10)); label->mouseUp (event (10));
    require (! label->isBeingEdited(), "Single click did not allow time for double-click");
    label->mouseDown (event (10, false, 2));
    label->mouseDoubleClick (event (10, false, 2));
    label->mouseUp (event (10, false, 2));
    require (std::abs (slider.getValue() - slider.getDoubleClickReturnValue()) < 0.001, "Double-click did not reset default");
    require (gestures.starts == gestures.ends, "Unbalanced value-field gestures");
    slider.removeListener (&gestures);
    slider.setValue (original, juce::sendNotificationSync);
}

void save (juce::Component& c, const juce::File& directory, const juce::String& name)
{
    pump(); checkBounds (c);
    auto image = c.createComponentSnapshot (c.getLocalBounds());
    auto stream = directory.getChildFile (name + ".png").createOutputStream();
    require (stream != nullptr, "Cannot write screenshot");
    stream->setPosition (0); stream->truncate();
    require (juce::PNGImageFormat().writeImageToStream (image, *stream), "PNG write failed");
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto output = argc > 1 ? juce::File (juce::String (argv[1]))
                                     : juce::File::getCurrentWorkingDirectory().getChildFile ("gui-previews");
        require (output.createDirectory().wasOk(), "Cannot create preview directory");
        TeratoamorAudioProcessor processor;
        TeratoamorAudioProcessorEditor editor (processor);
        editor.setVisible (true);
        // Every implemented parameter must have an attached control, including hidden pages.
        for (auto* base : processor.getParameters())
        {
            auto* p = dynamic_cast<juce::RangedAudioParameter*> (base);
            require (p != nullptr, "Unexpected parameter type");
            auto* slider = find<juce::Slider> (editor, p->paramID);
            auto* choice = find<juce::ComboBox> (editor, p->paramID);
            auto* toggle = find<juce::ToggleButton> (editor, p->paramID);
            require (slider || choice || toggle, "Missing control: " + p->paramID);
            const float before = p->getValue();
            p->setValueNotifyingHost (0.37f); pump();
            if (slider)
            {
                require (std::abs (slider->getValue() - p->convertFrom0to1 (p->getValue())) < 0.11f,
                         "Automation did not reach slider: " + p->paramID);
                slider->setValue (p->convertFrom0to1 (0.73f), juce::sendNotificationSync);
                require (std::abs (p->convertFrom0to1 (p->getValue()) - slider->getValue()) < 0.11f,
                         "Slider did not reach parameter: " + p->paramID);
            }
            if (choice)
            {
                const bool modTarget = p->paramID.startsWith ("mod") && p->paramID.endsWith ("_target");
                const int parameterIndex = juce::roundToInt (p->convertFrom0to1 (p->getValue()));
                require (modTarget ? choice->getSelectedId() == parameterIndex + 1
                                   : choice->getSelectedItemIndex() == parameterIndex,
                         "Choice automation failed: " + p->paramID);
                choice->setSelectedItemIndex (choice->getNumItems() - 1, juce::sendNotificationSync);
                require (juce::roundToInt (p->convertFrom0to1 (p->getValue()))
                             == (modTarget ? choice->getSelectedId() - 1 : choice->getSelectedItemIndex()),
                         "Choice edit failed: " + p->paramID);
            }
            if (toggle)
            {
                require (toggle->getToggleState() == (p->getValue() >= 0.5f), "Toggle automation failed");
                toggle->setToggleState (! toggle->getToggleState(), juce::sendNotificationSync);
                require (toggle->getToggleState() == (p->getValue() >= 0.5f), "Toggle edit failed");
            }
            p->setValueNotifyingHost (before);
        }
        pump();
        for (int section = 0; section < ParamIDs::numMods; ++section)
        {
            auto* target = find<juce::ComboBox> (editor, ParamIDs::mod (section, "target"));
            require (target->getItemText (0) == "Element1 Volume"
                     && target->getItemText (6) == "Element2 Volume"
                     && target->getItemText (11) == "Element3 Volume",
                     "Element Volume does not lead each target group");
        }
        auto* tempoSlider = find<juce::Slider> (editor, ParamIDs::tempoBpm);
        auto* masterSlider = find<juce::Slider> (editor, ParamIDs::masterLevel);
        auto* hostSync = find<juce::ToggleButton> (editor, ParamIDs::tempoSync);
        require (masterSlider->getTextFromValue (50.0) == juce::String::fromUTF8 (u8"\u22126.0")
                 && masterSlider->getTextFromValue (100.0) == "0.0"
                 && masterSlider->getTextFromValue (0.0) == juce::String::fromUTF8 (u8"\u2212\u221e"),
                 "Master readout does not match the linear output gain");
        const auto asciiGain = masterSlider->getValueFromText ("-6.0");
        const auto unicodeInput = juce::String::fromUTF8 (u8"\u22126.0");
        const auto unicodeGain = masterSlider->getValueFromText (unicodeInput);
        require (std::abs (asciiGain - 50.12) < 0.1 && std::abs (unicodeGain - 50.12) < 0.1,
                 "Typed master dB did not convert back to gain: ASCII " + juce::String (asciiGain)
                     + ", Unicode " + juce::String (unicodeGain));
        require (hostSync->getToggleState() && ! tempoSlider->isEnabled(),
                 "Host sync should disable manual tempo dragging");
        hostSync->setToggleState (false, juce::sendNotificationSync); pump();
        require (tempoSlider->isEnabled(), "Manual tempo did not become editable");
        const auto originalMaster = masterSlider->getValue();
        masterSlider->setValue (50.0, juce::sendNotificationSync);
        save (editor, output, "header-manual-minus-six");
        masterSlider->setValue (originalMaster, juce::sendNotificationSync);
        hostSync->setToggleState (true, juce::sendNotificationSync); pump();
        require (! tempoSlider->isEnabled(), "Host tempo knob remained editable");
        for (auto* base : processor.getParameters())
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
                if (auto* slider = find<juce::Slider> (editor, p->paramID)) checkValueDragging (*slider);

        // Real single-click editing and commit/cancel through JUCE's normal parser.
        auto* panSlider = find<juce::Slider> (editor, ParamIDs::pan (0));
        auto* panLabel = find<teratoamor::ui::DraggableValueLabel> (*panSlider, "draggableValue");
        auto click = juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 10, 10 },
                                      juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0,
                                      panLabel, panLabel, juce::Time::getCurrentTime(), { 10, 10 },
                                      juce::Time::getCurrentTime(), 1, false);
        panLabel->mouseDown (click); panLabel->mouseUp (click);
        pump (juce::MouseEvent::getDoubleClickTimeout() + 80);
        require (panLabel->isBeingEdited(), "Single click did not open text editing");
        panLabel->getCurrentTextEditor()->setText ("-25.0");
        panLabel->hideEditor (false);
        require (panSlider->getValue() == -25.0, "Typed negative value did not commit");
        require (processor.apvts.getRawParameterValue (ParamIDs::pan (0))->load() == -25.0f, "Typed value missed the parameter");
        panLabel->showEditor(); panLabel->getCurrentTextEditor()->setText ("50"); panLabel->hideEditor (true);
        require (panSlider->getValue() == -25.0, "Cancelled text edit changed the value");
        panSlider->setValue (0, juce::sendNotificationSync);

        require (! find<juce::Slider> (editor, ParamIDs::attack (1))->isEnabled(), "Linked envelope must be disabled");
        processor.apvts.getParameter (ParamIDs::link (1))->setValueNotifyingHost (0); pump();
        require (find<juce::Slider> (editor, ParamIDs::attack (1))->isEnabled(), "Unlinked envelope must be enabled");
        processor.apvts.getParameter (ParamIDs::link (1))->setValueNotifyingHost (1); pump();

        juce::MemoryBlock initialState;
        processor.getStateInformation (initialState);
        for (const int width : { 1200, 1440, 2400 })
        {
            editor.setSize (width, width * 5 / 8);
            save (editor, output, "elements-" + juce::String (width));
            for (auto* name : { "GLOBAL", "MODULATION (ENV)", "MODULATION (OSC)", "SPACE" })
            {
                find<juce::TextButton> (editor, name)->onClick();
                // All three relevant slots must be visible together, with the
                // other group hidden. Envelope stages use vertical sliders.
                for (int m = 0; m < 6; ++m)
                {
                    const bool env = juce::String (name) == "MODULATION (ENV)";
                    const bool osc = juce::String (name) == "MODULATION (OSC)";
                    auto* depth = find<juce::Slider> (editor, ParamIDs::mod (m, "depth"));
                    require (visibleInSnapshot (*depth) == (m < 3 ? env : osc), "Wrong modulation group visibility");
                    if (osc && m >= 3)
                    {
                        const auto targetWidth = find<juce::ComboBox> (editor, ParamIDs::mod (m, "target"))->getWidth();
                        require (targetWidth == find<juce::ComboBox> (editor, ParamIDs::mod (m, "wave"))->getWidth()
                                 && targetWidth == find<juce::ComboBox> (editor, ParamIDs::mod (m, "rate"))->getWidth(),
                                 "Destination dropdown is wider than Wave and Rate");
                    }
                    if (m < 3)
                        for (auto* stage : { "attack", "decay", "sustain", "release", "time" })
                            require (find<juce::Slider> (editor, ParamIDs::mod (m, stage))->getSliderStyle()
                                     == juce::Slider::LinearVertical, "Envelope stage must be vertical");
                }
                if (juce::String (name) == "SPACE")
                {
                    auto* first = find<juce::ComboBox> (editor, ParamIDs::delayRate (0));
                    auto* second = find<juce::ComboBox> (editor, ParamIDs::delayRate (1));
                    const auto top = editor.getLocalArea (first, first->getLocalBounds());
                    const auto bottom = editor.getLocalArea (second, second->getLocalBounds());
                    require (top.getX() == bottom.getX() && top.getBottom() < bottom.getY(), "Delay rows must stack vertically");
                }
                save (editor, output, juce::String (name).toLowerCase() + "-" + juce::String (width));
            }
        }
        editor.setSize (1440, 900);
        juce::MemoryBlock afterTabs;
        processor.getStateInformation (afterTabs);
        require (initialState == afterTabs, "Navigation changed plugin state");
        // A sounding block must drive the processor's existing output meter feed.
        processor.prepareToPlay (48000, 512);
        struct FixedPlayHead final : juce::AudioPlayHead
        {
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo position;
                position.setBpm (137.0);
                return position;
            }
        } hostPlayHead;
        processor.setPlayHead (&hostPlayHead);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        const auto activityBeforeNote = processor.midiActivityCounter.load();
        processor.processBlock (buffer, midi);
        require (processor.outputPeakLeft.load() > 0, "No output meter signal");
        require (processor.activeVoices.load() > 0, "No active MIDI voice count");
        require (processor.midiActivityCounter.load() == activityBeforeNote + 1, "MIDI activity light missed a note");
        require (processor.getCurrentBpm() == 137.0, "Host tempo was not published to the editor");
        std::array<float, TeratoamorAudioProcessor::spectrumSize> spectrumSamples {};
        processor.copySpectrumSamples (spectrumSamples);
        require (std::any_of (spectrumSamples.begin(), spectrumSamples.end(), [] (float value) { return std::abs (value) > 0.0f; }),
                 "No output spectrum signal");
        const float peak = processor.outputPeakLeft.load();
        midi.addEvent (juce::MidiMessage::allSoundOff (1), 0);
        for (int i = 0; i < 10; ++i) processor.processBlock (buffer, midi);
        require (processor.midiActivityCounter.load() == activityBeforeNote + 2, "MIDI activity light missed a command");
        require (processor.outputPeakLeft.load() >= peak, "Meter lost a transient before the GUI consumed it");
        find<juce::TextButton> (editor, "GLOBAL")->onClick();
        save (editor, output, "global-playing");
        require (processor.outputPeakLeft.load() == 0, "Meter did not consume its peak");
        pump (120);
        require (find<teratoamor::ui::DraggableValueLabel> (*tempoSlider, "draggableValue")->getText() == "137",
                 "Synced BPM readout did not follow the host");
        auto* bpmParameter = processor.apvts.getParameter (ParamIDs::tempoBpm);
        bpmParameter->setValueNotifyingHost (bpmParameter->convertTo0to1 (150.0f));
        hostSync->setToggleState (false, juce::sendNotificationSync);
        processor.processBlock (buffer, midi); pump (120);
        require (processor.getCurrentBpm() == 150.0 && tempoSlider->isEnabled()
                 && find<teratoamor::ui::DraggableValueLabel> (*tempoSlider, "draggableValue")->getText() == "150",
                 "Manual BPM did not replace the host tempo");
        hostSync->setToggleState (true, juce::sendNotificationSync);
        processor.setPlayHead (nullptr);
        processor.processBlock (buffer, midi); pump (120);
        require (processor.getCurrentBpm() == 150.0 && ! tempoSlider->isEnabled(),
                 "Saved manual BPM was not used when the host tempo disappeared");
        processor.releaseResources();
        processor.apvts.getParameter (ParamIDs::width (0))->setValueNotifyingHost (0.1f);
        processor.setStateInformation (initialState.getData(), int (initialState.getSize())); pump();
        require (std::abs (find<juce::Slider> (editor, ParamIDs::width (0))->getValue() - 90) < 0.1,
                 "Restored state did not reach editor");
        const auto presetFile = output.getChildFile ("editor-preset-test.teratoamor");
        require (processor.savePresetToFile (presetFile), "Preset file was not saved");
        auto* master = processor.apvts.getParameter (ParamIDs::masterLevel);
        master->setValueNotifyingHost (master->convertTo0to1 (22.0f));
        require (processor.loadPresetFromFile (presetFile), "Preset file was not loaded");
        require (std::abs (processor.apvts.getRawParameterValue (ParamIDs::masterLevel)->load() - 70.0f) < 0.1f,
                 "Loaded preset did not restore Master volume");
        require (processor.getPresetName() == "editor-preset-test", "Loaded preset name was not shown");
        master->setValueNotifyingHost (master->convertTo0to1 (22.0f));
        processor.resetToInitialState();
        require (std::abs (processor.apvts.getRawParameterValue (ParamIDs::masterLevel)->load() - 70.0f) < 0.1f
                 && processor.getPresetName() == "Initial State", "INIT did not restore parameter defaults");
        std::cout << "Editor checks passed: parameter coverage, two-way attachments, links, state, tabs, bounds and snapshots.\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

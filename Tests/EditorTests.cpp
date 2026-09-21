#include "PluginEditor.h"
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
void pump()
{
    const auto end = juce::Time::getMillisecondCounter() + 90;
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
                require (choice->getSelectedItemIndex() == juce::roundToInt (p->convertFrom0to1 (p->getValue())), "Choice automation failed");
                choice->setSelectedItemIndex (choice->getNumItems() - 1, juce::sendNotificationSync);
                require (juce::roundToInt (p->convertFrom0to1 (p->getValue())) == choice->getSelectedItemIndex(), "Choice edit failed");
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
        require (! find<juce::Slider> (editor, ParamIDs::attack (1))->isEnabled(), "Linked envelope must be disabled");
        processor.apvts.getParameter (ParamIDs::link (1))->setValueNotifyingHost (0); pump();
        require (find<juce::Slider> (editor, ParamIDs::attack (1))->isEnabled(), "Unlinked envelope must be enabled");
        processor.apvts.getParameter (ParamIDs::link (1))->setValueNotifyingHost (1); pump();

        juce::MemoryBlock initialState;
        processor.getStateInformation (initialState);
        for (const int width : { 1200, 1440, 2400 })
        {
            editor.setSize (width, width * 5 / 8);
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
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        processor.processBlock (buffer, midi);
        require (processor.outputPeakLeft.load() > 0, "No output meter signal");
        const float peak = processor.outputPeakLeft.load();
        midi.addEvent (juce::MidiMessage::allSoundOff (1), 0);
        for (int i = 0; i < 10; ++i) processor.processBlock (buffer, midi);
        require (processor.outputPeakLeft.load() >= peak, "Meter lost a transient before the GUI consumed it");
        find<juce::TextButton> (editor, "GLOBAL")->onClick();
        save (editor, output, "global-playing");
        require (processor.outputPeakLeft.load() == 0, "Meter did not consume its peak");
        processor.releaseResources();
        processor.apvts.getParameter (ParamIDs::width (0))->setValueNotifyingHost (0.1f);
        processor.setStateInformation (initialState.getData(), int (initialState.getSize())); pump();
        require (std::abs (find<juce::Slider> (editor, ParamIDs::width (0))->getValue() - 90) < 0.1,
                 "Restored state did not reach editor");
        std::cout << "Editor checks passed: parameter coverage, two-way attachments, links, state, tabs, bounds and snapshots.\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace teratoamor::ui
{
// JUCE still owns text parsing/editing and the slider's parameter attachment.
// Only the mouse interaction on its numeric label is specialised here.
class DraggableValueLabel final : public juce::Label, private juce::Timer
{
public:
    explicit DraggableValueLabel (juce::Slider& owner) : slider (owner)
    {
        setComponentID ("draggableValue");
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        stopTimer();
        gesture.reset();
        pressed = slider.isEnabled() && e.mods.isLeftButtonDown() && ! isBeingEdited();
        dragged = false;
        lastY = e.position.y;
        proportion = slider.valueToProportionOfLength (slider.getValue());
        // Slider disables label focus after its factory returns. Enable it when
        // interacting so click-to-type also works in a real host window.
        if (pressed)
        {
            setWantsKeyboardFocus (true);
            if (isShowing()) grabKeyboardFocus();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! pressed || ! slider.isEnabled()) return;
        if (! dragged)
        {
            if (std::abs (e.getDistanceFromDragStartY()) < 3) return;
            dragged = true;
            gesture = std::make_unique<juce::Slider::ScopedDragNotification> (slider);
        }
        const double sensitivity = e.mods.isShiftDown() ? 2000.0 : 200.0;
        proportion = juce::jlimit (0.0, 1.0, proportion + (lastY - e.position.y) / sensitivity);
        lastY = e.position.y;
        slider.setValue (slider.proportionOfLengthToValue (proportion), juce::sendNotificationSync);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        gesture.reset();
        if (pressed && ! dragged && ! e.mouseWasDraggedSinceMouseDown()
            && e.getNumberOfClicks() == 1 && getLocalBounds().contains (e.getPosition()))
            // Wait long enough to distinguish click-to-type from double-click reset.
            startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20);
        pressed = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        stopTimer();
        gesture.reset();
        pressed = false;
        if (slider.isEnabled() && ! e.mods.isPopupMenu()) slider.mouseDoubleClick (e);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey && slider.isEnabled() && slider.isTextBoxEditable())
        {
            stopTimer();
            showEditor();
            return true;
        }
        return slider.keyPressed (key);
    }

private:
    void timerCallback() override
    {
        stopTimer();
        // A tab change or another control taking focus cancels pending text entry.
        // Walk visibility explicitly so offscreen editor snapshots need no window.
        for (auto* ancestor = static_cast<juce::Component*> (this); ancestor != nullptr; ancestor = ancestor->getParentComponent())
            if (! ancestor->isVisible()) return;
        if (slider.isEnabled() && slider.isTextBoxEditable()
            && (getPeer() == nullptr || hasKeyboardFocus (true))) showEditor();
    }
    juce::Slider& slider;
    std::unique_ptr<juce::Slider::ScopedDragNotification> gesture;
    double proportion = 0;
    float lastY = 0;
    bool pressed = false, dragged = false;
};
}

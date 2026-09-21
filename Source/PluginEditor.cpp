#include "PluginEditor.h"
#include "ui/EditorContent.h"
#include "ui/Theme.h"

TeratoamorAudioProcessorEditor::TeratoamorAudioProcessorEditor (TeratoamorAudioProcessor& p)
    : AudioProcessorEditor (&p), content (std::make_unique<teratoamor::ui::EditorContent> (p))
{
    addAndMakeVisible (*content);
    using namespace teratoamor::ui;
    setResizable (true, true);
    setResizeLimits (Layout::minimumWidth, Layout::minimumWidth * Layout::height / Layout::width,
                     Layout::maximumWidth, Layout::maximumWidth * Layout::height / Layout::width);
    getConstrainer()->setFixedAspectRatio (double (Layout::width) / Layout::height);
    setSize (Layout::defaultWidth, Layout::defaultWidth * Layout::height / Layout::width);
}
TeratoamorAudioProcessorEditor::~TeratoamorAudioProcessorEditor() = default;
void TeratoamorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (teratoamor::ui::Theme::background);
}
void TeratoamorAudioProcessorEditor::resized()
{
    using namespace teratoamor::ui;
    const auto scale = juce::jmin (getWidth() / float (Layout::width), getHeight() / float (Layout::height));
    content->setBounds (0, 0, Layout::width, Layout::height);
    content->setTransform (juce::AffineTransform::scale (scale));
}

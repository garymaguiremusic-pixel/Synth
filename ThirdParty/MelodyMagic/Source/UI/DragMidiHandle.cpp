#include "DragMidiHandle.h"
#include "../Shared/CustomLookAndFeel.h"

using namespace juce;

DragMidiHandle::DragMidiHandle()
{
    setMouseCursor (MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto a DAW track to drop the arpeggiated pattern as a MIDI clip (locked to your scale when Scale Lock is on).\nRight-click to save the .mid file instead.");
}

void DragMidiHandle::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool lit = hover || dragging;

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b.translated (0.0f, 1.0f), 3.0f);
    g.setGradientFill (ColourGradient (lit ? Colour (0xff3a95f0) : Colour (0xff2f86e0), 0.0f, b.getY(),
                                       lit ? Colour (0xff1d5fa6) : Colour (0xff15508f), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine ((int) b.getY() + 1, b.getX() + 3.0f, b.getRight() - 3.0f);
    g.setColour (VirusColours::bevelDark);
    g.drawRoundedRectangle (b, 3.0f, 1.0f);

    // grip dots
    auto grip = b.removeFromLeft (22.0f).reduced (7.0f, b.getHeight() * 0.22f);
    g.setColour (Colours::white.withAlpha (0.75f));
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (grip.getX() + col * 5.0f, grip.getY() + row * grip.getHeight() / 2.5f, 2.6f, 2.6f);

    // tiny MIDI-clip icon: four note bars
    auto icon = b.removeFromRight (24.0f).reduced (5.0f, b.getHeight() * 0.28f);
    const float h = icon.getHeight() / 4.0f;
    const float xs[] = { 0.0f, 0.3f, 0.5f, 0.75f }, ys[] = { 3.0f, 1.0f, 2.0f, 0.0f }, ws[] = { 0.35f, 0.25f, 0.3f, 0.25f };
    for (int i = 0; i < 4; ++i)
        g.fillRect (icon.getX() + xs[i] * icon.getWidth(), icon.getY() + ys[i] * h, ws[i] * icon.getWidth(), h * 0.8f);

    g.setFont (CustomLookAndFeel::uiFont (11.5f, true));
    g.setColour (Colours::white);
    g.drawText ("Drag MIDI", b, Justification::centred);
}

void DragMidiHandle::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showMenu();
        return;
    }
    dragging = true;
    dragStarted = false;
    repaint();
}

void DragMidiHandle::mouseDrag (const MouseEvent& e)
{
    if (! dragging || dragStarted || e.getDistanceFromDragStart() < 4 || ! createFile)
        return;

    dragStarted = true;
    const auto file = createFile();
    if (file.existsAsFile())
        DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                              [safe = SafePointer<DragMidiHandle> (this)]
                                                              {
                                                                  if (safe != nullptr)
                                                                  {
                                                                      safe->dragging = false;
                                                                      safe->repaint();
                                                                  }
                                                              });
}

void DragMidiHandle::mouseUp (const MouseEvent&)
{
    dragging = false;
    repaint();
}

void DragMidiHandle::showMenu()
{
    PopupMenu menu;
    menu.addItem ("Save MIDI file...", [this]
    {
        if (! createFile)
            return;
        const auto source = createFile();
        if (! source.existsAsFile())
            return;

        chooser = std::make_unique<FileChooser> ("Save MIDI file",
                                                 File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (source.getFileName()),
                                                 "*.mid");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                              [source] (const FileChooser& fc)
                              {
                                  auto target = fc.getResult();
                                  if (target != File())
                                      source.copyFileTo (target.withFileExtension ("mid"));
                              });
    });
    menu.addItem ("Show export folder", [this]
    {
        if (createFile)
        {
            const auto file = createFile();
            if (file.existsAsFile())
                file.revealToUser();
        }
    });
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this));
}

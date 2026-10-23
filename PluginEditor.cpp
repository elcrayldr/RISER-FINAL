#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

//==============================================================================
// Identidad visual de RiserLab
//   - fondo casi negro con un tinte azul frio
//   - acento "riser": degradado violeta -> cian (la energia que sube)
//   - rojo coral solo para grabacion, verde/ambar solo para estados
//==============================================================================
namespace
{
    const juce::Colour kBg0       (0xff06080c);
    const juce::Colour kBg1       (0xff0c1119);
    const juce::Colour kPanelTop  (0xff121a26);
    const juce::Colour kPanelBot  (0xff0b1119);
    const juce::Colour kEdge      (0xff1b2535);
    const juce::Colour kEdgeHi    (0xff2c3b54);
    const juce::Colour kTrack     (0xff182130);

    const juce::Colour kInk       (0xffeaf2ff);
    const juce::Colour kMuted     (0xff8392aa);
    const juce::Colour kDim       (0xff4b586d);

    const juce::Colour kViolet    (0xff8b5cff);
    const juce::Colour kViolet2   (0xff5a3df0);
    const juce::Colour kCyan      (0xff3de2ff);

    const juce::Colour kRec       (0xffff4d6d);
    const juce::Colour kOk        (0xff46e6a4);
    const juce::Colour kWarn      (0xffffb84d);

    // Tamano de diseno. La interfaz se escala proporcionalmente a partir de aqui.
    constexpr int kBaseW = 1000;
    constexpr int kBaseH = 700;

    enum KnobIndex
    {
        kSize, kDamping, kLength, kDeEss, kLowCut, kHighCut,
        kDrive, kEcho, kDry, kReverbMix, kTremolo, kNumKnobs
    };

    struct KnobDef
    {
        const char* id;     // identificador del parametro (NO cambiar)
        const char* name;   // texto bajo el knob
        const char* tip;    // tooltip
        int panel;          // 0 reverb, 1 tone, 2 character, 3 time
        int slot;           // posicion dentro del panel
    };

    // El orden coincide con KnobIndex.
    const KnobDef kKnobDefs[] =
    {
        { "size",        "SIZE",      "Room size of the reverse reverb",                                  0, 0 },
        { "damping",     "DAMPING",   "High-frequency absorption in the reverb tail",                     0, 1 },
        { "length",      "LENGTH",    "Duration of the riser, in seconds",                                3, 0 },
        { "deess",       "DE-ESSER",  "Amount of sibilance reduction applied to the vocal",               1, 2 },
        { "lowCut",      "LOW CUT",   "High-pass filter frequency",                                       1, 0 },
        { "highCut",     "HIGH CUT",  "Low-pass filter frequency",                                        1, 1 },
        { "drive",       "DRIVE",     "Saturation amount",                                                2, 0 },
        { "echo",        "ECHO",      "Echo amount",                                                      2, 1 },
        { "dry",         "DRY VOICE", "Level of the original vocal mixed in (only when 'Cut at vocal start' is off)", 2, 2 },
        { "reverbMix",   "MIX",       "Reverb wet / dry balance",                                         0, 2 },
        { "tremoloRate", "TREMOLO",   "Tremolo rate as a note division, synced to the host tempo",        3, 1 }
    };

    const char* const kPanelTitles[] = { "REVERB", "TONE", "CHARACTER", "TIME & MOTION" };

    //--------------------------------------------------------------------------
    juce::Font uiFont (float size, bool bold = false, float kerning = 0.0f)
    {
        juce::Font f (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
        if (kerning != 0.0f)
            f = f.withExtraKerningFactor (kerning);
        return f;
    }

    juce::Font monoFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              size, juce::Font::bold));
    }

    float textWidth (const juce::Font& font, const juce::String& text)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, text, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, false).getWidth();
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float s)
    {
        const float cr = 10.0f * s;

        g.setGradientFill (juce::ColourGradient (kPanelTop, 0.0f, r.getY(),
                                                 kPanelBot, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, cr);

        g.setColour (kEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), cr, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.drawLine (r.getX() + cr, r.getY() + 1.0f, r.getRight() - cr, r.getY() + 1.0f, 1.0f);
    }

    void drawAccentBar (juce::Graphics& g, juce::Rectangle<float> bar)
    {
        g.setGradientFill (juce::ColourGradient (kCyan, 0.0f, bar.getY(),
                                                 kViolet, 0.0f, bar.getBottom(), false));
        g.fillRoundedRectangle (bar, bar.getWidth() * 0.5f);
    }

    void drawPanelTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, float s)
    {
        auto a = area.toFloat();
        drawAccentBar (g, juce::Rectangle<float> (a.getX(), a.getCentreY() - 6.0f * s, 3.0f * s, 12.0f * s));

        g.setColour (kInk);
        g.setFont (uiFont (10.5f * s, true, 0.22f));
        g.drawText (title, a.withTrimmedLeft (11.0f * s), juce::Justification::centredLeft, false);
    }

    void drawStepCaption (juce::Graphics& g, juce::Rectangle<int> area,
                          const juce::String& number, const juce::String& title, float s)
    {
        const auto numberFont = uiFont (9.5f * s, true, 0.2f);
        g.setFont (numberFont);
        g.setColour (kCyan);
        g.drawText (number, area, juce::Justification::centredLeft, false);

        const int offset = juce::roundToInt (textWidth (numberFont, number) + 9.0f * s);
        g.setFont (uiFont (10.5f * s, true, 0.2f));
        g.setColour (kInk);
        g.drawText (title, area.withTrimmedLeft (offset), juce::Justification::centredLeft, false);
    }

    // Formato de los valores de cada knob (se aplica DESPUES de crear el attachment,
    // porque el attachment instala sus propias funciones de texto).
    void applyValueFormat (juce::Slider& sl, int knob)
    {
        auto number = [] (const juce::String& t)
        {
            return t.replaceCharacter (',', '.').retainCharacters ("0123456789.-").getDoubleValue();
        };

        switch (knob)
        {
            case kLength:
                sl.textFromValueFunction = [] (double v) { return juce::String (v, 2) + " s"; };
                sl.valueFromTextFunction = [number] (const juce::String& t) { return number (t); };
                break;

            case kLowCut:
                sl.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; };
                sl.valueFromTextFunction = [number] (const juce::String& t)
                {
                    const double n = number (t);
                    return t.containsIgnoreCase ("k") ? n * 1000.0 : n;
                };
                break;

            case kHighCut:
                sl.textFromValueFunction = [] (double v)
                {
                    return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz"
                                       : juce::String (juce::roundToInt (v)) + " Hz";
                };
                sl.valueFromTextFunction = [number] (const juce::String& t)
                {
                    const double n = number (t);
                    return (t.containsIgnoreCase ("k") || n < 100.0) ? n * 1000.0 : n;
                };
                break;

            case kTremolo:
            {
                static const char* const names[] = { "1/2", "1/4", "1/8", "1/16", "1/32" };
                sl.textFromValueFunction = [] (double v)
                {
                    return juce::String (names[juce::jlimit (0, 4, juce::roundToInt (v))]);
                };
                sl.valueFromTextFunction = [number] (const juce::String& t)
                {
                    for (int i = 0; i < 5; ++i)
                        if (t.trim() == names[i])
                            return (double) i;
                    return juce::jlimit (0.0, 4.0, number (t));
                };
                break;
            }

            default: // size, damping, deess, drive, echo, dry, reverbMix -> porcentaje
                sl.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
                sl.valueFromTextFunction = [number] (const juce::String& t) { return number (t) / 100.0; };
                break;
        }

        sl.updateText();
    }
}

//==============================================================================
// Look & feel
//==============================================================================
RiserLabLookAndFeel::RiserLabLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0c1118));
    setColour (juce::PopupMenu::textColourId, kInk.withAlpha (0.85f));
    setColour (juce::PopupMenu::headerTextColourId, kMuted);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kViolet.withAlpha (0.55f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0121a26));
    setColour (juce::TooltipWindow::textColourId, kInk);
    setColour (juce::TooltipWindow::outlineColourId, kEdgeHi);

    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0a0e14));
    setColour (juce::TextEditor::textColourId, kInk);
    setColour (juce::TextEditor::highlightColourId, kViolet.withAlpha (0.5f));
    setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::outlineColourId, kEdgeHi);
    setColour (juce::TextEditor::focusedOutlineColourId, kCyan);
    setColour (juce::CaretComponent::caretColourId, kCyan);

    setColour (juce::ScrollBar::thumbColourId, kEdgeHi);
}

void RiserLabLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float pos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float d = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto area = bounds.withSizeKeepingCentre (d, d).reduced (d * 0.03f);
    const float R = area.getWidth() * 0.5f;
    const auto c = area.getCentre();
    const bool hot = slider.isMouseOverOrDragging();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const int steps = (int) slider.getProperties().getWithDefault ("steps", 0);

    // --- Arco exterior (pista + valor) ---
    const float arcR = R * 0.93f;
    const float arcW = juce::jmax (2.0f, R * 0.075f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (steps > 1)
    {
        // Knob escalonado: un punto por cada paso.
        const int current = juce::roundToInt (pos * (float) (steps - 1));
        for (int i = 0; i < steps; ++i)
        {
            const float a = startAngle + ((float) i / (float) (steps - 1)) * (endAngle - startAngle);
            const auto p = c.getPointOnCircumference (arcR, a);
            const float dotR = arcW * (i == current ? 0.95f : 0.6f);
            g.setColour (i <= current ? kCyan : kDim);
            g.fillEllipse (p.x - dotR, p.y - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }
    else if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);

        g.setColour (kViolet.withAlpha (hot ? 0.30f : 0.17f));
        g.strokePath (value, juce::PathStrokeType (arcW * 2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setGradientFill (juce::ColourGradient (kViolet, c.x - arcR, c.y, kCyan, c.x + arcR, c.y, false));
        g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- Cuerpo ---
    const float bodyR = R * 0.74f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (body.translated (0.0f, R * 0.06f).expanded (R * 0.02f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d3748), c.x - bodyR * 0.5f, c.y - bodyR * 0.95f,
                                             juce::Colour (0xff0a0f16), c.x + bodyR * 0.4f, c.y + bodyR, false));
    g.fillEllipse (body);

    g.setColour (hot ? kCyan.withAlpha (0.6f) : kEdgeHi);
    g.drawEllipse (body, juce::jmax (1.0f, R * 0.022f));

    // --- Tapa interior ---
    const float capR = bodyR * 0.66f;
    const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c2637), c.x, c.y - capR,
                                             juce::Colour (0xff0d131c), c.x, c.y + capR, false));
    g.fillEllipse (cap);
    g.setColour (juce::Colour (0xff2a3548));
    g.drawEllipse (cap, 1.0f);

    // --- Indicador ---
    const auto p1 = c.getPointOnCircumference (capR * 0.30f, angle);
    const auto p2 = c.getPointOnCircumference (capR * 0.93f, angle);
    const float lineW = juce::jmax (2.0f, R * 0.07f);

    juce::Path pointer;
    pointer.startNewSubPath (p1);
    pointer.lineTo (p2);

    g.setColour (kCyan.withAlpha (hot ? 0.40f : 0.25f));
    g.strokePath (pointer, juce::PathStrokeType (lineW * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (kInk);
    g.strokePath (pointer, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Label* RiserLabLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (uiFont ((float) slider.getTextBoxHeight() * 0.72f, true, 0.04f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void RiserLabLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool isButtonDown,
                                        int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (0.5f);
    const float cr = r.getHeight() * 0.3f;
    const bool hot = box.isMouseOver (true) || isButtonDown || box.isPopupActive();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff152030), 0.0f, r.getY(),
                                             juce::Colour (0xff0d141d), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, cr);

    g.setColour (hot ? kViolet.withAlpha (0.85f) : kEdgeHi);
    g.drawRoundedRectangle (r, cr, 1.0f);

    const float cx = r.getRight() - r.getHeight() * 0.55f;
    const float cy = r.getCentreY();
    const float a = r.getHeight() * 0.11f;

    juce::Path chevron;
    chevron.startNewSubPath (cx - a * 1.5f, cy - a * 0.6f);
    chevron.lineTo (cx, cy + a * 0.9f);
    chevron.lineTo (cx + a * 1.5f, cy - a * 0.6f);

    g.setColour (hot ? kCyan : kMuted);
    g.strokePath (chevron, juce::PathStrokeType (juce::jmax (1.4f, r.getHeight() * 0.06f),
                                                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font RiserLabLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return uiFont ((float) box.getHeight() * 0.46f, true, 0.06f);
}

void RiserLabLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const int h = box.getHeight();
    const int left = juce::roundToInt ((float) h * 0.4f);
    label.setBounds (left, 1, box.getWidth() - left - juce::roundToInt ((float) h * 0.9f), h - 2);
    label.setFont (getComboBoxFont (box));
}

void RiserLabLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (juce::Colour (0xff0c1118));
    g.setColour (kEdgeHi);
    g.drawRect (0, 0, width, height, 1);
}

juce::Font RiserLabLookAndFeel::getPopupMenuFont()
{
    return uiFont (14.0f, true, 0.04f);
}

void RiserLabLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                            bool highlighted, bool)
{
    const auto r = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    const float swH = r.getHeight() * 0.8f;
    const float swW = swH * 1.85f;
    const auto sw = juce::Rectangle<float> (r.getX() + 1.0f, r.getCentreY() - swH * 0.5f, swW, swH);

    if (on)
    {
        g.setColour (kViolet.withAlpha (highlighted ? 0.35f : 0.22f));
        g.fillRoundedRectangle (sw.expanded (swH * 0.14f), swH * 0.64f);

        g.setGradientFill (juce::ColourGradient (kViolet, sw.getX(), 0.0f, kCyan, sw.getRight(), 0.0f, false));
        g.fillRoundedRectangle (sw, swH * 0.5f);
    }
    else
    {
        g.setColour (kTrack);
        g.fillRoundedRectangle (sw, swH * 0.5f);
        g.setColour (highlighted ? kMuted : kEdgeHi);
        g.drawRoundedRectangle (sw.reduced (0.5f), swH * 0.5f, 1.0f);
    }

    const float thumbR = swH * 0.36f;
    const float thumbX = on ? sw.getRight() - swH * 0.5f : sw.getX() + swH * 0.5f;
    g.setColour (on ? juce::Colours::white : kMuted);
    g.fillEllipse (thumbX - thumbR, sw.getCentreY() - thumbR, thumbR * 2.0f, thumbR * 2.0f);

    const auto text = button.getButtonText();
    if (text.isNotEmpty())
    {
        g.setFont (uiFont (r.getHeight() * 0.42f, true, 0.08f));
        g.setColour (on ? button.findColour (juce::ToggleButton::textColourId) : kMuted);
        g.drawText (text, r.withTrimmedLeft (swW + r.getHeight() * 0.45f),
                    juce::Justification::centredLeft, false);
    }
}

void RiserLabLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    const float W = (float) w;
    const float H = (float) h;

    g.setColour ((isMouseOver || isMouseDragging) ? kCyan.withAlpha (0.85f) : kDim);

    for (int i = 0; i < 3; ++i)
    {
        const float o = W * (0.36f + 0.2f * (float) i);
        g.drawLine (W - o, H, W, H - o, 1.2f);
    }
}

//==============================================================================
// Capture
//==============================================================================
void RiserLabEditor::CaptureControl::paintButton (juce::Graphics& g, bool highlighted, bool pressed)
{
    const auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    const auto outer = b.withSizeKeepingCentre (d, d).reduced (d * 0.04f);
    const auto c = outer.getCentre();
    const float R = outer.getWidth() * 0.5f;

    const float pulse = active ? 0.5f + 0.5f * (float) std::sin (juce::Time::getMillisecondCounterHiRes() * 0.006)
                               : 0.0f;

    // Halo de grabacion
    if (active)
    {
        g.setColour (kRec.withAlpha (0.10f + 0.16f * pulse));
        g.fillEllipse (outer.expanded (R * 0.14f * (0.4f + pulse)));
    }

    // Anillo: pista + progreso del buffer de captura
    const float ringW = R * 0.10f;
    const float ringR = R - ringW * 0.5f;

    g.setColour (kTrack);
    g.drawEllipse (c.x - ringR, c.y - ringR, ringR * 2.0f, ringR * 2.0f, ringW);

    if (progress > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, 0.0f,
                           juce::MathConstants<float>::twoPi * juce::jlimit (0.0f, 1.0f, progress), true);
        g.setColour (active ? kRec : kViolet);
        g.strokePath (arc, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (highlighted)
    {
        g.setColour (kViolet.withAlpha (0.55f));
        g.drawEllipse (c.x - ringR, c.y - ringR, ringR * 2.0f, ringR * 2.0f, ringW * 0.5f);
    }

    // Disco central
    const auto disc = outer.reduced (R * 0.22f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (pressed ? 0xff10161f : 0xff1f2a3b), c.x, disc.getY(),
                                             juce::Colour (0xff0b1018), c.x, disc.getBottom(), false));
    g.fillEllipse (disc);
    g.setColour (highlighted ? kEdgeHi.brighter (0.2f) : kEdge);
    g.drawEllipse (disc, 1.0f);

    // Icono: punto de grabacion / cuadrado de parada
    if (active)
    {
        const auto sq = juce::Rectangle<float> (R * 0.36f, R * 0.36f).withCentre (c);
        g.setColour (kRec);
        g.fillRoundedRectangle (sq, R * 0.07f);
    }
    else
    {
        const float dot = R * 0.17f;
        g.setColour (kRec.withAlpha (highlighted ? 1.0f : 0.85f));
        g.fillEllipse (c.x - dot, c.y - dot, dot * 2.0f, dot * 2.0f);
    }
}

//==============================================================================
// Botones de accion y flechas de preset
//==============================================================================
void RiserLabEditor::ActionButton::setLabel (const juce::String& newText, Icon newIcon)
{
    if (label != newText || icon != newIcon)
    {
        label = newText;
        icon = newIcon;
        repaint();
    }
}

void RiserLabEditor::ActionButton::setDimmed (bool shouldBeDimmed)
{
    if (dimmed != shouldBeDimmed)
    {
        dimmed = shouldBeDimmed;
        repaint();
    }
}

void RiserLabEditor::ActionButton::paintButton (juce::Graphics& g, bool highlighted, bool pressed)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float h = r.getHeight();
    const float cr = h * 0.28f;

    if (dimmed)
        g.beginTransparencyLayer (0.5f);

    if (primary)
    {
        const auto c1 = pressed ? kViolet.darker (0.2f)  : (highlighted ? kViolet.brighter (0.14f)  : kViolet);
        const auto c2 = pressed ? kViolet2.darker (0.2f) : (highlighted ? kViolet2.brighter (0.14f) : kViolet2);
        g.setGradientFill (juce::ColourGradient (c1, r.getX(), r.getY(), c2, r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, cr);

        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.drawLine (r.getX() + cr, r.getY() + 1.0f, r.getRight() - cr, r.getY() + 1.0f, 1.0f);

        g.setColour (kViolet.brighter (0.5f).withAlpha (0.55f));
        g.drawRoundedRectangle (r, cr, 1.0f);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (pressed ? 0xff0e141c : 0xff172131), 0.0f, r.getY(),
                                                 juce::Colour (0xff0d131c), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, cr);
        g.setColour (highlighted ? kCyan.withAlpha (0.7f) : kEdgeHi);
        g.drawRoundedRectangle (r, cr, 1.0f);
    }

    // Icono + texto centrados
    const auto font = uiFont (h * 0.30f, true, 0.14f);
    const float iconSize = h * 0.34f;
    const float gap = h * 0.22f;
    const float tw = textWidth (font, label);
    const float total = iconSize + gap + tw;
    const float startX = r.getCentreX() - total * 0.5f;
    const juce::Point<float> ic (startX + iconSize * 0.5f, r.getCentreY());

    if (icon == Icon::sparkle)
    {
        juce::Path star;
        star.addStar (ic, 4, iconSize * 0.14f, iconSize * 0.58f, 0.0f);
        g.setColour (juce::Colours::white);
        g.fillPath (star);
    }
    else if (icon == Icon::play)
    {
        juce::Path tri;
        tri.addTriangle (ic.x - iconSize * 0.34f, ic.y - iconSize * 0.46f,
                         ic.x - iconSize * 0.34f, ic.y + iconSize * 0.46f,
                         ic.x + iconSize * 0.50f, ic.y);
        g.setColour (kCyan);
        g.fillPath (tri);
    }
    else
    {
        g.setColour (kRec);
        g.fillRoundedRectangle (juce::Rectangle<float> (iconSize * 0.8f, iconSize * 0.8f).withCentre (ic),
                                iconSize * 0.12f);
    }

    g.setFont (font);
    g.setColour (primary ? juce::Colours::white : kInk);
    g.drawText (label,
                juce::Rectangle<float> (startX + iconSize + gap, r.getY(), tw + 4.0f, h),
                juce::Justification::centredLeft, false);

    if (dimmed)
        g.endTransparencyLayer();
}

void RiserLabEditor::ArrowButton::paintButton (juce::Graphics& g, bool highlighted, bool pressed)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float cr = r.getHeight() * 0.3f;

    g.setColour (pressed ? juce::Colour (0xff0e141c) : juce::Colour (0xff111824));
    g.fillRoundedRectangle (r, cr);
    g.setColour (highlighted ? kCyan.withAlpha (0.7f) : kEdgeHi);
    g.drawRoundedRectangle (r, cr, 1.0f);

    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const float a = r.getHeight() * 0.14f;
    const float dir = right ? 1.0f : -1.0f;

    juce::Path chevron;
    chevron.startNewSubPath (cx - dir * a * 0.7f, cy - a * 1.5f);
    chevron.lineTo (cx + dir * a * 0.8f, cy);
    chevron.lineTo (cx - dir * a * 0.7f, cy + a * 1.5f);

    g.setColour (highlighted ? kCyan : kMuted);
    g.strokePath (chevron, juce::PathStrokeType (juce::jmax (1.5f, r.getHeight() * 0.07f),
                                                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
// Pantalla de estado
//==============================================================================
void RiserLabEditor::StatusDisplay::update (State newState, const juce::String& newMessage, double newSeconds)
{
    const bool changed = newState != state
                      || newMessage != message
                      || juce::roundToInt (newSeconds * 10.0) != juce::roundToInt (seconds * 10.0);

    state = newState;
    message = newMessage;
    seconds = newSeconds;

    if (changed)
        repaint();
}

void RiserLabEditor::StatusDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float s = r.getWidth() / 316.0f;
    const float cr = 8.0f * s;

    g.setColour (juce::Colour (0xff070a0f));
    g.fillRoundedRectangle (r, cr);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.4f), 0.0f, r.getY(),
                                             juce::Colours::transparentBlack, 0.0f, r.getCentreY(), false));
    g.fillRoundedRectangle (r, cr);
    g.setColour (juce::Colour (0xff1a2331));
    g.drawRoundedRectangle (r.reduced (0.5f), cr, 1.0f);

    juce::Colour stateColour = kMuted;
    juce::String stateText = "IDLE";

    switch (state)
    {
        case State::idle:      stateColour = kMuted; stateText = "IDLE";     break;
        case State::recording: stateColour = kRec;   stateText = "RECORDING"; break;
        case State::captured:  stateColour = kCyan;  stateText = "CAPTURED"; break;
        case State::playing:   stateColour = kViolet.brighter (0.3f); stateText = "PLAYING"; break;
        case State::ready:     stateColour = kOk;    stateText = "READY";    break;
        case State::error:     stateColour = kWarn;  stateText = "ATTENTION"; break;
    }

    // Bloque izquierdo: tiempo capturado
    g.setColour (kMuted);
    g.setFont (uiFont (8.5f * s, true, 0.2f));
    g.drawText ("CAPTURED SEC", juce::Rectangle<float> (r.getX() + 14.0f * s, r.getY() + 9.0f * s, 96.0f * s, 12.0f * s),
                juce::Justification::centredLeft, false);

    g.setColour (seconds > 0.0 ? kInk : kDim);
    g.setFont (monoFont (27.0f * s));
    g.drawText (juce::String::formatted ("%04.1f", seconds),
                juce::Rectangle<float> (r.getX() + 12.0f * s, r.getY() + 22.0f * s, 100.0f * s, 34.0f * s),
                juce::Justification::centredLeft, false);

    g.setColour (kEdge);
    g.fillRect (r.getX() + 116.0f * s, r.getY() + 12.0f * s, 1.0f, r.getHeight() - 34.0f * s);

    // Bloque derecho: estado + mensaje
    const float rx = r.getX() + 130.0f * s;
    const float rw = r.getRight() - rx - 12.0f * s;

    const float pulse = (state == State::recording || state == State::playing)
                          ? 0.55f + 0.45f * (float) std::sin (juce::Time::getMillisecondCounterHiRes() * 0.008)
                          : 1.0f;
    const float dot = 3.6f * s;
    const float dotY = r.getY() + 18.0f * s;

    g.setColour (stateColour.withAlpha (0.25f * pulse));
    g.fillEllipse (rx - dot * 1.7f, dotY - dot * 1.7f, dot * 3.4f, dot * 3.4f);
    g.setColour (stateColour.withAlpha (pulse));
    g.fillEllipse (rx - dot, dotY - dot, dot * 2.0f, dot * 2.0f);

    g.setColour (stateColour);
    g.setFont (uiFont (10.0f * s, true, 0.24f));
    g.drawText (stateText, juce::Rectangle<float> (rx + 12.0f * s, dotY - 7.0f * s, rw - 12.0f * s, 14.0f * s),
                juce::Justification::centredLeft, false);

    g.setColour (kInk.withAlpha (0.88f));
    g.setFont (uiFont (11.5f * s));
    g.drawFittedText (message,
                      juce::Rectangle<float> (rx, r.getY() + 30.0f * s, rw, 28.0f * s).toNearestInt(),
                      juce::Justification::topLeft, 2);

    // Barra inferior: llenado del buffer de captura
    const auto bar = juce::Rectangle<float> (r.getX() + 14.0f * s, r.getBottom() - 10.0f * s,
                                             r.getWidth() - 28.0f * s, 3.0f * s);
    g.setColour (kTrack);
    g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);

    const float fill = (float) juce::jlimit (0.0, 1.0, seconds / RiserLabProcessor::kMaxCaptureSeconds);
    if (fill > 0.002f)
    {
        auto filled = bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * fill));
        if (state == State::recording)
            g.setColour (kRec);
        else
            g.setGradientFill (juce::ColourGradient (kViolet, bar.getX(), 0.0f, kCyan, bar.getRight(), 0.0f, false));
        g.fillRoundedRectangle (filled, bar.getHeight() * 0.5f);
    }
}

//==============================================================================
// Zona de salida: forma de onda + arrastrar al DAW
//==============================================================================
void RiserLabEditor::DragArea::setData (std::vector<float> newPeaks, const juce::File& newFile)
{
    peaks = std::move (newPeaks);
    file = newFile;
    setMouseCursor (peaks.empty() ? juce::MouseCursor::NormalCursor : juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void RiserLabEditor::DragArea::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float s = r.getWidth() / 960.0f;
    const bool hasData = ! peaks.empty();

    drawPanel (g, r, s);

    if (hasData && isMouseOver())
    {
        g.setColour (kCyan.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f * s, 1.2f);
    }

    // Titulo
    drawPanelTitle (g, juce::Rectangle<float> (r.getX() + 14.0f * s, r.getY() + 7.0f * s, 160.0f * s, 22.0f * s).toNearestInt(),
                    "OUTPUT", s);

    if (hasData)
    {
        g.setColour (kMuted);
        g.setFont (uiFont (10.0f * s, false, 0.05f));
        g.drawText (file.getFileName(),
                    juce::Rectangle<float> (r.getX() + 96.0f * s, r.getY() + 7.0f * s, 300.0f * s, 22.0f * s),
                    juce::Justification::centredLeft, true);

        // Chip "DRAG TO DAW"
        const auto chipFont = uiFont (9.5f * s, true, 0.2f);
        const juce::String chipText = "DRAG TO DAW";
        const float tw = textWidth (chipFont, chipText);
        const float chipW = tw + 44.0f * s;
        const auto chip = juce::Rectangle<float> (r.getRight() - 14.0f * s - chipW, r.getY() + 9.0f * s,
                                                  chipW, 18.0f * s);
        g.setColour (kCyan.withAlpha (isMouseOver() ? 0.16f : 0.08f));
        g.fillRoundedRectangle (chip, chip.getHeight() * 0.5f);
        g.setColour (kCyan.withAlpha (0.6f));
        g.drawRoundedRectangle (chip.reduced (0.5f), chip.getHeight() * 0.5f, 1.0f);

        // Asa de arrastre (2 x 3 puntos)
        for (int col = 0; col < 2; ++col)
            for (int row = 0; row < 3; ++row)
            {
                const float px = chip.getX() + 11.0f * s + (float) col * 4.0f * s;
                const float py = chip.getCentreY() - 4.0f * s + (float) row * 4.0f * s;
                g.fillEllipse (px - 0.9f * s, py - 0.9f * s, 1.8f * s, 1.8f * s);
            }

        g.setColour (kCyan);
        g.setFont (chipFont);
        g.drawText (chipText, chip.withTrimmedLeft (24.0f * s), juce::Justification::centredLeft, false);

        if (playing)
        {
            const float dot = 3.4f * s;
            const float dx = chip.getX() - 78.0f * s;
            g.setColour (kViolet.brighter (0.3f));
            g.fillEllipse (dx, chip.getCentreY() - dot, dot * 2.0f, dot * 2.0f);
            g.setFont (uiFont (9.5f * s, true, 0.2f));
            g.drawText ("PLAYING", juce::Rectangle<float> (dx + dot * 2.0f + 6.0f * s, chip.getY(), 60.0f * s, chip.getHeight()),
                        juce::Justification::centredLeft, false);
        }
    }

    // Zona de la onda
    const auto wave = juce::Rectangle<float> (r.getX() + 16.0f * s, r.getY() + 36.0f * s,
                                              r.getWidth() - 32.0f * s, r.getHeight() - 48.0f * s);
    const float mid = wave.getCentreY();

    g.setColour (kEdge.withAlpha (0.8f));
    g.fillRect (wave.getX(), mid - 0.5f, wave.getWidth(), 1.0f);

    const float barW = 2.0f * s;
    const float step = 3.0f * s;
    const int bars = juce::jmax (1, (int) (wave.getWidth() / step));

    if (! hasData)
    {
        // Silueta fantasma de un riser (decorativa)
        g.setGradientFill (juce::ColourGradient (kViolet.withAlpha (0.14f), wave.getX(), 0.0f,
                                                 kCyan.withAlpha (0.14f), wave.getRight(), 0.0f, false));
        for (int i = 0; i < bars; ++i)
        {
            const float t = (float) i / (float) bars;
            const float wobble = 0.75f + 0.25f * std::sin ((float) i * 0.9f);
            const float hh = juce::jmax (1.0f * s, std::pow (t, 1.7f) * wobble * wave.getHeight() * 0.46f);
            g.fillRoundedRectangle (wave.getX() + (float) i * step, mid - hh, barW, hh * 2.0f, barW * 0.5f);
        }

        g.setColour (kInk.withAlpha (0.9f));
        g.setFont (uiFont (12.0f * s, true, 0.16f));
        g.drawText ("NO RISER YET", wave.withTrimmedBottom (wave.getHeight() * 0.42f),
                    juce::Justification::centredBottom, false);
        g.setColour (kMuted);
        g.setFont (uiFont (11.0f * s));
        g.drawText ("Capture a vocal, press Generate, then drag the WAV into your DAW",
                    wave.withTrimmedTop (wave.getHeight() * 0.6f), juce::Justification::centredTop, false);
        return;
    }

    // Onda real
    const int total = (int) peaks.size();
    float maxPeak = 0.01f;
    for (const float p : peaks)
        maxPeak = juce::jmax (maxPeak, p);

    auto drawBars = [&] (float alpha)
    {
        g.setGradientFill (juce::ColourGradient (kViolet.withAlpha (alpha), wave.getX(), 0.0f,
                                                 kCyan.withAlpha (alpha), wave.getRight(), 0.0f, false));
        for (int i = 0; i < bars; ++i)
        {
            const int a = (int) ((juce::int64) i * total / bars);
            const int b = juce::jmin (total, juce::jmax (a + 1, (int) ((juce::int64) (i + 1) * total / bars)));

            float m = 0.0f;
            for (int k = a; k < b; ++k)
                m = juce::jmax (m, peaks[(size_t) k]);

            const float amp = juce::jlimit (0.02f, 1.0f, m / maxPeak);
            const float hh = juce::jmax (1.0f * s, amp * wave.getHeight() * 0.5f * 0.94f);
            g.fillRoundedRectangle (wave.getX() + (float) i * step, mid - hh, barW, hh * 2.0f, barW * 0.5f);
        }
    };

    if (playing && playProgress >= 0.0f)
    {
        const float playX = wave.getX() + wave.getWidth() * playProgress;

        drawBars (0.32f);

        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (wave.getX() - 2.0f, r.getY(), playX - wave.getX() + 2.0f, r.getHeight()).toNearestInt());
        drawBars (1.0f);
        g.restoreState();

        g.setColour (kCyan.withAlpha (0.28f));
        g.fillRect (playX - 3.0f * s, wave.getY(), 6.0f * s, wave.getHeight());
        g.setColour (juce::Colours::white);
        g.fillRect (playX - 0.75f * s, wave.getY() - 2.0f * s, 1.5f * s, wave.getHeight() + 4.0f * s);
    }
    else
    {
        drawBars (playing ? 1.0f : 0.92f);
    }
}

void RiserLabEditor::DragArea::mouseDrag (const juce::MouseEvent& e)
{
    if (file.existsAsFile() && e.getDistanceFromDragStart() > 6)
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            juce::StringArray (file.getFullPathName()), false, this);
}

//==============================================================================
// Editor
//==============================================================================
RiserLabEditor::RiserLabEditor (RiserLabProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    setResizable (true, true);
    setResizeLimits (800, 560, 1600, 1120);
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);

    logoImage = juce::ImageFileFormat::loadFrom (BinaryData::CrayLDRLogo_png,
                                                  (size_t) BinaryData::CrayLDRLogo_pngSize);

    // --- Presets ---
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        proc.setCurrentProgram (presetBox.getSelectedItemIndex());
        repaint();
    };
    presetBox.setColour (juce::ComboBox::textColourId, kInk);
    presetBox.setTooltip ("Factory presets");
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    // --- Acciones ---
    captureButton.onClick = [this] { onCaptureClicked(); };
    captureButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    captureButton.setTooltip ("Click to start capturing the incoming audio, click again to stop");
    addAndMakeVisible (captureButton);

    generateButton.primary = true;
    generateButton.setLabel ("GENERATE RISER", ActionButton::Icon::sparkle);
    generateButton.onClick = [this] { onGenerateClicked(); };
    generateButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    generateButton.setTooltip ("Build the riser from the captured vocal and the current settings");
    addAndMakeVisible (generateButton);

    playButton.setLabel ("PLAY", ActionButton::Icon::play);
    playButton.onClick = [this] { onPreviewClicked(); };
    playButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    playButton.setTooltip ("Audition the generated riser through the plugin output");
    addAndMakeVisible (playButton);

    statusDisplay.setTooltip ("Captured time and capture buffer (30 s max)");
    addAndMakeVisible (statusDisplay);

    // --- Reverb ---
    reverbToggle.setTooltip ("Enable or bypass the reverb");
    reverbToggle.setColour (juce::ToggleButton::textColourId, kInk);
    addAndMakeVisible (reverbToggle);
    reverbAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "reverbEnabled", reverbToggle);

    reverbTypeBox.addItem ("HALL", 1);
    reverbTypeBox.addItem ("PLATE", 2);
    reverbTypeBox.addItem ("ROOM", 3);
    reverbTypeBox.setColour (juce::ComboBox::textColourId, kCyan);
    reverbTypeBox.setTooltip ("Reverb voicing");
    addAndMakeVisible (reverbTypeBox);
    reverbTypeAttachment = std::make_unique<ComboBoxAttachment> (proc.apvts, "reverbType", reverbTypeBox);

    // --- Knobs ---
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& k = knobs[(size_t) i];
        const auto& def = kKnobDefs[i];

        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setColour (juce::Slider::textBoxTextColourId, kCyan);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setColour (juce::Slider::textBoxHighlightColourId, kViolet.withAlpha (0.5f));
        k.slider.setTooltip (def.tip);

        if (i == kTremolo)
            k.slider.getProperties().set ("steps", 5);

        addAndMakeVisible (k.slider);

        k.label.setText (def.name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, kMuted);
        k.label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<SliderAttachment> (proc.apvts, def.id, k.slider);

        // Despues del attachment: el attachment sustituye las funciones de texto.
        applyValueFormat (k.slider, i);
    }

    // --- Opcion de corte ---
    cutToggle.setButtonText ("CUT AT VOCAL START");
    cutToggle.setColour (juce::ToggleButton::textColourId, kInk);
    cutToggle.setTooltip ("End the file exactly where the vocal starts");
    addAndMakeVisible (cutToggle);
    cutAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "cut", cutToggle);

    // --- Salida ---
    addAndMakeVisible (dragArea);
    auto peaks = proc.getRiserPeaks (300);
    if (! peaks.empty())
    {
        hasRiser = true;
        dragArea.setData (std::move (peaks), proc.getRiserFile());
    }

    refreshDependentControls (true);
    setSize (kBaseW, kBaseH);
    startTimerHz (30);
}

RiserLabEditor::~RiserLabEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

RiserLabEditor::Layout RiserLabEditor::computeLayout() const
{
    Layout L;

    const float s = juce::jmin ((float) getWidth() / (float) kBaseW,
                                (float) getHeight() / (float) kBaseH);
    L.s = s;

    const int ox = (getWidth()  - juce::roundToInt ((float) kBaseW * s)) / 2;
    const int oy = (getHeight() - juce::roundToInt ((float) kBaseH * s)) / 2;

    auto R = [s, ox, oy] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (ox + juce::roundToInt (x * s), oy + juce::roundToInt (y * s),
                                     juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    // Cabecera
    L.mark          = R (20, 29, 34, 32);
    L.wordmark      = R (64, 22, 240, 34);
    L.subtitle      = R (65, 55, 260, 14);
    L.presetCaption = R (612, 24, 272, 14);
    L.prevButton    = R (574, 40, 30, 30);
    L.presetBox     = R (612, 40, 272, 30);
    L.nextButton    = R (892, 40, 30, 30);
    L.logo          = R (936, 28, 44, 44);
    L.headerLine    = R (20, 84, 960, 1);

    // Franja de flujo: capturar -> generar -> escuchar
    L.strip         = R (20, 92, 960, 92);
    L.captureButton = R (28, 100, 76, 76);
    L.captureText   = R (114, 104, 144, 68);
    L.stripDividers = { R (266, 106, 1, 64), R (482, 106, 1, 64), R (648, 106, 1, 64) };
    L.genCaption    = R (274, 106, 200, 14);
    L.genButton     = R (274, 126, 200, 44);
    L.playCaption   = R (490, 106, 150, 14);
    L.playButton    = R (490, 126, 150, 44);
    L.display       = R (656, 100, 316, 76);

    // Paneles 2 x 2
    const float px[4] = { 20.0f, 506.0f, 20.0f, 506.0f };
    const float py[4] = { 196.0f, 196.0f, 380.0f, 380.0f };

    for (size_t i = 0; i < 4; ++i)
    {
        auto& p = L.panels[i];
        p.frame = R (px[i], py[i], 474, 172);
        p.title = R (px[i] + 14, py[i] + 8, 200, 22);

        const float by = py[i] + 38.0f;
        if (i < 3)
        {
            for (size_t k = 0; k < 3; ++k)
                p.slots[k] = R (px[i] + 12.0f + 150.0f * (float) k, by, 150, 126);
        }
        else
        {
            p.slots[0] = R (px[i] + 12,  by, 126, 126);
            p.slots[1] = R (px[i] + 138, by, 126, 126);
            p.slots[2] = R (px[i] + 264, by, 198, 126);
        }
    }

    L.reverbTypeBox = R (330, 203, 96, 26);
    L.reverbToggle  = R (436, 205, 44, 22);

    const float tx = 506.0f + 264.0f + 4.0f;
    L.cutToggle = R (tx, 380.0f + 38.0f + 14.0f, 190, 24);
    L.cutText   = R (tx, 380.0f + 38.0f + 48.0f, 188, 64);

    // Salida
    L.drag = R (20, 564, 960, 116);

    return L;
}

void RiserLabEditor::resized()
{
    const auto L = computeLayout();
    const float s = L.s;

    presetBox.setBounds (L.presetBox);
    prevButton.setBounds (L.prevButton);
    nextButton.setBounds (L.nextButton);

    captureButton.setBounds (L.captureButton);
    generateButton.setBounds (L.genButton);
    playButton.setBounds (L.playButton);
    statusDisplay.setBounds (L.display);

    reverbToggle.setBounds (L.reverbToggle);
    reverbTypeBox.setBounds (L.reverbTypeBox);
    cutToggle.setBounds (L.cutToggle);

    const int labelH = juce::roundToInt (18.0f * s);
    const int boxH = juce::roundToInt (18.0f * s);

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto slot = L.panels[(size_t) kKnobDefs[i].panel].slots[(size_t) kKnobDefs[i].slot];
        auto labelArea = slot.removeFromBottom (labelH);

        auto& k = knobs[(size_t) i];
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                                  juce::jmax (20, slot.getWidth() - juce::roundToInt (20.0f * s)), boxH);
        k.slider.setBounds (slot);

        k.label.setFont (uiFont (9.5f * s, true, 0.14f));
        k.label.setBounds (labelArea);
    }

    dragArea.setBounds (L.drag);
}

void RiserLabEditor::paint (juce::Graphics& g)
{
    const auto L = computeLayout();
    const float s = L.s;
    const auto bounds = getLocalBounds().toFloat();

    // --- Fondo ---
    g.setGradientFill (juce::ColourGradient (kBg1, 0.0f, 0.0f, kBg0, 0.0f, bounds.getHeight(), false));
    g.fillAll();

    g.setGradientFill (juce::ColourGradient (kViolet.withAlpha (0.16f), bounds.getWidth() * 0.16f, 0.0f,
                                             kViolet.withAlpha (0.0f), bounds.getWidth() * 0.70f, 0.0f, true));
    g.fillAll();

    g.setGradientFill (juce::ColourGradient (kCyan.withAlpha (0.07f), bounds.getWidth() * 0.94f, bounds.getHeight(),
                                             kCyan.withAlpha (0.0f), bounds.getWidth() * 0.50f, bounds.getHeight(), true));
    g.fillAll();

    // --- Cabecera: marca ---
    {
        const auto mk = L.mark.toFloat();
        const int bars = 5;
        const float gap = mk.getWidth() * 0.09f;
        const float bw = (mk.getWidth() - gap * (float) (bars - 1)) / (float) bars;

        for (int i = 0; i < bars; ++i)
        {
            const float frac = 0.30f + 0.70f * ((float) i / (float) (bars - 1));
            const float bh = mk.getHeight() * frac;
            const auto bar = juce::Rectangle<float> (mk.getX() + (float) i * (bw + gap), mk.getBottom() - bh, bw, bh);

            g.setGradientFill (juce::ColourGradient (kViolet, 0.0f, mk.getBottom(), kCyan, 0.0f, mk.getY(), false));
            g.fillRoundedRectangle (bar, bw * 0.4f);
        }

        const auto wordFont = uiFont (26.0f * s, true, 0.14f);
        const float riserW = textWidth (wordFont, "RISER");

        g.setFont (wordFont);
        g.setColour (kInk);
        g.drawText ("RISER", L.wordmark, juce::Justification::centredLeft, false);

        g.setColour (kCyan);
        g.drawText ("LAB", L.wordmark.withTrimmedLeft (juce::roundToInt (riserW + 2.0f * s)),
                    juce::Justification::centredLeft, false);

        g.setColour (kMuted);
        g.setFont (uiFont (8.5f * s, true, 0.3f));
        g.drawText ("VOCAL RISER DESIGNER", L.subtitle, juce::Justification::centredLeft, false);
    }

    // --- Cabecera: preset ---
    {
        g.setColour (kMuted);
        g.setFont (uiFont (8.5f * s, true, 0.3f));
        g.drawText ("PRESET", L.presetCaption, juce::Justification::centredLeft, false);

        const int n = presetBox.getNumItems();
        const int idx = presetBox.getSelectedItemIndex();
        if (n > 0 && idx >= 0)
        {
            g.setColour (kDim);
            g.setFont (monoFont (9.5f * s));
            g.drawText (juce::String::formatted ("%02d / %02d", idx + 1, n), L.presetCaption,
                        juce::Justification::centredRight, false);
        }
    }

    // --- Cabecera: logo ---
    if (logoImage.isValid())
    {
        const auto lr = L.logo.toFloat();
        const float cr = 10.0f * s;

        g.setColour (juce::Colour (0xff0d131c));
        g.fillRoundedRectangle (lr, cr);

        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (lr, cr);
        g.reduceClipRegion (clip);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const auto inner = L.logo.reduced (juce::roundToInt (4.0f * s));
        g.drawImageWithin (logoImage, inner.getX(), inner.getY(), inner.getWidth(), inner.getHeight(),
                           juce::RectanglePlacement::centred);
        g.restoreState();

        g.setColour (kEdge);
        g.drawRoundedRectangle (lr.reduced (0.5f), cr, 1.0f);
    }

    g.setGradientFill (juce::ColourGradient (kEdgeHi.withAlpha (0.0f), (float) L.headerLine.getX(), 0.0f,
                                             kEdgeHi, (float) L.headerLine.getCentreX(), 0.0f, false));
    g.fillRect (L.headerLine.withWidth (L.headerLine.getWidth() / 2));
    g.setGradientFill (juce::ColourGradient (kEdgeHi, (float) L.headerLine.getCentreX(), 0.0f,
                                             kEdgeHi.withAlpha (0.0f), (float) L.headerLine.getRight(), 0.0f, false));
    g.fillRect (L.headerLine.withTrimmedLeft (L.headerLine.getWidth() / 2));

    // --- Franja de flujo ---
    drawPanel (g, L.strip.toFloat(), s);

    g.setColour (kEdge);
    for (const auto& d : L.stripDividers)
        g.fillRect (d);

    {
        auto textArea = L.captureText;
        drawStepCaption (g, textArea.removeFromTop (juce::roundToInt (16.0f * s)), "01", "CAPTURE", s);

        g.setColour (kMuted);
        g.setFont (uiFont (10.5f * s));
        g.drawFittedText ("Click to start recording the incoming audio. Click again to stop.",
                          textArea.withTrimmedTop (juce::roundToInt (4.0f * s)),
                          juce::Justification::topLeft, 4);
    }

    drawStepCaption (g, L.genCaption, "02", "GENERATE", s);
    drawStepCaption (g, L.playCaption, "03", "AUDITION", s);

    // --- Paneles de parametros ---
    for (size_t i = 0; i < L.panels.size(); ++i)
    {
        drawPanel (g, L.panels[i].frame.toFloat(), s);
        drawPanelTitle (g, L.panels[i].title, kPanelTitles[i], s);
    }

    // Separador sutil entre el titulo y los controles de cada panel
    g.setColour (kEdge.withAlpha (0.7f));
    for (const auto& p : L.panels)
        g.fillRect (p.frame.getX() + juce::roundToInt (14.0f * s),
                    p.frame.getY() + juce::roundToInt (34.0f * s),
                    p.frame.getWidth() - juce::roundToInt (28.0f * s), 1);

    // Descripcion de la opcion de corte
    g.setColour (cutToggle.getToggleState() ? kMuted : kInk.withAlpha (0.75f));
    g.setFont (uiFont (10.5f * s));
    g.drawFittedText (cutToggle.getToggleState()
                          ? "The file ends exactly where the vocal begins. Turn off to blend the DRY VOICE in."
                          : "The vocal is kept after the riser. The DRY VOICE level is now active.",
                      L.cutText, juce::Justification::topLeft, 4);
}

void RiserLabEditor::stepPreset (int delta)
{
    const int n = presetBox.getNumItems();
    if (n <= 0)
        return;

    int current = presetBox.getSelectedItemIndex();
    if (current < 0)
        current = proc.getCurrentProgram();

    const int next = ((current + delta) % n + n) % n;
    presetBox.setSelectedItemIndex (next, juce::sendNotificationSync);
}

void RiserLabEditor::refreshDependentControls (bool force)
{
    const bool reverbOn = reverbToggle.getToggleState();
    const bool cutOn = cutToggle.getToggleState();

    if (! force && reverbOn == lastReverbOn && cutOn == lastCutOn)
        return;

    lastReverbOn = reverbOn;
    lastCutOn = cutOn;

    auto dim = [this] (int index, bool shouldDim)
    {
        auto& k = knobs[(size_t) index];
        const float a = shouldDim ? 0.38f : 1.0f;
        k.slider.setAlpha (a);
        k.label.setAlpha (a);
    };

    // Sin reverb, sus controles no tienen efecto; con "cortar" activo, la voz seca tampoco.
    dim (kSize, ! reverbOn);
    dim (kDamping, ! reverbOn);
    dim (kReverbMix, ! reverbOn);
    reverbTypeBox.setAlpha (reverbOn ? 1.0f : 0.38f);
    dim (kDry, cutOn);

    repaint();
}

void RiserLabEditor::onCaptureClicked()
{
    if (! proc.isCapturing())
    {
        lastMessage.clear();
        lastIsError = false;
        proc.startCapture();
        captureButton.active = true;
        captureButton.repaint();
    }
    else
    {
        proc.stopCapture();
        captureButton.active = false;
        captureButton.repaint();
    }
}

void RiserLabEditor::onGenerateClicked()
{
    proc.stopCapture();
    captureButton.active = false;
    captureButton.repaint();

    juce::String error;
    if (proc.generateRiser (error))
    {
        dragArea.setData (proc.getRiserPeaks (300), proc.getRiserFile());
        hasRiser = true;
        lastIsError = false;
        lastMessage = "Riser generated. Drag it into your DAW.";
    }
    else
    {
        lastIsError = true;
        lastMessage = error;
    }
}

void RiserLabEditor::onPreviewClicked()
{
    if (proc.isPreviewPlaying())
    {
        proc.setPreviewPlaying (false);
        lastIsError = false;
        lastMessage = "Preview stopped.";
        return;
    }

    if (! proc.getRiserFile().existsAsFile())
    {
        lastIsError = true;
        lastMessage = "Generate a riser before auditioning it.";
        return;
    }

    proc.setPreviewPlaying (true);
    if (proc.isPreviewPlaying())
    {
        lastIsError = false;
        lastMessage.clear();
    }
    else
    {
        lastIsError = true;
        lastMessage = "There is no audio to play.";
    }
}

void RiserLabEditor::timerCallback()
{
    const bool capturing = proc.isCapturing();
    const bool previewing = proc.isPreviewPlaying();
    const double seconds = proc.getCapturedSeconds();

    // Boton de captura (se redibuja mientras graba para animar el halo)
    if (captureButton.active != capturing)
    {
        captureButton.active = capturing;
        captureButton.repaint();
    }

    const float progress = (float) juce::jlimit (0.0, 1.0, seconds / RiserLabProcessor::kMaxCaptureSeconds);
    if (capturing || std::abs (progress - captureButton.progress) > 0.0005f)
    {
        captureButton.progress = progress;
        captureButton.repaint();
    }

    // Play / Stop
    playButton.setLabel (previewing ? "STOP" : "PLAY",
                         previewing ? ActionButton::Icon::stop : ActionButton::Icon::play);
    playButton.setDimmed (! hasRiser);

    // Forma de onda: cursor de reproduccion
    const float prog = previewing ? proc.getPreviewProgress() : -1.0f;
    if (dragArea.playing != previewing
        || (previewing && prog >= 0.0f && std::abs (prog - dragArea.playProgress) > 0.0005f))
    {
        dragArea.playing = previewing;
        dragArea.playProgress = previewing ? (prog >= 0.0f ? prog : dragArea.playProgress) : -1.0f;
        dragArea.repaint();
    }

    // Estado
    StatusDisplay::State state = StatusDisplay::State::idle;
    juce::String msg;

    if (capturing)
    {
        state = StatusDisplay::State::recording;
        msg = "Recording. Play the vocal now.";
    }
    else if (previewing)
    {
        state = StatusDisplay::State::playing;
        msg = "Auditioning the generated riser.";
    }
    else if (lastMessage.isNotEmpty())
    {
        state = lastIsError ? StatusDisplay::State::error : StatusDisplay::State::ready;
        msg = lastMessage;
    }
    else if (hasRiser)
    {
        state = StatusDisplay::State::ready;
        msg = "Riser ready. Play it or drag it to your DAW.";
    }
    else if (seconds > 0.0)
    {
        state = StatusDisplay::State::captured;
        msg = "Vocal captured. Adjust settings, then Generate.";
    }
    else
    {
        state = StatusDisplay::State::idle;
        msg = "Press Capture and play your vocal.";
    }

    statusDisplay.update (state, msg, seconds);
    if (capturing || previewing)
        statusDisplay.repaint(); // animacion del indicador

    // Sincroniza el preset si el host cambia de programa
    const int program = proc.getCurrentProgram();
    if (presetBox.getSelectedItemIndex() != program)
    {
        presetBox.setSelectedItemIndex (program, juce::dontSendNotification);
        repaint();
    }

    refreshDependentControls (false);
}

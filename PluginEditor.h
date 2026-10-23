#pragma once

#include "PluginProcessor.h"
#include <array>

//==============================================================================
// Look & feel propio de RiserLab: knobs rotativos, combos, interruptores, menus.
class RiserLabLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RiserLabLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
};

//==============================================================================
class RiserLabEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit RiserLabEditor (RiserLabProcessor&);
    ~RiserLabEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Boton circular de captura con anillo de progreso (llenado del buffer de captura).
    struct CaptureControl : public juce::Button
    {
        CaptureControl() : juce::Button ("Capture") {}
        void paintButton (juce::Graphics&, bool highlighted, bool pressed) override;

        bool active = false;
        float progress = 0.0f;
    };

    // Boton de accion (Generate / Play / Stop) dibujado a medida.
    struct ActionButton : public juce::Button
    {
        enum class Icon { sparkle, play, stop };

        explicit ActionButton (const juce::String& name) : juce::Button (name) {}
        void setLabel (const juce::String& newText, Icon newIcon);
        void setDimmed (bool shouldBeDimmed);
        void paintButton (juce::Graphics&, bool highlighted, bool pressed) override;

        juce::String label;
        Icon icon = Icon::sparkle;
        bool primary = false;
        bool dimmed = false;
    };

    // Flechas de preset anterior / siguiente.
    struct ArrowButton : public juce::Button
    {
        explicit ArrowButton (bool pointsRight)
            : juce::Button (pointsRight ? "Next preset" : "Previous preset"), right (pointsRight) {}
        void paintButton (juce::Graphics&, bool highlighted, bool pressed) override;

        bool right = true;
    };

    // Pantalla de estado: tiempo capturado, estado y mensaje.
    struct StatusDisplay : public juce::Component,
                           public juce::SettableTooltipClient
    {
        enum class State { idle, recording, captured, playing, ready, error };

        void update (State newState, const juce::String& newMessage, double newSeconds);
        void paint (juce::Graphics&) override;

        State state = State::idle;
        juce::String message;
        double seconds = 0.0;
    };

    // Forma de onda del riser; arrastrar desde aqui exporta el WAV al DAW.
    struct DragArea : public juce::Component
    {
        void setData (std::vector<float> newPeaks, const juce::File& newFile);
        void paint (juce::Graphics&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }

        std::vector<float> peaks;
        juce::File file;
        bool playing = false;
        float playProgress = -1.0f;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    // Todas las posiciones de la interfaz, calculadas a partir del tamano actual.
    struct Layout
    {
        struct Panel
        {
            juce::Rectangle<int> frame, title;
            std::array<juce::Rectangle<int>, 3> slots;
        };

        float s = 1.0f;
        juce::Rectangle<int> headerLine;
        juce::Rectangle<int> mark, wordmark, subtitle;
        juce::Rectangle<int> presetCaption, prevButton, presetBox, nextButton, logo;
        juce::Rectangle<int> strip, captureButton, captureText;
        juce::Rectangle<int> genCaption, genButton, playCaption, playButton, display;
        std::array<juce::Rectangle<int>, 3> stripDividers;
        std::array<Panel, 4> panels;
        juce::Rectangle<int> reverbToggle, reverbTypeBox;
        juce::Rectangle<int> cutToggle, cutText;
        juce::Rectangle<int> drag;
    };

    Layout computeLayout() const;

    void timerCallback() override;
    void onCaptureClicked();
    void onGenerateClicked();
    void onPreviewClicked();
    void stepPreset (int delta);
    void refreshDependentControls (bool force);

    RiserLabProcessor& proc;

    // Debe declararse antes que los componentes que lo usan (se destruye despues).
    RiserLabLookAndFeel laf;

    CaptureControl captureButton;
    ActionButton generateButton { "Generate" };
    ActionButton playButton { "Play" };
    ArrowButton prevButton { false }, nextButton { true };
    StatusDisplay statusDisplay;
    juce::ComboBox presetBox, reverbTypeBox;
    juce::ToggleButton reverbToggle, cutToggle;
    std::unique_ptr<ButtonAttachment> reverbAttachment, cutAttachment;
    std::unique_ptr<ComboBoxAttachment> reverbTypeAttachment;
    std::array<Knob, 11> knobs;
    DragArea dragArea;
    juce::Image logoImage;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::String lastMessage;
    bool lastIsError = false;
    bool hasRiser = false;
    bool lastReverbOn = true;
    bool lastCutOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiserLabEditor)
};

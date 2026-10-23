#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>
#include "RiserEngine.h"

// Convierte un literal UTF-8 (con acentos, simbolos) a juce::String.
#define TXT(x) juce::String (juce::CharPointer_UTF8 (x))

class RiserLabProcessor : public juce::AudioProcessor
{
public:
    static constexpr double kMaxCaptureSeconds = 30.0;

    RiserLabProcessor();
    ~RiserLabProcessor() override = default;

    // --- AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RiserLab"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Captura y generacion del riser ---
    void startCapture();
    void stopCapture();
    bool isCapturing() const { return capturing.load(); }
    double getCapturedSeconds() const;

    // Genera el riser con los parametros actuales. Llamar desde el hilo de la interfaz.
    bool generateRiser (juce::String& errorMessage);
    void setPreviewPlaying (bool shouldPlay);
    bool isPreviewPlaying() const { return previewPlaying.load(); }
    juce::File getRiserFile() const;

    // Progreso de la vista previa (0..1) para el cursor de reproduccion de la interfaz.
    // Solo lectura; devuelve -1 si no se puede leer en este momento.
    float getPreviewProgress() const
    {
        juce::CriticalSection::ScopedTryLockType lock (riserLock);
        if (! lock.isLocked())
            return -1.0f;

        const int n = riserBuffer.getNumSamples();
        if (n <= 0)
            return -1.0f;
        return juce::jlimit (0.0f, 1.0f, (float) previewPosition.load() / (float) n);
    }

    std::vector<float> getRiserPeaks (int bins) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double sampleRateHz = 44100.0;

    // Captura (escrita desde el hilo de audio)
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<int> captureLength { 0 };
    std::atomic<bool> capturing { false };
    juce::SpinLock captureLock;

    // Ultimo riser generado
    mutable juce::CriticalSection riserLock;
    juce::AudioBuffer<float> riserBuffer;
    juce::File riserFile;
    std::atomic<bool> previewPlaying { false };
    std::atomic<int> previewPosition { 0 };

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiserLabProcessor)
};

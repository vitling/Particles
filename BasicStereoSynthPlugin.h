//
// Created by David Whiting on 2021-01-10.
//

#ifndef PARTICLES_PLUGIN_BASICSTEREOSYNTHPLUGIN_H
#define PARTICLES_PLUGIN_BASICSTEREOSYNTHPLUGIN_H

#include <JuceHeader.h>

class BasicStereoSynthPlugin : public AudioProcessor {
private:
    String name;
protected:
    // Shortcut for getting true (non-normalised) float values out of a parameter tree
    float getParameterValue(StringRef parameterName) {
        auto param = parameterState().getParameter(parameterName);
        return param->convertFrom0to1(param->getValue());
    }
public:
    BasicStereoSynthPlugin(const char *name): name(name), AudioProcessor(BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)) {}
    virtual ~BasicStereoSynthPlugin() = default;

    // Various metadata about the plugin
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {return (layouts.getMainOutputChannels() == 2);}
    bool hasEditor() const override { return true; }
    const String getName() const override { return name;}
    bool acceptsMidi() const override {return true;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {
        return 0.0;
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 1; }
    void setCurrentProgram (int index) override {}
    const String getProgramName (int index) override { return "Default Program"; }
    void changeProgramName (int index, const String& newName) override { }

    virtual AudioProcessorValueTreeState & parameterState() = 0;

    void getStateInformation (MemoryBlock& destData) override {
        auto stateToSave = parameterState().copyState();
        std::unique_ptr<XmlElement> xml (stateToSave.createXml());
        copyXmlToBinary(*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override {
        std::unique_ptr<XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
        if (xmlState != nullptr) {
            if (xmlState->hasTagName(parameterState().state.getType())) {
                parameterState().replaceState(ValueTree::fromXml(*xmlState));
            }
        }
    }
};

#endif //PARTICLES_PLUGIN_BASICSTEREOSYNTHPLUGIN_H

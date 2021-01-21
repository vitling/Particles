/*
    Copyright 2021 David Whiting

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PARTICLES_PLUGIN_BASICSTEREOSYNTHPLUGIN_H
#define PARTICLES_PLUGIN_BASICSTEREOSYNTHPLUGIN_H

#include <JuceHeader.h>

// Base class for a stereo synth plugin, extracting all the non-Particles specific code to make it easier to work with
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
    explicit BasicStereoSynthPlugin(const char *name): name(name), AudioProcessor(BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)) {}
    ~BasicStereoSynthPlugin() override = default;

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

    // This plugin doesn't use program/preset functionality, so these are defaulted out
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 1; }
    void setCurrentProgram (int index) override {}
    const String getProgramName (int index) override { return "Default Program"; }
    void changeProgramName (int index, const String& newName) override { }

    /** Override this to return a reference to your plugin's parameter state, which will facilitate saving and loading of state */
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

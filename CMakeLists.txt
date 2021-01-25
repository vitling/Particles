# Copyright 2021 David Whiting
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Minimum OS X deployment version" FORCE)

project(PARTICLES_PLUGIN VERSION 0.1.0)

add_subdirectory(JUCE)

juce_add_plugin(ParticlesPlugin
        ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/icon_1024.png                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
        ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/icon_128.png
        COMPANY_NAME Vitling                          # Specify the name of the plugin's author
        IS_SYNTH TRUE                       # Is this a synth or an effect?
        NEEDS_MIDI_INPUT TRUE               # Does the plugin need midi input?
        NEEDS_MIDI_OUTPUT FALSE              # Does the plugin need midi output?
        IS_MIDI_EFFECT FALSE                 # Is this plugin a MIDI effect?
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE    # Does the editor need keyboard focus?
        COPY_PLUGIN_AFTER_BUILD TRUE        # Should the plugin be installed to a default location after building?
        PLUGIN_MANUFACTURER_CODE Vitl               # A four-character manufacturer id with at least one upper-case character
        PLUGIN_CODE Part                            # A unique four-character plugin id with at least one upper-case character
        DESCRIPTION "Particle Simulation Synthesiser from Vitling"
        VST3_CATEGORIES "Instrument Synth Stereo"
        AU_MAIN_TYPE "kAudioUnitType_MusicDevice"
        FORMATS VST3 AU Standalone                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
        BUNDLE_ID "xyz.vitling.plugins.particles"
        HARDENED_RUNTIME_ENABLED TRUE
        PRODUCT_NAME "Particles")        # The name of the final executable, which can differ from the target name


juce_generate_juce_header(ParticlesPlugin)

target_sources(ParticlesPlugin PRIVATE
        ParticlesPlugin.cpp)

target_compile_definitions(ParticlesPlugin
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)



target_link_libraries(ParticlesPlugin PRIVATE
                    juce::juce_audio_utils
                    juce::juce_dsp)

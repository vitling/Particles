# Particles

Particles is an experimental audio synthesis plugin which creates notes by simulating particle collision, available as a VST/AU effect for use inside DAWs like Ableton, Logic, FL Studio etc.


![Screenshot](https://github.com/vitling/Particles/blob/main/screenshot.png?raw=true)

It takes MIDI input (eg. from a music keyboard), and uses it to generate particles in the simulation corresponding to the notes which are being played, with a velocity set by the velocity of the incoming MIDI note.

The simulation uses perfectly elastic Newtonian collision mechanics, and each particle will "ring" whenever it hits another, at a volume dependent on the force of the collision, and panned to where it happened in the simulation space.

The effect produced is something like a chaotic non-synced arpeggiator, that can be used for all kinds of interesting random and/or textural effects.

If you want to get an idea of the potential, then you can check out my previous iteration [Gas](https://www.vitling.xyz/toys/gas/) which is a webaudio-based implementation of the same concept with fixed notes. **Particles** is a C++/JUCE version with higher performance, has more features, and is designed more as a *instrument* and less as a *toy*.

## Installation

### Mac & Windows

Go to the [Vitling.xyz plugin download page](https://www.vitling.xyz/plugins) for conveniently packaged donwloads for **Mac** and **Windows**

#### Extra notes for Mac users

I have now finally joined the Apple Developer Program, so I am pleased to be able to offer proper installers and notarized binaries for Mac

However, the Apple Developer Program still costs money, even for an open source developer. I suggest you write to Apple and lawmakers in your jurisdiction and complain about their anti-competitive practices.

If you use the Mac version, please consider [donating some money](https://paypal.me/vitling) to me to offset the cost I have incurred to make this possible.

### Linux

Building on Linux should be a fairly straightforward `cmake` situation, but I don't use Linux for music making so I'm interested in hearing reports from users of Bitwig or other Linux DAWs.

#### Dependencies
```cmake g++ libfreetype6-dev libx11-dev libxinerama-dev libxrandr-dev libxcursor-dev mesa-common-dev libasound2-dev freeglut3-dev libxcomposite-dev pkg-config```

These are Debian/Ubuntu package names (install with `sudo apt-get install ` and paste the above), you may need to translate for your distro

## Support

If you find this useful, then please consider supporting my work. You can do that by buying the music of [Bow Church](https://bowchurch.bandcamp.com)
or [Vitling](https://vitling.bandcamp.com); or listen and add to playlists on [Spotify](https://open.spotify.com/artist/411BFEtPxIZ7sT8f1LqNio?si=-wEbeNCjTgGnu68zi835BA) and/or [SoundCloud](https://soundcloud.com/vitling). 

You can also see my [website](https://www.vitling.xyz), [Instagram](https://instagram.com/vvitling) or [Twitter](https://twitter.com/vvitling) to follow
my latest work; and/or contact me to hire me for stuff.

## License

This plugin is free software, licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html). 

However, the JUCE framework that it depends on as a submodule has [its own license](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md)
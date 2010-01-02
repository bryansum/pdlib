pdlib
===========

The idea for pdblib came from the [iJam](/projects/ijam) project,
which needed an audio back end. After looking around for existing solutions,
few synthesizers were free to use in commercial projects, 
and those that were were typically limited in the sounds they could produce. 
I was familiar with [Rjdj](http://rjdj.me)
and their decision to use [Pure Data](http://crca.ucsd.edu/~msp/software.html) 
as their audio synthesizer, but they hadn't opted to release their port.

One of the goals of pdlib was to minimize changes to the vanilla Pure Data;
as such, only the command-line parsing and the audio hardware interface
have been rewritten. It is based on Pure Data v0.42-5.  

The [project is currently hosted on Github](http://github.com/bryansum/pdlib) under 
the FreeBSD license for my contribution. Pure Data itself is under a BSD-style license.
Be aware, though, that PdListTest uses example code from Martin Peach for the Pure Data OSC
parts and his work is under GPL. 

As it stands, Pure Data wasn't really written to be embedded in other systems,
and was dependent on [PortAudio](http://www.portaudio.com/), which wasn't 
compatible with the iPhone's audio system. I ended up basically rewriting the audio
hardware interface layer of Pure Data to use the iPhone's RemoteIO Audio unit. 
Apple's documentation for that system is sparse, so I found articles by
[Mike Tyson](http://atastypixel.com/blog/2008/11/04/using-remoteio-audio-unit/) 
and [Chris Adamson](http://www.subfurther.com/blog/?p=507) to be invaluable.

The current iteration of pdlib doesn't explicitly support any communication between
the host programming environment and the Pure Data system. What I've done for
iJam is use Martin Peach's `osc` patches, specifically `routeosc`, along with his 
`udpreceive` patch to receive audio events from the Objective-C code on a specific port.
Then we use [liblo](http://liblo.sourceforge.net/) to send OSC messages so 
communication is all socket-based. This was done in part because Pure Data is 
*not* thread-safe, and as such the library runs as a singleton. PdLibTest, included
in the release, shows how pdlib can be used in an Xcode project.

Installation
--------------------

See INSTALL.markdown in the distribution for installation instructions.

Usage
--------------------

`PdController` is an Objective-C singleton, and has a number of properties that
can be set to change how Pure Data is configured. Note that changes in these 
properties or only reflected in Pure Data after a start or restart of the system.
The properties include:

- `soundRate`: the frequency of Pure Data in Hz. Defaults to 22050
- `blockSize`: How many samples to generate for each DSP tick. Smaller blocks sizes
  mean greater latency, but is less processor intensive. I've defaulted to 256,
  while vanilla Pure Data is set to 64. Because of this, this value needs to be
  a multiple of 64.
- `nOutChannels`: 1 or 2. How many channels we want to support. Set to 2 for now.
  Changing to mono has not been tested, but *should* work.
- `callbackFn`: the function that is to be called on every DSP tick. This is 
  often useful as a timer mechanism for UI events, etc. The callback function 
  should be processed quickly, as this is running on CoreAudio's real-time 
  thread. I typically use it to call GUI events asynchronously, etc.
  
See PdListTest for a working example.

Future improvements
---------------------

- Add a mechanism to directly send messages to Pd via PdController
- Add support for microphone input (ADC~) 
- Add test cases
- Verify that `openFile:` is working correctly


pdlib
====

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
*not* thread-safe, and as such the library runs as a singleton. 

Usage
--------

See INSTALL.markdown for installation instructions.

Future improvements
---------------------

- Add a mechanism to directly send messages to Pd via PdController
- Add support for microphone input (ADC~) 
- Add test cases
- Verify that `openFile:` is working correctly


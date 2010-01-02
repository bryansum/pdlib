/*
 AudioOutput.c
 PdLib
 
 Copyright 2010 Bryan Summersett. All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice, this list of
 conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright notice, this list
 of conditions and the following disclaimer in the documentation and/or other materials
 provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY BRYAN SUMMERSETT ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BRYAN SUMMERSETT OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 The views and conclusions contained in the software and documentation are those of the
 authors and should not be interpreted as representing official policies, either expressed
 or implied, of Bryan Summersett.
 
 */

#import "AudioOutput.h"
#import "m_pd.h"
#import "s_stuff.h"

#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)<(b)?(b):(a))

static const UInt32 kOutputBus = 0;

static void interruptionListener(void *inClientData, UInt32 inInterruption)
{
    fprintf(stderr,"Session interrupted! %s, ", 
            inInterruption == kAudioSessionBeginInterruption ? "Begin Interruption" : "End Interruption");
	
	AudioCallbackParams *params = (AudioCallbackParams*)inClientData;
	
	if (inInterruption == kAudioSessionEndInterruption) {
		// make sure we are again the active session
		AudioSessionSetActive(true);
		AudioOutputUnitStart(params->audioUnit);
	}
	
	if (inInterruption == kAudioSessionBeginInterruption) {
        AudioSessionSetActive(false);
		AudioOutputUnitStop(params->audioUnit);
    }
}

static OSStatus playCallback(void *inRefCon,
                             AudioUnitRenderActionFlags *ioActionFlags __attribute__((unused)),
                             const AudioTimeStamp *inTimeStamp,
                             UInt32 inBusNumber,
                             UInt32 inNumberFrames,
                             AudioBufferList *ioData)
{
    assert(inBusNumber == kOutputBus);
    assert(ioData->mNumberBuffers == 1);  // doing one callback, right?
    assert(sizeof(t_sample) == 4);  // assume 32-bit floats here

    // sys_schedblocksize is How many frames we have per PD dsp_tick
    // inNumberFrames is how many CoreAudio wants
    
    // Make sure we're dealing with evenly divisible numbers between
    // the number of frames CoreAudio needs vs the block size for a given PD dsp tick.
    //Otherwise this looping scheme we're doing below doesn't make much sense.
    assert(inNumberFrames % sys_schedblocksize == 0);

    // How many times to generate a DSP event in PD
    UInt32 times = inNumberFrames / sys_schedblocksize;
    
    AudioBuffer *buffer = &ioData->mBuffers[0];
    SInt16 *data = (SInt16*) buffer->mData;

    AudioCallbackParams *params = (AudioCallbackParams*)inRefCon;

    for(UInt32 i = 0; i < times; i++) {

        sys_lock();
        (*params->callback)(inTimeStamp);
        sys_unlock();
                
        // This should cover sys_noutchannels channels. Turns non-interleaved into 
        // interleaved audio.
        for (int n = 0; n < sys_schedblocksize; n++) {
            for(int ch = 0; ch < sys_noutchannels; ch++) {
                t_sample fsample = sys_soundout[n+sys_schedblocksize*ch];
                SInt16 sample = (SInt16)(fsample * 32767.0);
                data[(n*sys_noutchannels+ch) + // offset in iteration
                     i*sys_schedblocksize*sys_noutchannels] // iteration starting address
                = sample;
            }
        }
        
        // After loading the samples, we need to clear them for the next iteration
        memset(sys_soundout, 0, sizeof(t_sample)*sys_noutchannels*sys_schedblocksize);        
    }

    return noErr;
}

OSStatus AudioOutputInitialize(AudioCallbackParams *params)
{    
    // Initialize Audio session
    OSStatus status;
    
    if ((status = AudioSessionInitialize(NULL, NULL, interruptionListener, params))) {
        fprintf(stderr,"couldn't initialize audio session");
        return status;
    }
        
    if ((status = AudioSessionSetActive(true))) {
        fprintf(stderr,"couldn't set active audio session");
        return status;
    }
    
    UInt32 audioCategory = kAudioSessionCategory_PlayAndRecord;
    
    if ((status = AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, 
                                          sizeof(audioCategory), 
                                          &audioCategory))) {
        fprintf(stderr,"couldn't set up audio category");
        return status;
    }
    // Now set output format
    
    AudioStreamBasicDescription outputFormat;
    memset(&outputFormat, 0, sizeof(outputFormat));
    outputFormat.mSampleRate = sys_dacsr;
    outputFormat.mFormatID = kAudioFormatLinearPCM;
        
    // Used a signed integer as per http://www.subfurther.com/blog/?p=507        
    outputFormat.mFormatFlags  = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    outputFormat.mFramesPerPacket = 1; // always this for PCM audio
    outputFormat.mBytesPerPacket = sizeof(SInt16) * sys_noutchannels;
    outputFormat.mBytesPerFrame = sizeof(SInt16) * sys_noutchannels;
    outputFormat.mChannelsPerFrame = sys_noutchannels;
    outputFormat.mBitsPerChannel = 8 * sizeof(SInt16);
    outputFormat.mReserved = 0;
    
    // Describe audio component
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    // Get component
    AudioComponent outputComponent = AudioComponentFindNext(NULL, &desc);
    if (outputComponent == NULL) {
        fprintf(stderr,"AudioComponentFindNext failed");
        return status;
    }
    
    // Get audio units
    status = AudioComponentInstanceNew(outputComponent, &params->audioUnit);
    if (status) {
        fprintf(stderr,"AudioComponentInstanceNew failed");
        return status;
    }
    
    // Enable playback
    UInt32 enableIO = 1;
    status = AudioUnitSetProperty(params->audioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  kOutputBus,
                                  &enableIO,
                                  sizeof(UInt32));
    if (status) {
        fprintf(stderr,"AudioUnitSetProperty EnableIO (out)");
        return status;
    }
    
    // Apply format
    status = AudioUnitSetProperty(params->audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  kOutputBus,
                                  &outputFormat,
                                  sizeof(AudioStreamBasicDescription));
    if (status) {
        fprintf(stderr,"AudioUnitSetProperty StreamFormat");
        return status;
    }
    
    AURenderCallbackStruct callback;
    callback.inputProc = &playCallback;
    callback.inputProcRefCon = params;
    
    // Set output callback
    status = AudioUnitSetProperty(params->audioUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  kOutputBus,
                                  &callback,
                                  sizeof(AURenderCallbackStruct));
    if (status) {
        fprintf(stderr,"AudioUnitSetProperty SetRenderCallback");
        return status;
    }
    
    status = AudioUnitInitialize(params->audioUnit);
    if (status) {
        fprintf(stderr,"AudioUnitInitialize failure");
        return status;
    }  
    
    status = AudioOutputUnitStart(params->audioUnit);
    if (status) {
        fprintf(stderr,"AudioOutputUnitStart failure");
        return status;
    }
    
    return noErr;
}

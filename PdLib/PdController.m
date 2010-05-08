/*
 PdController.m
 
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

#import "PdController.h"
#import "s_main.h"
#import <unistd.h>

static const NSInteger kDefaultSoundRate = 22050;
static const NSInteger kDefaultBlockSize = 256;
static const NSInteger kDefaultNChannels = 2;

static PdController *sharedSingleton = nil;

@implementation PdController

@synthesize externs, openfiles, libdir, soundRate, blockSize, nOutChannels, nInChannels, callbackFn;

+ (PdController*)sharedController
{
    if (sharedSingleton == nil) {
        sharedSingleton = [[super allocWithZone:NULL] init];
    }
    return sharedSingleton;
}

- (id)retain
{
    return self;
}

- (NSUInteger)retainCount
{
    return NSUIntegerMax;  //denotes an object that cannot be released
}

- (void)release
{
    //do nothing
}

- (id)autorelease
{
    return self;
}

#pragma mark public methods

-(int)openFile:(NSString*)path
{
    NSLog(@"Warning: opening files is still experimental");
    
    @synchronized(self) {
        if (![pdThread isExecuting]) {
            NSLog(@"pdlib is not currently running");
            return -1; 
        }        
    }

    NSArray *components = [path componentsSeparatedByString: @"/"];
    NSString *filename = [components lastObject];
    NSMutableArray *baseComponents = [NSMutableArray arrayWithArray:components];
    [baseComponents removeLastObject];
    NSString *dirname = [baseComponents componentsJoinedByString:@"/"];
    
    // obtain system-wide mutex so we don't race with our DSP callbacks (see AudioOutput.c)
    sys_lock();
    int r_value = openit([dirname UTF8String], [filename UTF8String]);
    sys_unlock();
    return r_value;
}

-(void)pdThreadMain
{    
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    if (!libdir) {
        NSLog(@"PdLib: libdir must be specified");
        return;
    }
    if (!soundRate || !blockSize || !(blockSize % 64 == 0) || !nOutChannels || !nInChannels) {
        NSLog(@"pd audio settings (soundRate, blockSize, nOutChannels, nInChannels) must be specified. \
              Additionally, blockSize must be a multiple of 64");
    }
    
    // shouldn't ever exit unless we explicitly call stop()
    sys_main([libdir UTF8String],
             [[externs componentsJoinedByString:@":"] UTF8String],
             [[openfiles componentsJoinedByString:@":"] UTF8String],
             [libdir UTF8String],
             soundRate,
             blockSize,
             nOutChannels,
             nInChannels,
             callbackFn);
    
    [pool drain];
}

    
-(void)start
{
    @synchronized(self) {
        if (![pdThread isExecuting]) {
            [pdThread start];
        }        
    }
}
    
-(void)stop
{
    @synchronized(self) {
        if ([pdThread isExecuting]) {
            sys_exit(); 
        }        
    }
}

-(void)restart
{
    @synchronized(self) {
        [self stop];
        
        // TODO: May not be necessary. But [self stop] is asynchronous, 
        // so there's a delay until the next DSP tick before pd actually stops.
        sleep(1);
        
        [self start];
    }
}

- (id) init
{
    self = [super init];
    if (self != nil) {
        pdThread = [[NSThread alloc] initWithTarget:self 
                                           selector:@selector(pdThreadMain)
                                             object:nil];
        externs = [[NSArray alloc] init];
        openfiles = [[NSArray alloc] init];
        libdir = NULL;
        soundRate = kDefaultSoundRate;
        blockSize = kDefaultBlockSize;
        nOutChannels = kDefaultNChannels;
        nInChannels = kDefaultNChannels;
        callbackFn = NULL;
    }
    
    return self;
}

- (void) dealloc
{
    [self stop];
    [pdThread release];
    [externs release];
    [openfiles release];
    [libdir release];
    [super dealloc];
}


@end

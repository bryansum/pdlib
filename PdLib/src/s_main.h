/*
 
 s_main.h
 
 pdlib is an open source port of Pure Data to the iPhone.
 Copyright (C) 2010 Bryan Summersett
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#import "AudioOutput.h"

int sys_main(const char *libdir, 
             const char *externs, 
             const char *openfiles,
             const char *searchpath,
             int soundRate,
             int blockSize,
             int nOutChannels,
             AudioCallbackFn callback);

int openit(const char *dirname, const char *filename);

void sys_send_msg(const char *msg);

void sys_exit(void);

// global lock for PD
void sys_lock(void);
void sys_unlock(void);

extern int sys_hasstarted;

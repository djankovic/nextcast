//
//  CurrentTrackOSX.m
//  butt
//
//  Created by Melchor Garau Madrigal on 8/7/15.
//  Copyright (c) 2015 Daniel Nöthen. All rights reserved.
//

#import "CurrentTrackOSX.h"
#import "iTunes.h"
#import "Spotify.h"
#import "VOX.h"

const char* getCurrentTrackFromiTunes(int artist_title_order) {
    NSProcessInfo *pInfo = [NSProcessInfo processInfo];
    NSOperatingSystemVersion version = [pInfo operatingSystemVersion];
    
    char* ret = NULL;
    iTunesApplication *iTunes;
    if(version.majorVersion < 11 && version.minorVersion < 15)
        iTunes = [SBApplication applicationWithBundleIdentifier:@"com.apple.itunes"];
    else
        iTunes = [SBApplication applicationWithBundleIdentifier:@"com.apple.music"];
    
    if([iTunes isRunning]) {
        if([iTunes playerState] != iTunesEPlSStopped) {
            NSString* track;
            if(artist_title_order == 0) {
                track = [NSString stringWithFormat:@"%@ - %@", [[iTunes currentTrack] artist], [[iTunes currentTrack] name]];
            }
            else {
                track = [NSString stringWithFormat:@"%@ - %@", [[iTunes currentTrack] name], [[iTunes currentTrack] artist]];
            }
            ret = (char*) malloc([track lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1);
            [track getCString:ret maxLength:([track lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1) encoding:NSUTF8StringEncoding];
        }
    }
    
    return ret;
}

const char* getCurrentTrackFromSpotify(int artist_title_order) {
    char* ret = NULL;
    SpotifyApplication *spotify = [SBApplication applicationWithBundleIdentifier:@"com.spotify.client"];
    if([spotify isRunning]) {
        if([spotify playerState] != SpotifyEPlSStopped) {
            NSString* track;
            if(artist_title_order == 0) {
                track = [NSString stringWithFormat:@"%@ - %@", [[spotify currentTrack] artist], [[spotify currentTrack] name]];
            }
            else {
                track = [NSString stringWithFormat:@"%@ - %@", [[spotify currentTrack] name], [[spotify currentTrack] artist]];
            }
            ret = (char*) malloc([track lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1);
            [track getCString:ret maxLength:([track lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1) encoding:NSUTF8StringEncoding];
        }
    }
    
    return ret;
}

const char* getCurrentTrackFromVOX(int artist_title_order) {
    char* ret = NULL;
    VOXApplication *vox = [SBApplication applicationWithBundleIdentifier:@"com.coppertino.Vox"];
    if([vox isRunning]) {
        if([vox playerState]) {
            NSString* track;
            if(artist_title_order == 0) {
                track = [NSString stringWithFormat:@"%@ - %@", [vox artist], [vox track]];
            }
            else {
                track = [NSString stringWithFormat:@"%@ - %@", [vox track], [vox artist]];
            }
            ret = (char*) malloc([track lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1);
            [track getCString:ret maxLength:([track lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1) encoding:NSUTF8StringEncoding];
        }
    }
    
    return ret;
}

typedef const char* (*currentTrackFunction)(int);
currentTrackFunction getCurrentTrackFunctionFromId(int i) {
    switch(i) {
        case 0: return &getCurrentTrackFromiTunes;
        case 1: return &getCurrentTrackFromSpotify;
        case 2: return &getCurrentTrackFromVOX;
        default: return NULL;
    }
}

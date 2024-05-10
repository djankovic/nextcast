//
//  CurrentTrackOSX.h
//  butt
//
//  Created by Melchor Garau Madrigal on 8/7/15.
//  Copyright (c) 2015 Daniel Nöthen. All rights reserved.
//

#import <Foundation/Foundation.h>

/**
 * Gets the current track from iTunes in a string formated as:
 * TITLE - ARTIST, or NULL if iTunes is closed or stopped.
 */
const char *getCurrentTrackFromiTunes(int);

/**
 * Gets the current track from Spotify in a string formated as:
 * TITLE - ARTIST, or NULL if Spotify is closed or stopped.
 */
const char *getCurrentTrackFromSpotify(int);

/**
 * Gets the current track from VOX in a string formated as:
 * TITLE - ARTIST, or NULL if VOX is closed or stopped.
 */
const char *getCurrentTrackFromVOX(int);

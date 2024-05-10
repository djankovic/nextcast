//
//  AskForMicPermission.m
//  butt
//
//  Created by Daniel Nöthen on 26.09.20.
//  Copyright © 2020 Daniel Nöthen. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>
#import "gettext.h"


void showAlert() {
    
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@(_("Microphone access"))];
        [alert setInformativeText:@(_("butt needs access to your microphone\nPlease go to\nSystem Preferences->Security & Privacy->Privacy->Microphone\nand activate the check mark next to the butt entry"))];
        [alert addButtonWithTitle:@"Ok"];
        [alert runModal];
    });
}

void askForMicPermission()
{
    if (@available(macOS 10.14, *)) {
        // Request permission to access the camera and microphone.
        switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
        {
            case AVAuthorizationStatusAuthorized:
            {
                break;
            }
            case AVAuthorizationStatusNotDetermined:
            {
                // The app hasn't yet asked the user for microphone access.
                [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
                    if (granted) {
                    } else {
                        showAlert();
                    }
                }];
                
                break;
            }
            case AVAuthorizationStatusDenied:
            {
                // The user has previously denied access.
                showAlert();
                return;
            }
            case AVAuthorizationStatusRestricted:
            {
                showAlert();
                // The user can't grant access due to restrictions.
                return;
            }
        }
    }
}

#import <AppKit/AppKit.h>
#include "mac_view_snapshot.hpp"
#include "platform.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

void capture_csv_native_view() {
    if (![NSThread isMainThread])
        throw std::runtime_error("Native view capture requires the UI thread.");
    @autoreleasepool {
        NSWindow *selected = nil;
        NSArray<NSWindow *> *const windows = [NSApp windows];
        const NSUInteger count = [windows count];
        for (NSUInteger index = 0; index < count; ++index) {
            NSWindow *const candidate = [windows objectAtIndex:index];
            NSString *const title = [candidate title];
            if ([title isEqualToString:@"SwiftEdit CSV native regression"]) {
                if (selected != nil)
                    throw std::runtime_error("Native CSV capture title is ambiguous.");
                selected = candidate;
            }
        }
        if (selected == nil || ![selected isVisible])
            throw std::runtime_error("Native CSV capture window is not visible.");
        NSView *const content = [selected contentView];
        const NSRect bounds = [content bounds];
        if (content == nil || bounds.size.width <= 0 || bounds.size.height <= 0 ||
            bounds.size.width > 4096 || bounds.size.height > 4096)
            throw std::runtime_error("Native CSV capture has invalid view bounds.");
        NSBitmapImageRep *const bitmap = [content bitmapImageRepForCachingDisplayInRect:bounds];
        if (bitmap == nil)
            throw std::runtime_error("Native CSV bitmap allocation failed.");
        [content cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
        NSData *const png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        if (png == nil || [png length] == 0)
            throw std::runtime_error("Native CSV PNG encoding failed.");
        const std::filesystem::path directory = std::filesystem::current_path() /
            ("native-visual-csv-" + std::to_string(test_process_id()));
        const bool created = std::filesystem::create_directory(directory);
        if (!created)
            throw std::runtime_error("Native CSV evidence directory already exists.");
        const std::filesystem::path path = directory / "csv.png";
        const std::string encoded_path = path.string();
        NSString *const destination = [NSString stringWithUTF8String:encoded_path.c_str()];
        if (destination == nil || ![png writeToFile:destination atomically:YES])
            throw std::runtime_error("Native CSV PNG write failed.");
        std::cout << "Native CSV AppKit view capture: " << encoded_path << " pixels="
                  << [bitmap pixelsWide] << 'x' << [bitmap pixelsHigh]
                  << "; view redraw, not a compositor screenshot or visual assertion.\n";
    }
}

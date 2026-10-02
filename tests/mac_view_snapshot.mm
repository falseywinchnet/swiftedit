#import <AppKit/AppKit.h>
#include "mac_view_snapshot.hpp"
#include "platform.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

void capture_native_view(const char *const title, const char *const evidence_name) {
    if (![NSThread isMainThread])
        throw std::runtime_error("Native view capture requires the UI thread.");
    @autoreleasepool {
        NSString *const expected_title = [NSString stringWithUTF8String:title];
        if (expected_title == nil)
            throw std::runtime_error("Native capture title is not UTF-8.");
        NSWindow *selected = nil;
        NSArray<NSWindow *> *const windows = [NSApp windows];
        const NSUInteger count = [windows count];
        for (NSUInteger index = 0; index < count; ++index) {
            NSWindow *const candidate = [windows objectAtIndex:index];
            NSString *const candidate_title = [candidate title];
            if ([candidate_title isEqualToString:expected_title]) {
                if (selected != nil)
                    throw std::runtime_error("Native capture title is ambiguous.");
                selected = candidate;
            }
        }
        if (selected == nil || ![selected isVisible])
            throw std::runtime_error("Native capture window is not visible.");
        NSView *const content = [selected contentView];
        const NSRect bounds = [content bounds];
        if (content == nil || bounds.size.width <= 0 || bounds.size.height <= 0 ||
            bounds.size.width > 4096 || bounds.size.height > 4096)
            throw std::runtime_error("Native capture has invalid view bounds.");
        NSBitmapImageRep *const bitmap = [content bitmapImageRepForCachingDisplayInRect:bounds];
        if (bitmap == nil)
            throw std::runtime_error("Native bitmap allocation failed.");
        [content cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
        NSData *const png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        if (png == nil || [png length] == 0)
            throw std::runtime_error("Native PNG encoding failed.");
        const std::filesystem::path directory = std::filesystem::current_path() /
            ("native-visual-" + std::string(evidence_name) + "-" + std::to_string(test_process_id()));
        const bool created = std::filesystem::create_directory(directory);
        if (!created)
            throw std::runtime_error("Native evidence directory already exists.");
        const std::filesystem::path path = directory / "view.png";
        const std::string encoded_path = path.string();
        NSString *const destination = [NSString stringWithUTF8String:encoded_path.c_str()];
        if (destination == nil || ![png writeToFile:destination atomically:YES])
            throw std::runtime_error("Native PNG write failed.");
        std::cout << "Native AppKit view capture: " << encoded_path << " pixels="
                  << [bitmap pixelsWide] << 'x' << [bitmap pixelsHigh]
                  << "; view redraw, not a compositor screenshot or visual assertion.\n";
    }
}

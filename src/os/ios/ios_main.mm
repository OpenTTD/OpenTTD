/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ios_main.mm Main entry for iOS. */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../crashlog.h"
#include "../../core/random_func.hpp"
#include "../../string_func.h"
#include "../../survey.h"
#include "../../3rdparty/fmt/format.h"
#include "ios.h"
#include "../../safeguards.h"
#include "../../saveload/saveload.h"
#include "../../command_func.h"
#include "../../misc_cmd.h"
#include "../../network/network.h"

#import <UIKit/UIKit.h>
#include <thread>
#include <span>
#include <sys/utsname.h>
#include <unistd.h>

extern int openttd_main(std::span<std::string_view> arguments);

static std::string _ios_documents_path;
static std::string _ios_bundle_path;

std::string GetIOSDocumentsPath()
{
	return _ios_documents_path;
}

std::string GetIOSBundlePath()
{
	return _ios_bundle_path;
}

void ShowIOSDialog(std::string_view title, std::string_view message, std::string_view button_label)
{
	dispatch_async(dispatch_get_main_queue(), ^{
		NSString *nsTitle = [NSString stringWithUTF8String:std::string(title).c_str()];
		NSString *nsMessage = [NSString stringWithUTF8String:std::string(message).c_str()];
		NSString *nsButton = [NSString stringWithUTF8String:std::string(button_label).c_str()];

		UIAlertController *alert = [UIAlertController alertControllerWithTitle:nsTitle message:nsMessage preferredStyle:UIAlertControllerStyleAlert];
		[alert addAction:[UIAlertAction actionWithTitle:nsButton style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
			if ([nsButton isEqualToString:@"Quit"]) {
				exit(0);
			}
		}]];

		UIViewController *rootVC = [[UIApplication sharedApplication] keyWindow].rootViewController;
		while (rootVC.presentedViewController) {
			rootVC = rootVC.presentedViewController;
		}
		[rootVC presentViewController:alert animated:YES completion:nil];
	});
}

void ShowMacDialog(std::string_view title, std::string_view message, std::string_view button_label)
{
	ShowIOSDialog(title, message, button_label);
}

/**
 * Show an error message box.
 * @param buf Error message text.
 * @param system True if this is a system error.
 */
void ShowOSErrorBox(std::string_view buf, bool system)
{
	if (system) {
		ShowIOSDialog("OpenTTD has encountered an error", buf, "Quit");
	} else {
		ShowIOSDialog(buf, "See the readme for more info.", "Quit");
	}
}

/**
 * Open a URL in the system browser.
 * @param url URL to open.
 */
void OSOpenBrowser(const std::string &url)
{
	dispatch_async(dispatch_get_main_queue(), ^{
		NSURL *nsUrl = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
		[[UIApplication sharedApplication] openURL:nsUrl options:@{} completionHandler:nil];
	});
}

/**
 * Determine and return the current user's locale.
 * @param param Unused.
 * @return The locale string, or nullopt if not available.
 */
std::optional<std::string> GetCurrentLocale(const char *)
{
	NSArray *languages = [NSLocale preferredLanguages];
	if ([languages count] == 0) return std::nullopt;
	
	NSString *preferredLang = [languages objectAtIndex:0];
	if (preferredLang == nil || [preferredLang length] == 0) return std::nullopt;
	
	return std::string([preferredLang UTF8String]);
}

/**
 * Survey information about the OS.
 * @param json JSON object to fill with OS information.
 */
void SurveyOS(nlohmann::json &json)
{
	auto [ver_maj, ver_min, ver_bug] = GetMacOSVersion();
	struct utsname uts;
	uname(&uts);
	
	json["os"] = "iOS";
	json["release"] = fmt::format("{}.{}.{}", ver_maj, ver_min, ver_bug);
	json["machine"] = uts.machine;
	
	json["memory"] = SurveyMemoryToText(MacOSGetPhysicalMemory());
	json["hardware_concurrency"] = std::thread::hardware_concurrency();
}

std::tuple<int, int, int> GetMacOSVersion()
{
	NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
	return std::make_tuple((int)version.majorVersion, (int)version.minorVersion, (int)version.patchVersion);
}

void MacOSSetThreadName(const std::string &name)
{
	[[NSThread currentThread] setName:[NSString stringWithUTF8String:name.c_str()]];
}

uint64_t MacOSGetPhysicalMemory()
{
	return [NSProcessInfo processInfo].physicalMemory;
}

void CocoaSetupAutoreleasePool()
{
	// No-op on iOS ARC/modern runtime usually, or handled by main's autoreleasepool
}

void CocoaReleaseAutoreleasePool()
{
	// No-op
}

@interface OTTDAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation OTTDAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
	// Initialize paths early
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if ([paths count] > 0) {
		_ios_documents_path = [[paths firstObject] UTF8String];
	}
	_ios_bundle_path = [[[NSBundle mainBundle] bundlePath] UTF8String];

	// Set working directory to app bundle so OpenTTD can find resources
	chdir(_ios_bundle_path.c_str());
	NSLog(@"[iOS] Set working directory to bundle: %s", _ios_bundle_path.c_str());

	return YES;
}

- (UISceneConfiguration *)application:(UIApplication *)application configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession options:(UISceneConnectionOptions *)options {
	return [[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
}

- (void)application:(UIApplication *)application didDiscardSceneSessions:(NSSet<UISceneSession *> *)sceneSessions {
	// Called when user discards a scene session
}

@end

@interface OTTDSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation OTTDSceneDelegate

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)connectionOptions {
	if (![scene isKindOfClass:[UIWindowScene class]]) {
		return;
	}
	
	UIWindowScene *windowScene = (UIWindowScene *)scene;
	
	// Create window with the scene
	self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
	self.window.backgroundColor = [UIColor blackColor];
	
	// Create a basic view controller for now (video driver will replace this)
	UIViewController *rootVC = [[UIViewController alloc] init];
	rootVC.view.backgroundColor = [UIColor blackColor];
	self.window.rootViewController = rootVC;
	[self.window makeKeyAndVisible];
	
	// Start OpenTTD in a background thread
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
			std::vector<std::string_view> params;
			params.push_back("openttd");

			CrashLog::InitialiseCrashLog();
			SetRandomSeed(static_cast<uint32_t>(time(nullptr)));

			openttd_main(std::span<std::string_view>(params));
		});
	});
}

- (void)sceneDidDisconnect:(UIScene *)scene {
	// Called when scene is released by the system
}

- (void)sceneDidBecomeActive:(UIScene *)scene {
	// Called when scene moved from inactive to active state
}

- (void)sceneWillResignActive:(UIScene *)scene {
	// Called when scene moves from active to inactive state
	if (_game_mode == GM_NORMAL) {
		Command<Commands::Pause>::Post(PauseMode::Normal, true);
	}
}

- (void)sceneWillEnterForeground:(UIScene *)scene {
	// Called as scene transitions from background to foreground
}

- (void)sceneDidEnterBackground:(UIScene *)scene {
	// Called as scene transitions from foreground to background
	if (_game_mode == GM_NORMAL) {
		DoExitSave();
	}
}

@end

int main(int argc, char * argv[]) {
	@autoreleasepool {
		return UIApplicationMain(argc, argv, nil, NSStringFromClass([OTTDAppDelegate class]));
	}
}

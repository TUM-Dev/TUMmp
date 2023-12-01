/* vmp-server - A virtual multimedia processor
 * Copyright (C) 2023 Hugo Melder
 *
 * SPDX-License-Identifier: MIT
 */

#import "VMPConfigModel.h"
#import "VMPPipelineManager.h"
#import "VMPProfileManager.h"
#import "VMPProfileModel.h"
#import "VMPServerMain.h"

#import <gst/rtsp-server/rtsp-server.h>

/**
	@brief RTSP server class

	This class is responsible for setting up the RTSP server, and
	creating the pipeline channels.

	Currently, this class is a wrapper around the GStreamer RTSP server, and additionally
	initialises the ingress pipelines.
*/
@interface VMPRTSPServer : NSObject <VMPPipelineManagerDelegate>

/**
	@brief Server configuration for configuring RTSP server

	@note This property is readonly, and can only be set during initialisation.
*/
@property (readonly) VMPConfigModel *configuration;

/**
	@brief The current pipeline profile

	@note This property is readonly, and is automatically set during initialisation.
*/
@property (readonly) VMPProfileModel *currentProfile;

+ (instancetype)serverWithConfiguration:(VMPConfigModel *)configuration
								profile:(VMPProfileModel *)profile;
- (instancetype)initWithConfiguration:(VMPConfigModel *)configuration
							  profile:(VMPProfileModel *)profile;

- (VMPPipelineManager *)pipelineManagerForChannel:(NSString *)channel;

- (NSArray *)channelInfo;

/**
	@brief Start the RTSP server

	@return YES if the server was started successfully, NO otherwise.
*/
- (BOOL)startWithError:(NSError **)error;

/**
	@brief Stop the RTSP server

	You should not restart the server after calling this method.
*/
- (void)stop;

@end
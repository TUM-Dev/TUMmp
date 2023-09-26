/* vmp-server - A virtual multimedia processor
 * Copyright (C) 2023 Hugo Melder
 *
 * SPDX-License-Identifier: MIT
 */

#import <systemd/sd-journal.h>

#import "VMPJournal.h"

// Generated project configuration
#include "../build/config.h"

@interface VMPJournal ()
@property (nonatomic, readwrite) NSString *subsystem;
@end

@interface NSMutableArray (VMPJournal)
- (const struct iovec *)UTF8StringIOVec;
@end

@implementation NSMutableArray (VMPJournal)
- (const struct iovec *)UTF8StringIOVec {
	struct iovec *iovec = malloc(sizeof(struct iovec) * [self count]);
	if (iovec == NULL) {
		return NULL;
	}

	for (int i = 0; i < [self count]; i++) {
		NSString *string = [self objectAtIndex:i];
		iovec[i].iov_base = (void *) [string UTF8String];
		iovec[i].iov_len = [string lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	}
	return iovec;
}
@end

@implementation VMPJournal

static VMPJournal *defaultJournal = nil;

+ (VMPJournal *)defaultJournal {
	if (defaultJournal == nil) {
		defaultJournal = [[VMPJournal alloc] init];
	}
	return defaultJournal;
}

+ (VMPJournal *)journalWithSubsystem:(NSString *)subsystem {
	return [[VMPJournal alloc] initWithSubsystem:subsystem];
}

- (instancetype)initWithSubsystem:(NSString *)subsystem {
	self = [super init];
	if (self) {
		_subsystem = subsystem;
	}
	return self;
}

- (instancetype)init {
	self = [super init];
	if (self) {
		_subsystem = @"default";
	}
	return self;
}

- (NSMutableArray<NSString *> *)_createJournalFields {
	NSMutableArray<NSString *> *fields = [[NSMutableArray alloc] init];

	[fields addObject:[NSString stringWithFormat:@"GS_SUBSYSTEM=%@", self.subsystem]];
	// Add bundle identifier from config.h
	[fields addObject:[NSString stringWithFormat:@"GS_BUNDLE_IDENTIFIER=%s", BUNDLE_IDENTIFIER]];
	return fields;
}

- (void)message:(NSString *)message withPriority:(VMPJournalType)priority {
	NSMutableArray<NSString *> *fields = [self _createJournalFields];

	[fields addObject:[NSString stringWithFormat:@"PRIORITY=%d", priority]];
	[fields addObject:[NSString stringWithFormat:@"MESSAGE=%@", message]];

	const struct iovec *vec = [fields UTF8StringIOVec];
	if (vec == NULL) {
		return;
	}

	sd_journal_sendv(vec, [fields count]);
	free((void *) vec);
}
- (void)error:(NSError *)error withPriority:(VMPJournalType)priority {
	[self message:[error description] withPriority:priority];
}

@end
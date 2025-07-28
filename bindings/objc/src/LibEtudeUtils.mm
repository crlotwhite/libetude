/**
 * @file LibEtudeUtils.mm
 * @brief LibEtude 유틸리티 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#import "LibEtudeUtils.h"
#import <mach/mach.h>
#import <sys/sysctl.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#elif TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif

NSErrorDomain const LibEtudeErrorDomain = @"com.libetude.error";

@implementation LibEtudeUtils

+ (NSError *)errorWithCode:(LibEtudeErrorCode)errorCode description:(nullable NSString *)description {
    NSString* errorDescription = description ?: [self stringFromErrorCode:errorCode];

    NSDictionary* userInfo = @{
        NSLocalizedDescriptionKey: errorDescription,
        NSLocalizedFailureReasonErrorKey: [self stringFromErrorCode:errorCode]
    };

    return [NSError errorWithDomain:LibEtudeErrorDomain code:errorCode userInfo:userInfo];
}

+ (NSString *)stringFromErrorCode:(LibEtudeErrorCode)errorCode {
    switch (errorCode) {
        case LibEtudeErrorCodeSuccess:
            return @"성공";
        case LibEtudeErrorCodeInvalidArgument:
            return @"유효하지 않은 인수";
        case LibEtudeErrorCodeOutOfMemory:
            return @"메모리 부족";
        case LibEtudeErrorCodeIO:
            return @"입출력 오류";
        case LibEtudeErrorCodeNotImplemented:
            return @"구현되지 않음";
        case LibEtudeErrorCodeRuntime:
            return @"런타임 오류";
        case LibEtudeErrorCodeHardware:
            return @"하드웨어 오류";
        case LibEtudeErrorCodeModel:
            return @"모델 오류";
        case LibEtudeErrorCodeTimeout:
            return @"타임아웃";
        default:
            return @"알 수 없는 오류";
    }
}

+ (NSData *)dataFromFloatArray:(const float *)floatArray length:(NSUInteger)length {
    if (!floatArray || length == 0) {
        return [NSData data];
    }

    return [NSData dataWithBytes:floatArray length:length * sizeof(float)];
}

+ (float *)floatArrayFromData:(NSData *)data {
    if (!data || data.length == 0) {
        return nullptr;
    }

    NSUInteger length = data.length / sizeof(float);
    float* result = (float*)malloc(length * sizeof(float));
    if (result) {
        memcpy(result, data.bytes, data.length);
    }

    return result;
}

+ (float *)floatArrayFromNumberArray:(NSArray<NSNumber *> *)numberArray {
    if (!numberArray || numberArray.count == 0) {
        return nullptr;
    }

    NSUInteger count = numberArray.count;
    float* result = (float*)malloc(count * sizeof(float));
    if (result) {
        for (NSUInteger i = 0; i < count; i++) {
            result[i] = numberArray[i].floatValue;
        }
    }

    return result;
}

+ (char *)cStringFromNSString:(NSString *)string {
    if (!string) {
        return nullptr;
    }

    const char* utf8String = [string UTF8String];
    if (!utf8String) {
        return nullptr;
    }

    size_t length = strlen(utf8String);
    char* result = (char*)malloc(length + 1);
    if (result) {
        strcpy(result, utf8String);
    }

    return result;
}

+ (nullable NSString *)nsStringFromCString:(const char *)cString {
    if (!cString) {
        return nil;
    }

    return [NSString stringWithUTF8String:cString];
}

+ (uint64_t)currentTimeMillis {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

+ (double)memoryUsageMB {
    struct mach_task_basic_info info;
    mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;

    kern_return_t kerr = task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                                  (task_info_t)&info, &size);

    if (kerr == KERN_SUCCESS) {
        return (double)info.resident_size / (1024.0 * 1024.0);
    }

    return 0.0;
}

+ (double)cpuUsagePercent {
    // 간단한 CPU 사용률 측정 (실제로는 더 복잡한 구현이 필요)
    static uint64_t lastIdleTime = 0;
    static uint64_t lastTotalTime = 0;

    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                       (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {

        uint64_t totalTicks = 0;
        for (int i = 0; i < CPU_STATE_MAX; i++) {
            totalTicks += cpuinfo.cpu_ticks[i];
        }

        uint64_t idleTicks = cpuinfo.cpu_ticks[CPU_STATE_IDLE];

        if (lastTotalTime > 0) {
            uint64_t totalTicksSinceLastTime = totalTicks - lastTotalTime;
            uint64_t idleTicksSinceLastTime = idleTicks - lastIdleTime;

            if (totalTicksSinceLastTime > 0) {
                double usage = 100.0 * (1.0 - ((double)idleTicksSinceLastTime) / totalTicksSinceLastTime);
                lastIdleTime = idleTicks;
                lastTotalTime = totalTicks;
                return usage;
            }
        }

        lastIdleTime = idleTicks;
        lastTotalTime = totalTicks;
    }

    return 0.0;
}

+ (NSDictionary<NSString *, id> *)deviceInfo {
    NSMutableDictionary* info = [NSMutableDictionary dictionary];

#if TARGET_OS_IOS
    UIDevice* device = [UIDevice currentDevice];
    info[@"platform"] = @"iOS";
    info[@"model"] = device.model;
    info[@"systemName"] = device.systemName;
    info[@"systemVersion"] = device.systemVersion;
    info[@"name"] = device.name;

    // 디바이스 모델 정보
    size_t size;
    sysctlbyname("hw.machine", NULL, &size, NULL, 0);
    char* machine = (char*)malloc(size);
    sysctlbyname("hw.machine", machine, &size, NULL, 0);
    info[@"machineModel"] = [NSString stringWithUTF8String:machine];
    free(machine);

#elif TARGET_OS_OSX
    info[@"platform"] = @"macOS";

    NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
    info[@"systemVersion"] = [NSString stringWithFormat:@"%ld.%ld.%ld",
                             (long)version.majorVersion, (long)version.minorVersion, (long)version.patchVersion];

    // 컴퓨터 모델 정보
    size_t size;
    sysctlbyname("hw.model", NULL, &size, NULL, 0);
    char* model = (char*)malloc(size);
    sysctlbyname("hw.model", model, &size, NULL, 0);
    info[@"model"] = [NSString stringWithUTF8String:model];
    free(model);
#endif

    // 공통 하드웨어 정보
    size_t size = sizeof(uint64_t);
    uint64_t memsize;
    sysctlbyname("hw.memsize", &memsize, &size, NULL, 0);
    info[@"totalMemoryMB"] = @(memsize / (1024 * 1024));

    int ncpu;
    size = sizeof(ncpu);
    sysctlbyname("hw.ncpu", &ncpu, &size, NULL, 0);
    info[@"cpuCount"] = @(ncpu);

    return [info copy];
}

+ (void)logWithLevel:(NSInteger)level format:(NSString *)format, ... {
    va_list args;
    va_start(args, format);

    NSString* message = [[NSString alloc] initWithFormat:format arguments:args];

    // 로그 레벨에 따른 출력
    switch (level) {
        case 0: // DEBUG
            NSLog(@"[DEBUG] LibEtude: %@", message);
            break;
        case 1: // INFO
            NSLog(@"[INFO] LibEtude: %@", message);
            break;
        case 2: // WARNING
            NSLog(@"[WARNING] LibEtude: %@", message);
            break;
        case 3: // ERROR
            NSLog(@"[ERROR] LibEtude: %@", message);
            break;
        case 4: // FATAL
            NSLog(@"[FATAL] LibEtude: %@", message);
            break;
        default:
            NSLog(@"[UNKNOWN] LibEtude: %@", message);
            break;
    }

    va_end(args);
}

@end
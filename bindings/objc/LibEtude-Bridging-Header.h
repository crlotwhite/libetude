/**
 * @file LibEtude-Bridging-Header.h
 * @brief LibEtude Swift 브리징 헤더
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 이 헤더는 Swift에서 LibEtude Objective-C API를 사용하기 위한 브리징 헤더입니다.
 */

#ifndef LibEtude_Bridging_Header_h
#define LibEtude_Bridging_Header_h

// LibEtude Objective-C 헤더들
#import "LibEtudeEngine.h"
#import "LibEtudeAudioStream.h"
#import "LibEtudePerformanceStats.h"
#import "LibEtudeUtils.h"

// 필요한 경우 C API 헤더들도 포함할 수 있습니다
// #import "libetude/api.h"

#endif /* LibEtude_Bridging_Header_h */
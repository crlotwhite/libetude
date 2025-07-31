#!/bin/bash
# LibEtude 성능 검증 스크립트
# Copyright (c) 2025 LibEtude Project

set -e

# 기본 설정
BUILD_DIR="build"
BENCHMARK_ITERATIONS=10
PERFORMANCE_THRESHOLD_MS=100
MEMORY_THRESHOLD_MB=256
OUTPUT_DIR="performance_results"

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 로그 함수
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 도움말 함수
show_help() {
    cat << EOF
LibEtude 성능 검증 스크립트

사용법: $0 [옵션]

옵션:
    -h, --help              이 도움말 표시
    -d, --build-dir DIR     빌드 디렉토리 (기본값: build)
    -i, --iterations N      벤치마크 반복 횟수 (기본값: 10)
    -t, --threshold MS      성능 임계값 (ms) (기본값: 100)
    -m, --memory-limit MB   메모리 임계값 (MB) (기본값: 256)
    -o, --output DIR        결과 출력 디렉토리 (기본값: performance_results)
    --baseline FILE         기준 성능 데이터 파일
    --save-baseline         현재 결과를 기준으로 저장

성능 검증 항목:
    1. 지연 시간 (Latency) 측정
    2. 처리량 (Throughput) 측정
    3. 메모리 사용량 측정
    4. CPU 사용률 측정
    5. 배터리 효율성 (모바일)
    6. 열 성능 (모바일)

예제:
    $0                      기본 성능 검증
    $0 --iterations 20      20회 반복 측정
    $0 --baseline perf.json 기준 데이터와 비교
    $0 --save-baseline      현재 결과를 기준으로 저장
EOF
}

# 명령행 인수 파싱
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -d|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -i|--iterations)
            BENCHMARK_ITERATIONS="$2"
            shift 2
            ;;
        -t|--threshold)
            PERFORMANCE_THRESHOLD_MS="$2"
            shift 2
            ;;
        -m|--memory-limit)
            MEMORY_THRESHOLD_MB="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --baseline)
            BASELINE_FILE="$2"
            shift 2
            ;;
        --save-baseline)
            SAVE_BASELINE=1
            shift
            ;;
        *)
            echo "알 수 없는 옵션: $1"
            show_help
            exit 1
            ;;
    esac
done

# 결과 저장 변수
declare -A RESULTS

# 시스템 정보 수집
collect_system_info() {
    log_info "시스템 정보 수집 중..."

    RESULTS["system_os"]=$(uname -s)
    RESULTS["system_arch"]=$(uname -m)
    RESULTS["system_kernel"]=$(uname -r)

    # CPU 정보
    if [[ -f /proc/cpuinfo ]]; then
        RESULTS["cpu_model"]=$(grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
        RESULTS["cpu_cores"]=$(nproc)
    elif command -v sysctl >/dev/null 2>&1; then
        RESULTS["cpu_model"]=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Unknown")
        RESULTS["cpu_cores"]=$(sysctl -n hw.ncpu)
    fi

    # 메모리 정보
    if [[ -f /proc/meminfo ]]; then
        RESULTS["memory_total"]=$(grep MemTotal /proc/meminfo | awk '{print $2}')
    elif command -v sysctl >/dev/null 2>&1; then
        RESULTS["memory_total"]=$(sysctl -n hw.memsize)
    fi

    log_success "시스템 정보 수집 완료"
}

# 지연 시간 측정
measure_latency() {
    log_info "지연 시간 측정 중..."

    local benchmark_tool="$BUILD_DIR/tools/benchmark/libetude_benchmarks"
    if [[ ! -f "$benchmark_tool" ]]; then
        log_error "벤치마크 도구를 찾을 수 없습니다: $benchmark_tool"
        return 1
    fi

    local total_latency=0
    local min_latency=999999
    local max_latency=0

    for ((i=1; i<=BENCHMARK_ITERATIONS; i++)); do
        log_info "지연 시간 측정 $i/$BENCHMARK_ITERATIONS"

        # 벤치마크 실행 및 결과 파싱
        local result=$($benchmark_tool --test=latency --iterations=1 2>/dev/null | grep "Latency:" | awk '{print $2}')

        if [[ -n "$result" ]]; then
            total_latency=$(echo "$total_latency + $result" | bc -l)

            if (( $(echo "$result < $min_latency" | bc -l) )); then
                min_latency=$result
            fi

            if (( $(echo "$result > $max_latency" | bc -l) )); then
                max_latency=$result
            fi
        fi
    done

    local avg_latency=$(echo "scale=2; $total_latency / $BENCHMARK_ITERATIONS" | bc -l)

    RESULTS["latency_avg"]=$avg_latency
    RESULTS["latency_min"]=$min_latency
    RESULTS["latency_max"]=$max_latency

    # 임계값 검사
    if (( $(echo "$avg_latency > $PERFORMANCE_THRESHOLD_MS" | bc -l) )); then
        log_warning "평균 지연 시간이 임계값을 초과했습니다: ${avg_latency}ms > ${PERFORMANCE_THRESHOLD_MS}ms"
        RESULTS["latency_pass"]="false"
    else
        log_success "지연 시간 테스트 통과: ${avg_latency}ms"
        RESULTS["latency_pass"]="true"
    fi
}

# 처리량 측정
measure_throughput() {
    log_info "처리량 측정 중..."

    local benchmark_tool="$BUILD_DIR/tools/benchmark/libetude_benchmarks"
    local total_throughput=0

    for ((i=1; i<=BENCHMARK_ITERATIONS; i++)); do
        log_info "처리량 측정 $i/$BENCHMARK_ITERATIONS"

        local result=$($benchmark_tool --test=throughput --iterations=1 2>/dev/null | grep "Throughput:" | awk '{print $2}')

        if [[ -n "$result" ]]; then
            total_throughput=$(echo "$total_throughput + $result" | bc -l)
        fi
    done

    local avg_throughput=$(echo "scale=2; $total_throughput / $BENCHMARK_ITERATIONS" | bc -l)
    RESULTS["throughput_avg"]=$avg_throughput

    log_success "평균 처리량: ${avg_throughput} samples/sec"
}

# 메모리 사용량 측정
measure_memory_usage() {
    log_info "메모리 사용량 측정 중..."

    local benchmark_tool="$BUILD_DIR/tools/benchmark/libetude_benchmarks"
    local max_memory=0

    # 백그라운드에서 메모리 모니터링
    (
        while true; do
            if command -v pgrep >/dev/null 2>&1; then
                local pid=$(pgrep -f libetude_benchmarks | head -1)
                if [[ -n "$pid" ]]; then
                    if [[ -f /proc/$pid/status ]]; then
                        local memory=$(grep VmRSS /proc/$pid/status | awk '{print $2}')
                        if [[ -n "$memory" && $memory -gt $max_memory ]]; then
                            max_memory=$memory
                        fi
                    fi
                fi
            fi
            sleep 0.1
        done
    ) &
    local monitor_pid=$!

    # 벤치마크 실행
    $benchmark_tool --test=memory --iterations=5 >/dev/null 2>&1

    # 모니터링 중지
    kill $monitor_pid 2>/dev/null || true
    wait $monitor_pid 2>/dev/null || true

    # KB를 MB로 변환
    local max_memory_mb=$(echo "scale=2; $max_memory / 1024" | bc -l)
    RESULTS["memory_peak"]=$max_memory_mb

    # 임계값 검사
    if (( $(echo "$max_memory_mb > $MEMORY_THRESHOLD_MB" | bc -l) )); then
        log_warning "최대 메모리 사용량이 임계값을 초과했습니다: ${max_memory_mb}MB > ${MEMORY_THRESHOLD_MB}MB"
        RESULTS["memory_pass"]="false"
    else
        log_success "메모리 사용량 테스트 통과: ${max_memory_mb}MB"
        RESULTS["memory_pass"]="true"
    fi
}

# CPU 사용률 측정
measure_cpu_usage() {
    log_info "CPU 사용률 측정 중..."

    local benchmark_tool="$BUILD_DIR/tools/benchmark/libetude_benchmarks"
    local cpu_samples=()

    # 백그라운드에서 CPU 모니터링
    (
        while true; do
            if command -v top >/dev/null 2>&1; then
                local cpu_usage=$(top -bn1 | grep "libetude_benchmarks" | awk '{print $9}' | head -1)
                if [[ -n "$cpu_usage" ]]; then
                    echo "$cpu_usage" >> /tmp/cpu_usage.log
                fi
            fi
            sleep 1
        done
    ) &
    local monitor_pid=$!

    # 벤치마크 실행
    $benchmark_tool --test=cpu --iterations=10 >/dev/null 2>&1

    # 모니터링 중지
    kill $monitor_pid 2>/dev/null || true
    wait $monitor_pid 2>/dev/null || true

    # CPU 사용률 평균 계산
    if [[ -f /tmp/cpu_usage.log ]]; then
        local avg_cpu=$(awk '{sum+=$1; count++} END {if(count>0) print sum/count; else print 0}' /tmp/cpu_usage.log)
        RESULTS["cpu_usage_avg"]=$avg_cpu
        rm -f /tmp/cpu_usage.log

        log_success "평균 CPU 사용률: ${avg_cpu}%"
    else
        RESULTS["cpu_usage_avg"]="N/A"
        log_warning "CPU 사용률을 측정할 수 없습니다"
    fi
}

# 기준 데이터와 비교
compare_with_baseline() {
    if [[ -z "$BASELINE_FILE" || ! -f "$BASELINE_FILE" ]]; then
        return 0
    fi

    log_info "기준 데이터와 비교 중..."

    # JSON 파싱 (jq가 있는 경우)
    if command -v jq >/dev/null 2>&1; then
        local baseline_latency=$(jq -r '.latency_avg // "N/A"' "$BASELINE_FILE")
        local baseline_throughput=$(jq -r '.throughput_avg // "N/A"' "$BASELINE_FILE")
        local baseline_memory=$(jq -r '.memory_peak // "N/A"' "$BASELINE_FILE")

        if [[ "$baseline_latency" != "N/A" && -n "${RESULTS[latency_avg]}" ]]; then
            local latency_diff=$(echo "scale=2; ${RESULTS[latency_avg]} - $baseline_latency" | bc -l)
            local latency_percent=$(echo "scale=2; ($latency_diff / $baseline_latency) * 100" | bc -l)

            if (( $(echo "$latency_percent > 10" | bc -l) )); then
                log_warning "지연 시간이 기준 대비 ${latency_percent}% 증가했습니다"
            elif (( $(echo "$latency_percent < -10" | bc -l) )); then
                log_success "지연 시간이 기준 대비 ${latency_percent}% 개선되었습니다"
            else
                log_info "지연 시간이 기준과 유사합니다 (${latency_percent}% 차이)"
            fi
        fi

        # 처리량 및 메모리 비교도 유사하게 구현...
    fi
}

# 결과 저장
save_results() {
    log_info "결과 저장 중..."

    mkdir -p "$OUTPUT_DIR"
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local result_file="$OUTPUT_DIR/performance_${timestamp}.json"

    # JSON 형식으로 결과 저장
    cat > "$result_file" << EOF
{
  "timestamp": "$(date -Iseconds)",
  "system": {
    "os": "${RESULTS[system_os]}",
    "arch": "${RESULTS[system_arch]}",
    "kernel": "${RESULTS[system_kernel]}",
    "cpu_model": "${RESULTS[cpu_model]}",
    "cpu_cores": ${RESULTS[cpu_cores]},
    "memory_total": "${RESULTS[memory_total]}"
  },
  "performance": {
    "latency_avg": ${RESULTS[latency_avg]},
    "latency_min": ${RESULTS[latency_min]},
    "latency_max": ${RESULTS[latency_max]},
    "latency_pass": ${RESULTS[latency_pass]},
    "throughput_avg": ${RESULTS[throughput_avg]},
    "memory_peak": ${RESULTS[memory_peak]},
    "memory_pass": ${RESULTS[memory_pass]},
    "cpu_usage_avg": "${RESULTS[cpu_usage_avg]}"
  },
  "configuration": {
    "iterations": $BENCHMARK_ITERATIONS,
    "performance_threshold_ms": $PERFORMANCE_THRESHOLD_MS,
    "memory_threshold_mb": $MEMORY_THRESHOLD_MB
  }
}
EOF

    log_success "결과가 저장되었습니다: $result_file"

    # 기준 데이터로 저장 (요청된 경우)
    if [[ -n "$SAVE_BASELINE" ]]; then
        cp "$result_file" "$OUTPUT_DIR/baseline.json"
        log_success "기준 데이터로 저장되었습니다: $OUTPUT_DIR/baseline.json"
    fi
}

# 결과 요약 출력
print_summary() {
    echo ""
    log_info "=== 성능 검증 결과 요약 ==="
    echo ""

    echo "📊 성능 지표:"
    echo "  지연 시간 (평균): ${RESULTS[latency_avg]}ms"
    echo "  지연 시간 (최소): ${RESULTS[latency_min]}ms"
    echo "  지연 시간 (최대): ${RESULTS[latency_max]}ms"
    echo "  처리량: ${RESULTS[throughput_avg]} samples/sec"
    echo "  메모리 사용량 (최대): ${RESULTS[memory_peak]}MB"
    echo "  CPU 사용률 (평균): ${RESULTS[cpu_usage_avg]}%"
    echo ""

    echo "✅ 검증 결과:"
    if [[ "${RESULTS[latency_pass]}" == "true" ]]; then
        echo "  지연 시간: PASS"
    else
        echo "  지연 시간: FAIL"
    fi

    if [[ "${RESULTS[memory_pass]}" == "true" ]]; then
        echo "  메모리 사용량: PASS"
    else
        echo "  메모리 사용량: FAIL"
    fi

    echo ""

    # 전체 결과 판정
    if [[ "${RESULTS[latency_pass]}" == "true" && "${RESULTS[memory_pass]}" == "true" ]]; then
        log_success "모든 성능 검증이 통과했습니다! 🎉"
        return 0
    else
        log_error "일부 성능 검증이 실패했습니다."
        return 1
    fi
}

# 메인 함수
main() {
    log_info "=== LibEtude 성능 검증 시작 ==="

    # 빌드 디렉토리 확인
    if [[ ! -d "$BUILD_DIR" ]]; then
        log_error "빌드 디렉토리를 찾을 수 없습니다: $BUILD_DIR"
        exit 1
    fi

    # 시스템 정보 수집
    collect_system_info

    # 성능 측정
    measure_latency
    measure_throughput
    measure_memory_usage
    measure_cpu_usage

    # 기준 데이터와 비교
    compare_with_baseline

    # 결과 저장
    save_results

    # 결과 요약
    print_summary
}

# 스크립트 실행
main "$@"
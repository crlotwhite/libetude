#!/bin/bash

# world4utau 성능 테스트 스크립트
# 다양한 조건에서 world4utau의 성능을 측정합니다.

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 함수 정의
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${CYAN}=== $1 ===${NC}"
}

# 시간 측정 함수
measure_time() {
    local start_time=$(date +%s.%N)
    "$@"
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc -l)
    echo "$duration"
}

# 메모리 사용량 측정 함수 (Linux/macOS)
measure_memory() {
    local pid=$1
    if command -v ps &> /dev/null; then
        # RSS in KB
        ps -o rss= -p $pid 2>/dev/null | awk '{print $1}' || echo "0"
    else
        echo "0"
    fi
}

# world4utau 실행 파일 경로 설정
WORLD4UTAU="../build/world4utau"
if [ ! -f "$WORLD4UTAU" ]; then
    WORLD4UTAU="../world4utau"
    if [ ! -f "$WORLD4UTAU" ]; then
        print_error "world4utau 실행 파일을 찾을 수 없습니다."
        exit 1
    fi
fi

# 테스트 결과 저장 디렉토리
TEST_DIR="performance_test_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# 결과 파일
RESULTS_FILE="performance_results.txt"
CSV_FILE="performance_results.csv"

print_header "world4utau 성능 테스트 시작"
print_status "테스트 디렉토리: $(pwd)"
print_status "결과 파일: $RESULTS_FILE, $CSV_FILE"

# 시스템 정보 수집
print_status "시스템 정보 수집 중..."
{
    echo "=== 시스템 정보 ==="
    echo "날짜: $(date)"
    echo "OS: $(uname -s)"
    echo "아키텍처: $(uname -m)"
    if command -v lscpu &> /dev/null; then
        echo "CPU: $(lscpu | grep 'Model name' | cut -d':' -f2 | xargs)"
        echo "CPU 코어: $(lscpu | grep '^CPU(s):' | cut -d':' -f2 | xargs)"
    elif command -v sysctl &> /dev/null; then
        echo "CPU: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo 'Unknown')"
        echo "CPU 코어: $(sysctl -n hw.ncpu 2>/dev/null || echo 'Unknown')"
    fi
    if command -v free &> /dev/null; then
        echo "메모리: $(free -h | grep '^Mem:' | awk '{print $2}')"
    elif command -v vm_stat &> /dev/null; then
        echo "메모리: $(echo "$(vm_stat | grep 'Pages free' | awk '{print $3}' | sed 's/\.//' ) * 4096 / 1024 / 1024" | bc 2>/dev/null || echo 'Unknown')MB"
    fi
    echo ""
} > "$RESULTS_FILE"

# CSV 헤더
echo "테스트,입력파일,피치,벨로시티,옵션,처리시간(초),메모리(KB),파일크기(KB),성공여부" > "$CSV_FILE"

# 테스트용 더미 WAV 파일 생성 (ffmpeg 또는 sox 필요)
create_test_audio() {
    local filename=$1
    local duration=$2
    local frequency=${3:-440}

    if command -v ffmpeg &> /dev/null; then
        ffmpeg -f lavfi -i "sine=frequency=$frequency:duration=$duration" -ar 44100 -ac 1 "$filename" -y &>/dev/null
    elif command -v sox &> /dev/null; then
        sox -n -r 44100 -c 1 "$filename" synth $duration sine $frequency &>/dev/null
    else
        print_warning "ffmpeg 또는 sox가 없어 더미 오디오 파일을 생성할 수 없습니다."
        return 1
    fi
}

# 테스트 실행 함수
run_test() {
    local test_name=$1
    local input_file=$2
    local pitch=$3
    local velocity=$4
    local options=$5
    local output_file="output_${test_name}.wav"

    print_status "테스트 실행: $test_name"

    if [ ! -f "$input_file" ]; then
        print_warning "입력 파일 $input_file이 없습니다. 건너뜁니다."
        echo "$test_name,$input_file,$pitch,$velocity,$options,0,0,0,SKIP" >> "$CSV_FILE"
        return
    fi

    local start_time=$(date +%s.%N)
    local success="SUCCESS"
    local memory_usage=0

    # 백그라운드에서 실행하여 메모리 모니터링
    $WORLD4UTAU "$input_file" "$output_file" "$pitch" "$velocity" $options &
    local pid=$!

    # 메모리 사용량 모니터링
    local max_memory=0
    while kill -0 $pid 2>/dev/null; do
        local current_memory=$(measure_memory $pid)
        if [ "$current_memory" -gt "$max_memory" ]; then
            max_memory=$current_memory
        fi
        sleep 0.1
    done

    wait $pid
    local exit_code=$?
    local end_time=$(date +%s.%N)

    if [ $exit_code -ne 0 ]; then
        success="FAIL"
    fi

    local duration=$(echo "$end_time - $start_time" | bc -l)
    local file_size=0

    if [ -f "$output_file" ]; then
        file_size=$(stat -f%z "$output_file" 2>/dev/null || stat -c%s "$output_file" 2>/dev/null || echo 0)
        file_size=$((file_size / 1024)) # KB로 변환
    fi

    # 결과 기록
    {
        echo "테스트: $test_name"
        echo "입력: $input_file"
        echo "피치: $pitch Hz"
        echo "벨로시티: $velocity"
        echo "옵션: $options"
        echo "처리 시간: ${duration}초"
        echo "최대 메모리: ${max_memory}KB"
        echo "출력 파일 크기: ${file_size}KB"
        echo "결과: $success"
        echo ""
    } >> "$RESULTS_FILE"

    echo "$test_name,$input_file,$pitch,$velocity,$options,$duration,$max_memory,$file_size,$success" >> "$CSV_FILE"

    if [ "$success" = "SUCCESS" ]; then
        print_success "완료: ${duration}초, ${max_memory}KB, ${file_size}KB"
    else
        print_error "실패: 종료 코드 $exit_code"
    fi
}

# 테스트용 오디오 파일 생성
print_header "테스트용 오디오 파일 생성"

create_test_audio "test_short.wav" 1 440    # 1초
create_test_audio "test_medium.wav" 5 440   # 5초
create_test_audio "test_long.wav" 10 440    # 10초

# 기본 성능 테스트
print_header "기본 성능 테스트"

run_test "basic_short" "test_short.wav" 440 100 ""
run_test "basic_medium" "test_medium.wav" 440 100 ""
run_test "basic_long" "test_long.wav" 440 100 ""

# 피치 변화 테스트
print_header "피치 변화 테스트"

run_test "pitch_low" "test_medium.wav" 220 100 ""
run_test "pitch_normal" "test_medium.wav" 440 100 ""
run_test "pitch_high" "test_medium.wav" 880 100 ""

# 벨로시티 변화 테스트
print_header "벨로시티 변화 테스트"

run_test "velocity_low" "test_medium.wav" 440 50 ""
run_test "velocity_normal" "test_medium.wav" 440 100 ""
run_test "velocity_high" "test_medium.wav" 440 150 ""

# 옵션별 성능 테스트
print_header "옵션별 성능 테스트"

run_test "with_volume" "test_medium.wav" 440 100 "--volume 0.8"
run_test "with_modulation" "test_medium.wav" 440 100 "--modulation 0.3"
run_test "with_verbose" "test_medium.wav" 440 100 "--verbose"
run_test "high_quality" "test_medium.wav" 440 100 "--sample-rate 48000 --bit-depth 24"
run_test "no_cache" "test_medium.wav" 440 100 "--no-cache"
run_test "no_optimization" "test_medium.wav" 440 100 "--no-optimization"

# 스트레스 테스트 (긴 오디오)
if [ -f "test_long.wav" ]; then
    print_header "스트레스 테스트"
    run_test "stress_test" "test_long.wav" 440 100 "--verbose"
fi

# 결과 분석
print_header "결과 분석"

{
    echo "=== 성능 분석 ==="
    echo ""

    # 평균 처리 시간 계산
    if command -v awk &> /dev/null; then
        echo "=== 처리 시간 통계 ==="
        awk -F',' 'NR>1 && $9=="SUCCESS" {sum+=$6; count++} END {
            if(count>0) {
                printf "평균 처리 시간: %.3f초\n", sum/count
                printf "총 성공한 테스트: %d개\n", count
            }
        }' "$CSV_FILE"
        echo ""

        echo "=== 메모리 사용량 통계 ==="
        awk -F',' 'NR>1 && $9=="SUCCESS" {sum+=$7; count++; if($7>max) max=$7} END {
            if(count>0) {
                printf "평균 메모리 사용량: %.0fKB\n", sum/count
                printf "최대 메모리 사용량: %dKB\n", max
            }
        }' "$CSV_FILE"
        echo ""

        echo "=== 테스트 성공률 ==="
        awk -F',' 'NR>1 {total++; if($9=="SUCCESS") success++} END {
            if(total>0) {
                printf "성공: %d/%d (%.1f%%)\n", success, total, success*100/total
            }
        }' "$CSV_FILE"
        echo ""
    fi

    echo "=== 상위 5개 느린 테스트 ==="
    if command -v sort &> /dev/null; then
        tail -n +2 "$CSV_FILE" | sort -t',' -k6 -nr | head -5 | while IFS=',' read -r test input pitch velocity options time memory size status; do
            printf "%-20s: %.3f초\n" "$test" "$time"
        done
    fi
    echo ""

    echo "=== 상위 5개 메모리 사용량 테스트 ==="
    if command -v sort &> /dev/null; then
        tail -n +2 "$CSV_FILE" | sort -t',' -k7 -nr | head -5 | while IFS=',' read -r test input pitch velocity options time memory size status; do
            printf "%-20s: %dKB\n" "$test" "$memory"
        done
    fi
    echo ""

} >> "$RESULTS_FILE"

# 결과 출력
print_success "성능 테스트 완료!"
print_status "결과 파일:"
print_status "  - 상세 결과: $RESULTS_FILE"
print_status "  - CSV 데이터: $CSV_FILE"

# 간단한 요약 출력
if [ -f "$RESULTS_FILE" ]; then
    echo ""
    print_header "테스트 요약"
    tail -20 "$RESULTS_FILE"
fi

# 권장사항 출력
print_status ""
print_status "성능 최적화 권장사항:"
print_status "1. SIMD 최적화 활성화 (기본값)"
print_status "2. 캐시 사용 (기본값)"
print_status "3. 적절한 스레드 수 설정"
print_status "4. 메모리 풀 크기 조정"
print_status ""
print_status "추가 분석을 위해 CSV 파일을 스프레드시트로 열어보세요."

# 정리
cd ..
print_status "테스트 결과는 $TEST_DIR 디렉토리에 저장되었습니다."
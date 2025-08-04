#!/bin/bash

# world4utau 기본 사용 예제 스크립트
# 이 스크립트는 world4utau의 기본적인 사용법을 보여줍니다.

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

# world4utau 실행 파일 경로 설정
WORLD4UTAU="../build/world4utau"
if [ ! -f "$WORLD4UTAU" ]; then
    WORLD4UTAU="../world4utau"
    if [ ! -f "$WORLD4UTAU" ]; then
        print_error "world4utau 실행 파일을 찾을 수 없습니다."
        print_status "빌드를 먼저 수행해주세요: cmake --build build --target world4utau"
        exit 1
    fi
fi

# 예제 데이터 디렉토리 생성
mkdir -p example_data
cd example_data

print_status "world4utau 기본 사용 예제를 시작합니다..."

# 예제 1: 기본 사용법
print_status ""
print_status "=== 예제 1: 기본 사용법 ==="
print_status "입력 파일이 있다면 기본적인 음성 합성을 수행합니다."

if [ -f "input_voice.wav" ]; then
    print_status "기본 음성 합성 실행..."
    $WORLD4UTAU input_voice.wav output_basic.wav 440.0 100

    if [ -f "output_basic.wav" ]; then
        print_success "기본 합성 완료: output_basic.wav"
    else
        print_error "기본 합성 실패"
    fi
else
    print_warning "입력 파일 input_voice.wav가 없습니다."
    print_status "테스트용 WAV 파일을 준비해주세요."
fi

# 예제 2: 피치 변경
print_status ""
print_status "=== 예제 2: 피치 변경 ==="
print_status "다양한 피치로 음성을 합성합니다."

if [ -f "input_voice.wav" ]; then
    # 낮은 피치 (220Hz)
    print_status "낮은 피치로 합성 (220Hz)..."
    $WORLD4UTAU input_voice.wav output_low_pitch.wav 220.0 100

    # 높은 피치 (880Hz)
    print_status "높은 피치로 합성 (880Hz)..."
    $WORLD4UTAU input_voice.wav output_high_pitch.wav 880.0 100

    print_success "피치 변경 예제 완료"
else
    print_warning "입력 파일이 없어 피치 변경 예제를 건너뜁니다."
fi

# 예제 3: 볼륨 및 모듈레이션 조절
print_status ""
print_status "=== 예제 3: 볼륨 및 모듈레이션 조절 ==="

if [ -f "input_voice.wav" ]; then
    # 볼륨 조절
    print_status "볼륨 50%로 합성..."
    $WORLD4UTAU input_voice.wav output_volume_50.wav 440.0 100 --volume 0.5

    # 모듈레이션 적용
    print_status "모듈레이션 적용 합성..."
    $WORLD4UTAU input_voice.wav output_modulation.wav 440.0 100 --modulation 0.3

    # 볼륨과 모듈레이션 동시 적용
    print_status "볼륨 80%, 모듈레이션 20% 적용..."
    $WORLD4UTAU input_voice.wav output_vol_mod.wav 440.0 100 --volume 0.8 --modulation 0.2

    print_success "볼륨 및 모듈레이션 예제 완료"
else
    print_warning "입력 파일이 없어 볼륨/모듈레이션 예제를 건너뜁니다."
fi

# 예제 4: UTAU 파라미터 적용
print_status ""
print_status "=== 예제 4: UTAU 파라미터 적용 ==="

if [ -f "input_voice.wav" ]; then
    print_status "UTAU 파라미터 적용 합성..."
    $WORLD4UTAU input_voice.wav output_utau_params.wav 440.0 100 \
        --pre-utterance 50.0 \
        --overlap 30.0 \
        --consonant 80 \
        --start-point 10.0

    print_success "UTAU 파라미터 예제 완료"
else
    print_warning "입력 파일이 없어 UTAU 파라미터 예제를 건너뜁니다."
fi

# 예제 5: 고품질 출력
print_status ""
print_status "=== 예제 5: 고품질 출력 ==="

if [ -f "input_voice.wav" ]; then
    print_status "고품질 출력 (24bit, 48kHz)..."
    $WORLD4UTAU input_voice.wav output_hq.wav 440.0 100 \
        --sample-rate 48000 \
        --bit-depth 24

    print_success "고품질 출력 예제 완료"
else
    print_warning "입력 파일이 없어 고품질 출력 예제를 건너뜁니다."
fi

# 예제 6: 디버그 모드
print_status ""
print_status "=== 예제 6: 디버그 모드 ==="

if [ -f "input_voice.wav" ]; then
    print_status "디버그 모드로 실행..."
    $WORLD4UTAU input_voice.wav output_debug.wav 440.0 100 --verbose

    print_success "디버그 모드 예제 완료"
else
    print_warning "입력 파일이 없어 디버그 모드 예제를 건너뜁니다."
fi

# 예제 7: 피치 벤드 파일 생성 및 적용
print_status ""
print_status "=== 예제 7: 피치 벤드 적용 ==="

# 간단한 피치 벤드 파일 생성
print_status "피치 벤드 파일 생성..."
cat > pitch_bend.txt << EOF
# 피치 벤드 데이터 (시간:피치 비율)
0.0:1.0
0.1:1.1
0.2:1.2
0.3:1.1
0.4:1.0
0.5:0.9
0.6:0.8
0.7:0.9
0.8:1.0
0.9:1.0
1.0:1.0
EOF

if [ -f "input_voice.wav" ]; then
    print_status "피치 벤드 적용 합성..."
    $WORLD4UTAU input_voice.wav output_pitch_bend.wav 440.0 100 --pitch-bend pitch_bend.txt

    print_success "피치 벤드 예제 완료"
else
    print_warning "입력 파일이 없어 피치 벤드 예제를 건너뜁니다."
fi

# 결과 요약
print_status ""
print_status "=== 예제 실행 결과 ==="
print_status "생성된 파일들:"

for file in output_*.wav; do
    if [ -f "$file" ]; then
        size=$(ls -lh "$file" | awk '{print $5}')
        print_success "  $file ($size)"
    fi
done

if [ -f "pitch_bend.txt" ]; then
    print_status "  pitch_bend.txt (피치 벤드 데이터)"
fi

print_status ""
print_status "추가 테스트를 위해 다음 명령을 사용할 수 있습니다:"
print_status "  $WORLD4UTAU --help                    # 도움말 보기"
print_status "  $WORLD4UTAU --version                 # 버전 정보"
print_status "  $WORLD4UTAU input.wav output.wav 440.0 100 --verbose  # 상세 출력"

print_success ""
print_success "world4utau 기본 사용 예제가 완료되었습니다!"
print_status "생성된 WAV 파일들을 오디오 플레이어로 재생해보세요."
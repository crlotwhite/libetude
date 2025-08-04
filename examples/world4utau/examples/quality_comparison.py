#!/usr/bin/env python3
"""
world4utau 품질 비교 도구

이 스크립트는 world4utau의 출력 품질을 분석하고 비교합니다.
다양한 객관적 지표를 사용하여 음성 품질을 평가합니다.
"""

import os
import sys
import argparse
import subprocess
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import json
from datetime import datetime

try:
    import librosa
    import soundfile as sf
    from scipy import signal
    from scipy.stats import pearsonr

    AUDIO_LIBS_AVAILABLE = True
except ImportError:
    print("경고: librosa, soundfile, scipy가 설치되지 않았습니다.")
    print("설치 명령: pip install librosa soundfile scipy matplotlib")
    AUDIO_LIBS_AVAILABLE = False


class AudioQualityAnalyzer:
    """오디오 품질 분석 클래스"""

    def __init__(self):
        self.sample_rate = 44100

    def load_audio(self, file_path):
        """오디오 파일 로드"""
        if not AUDIO_LIBS_AVAILABLE:
            raise ImportError("오디오 라이브러리가 설치되지 않았습니다.")

        try:
            audio, sr = librosa.load(file_path, sr=self.sample_rate, mono=True)
            return audio, sr
        except Exception as e:
            print(f"오디오 로드 실패 {file_path}: {e}")
            return None, None

    def calculate_snr(self, signal_audio, noise_audio):
        """신호 대 잡음비 (SNR) 계산"""
        signal_power = np.mean(signal_audio**2)
        noise_power = np.mean(noise_audio**2)

        if noise_power == 0:
            return float("inf")

        snr = 10 * np.log10(signal_power / noise_power)
        return snr

    def calculate_thd_n(self, audio, fundamental_freq=440):
        """전고조파 왜곡 + 잡음 (THD+N) 계산"""
        # FFT 수행
        fft = np.fft.fft(audio)
        freqs = np.fft.fftfreq(len(audio), 1 / self.sample_rate)
        magnitude = np.abs(fft)

        # 기본 주파수 찾기
        fundamental_idx = np.argmin(np.abs(freqs - fundamental_freq))
        fundamental_power = magnitude[fundamental_idx] ** 2

        # 고조파 찾기 (2차, 3차, 4차, 5차)
        harmonic_power = 0
        for harmonic in range(2, 6):
            harmonic_freq = fundamental_freq * harmonic
            if harmonic_freq < self.sample_rate / 2:
                harmonic_idx = np.argmin(np.abs(freqs - harmonic_freq))
                harmonic_power += magnitude[harmonic_idx] ** 2

        # 전체 파워에서 기본 주파수 제외
        total_power = np.sum(magnitude**2)
        noise_and_distortion_power = total_power - fundamental_power

        if fundamental_power == 0:
            return float("inf")

        thd_n = np.sqrt(noise_and_distortion_power / fundamental_power) * 100
        return thd_n

    def calculate_frequency_response(self, audio):
        """주파수 응답 계산"""
        # 스펙트로그램 계산
        f, t, Sxx = signal.spectrogram(audio, self.sample_rate, nperseg=1024)

        # 평균 파워 스펙트럼
        avg_power = np.mean(Sxx, axis=1)

        return f, avg_power

    def calculate_correlation(self, audio1, audio2):
        """두 오디오 간 상관관계 계산"""
        # 길이 맞추기
        min_len = min(len(audio1), len(audio2))
        audio1 = audio1[:min_len]
        audio2 = audio2[:min_len]

        correlation, p_value = pearsonr(audio1, audio2)
        return correlation, p_value

    def calculate_rms(self, audio):
        """RMS (Root Mean Square) 계산"""
        return np.sqrt(np.mean(audio**2))

    def calculate_peak_level(self, audio):
        """피크 레벨 계산 (dBFS)"""
        peak = np.max(np.abs(audio))
        if peak == 0:
            return -float("inf")
        return 20 * np.log10(peak)

    def calculate_dynamic_range(self, audio):
        """다이나믹 레인지 계산"""
        # 윈도우 단위로 RMS 계산
        window_size = int(0.1 * self.sample_rate)  # 100ms 윈도우
        rms_values = []

        for i in range(0, len(audio) - window_size, window_size):
            window = audio[i : i + window_size]
            rms = self.calculate_rms(window)
            if rms > 0:
                rms_values.append(20 * np.log10(rms))

        if len(rms_values) < 2:
            return 0

        return np.max(rms_values) - np.min(rms_values)


class World4UtauTester:
    """world4utau 테스터 클래스"""

    def __init__(self, world4utau_path):
        self.world4utau_path = world4utau_path
        self.analyzer = AudioQualityAnalyzer()

    def run_world4utau(
        self, input_file, output_file, pitch=440, velocity=100, options=""
    ):
        """world4utau 실행"""
        cmd = [self.world4utau_path, input_file, output_file, str(pitch), str(velocity)]

        if options:
            cmd.extend(options.split())

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            return result.returncode == 0, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return False, "", "타임아웃"
        except Exception as e:
            return False, "", str(e)

    def create_test_audio(self, output_file, duration=5, frequency=440):
        """테스트용 오디오 생성"""
        if not AUDIO_LIBS_AVAILABLE:
            return False

        t = np.linspace(0, duration, int(duration * self.analyzer.sample_rate))
        audio = 0.5 * np.sin(2 * np.pi * frequency * t)

        # 약간의 노이즈 추가 (더 현실적인 테스트를 위해)
        noise = 0.01 * np.random.randn(len(audio))
        audio += noise

        try:
            sf.write(output_file, audio, self.analyzer.sample_rate)
            return True
        except Exception as e:
            print(f"테스트 오디오 생성 실패: {e}")
            return False

    def analyze_quality(self, original_file, processed_file):
        """품질 분석 수행"""
        results = {}

        # 오디오 로드
        original, _ = self.analyzer.load_audio(original_file)
        processed, _ = self.analyzer.load_audio(processed_file)

        if original is None or processed is None:
            return None

        # 기본 통계
        results["original_rms"] = self.analyzer.calculate_rms(original)
        results["processed_rms"] = self.analyzer.calculate_rms(processed)
        results["original_peak"] = self.analyzer.calculate_peak_level(original)
        results["processed_peak"] = self.analyzer.calculate_peak_level(processed)

        # 상관관계
        correlation, p_value = self.analyzer.calculate_correlation(original, processed)
        results["correlation"] = correlation
        results["correlation_p_value"] = p_value

        # THD+N 계산
        results["original_thd_n"] = self.analyzer.calculate_thd_n(original)
        results["processed_thd_n"] = self.analyzer.calculate_thd_n(processed)

        # 다이나믹 레인지
        results["original_dynamic_range"] = self.analyzer.calculate_dynamic_range(
            original
        )
        results["processed_dynamic_range"] = self.analyzer.calculate_dynamic_range(
            processed
        )

        # 주파수 응답
        orig_freqs, orig_power = self.analyzer.calculate_frequency_response(original)
        proc_freqs, proc_power = self.analyzer.calculate_frequency_response(processed)

        results["frequency_response"] = {
            "original": {"freqs": orig_freqs.tolist(), "power": orig_power.tolist()},
            "processed": {"freqs": proc_freqs.tolist(), "power": proc_power.tolist()},
        }

        return results

    def run_quality_tests(self, test_cases):
        """품질 테스트 실행"""
        results = {}

        for test_name, test_config in test_cases.items():
            print(f"테스트 실행: {test_name}")

            # 테스트 오디오 생성
            input_file = f"test_input_{test_name}.wav"
            output_file = f"test_output_{test_name}.wav"

            if not self.create_test_audio(
                input_file,
                test_config.get("duration", 5),
                test_config.get("frequency", 440),
            ):
                print(f"테스트 오디오 생성 실패: {test_name}")
                continue

            # world4utau 실행
            success, stdout, stderr = self.run_world4utau(
                input_file,
                output_file,
                test_config.get("pitch", 440),
                test_config.get("velocity", 100),
                test_config.get("options", ""),
            )

            if not success:
                print(f"world4utau 실행 실패: {test_name}")
                print(f"에러: {stderr}")
                continue

            # 품질 분석
            quality_results = self.analyze_quality(input_file, output_file)
            if quality_results:
                results[test_name] = {
                    "config": test_config,
                    "quality": quality_results,
                    "success": True,
                }
            else:
                results[test_name] = {"config": test_config, "success": False}

            # 임시 파일 정리
            try:
                os.remove(input_file)
                os.remove(output_file)
            except:
                pass

        return results

    def generate_report(self, results, output_dir):
        """결과 리포트 생성"""
        os.makedirs(output_dir, exist_ok=True)

        # JSON 결과 저장
        json_file = os.path.join(output_dir, "quality_results.json")
        with open(json_file, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2, ensure_ascii=False)

        # 텍스트 리포트 생성
        report_file = os.path.join(output_dir, "quality_report.txt")
        with open(report_file, "w", encoding="utf-8") as f:
            f.write("world4utau 품질 분석 리포트\n")
            f.write("=" * 50 + "\n")
            f.write(f"생성 시간: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")

            for test_name, result in results.items():
                if not result["success"]:
                    continue

                f.write(f"테스트: {test_name}\n")
                f.write("-" * 30 + "\n")

                quality = result["quality"]
                f.write(f"상관관계: {quality['correlation']:.4f}\n")
                f.write(f"원본 RMS: {quality['original_rms']:.6f}\n")
                f.write(f"처리된 RMS: {quality['processed_rms']:.6f}\n")
                f.write(f"원본 피크: {quality['original_peak']:.2f} dBFS\n")
                f.write(f"처리된 피크: {quality['processed_peak']:.2f} dBFS\n")
                f.write(f"원본 THD+N: {quality['original_thd_n']:.4f}%\n")
                f.write(f"처리된 THD+N: {quality['processed_thd_n']:.4f}%\n")
                f.write(
                    f"원본 다이나믹 레인지: {quality['original_dynamic_range']:.2f} dB\n"
                )
                f.write(
                    f"처리된 다이나믹 레인지: {quality['processed_dynamic_range']:.2f} dB\n"
                )
                f.write("\n")

        # 그래프 생성
        if AUDIO_LIBS_AVAILABLE:
            self.generate_plots(results, output_dir)

        print(f"리포트 생성 완료: {output_dir}")

    def generate_plots(self, results, output_dir):
        """그래프 생성"""
        # 상관관계 그래프
        correlations = []
        test_names = []

        for test_name, result in results.items():
            if result["success"]:
                correlations.append(result["quality"]["correlation"])
                test_names.append(test_name)

        if correlations:
            plt.figure(figsize=(12, 6))
            plt.bar(test_names, correlations)
            plt.title("테스트별 원본-처리된 오디오 상관관계")
            plt.ylabel("상관계수")
            plt.xticks(rotation=45)
            plt.tight_layout()
            plt.savefig(os.path.join(output_dir, "correlation_comparison.png"))
            plt.close()

        # THD+N 비교 그래프
        original_thd = []
        processed_thd = []

        for test_name, result in results.items():
            if result["success"]:
                original_thd.append(result["quality"]["original_thd_n"])
                processed_thd.append(result["quality"]["processed_thd_n"])

        if original_thd and processed_thd:
            x = np.arange(len(test_names))
            width = 0.35

            plt.figure(figsize=(12, 6))
            plt.bar(x - width / 2, original_thd, width, label="원본")
            plt.bar(x + width / 2, processed_thd, width, label="처리된")
            plt.title("THD+N 비교")
            plt.ylabel("THD+N (%)")
            plt.xlabel("테스트")
            plt.xticks(x, test_names, rotation=45)
            plt.legend()
            plt.tight_layout()
            plt.savefig(os.path.join(output_dir, "thd_comparison.png"))
            plt.close()


def main():
    parser = argparse.ArgumentParser(description="world4utau 품질 비교 도구")
    parser.add_argument(
        "--world4utau", default="../build/world4utau", help="world4utau 실행 파일 경로"
    )
    parser.add_argument(
        "--output-dir", default="quality_analysis", help="결과 출력 디렉토리"
    )

    args = parser.parse_args()

    if not os.path.exists(args.world4utau):
        print(f"world4utau 실행 파일을 찾을 수 없습니다: {args.world4utau}")
        sys.exit(1)

    if not AUDIO_LIBS_AVAILABLE:
        print("오디오 분석 라이브러리가 설치되지 않았습니다.")
        print(
            "설치 후 다시 실행해주세요: pip install librosa soundfile scipy matplotlib"
        )
        sys.exit(1)

    # 테스트 케이스 정의
    test_cases = {
        "basic_440hz": {
            "duration": 3,
            "frequency": 440,
            "pitch": 440,
            "velocity": 100,
            "options": "",
        },
        "pitch_shift_up": {
            "duration": 3,
            "frequency": 440,
            "pitch": 880,
            "velocity": 100,
            "options": "",
        },
        "pitch_shift_down": {
            "duration": 3,
            "frequency": 440,
            "pitch": 220,
            "velocity": 100,
            "options": "",
        },
        "with_modulation": {
            "duration": 3,
            "frequency": 440,
            "pitch": 440,
            "velocity": 100,
            "options": "--modulation 0.3",
        },
        "high_quality": {
            "duration": 3,
            "frequency": 440,
            "pitch": 440,
            "velocity": 100,
            "options": "--sample-rate 48000 --bit-depth 24",
        },
        "low_frequency": {
            "duration": 3,
            "frequency": 220,
            "pitch": 220,
            "velocity": 100,
            "options": "",
        },
        "high_frequency": {
            "duration": 3,
            "frequency": 880,
            "pitch": 880,
            "velocity": 100,
            "options": "",
        },
    }

    # 테스터 생성 및 실행
    tester = World4UtauTester(args.world4utau)

    print("world4utau 품질 분석을 시작합니다...")
    results = tester.run_quality_tests(test_cases)

    # 리포트 생성
    tester.generate_report(results, args.output_dir)

    # 요약 출력
    successful_tests = sum(1 for r in results.values() if r["success"])
    total_tests = len(results)

    print(f"\n품질 분석 완료!")
    print(f"성공한 테스트: {successful_tests}/{total_tests}")
    print(f"결과 디렉토리: {args.output_dir}")

    if successful_tests > 0:
        avg_correlation = np.mean(
            [r["quality"]["correlation"] for r in results.values() if r["success"]]
        )
        print(f"평균 상관관계: {avg_correlation:.4f}")


if __name__ == "__main__":
    main()

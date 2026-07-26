#!/usr/bin/env python3
"""Run the built-in Hey Jarvis model on the processed Voice DSP+ stream."""

from __future__ import annotations

import argparse
import fcntl
import math
import os
import queue
import re
import signal
import subprocess
import sys
import threading
import time
from array import array

from pyopen_wakeword import Model, OpenWakeWord, OpenWakeWordFeatures


I2C_SLAVE = 0x0703
XVF3800_I2C_ADDR = 0x2C
PCAL6408A_I2C_ADDR = 0x20
GPO_RESID = 0x14
GPO_PIN_VAL = 0x01
LED_RED_PIN = 6
LED_GREEN_PIN = 7
P5_JACK_MASK = 0x20
P6_MIC_MASK = 0x40


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="plughw:2,0")
    parser.add_argument("--threshold", type=float, default=0.40)
    parser.add_argument("--cooldown", type=float, default=1.5)
    parser.add_argument("--status-interval", type=float, default=5.0)
    parser.add_argument("--reset-after-detection", action="store_true")
    parser.add_argument("--feedback-duration", type=float, default=2.0)
    parser.add_argument("--beep-duration", type=float, default=0.6)
    parser.add_argument("--feedback-sink", default="pi_ai_mic_vocalfusion_speaker")
    parser.add_argument("--no-feedback", action="store_true")
    return parser.parse_args()


def rms_s16le(audio: bytes) -> float:
    samples = array("h")
    samples.frombytes(audio)
    if sys.byteorder != "little":
        samples.byteswap()
    if not samples:
        return 0.0
    return math.sqrt(sum(sample * sample for sample in samples) / len(samples)) / 32768.0


def write_xmos_led_pair(red: int, green: int) -> None:
    fd = os.open("/dev/i2c-1", os.O_RDWR)
    try:
        fcntl.ioctl(fd, I2C_SLAVE, XVF3800_I2C_ADDR)
        for pin, state in ((LED_RED_PIN, red), (LED_GREEN_PIN, green)):
            payload = bytes((0, pin, 1 if state else 0))
            packet = bytes((GPO_RESID, GPO_PIN_VAL, len(payload))) + payload
            os.write(fd, packet)
            status = os.read(fd, 1)[0]
            if status != 0:
                raise OSError(f"XMOS GPO command failed with status {status}")
    finally:
        os.close(fd)


def read_normal_led_state() -> tuple[int, int]:
    fd = os.open("/dev/i2c-1", os.O_RDWR)
    try:
        fcntl.ioctl(fd, I2C_SLAVE, PCAL6408A_I2C_ADDR)
        os.write(fd, b"\x00")
        value = os.read(fd, 1)[0]
    finally:
        os.close(fd)

    return (
        1 if value & P5_JACK_MASK else 0,
        1 if value & P6_MIC_MASK else 0,
    )


def get_sink_volume_percent(sink: str) -> int | None:
    for _ in range(5):
        result = subprocess.run(
            ["pactl", "get-sink-volume", sink],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode == 0:
            match = re.search(r"/\s*(\d+)%", result.stdout)
            if match:
                return int(match.group(1))
        time.sleep(0.05)
    return None


def set_sink_volume_percent(sink: str, percent: int) -> bool:
    for _ in range(5):
        result = subprocess.run(
            ["pactl", "set-sink-volume", sink, f"{percent}%"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if result.returncode == 0:
            return True
        time.sleep(0.05)
    return False


class DetectionFeedback:
    def __init__(self, duration: float, beep_duration: float, sink: str) -> None:
        self.duration = max(0.1, duration)
        self.beep_duration = max(0.1, min(beep_duration, self.duration))
        self.sink = sink
        self.pending: queue.Queue[bool | None] = queue.Queue(maxsize=1)
        self.stop_requested = threading.Event()
        self.busy = threading.Event()
        self.worker = threading.Thread(target=self._worker, name="detection-feedback", daemon=True)
        self.worker.start()

    def trigger(self) -> None:
        if self.busy.is_set():
            return
        self.busy.set()
        try:
            self.pending.put_nowait(True)
        except queue.Full:
            self.busy.clear()

    def close(self) -> None:
        self.stop_requested.set()
        try:
            self.pending.put_nowait(None)
        except queue.Full:
            pass
        self.worker.join(timeout=self.duration + 1.0)

    def _worker(self) -> None:
        while True:
            request = self.pending.get()
            if request is None:
                return
            try:
                self._run_once()
            except Exception as error:
                print(f"FEEDBACK_WARN {type(error).__name__}: {error}", flush=True)
            finally:
                self.busy.clear()

    def _run_once(self) -> None:
        beep: subprocess.Popen[bytes] | None = None
        original_volume = get_sink_volume_percent(self.sink)
        volume_raised = original_volume is not None and set_sink_volume_percent(
            self.sink,
            50,
        )
        try:
            # Both dies on the bicolor LED produce an amber indication.
            write_xmos_led_pair(1, 1)
            beep = subprocess.Popen(
                [
                    "speaker-test",
                    "-D",
                    "pulse",
                    "-c",
                    "2",
                    "-r",
                    "16000",
                    "-F",
                    "S32_LE",
                    "-t",
                    "sine",
                    "-f",
                    "880",
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )

            deadline = time.monotonic() + self.duration
            beep_deadline = time.monotonic() + self.beep_duration
            while not self.stop_requested.is_set() and time.monotonic() < deadline:
                # The route monitor also owns these LEDs, so reinforce the
                # temporary state until the feedback interval ends.
                write_xmos_led_pair(1, 1)
                if beep.poll() is None and time.monotonic() >= beep_deadline:
                    if volume_raised and set_sink_volume_percent(self.sink, original_volume):
                        volume_raised = False
                    beep.terminate()
                self.stop_requested.wait(0.05)
        finally:
            try:
                red, green = read_normal_led_state()
                write_xmos_led_pair(red, green)
            finally:
                if volume_raised and original_volume is not None:
                    set_sink_volume_percent(self.sink, original_volume)
                if beep is not None:
                    try:
                        beep.wait(timeout=1.0)
                    except subprocess.TimeoutExpired:
                        beep.terminate()


def main() -> int:
    args = parse_args()
    command = [
        "arecord",
        "-q",
        "-D",
        args.device,
        "-f",
        "S16_LE",
        "-r",
        "16000",
        "-c",
        "1",
        "-t",
        "raw",
    ]

    wakeword = OpenWakeWord.from_builtin(Model.HEY_JARVIS)
    features = OpenWakeWordFeatures.from_builtin()
    capture = subprocess.Popen(command, stdout=subprocess.PIPE)
    feedback = None if args.no_feedback else DetectionFeedback(
        args.feedback_duration,
        args.beep_duration,
        args.feedback_sink,
    )
    stop_requested = False

    def stop_capture(_signum: int, _frame: object) -> None:
        nonlocal stop_requested
        stop_requested = True

    signal.signal(signal.SIGINT, stop_capture)
    signal.signal(signal.SIGTERM, stop_capture)

    print(
        f"READY model=hey_jarvis device={args.device} threshold={args.threshold:.2f} "
        f"cooldown={args.cooldown:.1f}s",
        flush=True,
    )
    last_detection = -args.cooldown
    last_status = time.monotonic()
    status_peak = 0.0
    status_rms = 0.0

    try:
        assert capture.stdout is not None
        while not stop_requested:
            audio = capture.stdout.read(2048)
            if not audio:
                break

            status_rms = max(status_rms, rms_s16le(audio))
            reset_requested = False
            for feature_batch in features.process_streaming(audio):
                for probability in wakeword.process_streaming(feature_batch):
                    probability = float(probability)
                    status_peak = max(status_peak, probability)
                    now = time.monotonic()
                    if (
                        probability >= args.threshold
                        and now - last_detection >= args.cooldown
                    ):
                        last_detection = now
                        print(
                            f"DETECTED model=hey_jarvis score={probability:.3f} "
                            f"rms={status_rms:.4f} time={time.strftime('%Y-%m-%dT%H:%M:%S')}",
                            flush=True,
                        )
                        if feedback is not None:
                            feedback.trigger()
                        reset_requested = args.reset_after_detection
                        if reset_requested:
                            break
                if reset_requested:
                    break

            if reset_requested:
                wakeword.reset()
                features.reset()

            now = time.monotonic()
            if now - last_status >= args.status_interval:
                print(
                    f"STATUS max_score={status_peak:.3f} max_rms={status_rms:.4f}",
                    flush=True,
                )
                last_status = now
                status_peak = 0.0
                status_rms = 0.0
    finally:
        if feedback is not None:
            feedback.close()
        capture.terminate()
        try:
            capture.wait(timeout=2)
        except subprocess.TimeoutExpired:
            capture.kill()
        wakeword.close()
        features.close()

    return 0 if stop_requested else (capture.returncode or 0)


if __name__ == "__main__":
    raise SystemExit(main())

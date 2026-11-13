"""Explicit Pi runtime candidate. Importing this module does not open devices."""
import argparse
import ctypes as c
import ctypes.util
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time


class ClockError(RuntimeError):
    pass


class AlsaSilence:
    """One nonblocking S32_LE stereo PCM owner; never recover/restart an XRUN."""

    def __init__(self, pcm, rate, now=time.monotonic):
        if rate not in (16000, 48000):
            raise ValueError("Unsupported paired I2S rate")
        self.pcm, self.rate, self.now = pcm, rate, now
        self.handle = c.c_void_p()
        self.zeros = c.create_string_buffer(256 * 8)
        self.lib = None
        self.last_progress = 0.0

    def bind(self):
        name = ctypes.util.find_library("asound")
        if not name:
            raise ClockError("libasound is unavailable")
        self.lib = c.CDLL(name)
        ptr = c.c_void_p
        pp = c.POINTER(ptr)
        uintp, ulongp = c.POINTER(c.c_uint), c.POINTER(c.c_ulong)
        signatures = {
            "snd_pcm_open": ([pp, c.c_char_p, c.c_int, c.c_int], c.c_int),
            "snd_pcm_close": ([ptr], c.c_int),
            "snd_pcm_drop": ([ptr], c.c_int),
            "snd_pcm_state": ([ptr], c.c_int),
            "snd_pcm_start": ([ptr], c.c_int),
            "snd_pcm_writei": ([ptr, ptr, c.c_ulong], c.c_long),
            "snd_pcm_format_value": ([c.c_char_p], c.c_int),
            "snd_pcm_hw_params_malloc": ([pp], c.c_int),
            "snd_pcm_hw_params_free": ([ptr], None),
            "snd_pcm_hw_params_any": ([ptr, ptr], c.c_int),
            "snd_pcm_hw_params_set_access": ([ptr, ptr, c.c_int], c.c_int),
            "snd_pcm_hw_params_set_format": ([ptr, ptr, c.c_int], c.c_int),
            "snd_pcm_hw_params_set_channels": ([ptr, ptr, c.c_uint], c.c_int),
            "snd_pcm_hw_params_set_rate": ([ptr, ptr, c.c_uint, c.c_int], c.c_int),
            "snd_pcm_hw_params_set_period_size_near": ([ptr, ptr, ulongp, c.POINTER(c.c_int)], c.c_int),
            "snd_pcm_hw_params_set_buffer_size_near": ([ptr, ptr, ulongp], c.c_int),
            "snd_pcm_hw_params": ([ptr, ptr], c.c_int),
            "snd_pcm_hw_params_current": ([ptr, ptr], c.c_int),
            "snd_pcm_hw_params_get_rate": ([ptr, uintp, c.POINTER(c.c_int)], c.c_int),
            "snd_pcm_hw_params_get_channels": ([ptr, uintp], c.c_int),
            "snd_pcm_hw_params_get_format": ([ptr, c.POINTER(c.c_int)], c.c_int),
            "snd_pcm_get_params": ([ptr, ulongp, ulongp], c.c_int),
            "snd_pcm_sw_params_malloc": ([pp], c.c_int),
            "snd_pcm_sw_params_free": ([ptr], None),
            "snd_pcm_sw_params_current": ([ptr, ptr], c.c_int),
            "snd_pcm_sw_params_get_boundary": ([ptr, ulongp], c.c_int),
            "snd_pcm_sw_params_set_start_threshold": ([ptr, ptr, c.c_ulong], c.c_int),
            "snd_pcm_sw_params": ([ptr, ptr], c.c_int),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.lib, name)
            function.argtypes, function.restype = args, result

    def call(self, name, *args):
        code = getattr(self.lib, "snd_pcm_" + name)(*args)
        if code < 0:
            raise ClockError(f"ALSA {name}: {code}; no automatic recovery")
        return code

    def start(self, cancelled=lambda: False):
        if cancelled():
            raise InterruptedError("Stopped before PCM open")
        self.bind()
        # ALSA ABI: playback=0, NONBLOCK=1, RW_INTERLEAVED=3.
        self.call("open", c.byref(self.handle), self.pcm.encode(), 0, 1)
        hw, sw = c.c_void_p(), c.c_void_p()
        try:
            self.call("hw_params_malloc", c.byref(hw))
            self.call("hw_params_any", self.handle, hw)
            fmt = self.call("format_value", b"S32_LE")
            self.call("hw_params_set_access", self.handle, hw, 3)
            self.call("hw_params_set_format", self.handle, hw, fmt)
            self.call("hw_params_set_channels", self.handle, hw, 2)
            self.call("hw_params_set_rate", self.handle, hw, self.rate, 0)
            period, buffer = c.c_ulong(256), c.c_ulong(1024)
            direction = c.c_int(0)
            self.call("hw_params_set_period_size_near", self.handle, hw, c.byref(period), c.byref(direction))
            self.call("hw_params_set_buffer_size_near", self.handle, hw, c.byref(buffer))
            self.call("hw_params", self.handle, hw)
            self.call("hw_params_current", self.handle, hw)
            rate, channels, actual_format = c.c_uint(), c.c_uint(), c.c_int()
            self.call("hw_params_get_rate", hw, c.byref(rate), c.byref(direction))
            self.call("hw_params_get_channels", hw, c.byref(channels))
            self.call("hw_params_get_format", hw, c.byref(actual_format))
            if (rate.value, channels.value, actual_format.value, direction.value) != (self.rate, 2, fmt, 0):
                raise ClockError("PCM changed the requested rate/channels/format")
            self.call("get_params", self.handle, c.byref(buffer), c.byref(period))
            if not 256 <= buffer.value <= 8192:
                raise ClockError("Unexpected PCM buffer size")
            self.call("sw_params_malloc", c.byref(sw))
            self.call("sw_params_current", self.handle, sw)
            boundary = c.c_ulong()
            self.call("sw_params_get_boundary", sw, c.byref(boundary))
            self.call("sw_params_set_start_threshold", self.handle, sw, boundary.value)
            self.call("sw_params", self.handle, sw)
            remaining = buffer.value
            for _ in range(64):
                if cancelled():
                    raise InterruptedError("Stopped while priming PCM")
                if not remaining:
                    break
                written = self.write(min(256, remaining))
                if not written:
                    raise ClockError("PCM did not accept its startup silence")
                remaining -= written
            if remaining:
                raise ClockError("PCM startup did not complete")
            self.call("start", self.handle)
            self.check_running()
            self.last_progress = self.now()
        finally:
            if hw:
                self.lib.snd_pcm_hw_params_free(hw)
            if sw:
                self.lib.snd_pcm_sw_params_free(sw)

    def write(self, frames):
        result = self.lib.snd_pcm_writei(self.handle, self.zeros, frames)
        if result == -errno.EAGAIN:
            return 0
        if result < 0 or result > frames:
            raise ClockError(f"ALSA writei: {result}; stream lost, no retry/recover")
        return result

    def check_running(self):
        # ALSA RUNNING=3. No prepare/resume on any other runtime state.
        state = self.lib.snd_pcm_state(self.handle)
        if state != 3:
            raise ClockError(f"PCM is not RUNNING (state={state})")

    def pump(self):
        self.check_running()
        for _ in range(4):
            if not self.write(256):
                break
            self.last_progress = self.now()
        self.check_running()
        if self.now() - self.last_progress > 0.250:
            raise ClockError("No PCM write progress for 250 ms")

    def close(self):
        if self.handle:
            try:
                self.call("drop", self.handle)
            finally:
                handle, self.handle = self.handle, c.c_void_p()
                self.call("close", handle)


def supervise(clock, launch, cancelled, now=time.monotonic, sleep=time.sleep):
    """Keep healthy clocks during controller shutdown; terminal faults never retry."""
    child = None
    fault = None
    requested = False
    deadline = None
    forced = False
    code = None
    try:
        clock.start(cancelled)
        if cancelled():
            requested = True
        else:
            child = launch()
        while child is not None:
            code = child.poll()
            if code is not None:
                break
            if fault is None:
                try:
                    clock.pump()
                except Exception as error:
                    fault = str(error)
            if deadline is None and (fault is not None or cancelled()):
                requested = cancelled()
                child.terminate()
                deadline = now() + 5.0
            if deadline is not None and now() >= deadline:
                forced = True
                child.kill()
                try:
                    code = child.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    fault = fault or "Controller did not exit after SIGKILL; output state UNKNOWN"
                break
            sleep(0.002)
    except InterruptedError as error:
        requested = cancelled()
        if not requested:
            fault = str(error)
    except Exception as error:
        fault = str(error)
    finally:
        # Also stop a child after an unexpected Python/OS exception, while keeping
        # healthy clocks alive as long as possible. Never respawn it.
        if child is not None and child.poll() is None and not forced:
            child.terminate()
            end = now() + 5.0
            while child.poll() is None and now() < end:
                try:
                    clock.pump()
                except Exception as error:
                    fault = fault or str(error)
                    break
                sleep(0.002)
            if child.poll() is None:
                child.kill()
                forced = True
            try:
                code = child.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                fault = fault or "Controller shutdown unconfirmed"
        try:
            clock.close()
        except Exception as error:
            fault = fault or str(error)
    ok = requested and fault is None and not forced and code in (None, 0)
    return {"ok": ok, "requested_stop": requested, "fault": fault,
            "controller_exit": code, "forced_kill": forced,
            "hardware_qualified": False}


def parent_death_guard(parent_pid):
    """Linux child pre-exec hook, used only by this single-threaded supervisor."""
    signal.signal(signal.SIGTERM, signal.SIG_DFL)
    libc = c.CDLL(None, use_errno=True)
    prctl = libc.prctl
    prctl.argtypes = [c.c_int, c.c_ulong, c.c_ulong, c.c_ulong, c.c_ulong]
    prctl.restype = c.c_int
    if prctl(1, signal.SIGTERM, 0, 0, 0) != 0:  # PR_SET_PDEATHSIG
        raise OSError(c.get_errno(), "Cannot arrange controller termination on parent death")
    if os.getppid() != parent_pid:
        os._exit(125)


def checked_config(folder, rate):
    """Accept only an intact generated configuration with a matching sample rate."""
    manifest = json.loads((folder / "MANIFEST.json").read_text())
    config = folder / "voicedsp-v2.conf"
    if manifest["rate"] != rate:
        raise ValueError("XMOS/controller/ALSA rates must match")
    if hashlib.sha256(config.read_bytes()).hexdigest() != manifest["outputs"][config.name]:
        raise ValueError("ALSA configuration changed after generation; review/regenerate it")
    return config


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--config-dir", type=Path, required=True)
    p.add_argument("--controller", type=Path, required=True)
    p.add_argument("--device", required=True)
    p.add_argument("--paired-0.6-image", action="store_true", required=True)
    p.add_argument("--rate", type=int, choices=(16000, 48000), required=True)
    p.add_argument("--route", choices=("off", "line", "ht326", "dac"), default="off")
    p.add_argument("--attenuation", type=int, choices=range(128), default=60)
    args = p.parse_args()
    if sys.platform != "linux" or not re.fullmatch(r"/dev/i2c-[0-9]+", args.device):
        p.error("Linux and an explicit /dev/i2c-N are required")
    controller = args.controller.resolve()
    if not controller.is_file() or not os.access(controller, os.X_OK):
        p.error("Build the paired V2 Linux controller and provide its executable")
    config = checked_config(args.config_dir.resolve(), args.rate)
    os.environ["ALSA_CONFIG_PATH"] = str(config)
    stop = [False]
    def request_stop(_sig, _frame):
        stop[0] = True
    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)
    command = [str(controller), "--device", args.device, "--paired-0.6-image",
               "--external-clocks", "--rate", str(args.rate), "--route", args.route,
               "--attenuation", str(args.attenuation)]
    parent = os.getpid()
    def launch():
        return subprocess.Popen(command, start_new_session=True,
                                preexec_fn=lambda: parent_death_guard(parent))
    result = supervise(AlsaSilence("voicedsp_v2_dmix", args.rate), launch, lambda: stop[0])
    print(json.dumps(result), flush=True)
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())

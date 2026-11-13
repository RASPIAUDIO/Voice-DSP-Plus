"""Actual ALSA route/plug through file->null only. Never open a sound card."""
import ctypes as c
import ctypes.util
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from configure import configure


@unittest.skipUnless(sys.platform=='linux' and ctypes.util.find_library('asound'), 'ALSA library')
class MatrixTests(unittest.TestCase):
    def check_signal(self,channels,input_frame,expected):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);folder=root/'generated'
            configure(folder,'NeverOpened',0,1)
            text=(folder/'voicedsp-v2.conf').read_text()
            # Exercise verbatim generated route+plug, replacing only the physical sink.
            matrix=text[text.index('# Stereo source'):text.index('pcm.voicedsp_v2_capture')]
            # Isolate fixture names from any installed desktop ALSA fragments.
            matrix=matrix.replace('voicedsp_v2_', 'matrix_test_')
            config=root/'null-only.conf';raw=root/'output.raw'
            config.write_text('</usr/share/alsa/alsa.conf>\npcm.matrix_test_dmix { type file slave.pcm "null" file "'+str(raw)+'" format "raw" }\n'+matrix)
            old=os.environ.get('ALSA_CONFIG_PATH')
            os.environ['ALSA_CONFIG_PATH']=str(config)
            lib=c.CDLL(ctypes.util.find_library('asound'))
            handle=c.c_void_p()
            lib.snd_pcm_open.argtypes=[c.POINTER(c.c_void_p),c.c_char_p,c.c_int,c.c_int]
            lib.snd_pcm_set_params.argtypes=[c.c_void_p,c.c_int,c.c_int,c.c_uint,c.c_uint,c.c_int,c.c_uint]
            lib.snd_pcm_writei.argtypes=[c.c_void_p,c.c_void_p,c.c_ulong]
            lib.snd_pcm_writei.restype=c.c_long
            lib.snd_pcm_drain.argtypes=[c.c_void_p]
            lib.snd_pcm_close.argtypes=[c.c_void_p]
            lib.snd_config_update_free_global()
            try:
                self.assertEqual(lib.snd_pcm_open(c.byref(handle),b'matrix_test_playback',0,0),0)
                fmt=lib.snd_pcm_format_value(b'S32_LE')
                self.assertEqual(lib.snd_pcm_set_params(handle,fmt,3,channels,48000,0,20000),0)
                samples=struct.pack('<'+'i'*len(input_frame),*input_frame)*256
                buf=c.create_string_buffer(samples)
                self.assertEqual(lib.snd_pcm_writei(handle,buf,256),256)
                self.assertEqual(lib.snd_pcm_drain(handle),0)
            finally:
                if handle:lib.snd_pcm_close(handle)
                lib.snd_config_update_free_global()
                if old is None:os.environ.pop('ALSA_CONFIG_PATH',None)
                else:os.environ['ALSA_CONFIG_PATH']=old
            first=struct.unpack('<ii',raw.read_bytes()[:8])
            self.assertEqual(first,(expected,expected))

    def test_equal_stereo_is_unity(self):
        self.check_signal(2,[0x20000000,0x20000000],0x20000000)

    def test_distinct_stereo_is_mono_average(self):
        self.check_signal(2,[0x10000000,0x30000000],0x20000000)

    def test_mono_is_unity_not_half(self):
        self.check_signal(1,[0x20000000],0x20000000)


if __name__=='__main__':unittest.main()

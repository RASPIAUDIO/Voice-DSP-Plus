import importlib.util
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import configure
import spi_boot


class KitTests(unittest.TestCase):
    def test_config_mono(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)/'config'
            result=configure.configure(out,'VoiceDSPV2I2S',0,1)
            text=(out/'voicedsp-v2.conf').read_text()
            self.assertEqual(result['rate'],48000)
            self.assertIn('0.0 0.5 1.0 0.5 0.1 0.5 1.1 0.5',text)
            self.assertIn('ttable.0.0 1',text)
            self.assertNotIn('pcm.!default',text)
            playback,capture=text.split('pcm.voicedsp_v2_dsnoop {',1)
            self.assertIn('buffer_size 1024',playback)
            self.assertIn('period_size 1024',capture)
            self.assertIn('buffer_size 8192',capture)

    def test_bad_card(self):
        with self.assertRaises(ValueError):
            configure.configure(Path('/unused'),'bad"card',0,1)

    @unittest.skipUnless((ROOT/'firmware/xmos-i2s48.spi_boot.bin').exists(), 'Source-only kit: paired image absent')
    def test_boot_order_and_bit_reversal(self):
        data=spi_boot.checked_image(ROOT/'firmware/xmos-i2s48.spi_boot.bin')
        events=[];registers={6:255,2:255,0:0}
        def write(r,v):
            events.append(('write',r,v));registers[r]=v
        def transfer(block):
            events.append(('block',block))
        result=spi_boot.boot(data,registers.__getitem__,write,transfer,sleep=lambda _:None)
        blocks=[e[1] for e in events if e[0]=='block']
        self.assertEqual(len(blocks),211)
        self.assertEqual(b''.join(blocks).translate(spi_boot.REVERSE),data)
        self.assertEqual(events[0],('write',2,(255&~0x15)|8))
        self.assertEqual(events[-2][0],'write')
        self.assertEqual(events[-2][1],6)
        self.assertEqual(registers[6]&0x0d,0x0d)
        self.assertFalse(result['execution_proven'])
        self.assertFalse(result['qspi_written'])

    @unittest.skipUnless((ROOT/'firmware/xmos-i2s48.spi_boot.bin').exists(), 'Source-only kit: paired image absent')
    def test_boot_failure_holds_reset(self):
        data=spi_boot.checked_image(ROOT/'firmware/xmos-i2s48.spi_boot.bin')
        writes=[]
        def broken(_):raise IOError('injected SPI failure')
        with self.assertRaises(IOError):
            spi_boot.boot(data,lambda r:0 if r==0 else 255,
                          lambda r,v:writes.append((r,v)),broken,sleep=lambda _:None)
        self.assertEqual(writes[-1][0],6)
        self.assertEqual(writes[-1][1]&0x1d,0)
        self.assertEqual(writes[-2][1]&0x15,0)

    def test_wrong_image_no_io(self):
        def forbidden(*_):raise AssertionError('I/O before hash validation')
        with self.assertRaises(ValueError):
            spi_boot.boot(bytes(864256),forbidden,forbidden,forbidden)

    @unittest.skipUnless(sys.platform=='linux','Linux installer fixture')
    def test_install_uninstall_roundtrip(self):
        import install
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            (root/install.BOOT).parent.mkdir(parents=True)
            before=b'# untouched user config\n[all]\narm_64bit=1\n'
            (root/install.BOOT).write_bytes(before)
            install.install(root,ROOT,'testuser','/dev/i2c-1','VoiceDSPV2I2S',0,1,True,True)
            unit=(root/install.UNIT).read_text()
            self.assertIn('Restart=no',unit)
            self.assertIn('boot_linux.py',unit)
            self.assertIn('User=testuser',unit)
            policy=json.loads((root/install.PREFIX/'accepted-aec.json').read_text())
            self.assertEqual(policy['default_profile'],4)
            self.assertEqual(policy['checkpoint_tag'],'v2-pi4-aec-validated-20261112')
            self.assertTrue((root/install.PREFIX/'scripts/verify_installation.py').is_file())
            self.assertTrue((root/install.OVERLAY).is_file())
            install.uninstall(root)
            self.assertEqual((root/install.BOOT).read_bytes(),before)
            self.assertFalse((root/install.UNIT).exists())
            self.assertFalse((root/install.PREFIX).exists())

    @unittest.skipUnless(sys.platform=='linux','Linux installer fixture')
    def test_installed_cli_uninstall_leaves_no_bytecode(self):
        import install
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            install.install(root,ROOT,'testuser','/dev/i2c-1','I2S',1,0,True,False)
            scripts=root/install.PREFIX/'scripts'
            subprocess.run([sys.executable,str(scripts/'verify_installation.py'),'--help'],check=True,capture_output=True)
            subprocess.run([sys.executable,str(scripts/'install.py'),'uninstall',
                            '--root',str(root),'--execute'],check=True,capture_output=True)
            self.assertFalse((root/install.PREFIX).exists(), list((root/install.PREFIX).rglob('*')))
            self.assertFalse((root/install.CONFIG).exists())

    @unittest.skipUnless(sys.platform=='linux','Linux installer fixture')
    def test_install_refuses_changed_config(self):
        import install
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            install.install(root,ROOT,'testuser','/dev/i2c-1','VoiceDSPV2I2S',0,1,False,False)
            config=root/install.CONFIG/'voicedsp-v2.conf'
            config.write_text(config.read_text()+'# user change\n')
            with self.assertRaises(ValueError):install.uninstall(root)
            self.assertTrue(config.is_file())
            with self.assertRaises(FileExistsError):
                install.install(root,ROOT,'testuser','/dev/i2c-1','VoiceDSPV2I2S',0,1,False,False)

    @unittest.skipUnless(sys.platform=='linux','Linux pinctrl parser, no GPIO')
    def test_pinctrl_formats(self):
        import boot_linux
        self.assertEqual(boot_linux.mode('18: a0 pn | lo // GPIO18 = PCM_CLK'),'a0')
        self.assertEqual(boot_linux.mode('18: a2 pn | lo // GPIO18 = I2S0_SCLK'),'a2')
        self.assertEqual(boot_linux.spi_function('10, GPIO10, SPI0_MOSI, other',('SPI0_MOSI',)),'a0')
        with self.assertRaises(ValueError):boot_linux.mode('not a pin')
        with self.assertRaises(ValueError):boot_linux.spi_function('10, GPIO10, other',('SPI0_MOSI',))


if __name__=='__main__':
    unittest.main()

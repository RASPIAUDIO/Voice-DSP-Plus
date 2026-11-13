"""Desktop output routing via the existing controller. No direct hardware access."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import threading

sys.dont_write_bytecode = True
sys.path.insert(0, '/opt/voicedsp-v2-48k/scripts')
from control import request

SOCKET = '/run/voicedsp-v2-48k/control.sock'
SINK = 'voicedsp_speaker'
MODES = {'internal': 'Internal', 'external': 'External', 'full': 'Full Speaker'}
AEC_KEYS = ('aec_parameter_words', 'aec_mux', 'aec_profile', 'aec_system_delay', 'aec_asr_output')
PD_WARNING = (
    'External and Full Speaker require a USB-C Power Delivery charger connected '
    'directly to Voice DSP+ (65 W or higher recommended).\n\n'
    'This powers both Voice DSP+ and the Raspberry Pi through the 40-pin header. '
    'Power off before rewiring: disconnect the Pi\'s USB-C supply and connect the '
    'PD charger to Voice DSP+ instead. Never power both USB-C inputs at once.')


def confirmation_text(mode):
    if mode not in ('external', 'full'):
        return None
    text = PD_WARNING
    if mode == 'external':
        text += ('\n\nExternal is unfiltered: use a full-range speaker or passive crossover. '
                 'For a separate L woofer / R tweeter, choose Full Speaker.')
    return text


def run(*args):
    return subprocess.check_output(args, text=True, timeout=10).strip()


def mode_of(status):
    if status['requested_route'] == 4:
        return 'internal'
    if status['requested_route'] == 3:
        return 'full' if status['full_speaker'] else 'external'
    return None


def switch(mode, confirmed=False, power_confirmed=False):
    if mode not in MODES:
        raise ValueError('Unknown output')
    if confirmation_text(mode) and not power_confirmed:
        raise ValueError(PD_WARNING + ' Confirm the wiring with --confirm-pd.')
    if mode == 'external' and not confirmed:
        raise ValueError('External requires a full-range speaker or a passive crossover')
    import fcntl
    lock = Path(os.environ.get('XDG_RUNTIME_DIR', '/run/user/' + str(os.getuid()))) / 'voicedsp-output.lock'
    with lock.open('w') as handle:
        fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        before = request(SOCKET, 'status')
        if not before.get('ready') or not before.get('aec_valid'):
            raise RuntimeError('Audio controller / accepted AEC is not ready')
        sinks = json.loads(run('pactl', '-f', 'json', 'list', 'sinks'))
        sink = next(s for s in sinks if s['name'] == SINK)
        run('pactl', 'set-sink-mute', SINK, '1')
        # Restore the previous mute only after successful routing and readback.
        # On any error leave playback muted; never increase the hardware gain.
        request(SOCKET, 'output ' + mode)
        after = request(SOCKET, 'status')
        if not after.get('ready') or not after.get('aec_valid') or mode_of(after) != mode:
            raise RuntimeError('Output was not confirmed; playback remains muted')
        if any(before.get(k) != after.get(k) for k in AEC_KEYS):
            raise RuntimeError('AEC readback changed; playback remains muted')
        if mode == 'full' and not after.get('crossover_verified'):
            raise RuntimeError('Crossover not confirmed; playback remains muted')
        run('pactl', 'set-sink-mute', SINK, str(int(sink['mute'])))
        return after


def gui(argv):
    import gi
    gi.require_version('Gtk', '3.0')
    gi.require_version('AyatanaAppIndicator3', '0.1')
    from gi.repository import Gtk, Gio, GLib, AyatanaAppIndicator3

    class App(Gtk.Application):
        def __init__(self):
            super().__init__(application_id='com.raspiaudio.VoiceDSPOutput',
                             flags=Gio.ApplicationFlags.HANDLES_COMMAND_LINE)
            self.add_main_option('tray', 0, GLib.OptionFlags.NONE, GLib.OptionArg.NONE,
                                 'Start in the desktop tray', None)
            self.window = None
            self.busy = False
            self.confirmation_open = False
            self.current = 'Connecting...'

        def do_startup(self):
            Gtk.Application.do_startup(self)
            self.hold()
            self.indicator = AyatanaAppIndicator3.Indicator.new(
                'voicedsp-output', 'audio-card', AyatanaAppIndicator3.IndicatorCategory.HARDWARE)
            self.indicator.set_status(AyatanaAppIndicator3.IndicatorStatus.ACTIVE)
            self.indicator.set_title('Voice DSP+ Output')
            self.menu = Gtk.Menu()
            self.state_item = Gtk.MenuItem(label=self.current)
            self.state_item.set_sensitive(False)
            self.menu.append(self.state_item)
            self.items = []
            for key, title in MODES.items():
                item = Gtk.MenuItem(label=title)
                item.connect('activate', lambda _, mode=key: self.choose(mode))
                self.menu.append(item)
                self.items.append(item)
            item = Gtk.MenuItem(label='Open output selector...')
            item.connect('activate', lambda _: self.show_window())
            self.menu.append(item)
            self.menu.show_all()
            self.indicator.set_menu(self.menu)
            GLib.timeout_add_seconds(3, self.refresh)
            self.refresh()

        def do_command_line(self, command):
            option = command.get_options_dict().lookup_value('tray', None)
            if option is None or not option.get_boolean():
                self.show_window()
            return 0

        def show_window(self):
            if self.window is None:
                self.window = Gtk.ApplicationWindow(application=self, title='Voice DSP+ Output')
                self.window.set_default_size(460, 240)
                self.window.set_border_width(20)
                self.window.connect('delete-event', lambda w, _: w.hide() or True)
                box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
                self.window.add(box)
                self.label = Gtk.Label(label=self.current, xalign=0)
                self.label.set_line_wrap(True)
                box.pack_start(self.label, False, False, 0)
                for key, title in MODES.items():
                    button = Gtk.Button(label=title)
                    button.connect('clicked', lambda _, mode=key: self.choose(mode))
                    self.items.append(button)
                    box.pack_start(button, False, False, 0)
                note = Gtk.Label(label='Volume and microphone: use the desktop sound icons.\n'
                                      'Full Speaker: L woofer / R tweeter, validated crossover.\n'
                                      'AEC settings and volume are preserved.', xalign=0)
                note.set_line_wrap(True)
                box.pack_start(note, False, False, 0)
            self.window.show_all()
            self.window.present()

        def work(self, operation):
            if self.busy:
                return
            self.busy = True
            for item in self.items:
                item.set_sensitive(False)
            def worker():
                try:
                    result, error = operation(), None
                except Exception as exc:
                    result, error = None, str(exc)
                GLib.idle_add(self.finished, result, error)
            threading.Thread(target=worker, daemon=True).start()

        def finished(self, status, error):
            self.busy = False
            if error:
                self.current = 'Unavailable: ' + error
            else:
                self.current = 'Output: ' + MODES.get(mode_of(status), 'Off / other')
                if not status.get('ready'):
                    self.current = 'Audio controller not ready'
            self.state_item.set_label(self.current)
            for item in self.items:
                item.set_sensitive(True)
            if self.window:
                self.label.set_text(self.current)
            return False

        def refresh(self):
            if not self.busy and not self.confirmation_open:
                self.work(lambda: request(SOCKET, 'status'))
            return True

        def choose(self, mode):
            if self.busy or self.confirmation_open:
                return
            warning = confirmation_text(mode)
            if warning:
                self.confirmation_open = True
                self.show_window()
                dialog = Gtk.MessageDialog(transient_for=self.window, modal=True,
                    message_type=Gtk.MessageType.WARNING, buttons=Gtk.ButtonsType.NONE,
                    text='USB-C Power Delivery required')
                dialog.format_secondary_text(warning)
                dialog.add_button('Cancel', Gtk.ResponseType.CANCEL)
                dialog.add_button('PD connected - continue', Gtk.ResponseType.OK)
                dialog.set_default_response(Gtk.ResponseType.CANCEL)
                def response(d, answer):
                    d.destroy()
                    self.confirmation_open = False
                    if answer == Gtk.ResponseType.OK:
                        self.work(lambda: switch(mode, confirmed=True, power_confirmed=True))
                dialog.connect('response', response)
                dialog.show_all()
            else:
                self.work(lambda: switch(mode))

    return App().run(argv)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tray', action='store_true')
    parser.add_argument('--status', action='store_true')
    parser.add_argument('--select', choices=MODES)
    parser.add_argument('--confirm-full-range', action='store_true')
    parser.add_argument('--confirm-pd', action='store_true',
                        help='Confirm PD powers Voice DSP+ and the Pi USB-C supply is disconnected')
    args = parser.parse_args()
    if args.status:
        print(json.dumps(request(SOCKET, 'status'), indent=2))
    elif args.select:
        print(json.dumps(switch(args.select, args.confirm_full_range, args.confirm_pd), indent=2))
    else:
        sys.exit(gui(sys.argv))

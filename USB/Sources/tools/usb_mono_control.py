"""Private USB mono control. Dry run unless --execute is explicitly given."""
import argparse
import json
import struct
import math
import time

VID, PID, RELEASE = 0x20B1, 0x4FC7, 0x0080
SERIAL = "VDSP2-USBMONO-080-DEV"
IDENTITIES = {(RELEASE, SERIAL), (0x0081, "VDSP2-USBMONO-081-DEV"), (0x0082, "VDSP2-USBMONO-082-DEV"), (0x0083, "VDSP2-USBMONO-083-DEV")}
IDENTITIES.add((0x0084, "VDSP2-USBMONO-084-DEV"))
IDENTITIES.add((0x0085, "VDSP2-USBMONO-085-DEV"))
IDENTITIES.add((0x0086, "VDSP2-USBMONO-086-DEV"))
IDENTITIES.add((0x0087, "VDSP2-USBMONO-087-DEV"))
IDENTITIES.add((0x0088, "VDSP2-USBMONO-088-DEV"))
IDENTITIES.add((0x0089, "VDSP2-USBMONO-089-DEV"))
RESOURCE, REQUEST, STATUS = 0x30, 17, 18
ROUTES = {"off": 1, "line": 2, "external": 3, "internal": 4, "full-speaker": 3}


def read_response(device, command_id, size):
    # XMOS servicer reads prefix the application payload with a status byte.
    response = bytes(device.ctrl_transfer(0xC0, 0, command_id | 0x80,
                                         RESOURCE, size + 1, timeout=2000))
    if len(response) != size + 1:
        raise RuntimeError("Short USB control response")
    if response[0] != 0:
        raise RuntimeError(f"XMOS command {command_id} status 0x{response[0]:02x}")
    return response[1:]


def filter_command(cutoff,left,right):
    if not all(math.isfinite(x) for x in (cutoff,left,right)) or not 6000<=cutoff<=10000 or not -24<=left<=0 or not -24<=right<=0:
        raise ValueError('Crossover: 6000..10000 Hz, each output trim -24..0 dB')
    return struct.pack('<fff',cutoff,left,right)


def filter_exchange(device,payload=None):
    if device.bcdDevice not in (0x0082,0x0083,0x0084,0x0085,0x0086,0x0087,0x0088,0x0089):
        raise ValueError('Parametric Full Speaker requires firmware 0.8.2..0.8.9')
    if payload is not None:
        # Explicit OFF, verified applied, before replacing the optional filter.
        state=exchange(device,command('off',-30,True))
        deadline=time.monotonic()+3
        while state['active_route']!=1 or state['applied_epoch']!=state['accepted_epoch']:
            if time.monotonic()>deadline:raise RuntimeError('Audio OFF was not confirmed')
            time.sleep(.05);state=exchange(device)
        count=device.ctrl_transfer(0x40,0,19,RESOURCE,payload,timeout=2000)
        ack=bytes(device.ctrl_transfer(0xC0,0,19,RESOURCE,1,timeout=2000))
        if count!=12 or ack!=b'\0':raise RuntimeError('Filter request rejected')
    data=read_response(device,20,16)
    if len(data)!=16:raise RuntimeError('Short filter readback')
    if payload is not None and data[:12]!=payload:raise RuntimeError('Filter readback mismatch')
    cutoff,left,right,revision=struct.unpack('<fffI',data)
    filter_command(cutoff,left,right)
    return {'cutoff_hz':cutoff,'woofer_trim_db':left,'tweeter_trim_db':right,'revision':revision}


def command(route, db, muted):
    if not -60 <= db <= 0 or db * 2 != int(db * 2):
        raise ValueError("Level must be -60..0 dB in 0.5 dB steps")
    return bytes((2, ROUTES[route], int(-2 * db), int(muted) | (2 if route == "full-speaker" else 0)))


def decode(data):
    if len(data) != 56 or data[0] != 2:
        raise ValueError("Unexpected firmware status schema")
    flags = data[5]
    return {
        "ready": bool(data[1]), "requested_route": data[2], "active_route": data[3],
        "ui_state": data[4], "io_valid": bool(flags & 1),
        "power_good": bool(flags & 4), "jack": bool(flags & 8),
        "fault": bool(flags & 16), "speaker_muted": bool(flags & 32),
        "clocks": bool(flags & 64), "aec_profiles_applied": bool(flags & 128),
        "pcal_ports": list(data[6:8]), "gesture_ready": bool(data[8]),
        "gesture_raw": int.from_bytes(data[9:11], "little"), "aec_profile": data[11],
        "error": struct.unpack_from("<i", data, 12)[0],
        "accepted_epoch":int.from_bytes(data[16:20],'little'),
        "applied_epoch":int.from_bytes(data[20:24],'little'),
        "uptime_ms": int.from_bytes(data[24:28], "little"),
        "audio_frames": int.from_bytes(data[28:32], "little"),
        "gesture_errors": int.from_bytes(data[32:36], "little"),
        "gesture_sequence": int.from_bytes(data[36:40], "little"),
        "settings_state": data[52], "settings_restored": bool(data[53]),
        "settings_sequence": int.from_bytes(data[54:56], "little"),
        "geometry_code": data[43], "level_db": -data[44] / 2,
        "full_speaker": bool(data[40] & 2), "raw_hex": bytes(data).hex(),
    }


def exchange(device, payload=None):
    if payload is not None:
        count = device.ctrl_transfer(0x40, 0, REQUEST, RESOURCE, payload, timeout=2000)
        if count != 4:
            raise RuntimeError("Short USB command write")
        ack = bytes(device.ctrl_transfer(0xC0, 0, REQUEST, RESOURCE, 1, timeout=2000))
        if ack != b"\x00":
            raise RuntimeError(f"Firmware rejected command: {ack.hex()}")
    return decode(read_response(device, STATUS, 56))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--libusb', help='Explicit libusb-1.0 DLL path on Windows')
    parser.add_argument("--route", choices=ROUTES)
    parser.add_argument("--db", type=float, default=-30)
    parser.add_argument("--mute", action="store_true")
    parser.add_argument('--crossover-hz',type=float)
    parser.add_argument('--woofer-trim-db',type=float,default=0)
    parser.add_argument('--tweeter-trim-db',type=float,default=-6)
    parser.add_argument('--filter-status',action='store_true')
    parser.add_argument("--execute", action="store_true", help="Access USB; without --route, read status only")
    args = parser.parse_args()
    payload = command(args.route, args.db, args.mute) if args.route else None
    filter_payload=filter_command(args.crossover_hz,args.woofer_trim_db,args.tweeter_trim_db) if args.crossover_hz is not None else None
    if filter_payload is None and (args.woofer_trim_db!=0 or args.tweeter_trim_db!=-6):
        parser.error('Specify --crossover-hz with output trims')
    if not args.route and (args.mute or args.db != -30):
        parser.error("Specify --route when setting volume or mute")
    if not args.execute:
        print(json.dumps({"dry_run": True, "usb_access": False,
                          "operation": "set_route" if payload else "read_status",
                          "payload_hex": payload.hex() if payload else None,
                          'filter_payload_hex':filter_payload.hex() if filter_payload else None,
                          'filter_read':args.filter_status or filter_payload is not None}))
        return
    # No reset, configuration switch, driver detachment, flash or DSP tuning here.
    import usb.core
    import usb.util
    import usb.backend.libusb1
    backend = None
    if args.libusb:
        backend = usb.backend.libusb1.get_backend(find_library=lambda name: args.libusb)
        if backend is None:
            raise RuntimeError('Could not load the specified libusb library')
    devices = list(usb.core.find(find_all=True, idVendor=VID, idProduct=PID, backend=backend))
    candidates = [dev for dev in devices if
                  (dev.bcdDevice, usb.util.get_string(dev, dev.iSerialNumber)) in IDENTITIES]
    if len(candidates) != 1:
        raise RuntimeError(f"Expected exactly one supported USB mono target, found {len(candidates)}")
    device = candidates[0]
    try:
        filtered=filter_exchange(device,filter_payload) if args.filter_status or filter_payload is not None else None
        result=exchange(device,payload)
        if filtered is not None:result['full_speaker_settings']=filtered
        print(json.dumps(result,indent=2))
    finally:
        usb.util.dispose_resources(device)


if __name__ == "__main__":
    main()

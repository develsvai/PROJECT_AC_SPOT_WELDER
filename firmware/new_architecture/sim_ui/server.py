from __future__ import annotations

import ctypes
import json
import os
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


ROOT = Path(__file__).resolve().parent
SIM_DIR = ROOT.parent / "sim"
LIB_PATH = SIM_DIR / "libacspot_sim.dylib"


class SimSnapshot(ctypes.Structure):
    _fields_ = [
        ("time_ms", ctypes.c_uint32),
        ("setting_time_ms", ctypes.c_uint16),
        ("setting_multiplier", ctypes.c_uint8),
        ("setting_rest_ms", ctypes.c_uint16),
        ("setting_mode", ctypes.c_uint8),
        ("ui_state", ctypes.c_uint8),
        ("menu_item", ctypes.c_uint8),
        ("edit_value", ctypes.c_int16),
        ("process_state", ctypes.c_uint8),
        ("process_pulses_remaining", ctypes.c_uint8),
        ("auto_state", ctypes.c_uint8),
        ("auto_baseline_feature", ctypes.c_uint16),
        ("auto_half_feature", ctypes.c_uint16),
        ("triac_on", ctypes.c_bool),
        ("buzzer_on", ctypes.c_bool),
        ("sense_on", ctypes.c_bool),
        ("contact_on", ctypes.c_bool),
        ("pulse_count", ctypes.c_uint32),
        ("weld_count", ctypes.c_uint32),
        ("lcd_line0", ctypes.c_char * 17),
        ("lcd_line1", ctypes.c_char * 17),
    ]


def load_sim():
    if not LIB_PATH.exists():
        raise FileNotFoundError(
            f"{LIB_PATH} not found. Run `make -C {SIM_DIR}` first."
        )

    lib = ctypes.CDLL(str(LIB_PATH))
    lib.sim_api_init.restype = None
    lib.sim_api_step_ms.argtypes = [ctypes.c_uint32]
    lib.sim_api_step_ms.restype = None
    lib.sim_api_press_manual.restype = None
    lib.sim_api_manual_cycle.restype = None
    lib.sim_api_press_encoder.restype = None
    lib.sim_api_rotate_encoder.argtypes = [ctypes.c_int8]
    lib.sim_api_rotate_encoder.restype = None
    lib.sim_api_set_contact.argtypes = [ctypes.c_bool]
    lib.sim_api_set_contact.restype = None
    lib.sim_api_toggle_contact.restype = None
    lib.sim_api_touch_pulse.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    lib.sim_api_touch_pulse.restype = None
    lib.sim_api_set_settings.argtypes = [
        ctypes.c_uint16,
        ctypes.c_uint8,
        ctypes.c_uint16,
        ctypes.c_uint8,
    ]
    lib.sim_api_set_settings.restype = None
    lib.sim_api_get_snapshot.argtypes = [ctypes.POINTER(SimSnapshot)]
    lib.sim_api_get_snapshot.restype = None
    lib.sim_api_init()
    return lib


SIM = load_sim()
SIM_LOCK = threading.Lock()


def snapshot_to_dict() -> dict:
    snap = SimSnapshot()
    with SIM_LOCK:
        SIM.sim_api_get_snapshot(ctypes.byref(snap))
    return {
        "time_ms": snap.time_ms,
        "settings": {
            "time_ms": snap.setting_time_ms,
            "multiplier": snap.setting_multiplier,
            "rest_ms": snap.setting_rest_ms,
            "mode": snap.setting_mode,
        },
        "ui_state": snap.ui_state,
        "menu_item": snap.menu_item,
        "edit_value": snap.edit_value,
        "process_state": snap.process_state,
        "process_pulses_remaining": snap.process_pulses_remaining,
        "auto_state": snap.auto_state,
        "auto_baseline_feature": snap.auto_baseline_feature,
        "auto_half_feature": snap.auto_half_feature,
        "triac_on": bool(snap.triac_on),
        "buzzer_on": bool(snap.buzzer_on),
        "sense_on": bool(snap.sense_on),
        "contact_on": bool(snap.contact_on),
        "pulse_count": snap.pulse_count,
        "weld_count": snap.weld_count,
        "lcd_line0": snap.lcd_line0.decode("utf-8", "ignore"),
        "lcd_line1": snap.lcd_line1.decode("utf-8", "ignore"),
    }


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def _json(self, payload: dict, code: int = 200):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/state":
            self._json(snapshot_to_dict())
            return

        if parsed.path == "/api/action":
            params = parse_qs(parsed.query)
            action = params.get("name", [""])[0]
            value = params.get("value", [""])[0]

            try:
                if action == "step":
                    with SIM_LOCK:
                        SIM.sim_api_step_ms(int(value or "0"))
                elif action == "manual_press":
                    with SIM_LOCK:
                        SIM.sim_api_manual_cycle()
                elif action == "encoder_press":
                    with SIM_LOCK:
                        SIM.sim_api_press_encoder()
                elif action == "encoder_rotate":
                    with SIM_LOCK:
                        SIM.sim_api_rotate_encoder(int(value or "0"))
                elif action == "contact_set":
                    with SIM_LOCK:
                        SIM.sim_api_set_contact(value == "1")
                elif action == "contact_toggle":
                    with SIM_LOCK:
                        SIM.sim_api_toggle_contact()
                elif action == "touch_pulse":
                    contact_ms = max(10, min(1000, int(params.get("contact_ms", ["300"])[0])))
                    settle_ms = max(10, min(2000, int(params.get("settle_ms", ["400"])[0])))
                    with SIM_LOCK:
                        SIM.sim_api_touch_pulse(contact_ms, settle_ms)
                elif action == "set_settings":
                    time_ms = max(1, min(150, int(params.get("time_ms", ["50"])[0])))
                    multiplier = max(1, min(20, int(params.get("multiplier", ["1"])[0])))
                    rest_ms = max(0, min(1000, int(params.get("rest_ms", ["200"])[0])))
                    mode = 1 if params.get("mode", ["0"])[0] == "1" else 0
                    with SIM_LOCK:
                        SIM.sim_api_set_settings(time_ms, multiplier, rest_ms, mode)
                elif action == "reset":
                    with SIM_LOCK:
                        SIM.sim_api_init()
                else:
                    self._json({"error": f"unknown action: {action}"}, code=400)
                    return
            except Exception as exc:
                self._json({"error": str(exc)}, code=500)
                return

            self._json(snapshot_to_dict())
            return

        return super().do_GET()


def main():
    port = int(os.environ.get("PORT", "8000"))
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"Serving simulator UI at http://127.0.0.1:{port}")
    server.serve_forever()


if __name__ == "__main__":
    main()

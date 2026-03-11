#!/usr/bin/env python3
"""
read_serial.py  –  W65C02S Bus Monitor reader
Usage:
    python3 read_serial.py [PORT] [BAUD]
"""

import sys
import serial
import serial.tools.list_ports
from datetime import datetime
from rich.console import Console
from rich.text import Text

console = Console()

HEADER = (
    f"{'#':>8}  {'Timestamp':<26}  {'Boot ms':>10}  "
    f"{'Address':^9}  {'Data':^6}  {'R/W':^5}"
)
DIVIDER = "─" * len(HEADER)


def auto_detect_port() -> str:
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        mfr  = (p.manufacturer or "").lower()
        if any(kw in desc or kw in mfr
               for kw in ("arduino", "ch340", "cp210", "ftdi", "usbmodem")):
            return p.device
    candidates = serial.tools.list_ports.comports()
    if candidates:
        return candidates[0].device
    raise RuntimeError("No serial port found. Pass PORT explicitly.")


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else auto_detect_port()
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    console.rule("[bold cyan]W65C02S Bus Monitor")
    console.print(f"  Port [green]{port}[/]  ·  [green]{baud}[/] baud  ·  Ctrl-C to quit\n")
    console.print(f"[dim]{HEADER}[/]")
    console.print(f"[dim]{DIVIDER}[/]")

    try:
        with serial.Serial(port, baud, timeout=1) as ser:
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if not line or line == "READY":
                    continue

                parts = line.split(",")
                if len(parts) != 5:
                    continue

                try:
                    cycle   = int(parts[0])
                    boot_ms = int(parts[1])
                    address = parts[2].upper()
                    data    = parts[3].upper()
                    rw      = parts[4].strip().upper()
                except ValueError:
                    continue

                ts     = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
                rw_fmt = f"[bold cyan]  R  [/]" if rw == "R" else f"[bold yellow]  W  [/]"

                row = Text.assemble(
                    (f"{cycle:>8}", "dim"),
                    "  ",
                    (f"{ts:<26}", "white"),
                    "  ",
                    (f"{boot_ms:>10,}", "dim yellow"),
                    "  ",
                    (f"${address:^8}", "bold green"),
                    "  ",
                    (f"${data:^5}", "bold magenta"),
                    "  ",
                )
                console.print(row, rw_fmt, sep="", end="\n")

    except serial.SerialException as e:
        console.print(f"[red]Serial error:[/] {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        console.print("\n[dim]Stopped.[/]")


if __name__ == "__main__":
    main()

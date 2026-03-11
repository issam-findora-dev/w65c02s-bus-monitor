#!/usr/bin/env python3
"""
read_serial.py  –  W65C02S Bus Monitor reader
Usage:
    python3 read_serial.py [PORT] [BAUD]

Defaults:
    PORT  – auto-detected Arduino
    BAUD  – 115200
"""

import sys
import serial
import serial.tools.list_ports
from datetime import datetime
from rich.console import Console
from rich.table import Table
from rich.live import Live
from rich.text import Text
from rich import box


def auto_detect_port() -> str:
    candidates = serial.tools.list_ports.comports()
    for p in candidates:
        desc = (p.description or "").lower()
        mfr  = (p.manufacturer or "").lower()
        if any(kw in desc or kw in mfr
               for kw in ("arduino", "ch340", "cp210", "ftdi", "usbmodem")):
            return p.device
    if candidates:
        return candidates[0].device
    raise RuntimeError("No serial port found. Pass PORT explicitly.")


def make_table() -> Table:
    t = Table(
        box=box.ROUNDED,
        show_header=True,
        header_style="bold cyan",
        border_style="bright_black",
        show_lines=False,
        expand=False,
    )
    t.add_column("#",          style="dim",          justify="right",  width=8)
    t.add_column("Timestamp",  style="white",        justify="left",   width=26)
    t.add_column("Boot (ms)",  style="dim yellow",   justify="right",  width=12)
    t.add_column("Address",    style="bold green",   justify="center", width=9)
    t.add_column("Data",       style="bold magenta", justify="center", width=6)
    t.add_column("R/W",        justify="center",                       width=5)
    return t


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else auto_detect_port()
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    console = Console()
    console.rule("[bold cyan]W65C02S Bus Monitor")
    console.print(f"  Port [green]{port}[/]  ·  [green]{baud}[/] baud  ·  Ctrl-C to quit\n")

    table = make_table()

    try:
        with serial.Serial(port, baud, timeout=2) as ser, Live(table, console=console, refresh_per_second=20):
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()

                if line == "READY":
                    continue

                # Expect CSV: cycle,millis,address,data,R/W
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

                ts     = datetime.now().strftime("%Y-%m-%d  %H:%M:%S.%f")[:-3]
                rw_fmt = "[bold cyan]R[/]" if rw == "R" else "[bold yellow]W[/]"

                table.add_row(
                    str(cycle),
                    ts,
                    f"{boot_ms:,}",
                    f"${address}",
                    f"${data}",
                    rw_fmt,
                )

    except serial.SerialException as e:
        console.print(f"[red]Serial error:[/] {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        console.print("\n[dim]Stopped.[/]")


if __name__ == "__main__":
    main()

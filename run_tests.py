#!/usr/bin/env python3
"""
run_tests.py  –  W65C02S Bus Monitor test runner
Reads mock-mode output from the Arduino, validates PASS/FAIL lines,
exits 0 if all pass, 1 if any fail or if no output is received.

Usage: python3 run_tests.py [PORT] [CYCLES]
  PORT    serial port (auto-detected if omitted)
  CYCLES  number of cycles to validate before exiting (default: 20)
"""

import sys
import time
import serial
import serial.tools.list_ports
from rich.console import Console
from rich.table import Table
from rich import box

TIMEOUT_S  = 10   # seconds to wait for first output before giving up


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
    port       = sys.argv[1] if len(sys.argv) > 1 else auto_detect_port()
    want_cycles = int(sys.argv[2]) if len(sys.argv) > 2 else 20

    console = Console()
    console.rule("[bold cyan]W65C02S Bus Monitor — Mock Test")
    console.print(f"  Port [green]{port}[/]  ·  expecting [cyan]{want_cycles}[/] cycles\n")

    table = Table(
        box=box.ROUNDED,
        header_style="bold cyan",
        border_style="bright_black",
        show_lines=False,
    )
    table.add_column("Cycle",         justify="right",  style="dim",          width=7)
    table.add_column("Expected addr", justify="center", style="cyan",         width=15)
    table.add_column("Expected data", justify="center", style="cyan",         width=15)
    table.add_column("Exp R/W",       justify="center", style="cyan",         width=8)
    table.add_column("Got addr",      justify="center", style="green",        width=10)
    table.add_column("Got data",      justify="center", style="green",        width=10)
    table.add_column("Got R/W",       justify="center", style="green",        width=8)
    table.add_column("Result",        justify="center",                       width=14)

    passed = 0
    failed = 0
    received = 0

    try:
        with serial.Serial(port, 115200, timeout=1) as ser:
            # Arduino resets on serial open — wait for MOCK MODE banner
            deadline = time.time() + TIMEOUT_S
            ready = False
            while time.time() < deadline:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if "MOCK MODE" in line:
                    ready = True
                    break

            if not ready:
                console.print("[red]ERROR:[/] no output from Arduino within "
                              f"{TIMEOUT_S}s. Is the mock build flashed?")
                sys.exit(1)

            # Drain header lines until we hit data rows
            for _ in range(5):
                ser.readline()

            while received < want_cycles:
                raw = ser.readline().decode("utf-8", errors="replace").strip()
                if not raw:
                    continue

                # Expected format (from sketch):
                # "  1 |    $0000 |       $00 |   R |    $0000 |       $00 |   R | PASS  [P:1 F:0]"
                ok_str = "PASS" if "PASS" in raw else "FAIL" if "FAIL" in raw else None
                if ok_str is None:
                    continue

                parts = [p.strip() for p in raw.split("|")]
                if len(parts) < 8:
                    continue

                try:
                    cycle     = parts[0].strip()
                    exp_addr  = parts[1].strip()
                    exp_data  = parts[2].strip()
                    exp_rw    = parts[3].strip()
                    got_addr  = parts[4].strip()
                    got_data  = parts[5].strip()
                    got_rw    = parts[6].strip()
                    result    = parts[7].split()[0]
                except IndexError:
                    continue

                received += 1
                if result == "PASS":
                    passed += 1
                    result_cell = "[bold green]PASS ✓[/]"
                else:
                    failed += 1
                    result_cell = "[bold red]FAIL ✗[/]"

                rw_color = "cyan" if exp_rw == "R" else "yellow"
                table.add_row(
                    cycle, exp_addr, exp_data,
                    f"[{rw_color}]{exp_rw}[/]",
                    got_addr, got_data,
                    f"[{rw_color}]{got_rw}[/]",
                    result_cell,
                )

    except serial.SerialException as e:
        console.print(f"[red]Serial error:[/] {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        console.print("\n[dim]Interrupted.[/]")

    console.print(table)
    console.print()

    if received == 0:
        console.print("[red]No test cycles received.[/]")
        sys.exit(1)

    if failed == 0:
        console.print(f"[bold green]All {passed}/{received} cycles PASSED.[/]")
        sys.exit(0)
    else:
        console.print(f"[bold red]{failed} of {received} cycles FAILED.[/]")
        sys.exit(1)


if __name__ == "__main__":
    main()

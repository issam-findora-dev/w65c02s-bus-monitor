FQBN   := arduino:avr:uno
PORT   := /dev/cu.usbserial-210
SKETCH := bus_monitor

.PHONY: compile upload flash monitor clean compile-test flash-test compile-mock flash-mock

compile:
	arduino-cli compile --fqbn $(FQBN) $(SKETCH)

upload:
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) $(SKETCH)

flash: compile upload

monitor:
	arduino-cli monitor --port $(PORT) --config baudrate=115200

read:
	.venv/bin/python read_serial.py $(PORT)

clean:
	arduino-cli compile --fqbn $(FQBN) $(SKETCH) --clean

compile-test:
	arduino-cli compile --fqbn $(FQBN) bus_monitor_test

flash-test: compile-test
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) bus_monitor_test

compile-mock:
	arduino-cli compile --fqbn $(FQBN) --build-property "compiler.cpp.extra_flags=-DMOCK_HW" bus_monitor

flash-mock: compile-mock
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) bus_monitor

test: flash-mock
	.venv/bin/python run_tests.py $(PORT)

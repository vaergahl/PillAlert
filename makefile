FQBN = arduino:avr:uno
BUILD_PATH = ./build
PORT = /dev/ttyUSB0
CLI = arduino-cli --config-dir $(ARDUINO_CONFIG)

upload: compile
	$(CLI) upload --port $(PORT) --fqbn $(FQBN) --input-dir $(BUILD_PATH)

compile:
	$(CLI) compile --fqbn $(FQBN) --build-path $(BUILD_PATH) .

monitor:
	$(CLI) monitor --port $(PORT)

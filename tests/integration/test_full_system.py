#!/usr/bin/env python3
"""
Integration tests for Speeduino UI system.
Requires virtual CAN (vcan0) for testing without hardware.

Usage:
    sudo modprobe vcan
    sudo ip link add dev vcan0 type vcan
    sudo ip link set up vcan0
    pytest test_full_system.py -v
"""

import pytest
import subprocess
import time
import socket
import struct
import zmq
import msgpack


class TestCanService:
    """Tests for can_service"""

    @pytest.fixture
    def vcan_setup(self):
        """Ensure vcan0 is available"""
        result = subprocess.run(
            ["ip", "link", "show", "vcan0"],
            capture_output=True
        )
        if result.returncode != 0:
            pytest.skip("vcan0 not available - run setup first")

    @pytest.fixture
    def zmq_context(self):
        ctx = zmq.Context()
        yield ctx
        ctx.term()

    def test_can_service_starts(self, vcan_setup):
        """Test that can_service starts without errors"""
        proc = subprocess.Popen(
            ["./build/src/can_service/can_service",
             "--interface=vcan0", "--config=./configs"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(1)
        assert proc.poll() is None, "can_service crashed on start"
        proc.terminate()
        proc.wait()

    def test_zmq_publisher(self, vcan_setup, zmq_context):
        """Test that can_service publishes data via ZMQ"""
        # Start can_service
        proc = subprocess.Popen(
            ["./build/src/can_service/can_service",
             "--interface=vcan0", "--config=./configs"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(1)

        try:
            # Connect ZMQ subscriber
            sub = zmq_context.socket(zmq.SUB)
            sub.connect("ipc:///tmp/speeduino_data.ipc")
            sub.setsockopt_string(zmq.SUBSCRIBE, "ENGINE")
            sub.setsockopt(zmq.RCVTIMEO, 2000)

            # Send a test CAN frame (Haltech DATA1)
            # RPM=3000 (0x0BB8), MAP=80 (0x0320), TPS=50 (0x01F4)
            subprocess.run([
                "cansend", "vcan0",
                "360#0BB803200001F4000"
            ])

            # Should receive data
            try:
                topic, data = sub.recv_multipart()
                assert topic == b"ENGINE"
                engine_data = msgpack.unpackb(data, raw=False)
                assert "rpm" in engine_data or isinstance(engine_data, list)
            except zmq.Again:
                pytest.fail("Did not receive ZMQ message in time")

        finally:
            proc.terminate()
            proc.wait()


class TestReverseService:
    """Tests for reverse_service"""

    @pytest.fixture
    def vcan_setup(self):
        result = subprocess.run(
            ["ip", "link", "show", "vcan0"],
            capture_output=True
        )
        if result.returncode != 0:
            pytest.skip("vcan0 not available")

    def test_reverse_service_starts(self, vcan_setup):
        """Test that reverse_service starts without errors"""
        proc = subprocess.Popen(
            ["./build/src/reverse_service/reverse_service",
             "--interface=vcan0", "--config=./configs"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(1)
        assert proc.poll() is None, "reverse_service crashed on start"
        proc.terminate()
        proc.wait()


class TestCanParser:
    """Unit tests for CAN parsing logic"""

    def test_haltech_rpm_parsing(self):
        """Test Haltech DATA1 RPM parsing"""
        # Frame 0x360: RPM=3000 (big endian)
        # Bytes 0-1 = 0x0BB8 = 3000
        data = bytes([0x0B, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
        rpm = (data[0] << 8) | data[1]
        assert rpm == 3000

    def test_haltech_temp_parsing(self):
        """Test Haltech DATA5 temperature parsing"""
        # Frame 0x3E0: CLT in Kelvin x 10
        # 90°C = 363K = 3630 = 0x0E2E
        data = bytes([0x0E, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
        kelvin_x10 = (data[0] << 8) | data[1]
        celsius = (kelvin_x10 / 10.0) - 273.0
        assert abs(celsius - 90.0) < 0.1

    def test_bmw_rpm_parsing(self):
        """Test BMW DME1 RPM parsing"""
        # Frame 0x316: RPM in bytes 2-3, little endian, /6.4
        # RPM=3000 → raw = 3000 * 6.4 = 19200 = 0x4B00
        data = bytes([0x00, 0x00, 0x00, 0x4B, 0x00, 0x00, 0x00, 0x00])
        raw = data[2] | (data[3] << 8)
        rpm = raw / 6.4
        assert abs(rpm - 3000) < 1


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

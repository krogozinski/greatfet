#
# This file is part of GreatFET
#

from ...interface import SerialInterface
from ..i2c_device import I2CDevice

class I2DeviceChannel(SerialInterface):
    """
        Class representing an I2C device channel connected to a GreatFET I2C Bus.

        This acts as a concrete implementation of the SerialInterface for an I2C device.
    """

    def __init__(self, device: I2CDevice):
        """
            Initialize a new I2C device channel implementing a serial interface.

            Args:
                device -- An I2C device.
        """
        self._i2c_device = device

    # @override
    def read(self, receive_length: int=0):
        self._i2c_device.read(self, receive_length)

    # @override
    def write(self, data: list[int]):
        self._i2c_device.write(self, data)

    # @override
    def transmit(self, data: list[int], receive_length: int=0, count: int=1):
        self._i2c_device.transmit(data, receive_length, count)

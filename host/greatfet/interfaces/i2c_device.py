#
# This file is part of GreatFET
#

from typing import List
from ..interface import (
    GreatFETInterface,
    SerialInterface,
)
from .i2c_bus import I2CBus

class I2CDevice(GreatFETInterface):
    """
        Class representing an generic I2C device connected to a GreatFET I2C Bus.

        This acts both as the base class for I2C devices, and as a generic class
        that can be used to access I2C devices for which no existing driver exists.
    """

    def __init__(self, bus, address, name='i2c device'):
        """
            Initialize a new generic I2C device.

            Args:
                bus -- An object representing the I2C bus on which this device
                    resides.
                address - The address for the given I2C device on the bus.
                name -- The display name for the given I2C device.
        """

        # Note: this will have to change if we decide to support 10-bit I2C addresses.
        if address > 127 or address < 0:
            raise ValueError("Tried to attach a device to an unsupported I2C address.")

        # Store our device parameters.
        self.bus = bus
        self.address = address
        self.name = name

        # Attach our device to the parent bus.
        self.bus.attach_device(self)


    def transmit(self, data, receive_length, count=1):
        """
            Sends data over the I2C bus, and receives
            data in response.

            Args:
                data -- The data to be sent to the given device.
                receive_length -- If provided, the I2C controller will attempt
                        to read the provided amount of data, in bytes.
                count -- number of times to repeat the
                        write/read transmission
        """
        return self.bus.transmit(self.address, data, receive_length, count)
    

    def read(self, receive_length=0):
        """
            Reads data from the I2C bus.

            Args:
                receive_length -- The I2C controller will attempt
                        to read the provided amount of data, in bytes.
        """
        return self.bus.read(self.address, receive_length)


    def write(self, data):
        """
            Sends data over the I2C bus.

            Args:
                data -- The data to be sent to the given device.
        """
        return self.bus.write(self.address, data)


class I2DeviceChannel(SerialInterface):
    """
        Class representing an I2C device channel connected to a GreatFET I2C Bus.

        This acts as a concrete implementation of the SerialInterface for an I2C device.
    """

    def __init__(self, bus: I2CBus, address: int, name: str='i2c device'):
        """
            Initialize a new I2C device channel implementing a serial interface.

            Args:
                bus -- An object representing the I2C bus on which this device
                    resides.
                address - The address for the given I2C device on the bus.
                name -- The display name for the given I2C device.
        """
        self._i2c_device = I2CDevice(bus, address, name)

    # @override
    def read(self, receive_length: int=0):
        self._i2c_device.read(self, receive_length)

    # @override
    def write(self, data: List[int]):
        self._i2c_device.write(self, data)

    # @override
    def transmit(self, data: List[int], receive_length: int=0, count: int=1):
        self._i2c_device.transmit(data, receive_length, count)

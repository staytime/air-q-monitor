#!/usr/bin/env python3



import logging
#from systemd.journal import JournalHandler
from prometheus_client import Gauge, start_http_server
from datetime import datetime
import time
from bluepy.btle import Scanner, DefaultDelegate
from bluepy import btle
import argparse



READ_INTERVAL = 10



# Setup logging to the Systemd Journal
log = logging.getLogger('airq sensor')
#log.addHandler(JournalHandler())
log.setLevel(logging.INFO)



# define metrices
metrices_humidity = Gauge('humidity_percent', \
                          'Humidity percentage measured by module')

metrices_temperature = Gauge('temperature', \
                             'Temperature measured by module', ['scale'])

metrices_temperature.labels('celsius')

metrices_co2_level = Gauge('co2_level', \
                           'co2 level measured by module', ['scale'])

metrices_co2_level.labels('ppm')

metrices_tvoc_level = Gauge('tvoc_level', \
                            'co2 level measured by module', ['scale'])

metrices_tvoc_level.labels('ppb')

metrices_ccs811_baseline = Gauge('ccs811_baseline', \
                                 'ccs811 internal adjustment number')

class ENVLETInstrumentState:
    @staticmethod
    def from_fixpoint16f4(data):
        return float(int.from_bytes(data[0:2], byteorder = 'little', signed = True)) / 16

    @staticmethod
    def from_fixpoint32f4(data):
        return float(int.from_bytes(data[0:4], byteorder = 'little', signed = True)) / 16

    @staticmethod
    def from_uint16(data):
        return int.from_bytes(data[0:2], byteorder = 'little', signed = False)

    def __init__(self):
        self.temperature = 25
        self.humidity = 50
        self.pressure = 10000

        self.co2 = 400
        self.tvoc = 0
        self.baseline = 40000

    def deserialize(self, data):
        assert len(data) == 14, "data is incorrect"
        self.temperature = self.__class__.from_fixpoint16f4(data[0:2])
        self.humidity = self.__class__.from_fixpoint16f4(data[2:4])
        self.pressure = self.__class__.from_fixpoint32f4(data[4:8])
        self.co2 = self.__class__.from_uint16(data[8:10])
        self.tvoc = self.__class__.from_uint16(data[10:12])
        self.baseline = self.__class__.from_uint16(data[12:14])

    def __repr__(self):
        return ("InstrumentState("
                f"temperature = {self.temperature:.1f}, "
                f"humidity = {self.humidity:.0f}, "
                f"pressure = {self.pressure:.2f}, "
                f"co2 = {self.co2:d}, "
                f"tvoc = {self.tvoc:d}, "
                f"baseline = {self.baseline:d}"
                ")")


def reading_sensor(dev_mac):
    global metrices_co2_level
    global metrices_tvoc_level
    global metrices_temperature
    global metrices_humidity
    global metrices_ccs811_baseline

    for retry in range(3):
        isd = None
        try:
            p = btle.Peripheral(dev_mac)
            data = p.readCharacteristic(72)
            isd = ENVLETInstrumentState()
            isd.deserialize(data)

        except Exception as e:
            log.error(e, exc_info = True)
            time.sleep(READ_INTERVAL / 2)

        else:
            timestamp = datetime.now()
            __ = 'sensor reading successful @{}, co2: {}, baseline: {}, T: {:.1f}°C, H: {:.0f}%'
            log.info(__.format(timestamp.strftime("%Y/%m/%d %H:%M:%S"), \
                               isd.co2, \
                               isd.baseline, \
                               isd.temperature, \
                               isd.humidity))

            metrices_co2_level.labels('ppm').set(isd.co2)
            metrices_tvoc_level.labels('ppb').set(isd.tvoc)
            metrices_temperature.labels('celsius').set(isd.temperature)
            metrices_humidity.set(isd.humidity)
            metrices_ccs811_baseline.set(isd.baseline)

            time.sleep(READ_INTERVAL)
            break

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = 'Retrieve read from sensor')
    parser.add_argument('--target_mac', '-t', \
                        type = str, \
                        required = True, \
                        help = 'Sensor bluetooth mac address')

    parser.add_argument('--host_port', '-p', \
                        type = int, \
                        default = 8000, \
                        help = 'Sensor bluetooth mac address')

    parameter = parser.parse_args()

    start_http_server(parameter.host_port)
    log.info("Serving metrices on :{}".format(parameter.host_port))
    while True:
        reading_sensor(parameter.target_mac)

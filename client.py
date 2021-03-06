#!/usr/bin/python
# -*- coding: utf-8 -*-
""" Trurido client

  TsuridoMonitor: Sensor monitoring program for surf fishing

TODO:
    require modules: numpy, matplotlib, pydub, simpleaudio,
                     Adafruit_bluefruitLE

Copyright 2020 Hideto Manjo
Licence: LGPL
"""


import time
import uuid
import csv
import argparse
from datetime import datetime
import numpy as np

from matplotlib import pyplot as plt
from matplotlib import _pylab_helpers

from pydub import AudioSegment
import simpleaudio

import Adafruit_BluefruitLE

plt.style.use('dark_background')
plt.rcParams['toolbar'] = 'None'

SERVICE_UUID = uuid.UUID("8da64251-bc69-4312-9c78-cbfc45cd56ff")
CHAR_UUID    = uuid.UUID("deb894ea-987c-4339-ab49-2393bcc6ad26")


class SoundPlayer:
    """SoundPlayer module."""
    @classmethod
    def play(cls, filename, audio_format="mp3", wait=False, stop=False):
        """Play audio file."""

        if stop:
            simpleaudio.stop_all()

        seg = AudioSegment.from_file(filename, audio_format)
        playback = simpleaudio.play_buffer(
            seg.raw_data,
            num_channels=seg.channels,
            bytes_per_sample=seg.sample_width,
            sample_rate=seg.frame_rate
        )

        if wait:
            playback.wait_done()


class SerialParser:
    """Serial Parser"""
    def __init__(self, data):
        self.labels, self.values = self._parse(data)

    def __getitem__(self, items):
        if isinstance(items, (tuple, list)):
            return [self.values[self.labels.index(item)] for item in items]

        if isinstance(items, str):
            return self.values[self.labels.index(items)]

        if isinstance(items, (int, slice)):
            return self.values[items]

        raise NotImplementedError()

    @classmethod
    def _parse(cls, data):
        labels, values, = str(data).split(":")
        labels = [label.strip() for label in labels.split(',')]
        values = [value.strip() for value in values.split(',')]

        # Make the lists of labels and values the same length, fill in the blanks
        if len(labels) > len(values):
            labels = labels[0:len(values)]
        else:
            labels += (len(values) - len(labels)) * [""]

        return labels, values


class SerialLogger:
    def __init__(self, labels, write_interval=20):
        self._logger_buff = [["count", "unixtime"] + labels]
        self._logger_uid = datetime.now().strftime('%Y%m%d_%H%M%S')
        self._write_interval = write_interval
        self._count = 0

    def write(self, data):
        self._count += 1
        self._logger_buff.append([self._count, time.time()] + data)

        if len(self._logger_buff) > self._write_interval:
            with open(f'{self._logger_uid}.csv', 'a') as f:
                writer = csv.writer(f)
                writer.writerows(self._logger_buff)
                self._logger_buff.clear()


class Plotter:
    """Tsurido Plotter module"""
    def __init__(self, interval=1, width=200, pause=0.001, sigma=(5, 10),
                 angle=True, xlabel=False, ylabel=True, logging=False,
                 title="Tsurido Plotter"):
        # define
        self.__indexes = ("Ax", "Ay", "Az", "A")
        self.__target_index = 3 # detection target

        # main plot config
        self._interval = interval
        self._pause = pause
        self._width = width
        self._angle = angle
        self.sigma = sigma   # [warning, overrange]
        self._logging = logging

        self.count = 0
        self.closed = False # plotter end flag
        self._last_ring = time.time() + 3

        self.t = np.zeros(self._width)
        self.values = np.zeros((4, self._width))

        # logger
        if logging:
            self._logger = SerialLogger(list(self.__indexes))

        # initial plot
        plt.ion()
        self.fig = plt.figure()
        self.fig.canvas.set_window_title(title)
        if self._angle:
            self.ax1 = self.fig.add_subplot(2, 1, 1)
        else:
            self.ax1 = self.fig.add_subplot(1, 1, 1)

        self.li, = self.ax1.plot(self.t, self.values[self.__target_index],
                                 label="Acc", color="c")
        self.li_sigma = self.ax1.axhline(y=0)

        if xlabel:
            plt.xlabel("count")

        if ylabel:
            self.ax1.set_ylabel(self.li.get_label())

        if angle:
            self.tip_angle = np.zeros(self._width)
            # self.ax2 = self.ax1.twinx()
            self.ax2 = self.fig.add_subplot(2, 1, 2)
            self.li2, = self.ax2.plot(self.t, self.tip_angle,
                                      label="Rod angle", color="r")
            self.ax2.set_ylabel(self.li2.get_label())

    @staticmethod
    def _get_angle(ax, ay, az):
        if ay != 0:
            theta = np.arctan(ax / ay) * 180 / np.pi
        else:
            if ax > 0:
                theta = 90
            elif ax < 0:
                theta = -90
            else:
                theta = 0

        scalor = np.sqrt(ax*ax + ay*ay + az*az)

        if scalor != 0:
            phi = np.arccos(az / scalor) * 180 / np.pi
        else:
            phi = 0

        return theta, phi

    def _store_values(self, values):
        self.t[0:-1] = self.t[1:]
        self.t[-1] = float(self.count)
        self.count += 1

        self.values[:, 0:-1] = self.values[:, 1:]
        self.values[:, -1] = [float(v) for v in values]

    def _store_angle(self, values):
        angle = self._get_angle(*[float(v) for v in values[:3]])
        self.tip_angle[0:-1] = self.tip_angle[1:]
        self.tip_angle[-1] = np.abs(angle[0])

    def _check_warning(self, diff, std):
        if time.time() - self._last_ring > 2:
            if diff[-1] > self.sigma[1] * std:
                # over range
                self._last_ring = time.time()
                SoundPlayer.play("sfx/warning2.mp3")
            elif diff[-1] > self.sigma[0] * std:
                # warning
                self._last_ring = time.time()
                SoundPlayer.play("sfx/warning1.mp3")

    def _plot(self, y, std):
        self.li.set_xdata(self.t)
        self.li.set_ydata(y)

        self.li_sigma.set_ydata([self.sigma[0] * std, self.sigma[0] * std])

        self.ax1.set_ylim(0, self.sigma[1] * std)
        if self.count != 1:
            self.ax1.set_xlim(np.min(self.t), np.max(self.t))

        if self._angle:
            self.li2.set_xdata(self.t)
            self.li2.set_ydata(self.tip_angle)
            self.ax2.set_xlim(np.min(self.t), np.max(self.t))
            self.ax2.set_ylim(self.tip_angle.mean() - 10,
                              self.tip_angle.mean() + 10)

        plt.gcf().canvas.draw_idle()
        plt.gcf().canvas.start_event_loop(self._pause)

    def received(self, data):
        """Recieved Event
            Event to be performed when data is received
        Args:
            data (str): bluetooth received data

        Returns:
            Always None
        """
        if _pylab_helpers.Gcf.get_active() is None:
            # end flag
            self.closed = True
            return

        values = SerialParser(data)[self.__indexes]

        # Store value as new values
        self._store_values(values)

        # Calculate and store rod tip angle
        if self._angle:
            self._store_angle(values)

        # Calculate standard deviation
        y = self.values[self.__target_index]
        std = y.std()

        # Calculate deviation from average value
        diff = np.abs(y - y.mean())

        # Sounds a warning when the deviation exceeds a specified value
        self._check_warning(diff, std)

        # Since plotting is slow, accurate graphs can be drawn by
        # thinning out the display.
        if self.count % self._interval == 0:
            self._plot(diff, std)

        if self._logging:
            self._logger.write(values)


def main():
    BLE.clear_cached_data()

    adapter = BLE.get_default_adapter()
    adapter.power_on()
    print('Using adapter: {0}'.format(adapter.name))

    if ARGS.reset:
        print('Disconnecting any connected devices...')
        BLE.disconnect_devices([SERVICE_UUID])

    devices = BLE.list_devices()
    if devices:
        print("Current connections:")
    else:
        print("No current connetion.")

    for i, dev in enumerate(devices, start=1):
        print(f" {i}.{dev.name}")
        if dev.name == ARGS.DEVICE_NAME:
            print(f"Disconnecting {dev.name}...")
            dev.disconnect()

    print('Searching device...')
    adapter.start_scan()

    try:
        device = BLE.find_device(name=ARGS.DEVICE_NAME, timeout_sec=5)
        if device is None:
            raise RuntimeError('Failed to find device!')
    finally:
        adapter.stop_scan()

    print('Connecting to device...')
    device.connect()

    print('Discovering services...')
    device.discover([SERVICE_UUID], [CHAR_UUID])

    uart = device.find_service(SERVICE_UUID)
    chara = uart.find_characteristic(CHAR_UUID)

    print('Subscribing to characteristic changes...')
    chara.start_notify(PLOTTER.received)

    # wait
    while not PLOTTER.closed:
        time.sleep(0.1)

    if ARGS.keepalive:
        print('Keep-alive connection...')
    else:
        print('Disconnecting to device...')
        device.disconnect()

    print("Stop.")
    return 0


def parser():
    """Paser"""

    def check_positive(value):
        ivalue = int(value)
        if ivalue <= 0:
            raise argparse.ArgumentTypeError("%s is an invalid positive int value" % value)
        return ivalue


    usage = f'Usage: python {__file__} DEVICE_NAME [OPTIONS] [--keepalive] [--reset][--help]'
    formatter_class = argparse.ArgumentDefaultsHelpFormatter
    argparser = argparse.ArgumentParser(usage=usage,
                                        formatter_class=formatter_class)
    argparser.add_argument('DEVICE_NAME',
                           action='store',
                           nargs='?',
                           default="Tsurido",
                           type=str,
                           help='Enter the device name')

    argparser.add_argument('-i', '--interval',
                           type=check_positive,
                           default=4,
                           help='Plot interval Ex: 1 = realtime, 2 = only even times')

    argparser.add_argument('-w', '--width',
                           type=check_positive,
                           default=200,
                           help='Plot width')

    argparser.add_argument('-s', '--sigma',
                           type=check_positive,
                           default=[5, 10],
                           nargs=2,
                           metavar=('warn', 'over range'),
                           help='Specify warn/OR range')

    argparser.add_argument('-na', '--noangle',
                           action='store_false',
                           help='Disable rod angle plot')

    argparser.add_argument('--logging',
                           action='store_true',
                           help='Enable logging')

    argparser.add_argument('--keepalive',
                           action='store_true',
                           help='No disconnect')

    argparser.add_argument('--reset',
                           action='store_true',
                           help='Reset current connection')

    return argparser.parse_args()


if __name__ == '__main__':
    ARGS = parser()
    BLE = Adafruit_BluefruitLE.get_provider()
    PLOTTER = Plotter(interval=ARGS.interval,
                      width=ARGS.width,
                      angle=ARGS.noangle,
                      logging=ARGS.logging,
                      sigma=ARGS.sigma if ARGS.sigma[1] > ARGS.sigma[0] else [5, 10],
                      title=f"Tsurido Plotter - {ARGS.DEVICE_NAME}")
    BLE.initialize()
    BLE.run_mainloop_with(main)

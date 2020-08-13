""" Trurido client

Copyright 2020 Hideto Manjo.
Licence: LGPL
"""

import time
import uuid
import numpy as np

from matplotlib import pyplot as plt
from matplotlib import _pylab_helpers

from pydub import AudioSegment
import simpleaudio

import Adafruit_BluefruitLE

plt.style.use('dark_background')
plt.rcParams['toolbar'] = 'None' 

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


class Plotter:
    def __init__(self, target_index=3, interval=1, width=400, pause=0.005,
                 sigma=(5, 7), xlabel=None, ylabel=None):
        # field length
        self.__fieldlength = 4

        # main plot config
        self._target_index = target_index
        self._interval = interval
        self._pause = pause
        self._width = width
        self.sigma = sigma   # [warning, overrange]

        self.count = 0
        self.closed = False # plotter end flag
        self.last_ring = time.time() + 3

        self.t = np.zeros(self._width)
        self.values = np.zeros((self.__fieldlength, self._width))

        # initial plot
        plt.ion()
        self.fig = plt.figure()
        self.fig.canvas.set_window_title('Acc')
        self.li, = plt.plot(self.t, self.values[self._target_index], label="Sensor")
        self.li_sigma = plt.axhline(y=0)

        if xlabel is not None:
            plt.xlabel(xlabel)
        if ylabel is not None:
            plt.ylabel(ylabel)

    @staticmethod
    def _parse(data):
        labels, values, = str(data).split(":")
        labels = [label.strip() for label in labels.split(',')]
        values = [value.strip() for value in values.split(',')]

        # Make the lists of labels and values the same length, fill in the blanks
        if len(labels) > len(values):
            labels = labels[0:len(values)]
        else:
            labels += (len(values) - len(labels)) * [""]

        return labels, values

    @staticmethod
    def angle(ax, ay, az):
        if ay != 0:
            theta = np.arctan(ax / ay) * 180 / np.pi
        else:
            if ax > 0:
                theta = 90
            elif ax < 0:
                theta = -90
            else:
                theta = 0

        phi = np.arccos(az / np.sqrt(ax*ax + ay*ay + az*az)) * 180 / np.pi

        return theta, phi

    def _store_values(self, values):
        self.t[0:-1] = self.t[1:]
        self.t[-1] = float(self.count)
        self.count += 1

        self.values[:, 0:-1] = self.values[:, 1:]
        self.values[:, -1] = [float(v) for v in values]

    def _check_warning(self, diff, std):
        if time.time() - self.last_ring > 2:
            if diff[-1] > self.sigma[1] * std:
                # over range
                self.last_ring = time.time()
                SoundPlayer.play("sfx/warning2.mp3")
            elif diff[-1] > self.sigma[0] * std:
                # warning
                self.last_ring = time.time()
                SoundPlayer.play("sfx/warning1.mp3")

    def _plot(self, y, std):
        t = self.t
        self.li.set_xdata(t)
        self.li.set_ydata(y)

        self.li_sigma.set_ydata([self.sigma[0] * std, self.sigma[0] * std])

        plt.ylim(0, self.sigma[1] * std)
        if self.count != 1:
            plt.xlim(np.min(t), np.max(t))

        plt.gcf().canvas.draw_idle()
        plt.gcf().canvas.start_event_loop(self._pause)


    def received(self, data):
        if _pylab_helpers.Gcf.get_active() is None:
            # end flag
            self.closed = True
            return

        # Parse
        _labels, values = self._parse(data)

        # Store value as new values
        self._store_values(values)

        # Calculate standard deviation
        y = self.values[self._target_index]
        std = y.std()

        angle = self.angle(self.values[0, -10:].mean(),
                           self.values[1, -10:].mean(),
                           self.values[2, -10:].mean())
        print(angle)

        # Calculate deviation from average value
        diff = np.abs(y - y.mean())

        # Sounds a warning when the deviation exceeds a specified value
        self._check_warning(diff, std)

        # Since plotting is slow, accurate graphs can be drawn by
        # thinning out the display.
        if self.count % self._interval == 0:
            self._plot(diff, std)

        # print('{0}'.format(data))


SERVICE_UUID = uuid.UUID("8da64251-bc69-4312-9c78-cbfc45cd56ff")
CHAR_UUID    = uuid.UUID("deb894ea-987c-4339-ab49-2393bcc6ad26")

BLE = Adafruit_BluefruitLE.get_provider()
PLOTTER = Plotter(interval=1)

def main():
    BLE.clear_cached_data()

    adapter = BLE.get_default_adapter()
    adapter.power_on()
    print('Using adapter: {0}'.format(adapter.name))

    print('Disconnecting any connected devices...')
    BLE.disconnect_devices([SERVICE_UUID])

    print('Searching device...')
    adapter.start_scan()

    try:
        device = BLE.find_device(name="Tsurido", timeout_sec=5)
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

    print('Disconnecting to device...')
    device.disconnect()

    print("Stop.")
    return 0


BLE.initialize()
BLE.run_mainloop_with(main)
# -*- coding: utf-8 -*-
import os
import sys
import syio as sy
from syio import InputWaitResult

import cv2 as cv
from dlclive import DLCLive, Processor


class DLCLiveModule:
    ''' DeepLabCut Live Syntalos Module '''

    def __init__(self):
        self._iport = None
        self._oport_img = None
        self._oport_rows = None

    def prepare(self):
        # Get port references
        self._iport = sy.get_input_port('frames-in')
        self._oport_img = sy.get_output_port('frames-out')
        self._oport_rows = sy.get_output_port('rows-out')

    def start(self):
        pass

    def loop(self) -> bool:
        # wait for new input to arrive
        wait_result = sy.await_new_input()
        if wait_result == InputWaitResult.CANCELLED:
            return False

        # retrieve data from our port until we run out of data to process
        while True:
            frame = self._iport.next()
            if frame is None:
                # no more data, exit
                break

            mat = frame.mat

            # TODO

            # submit new data to an output port
            frame.mat = mat
            self._oport_img.submit(frame)

        # we don't want to quite data processing, so return True
        return True

    def stop(self):
        pass

    def change_settings(self, old_settings: bytes) -> bytes:
        pass

    def set_settings(self, data: bytes):
        pass


# create our module wrapper object
mod = DLCLiveModule()


def set_settings(settings):
    mod.set_settings(settings)


def prepare():
    mod.prepare()


def start():
    mod.start()


def loop() -> bool:
    return mod.loop()


def stop():
    mod.stop()


def change_settings(old_settings: bytes) -> bytes:
    return mod.change_settings(old_settings)

# -*- coding: utf-8 -*-
'''
Example Standalone Python Module For Syntalos

This demonstrates how a generic Syntalos Python module works.
If you just want to run some custom Python code quickly,
you can also use the built-in PyScript module, and not
go through the more involved process of creating a new
module from scratch. If you want to do that though, this
example is a good starting point.
'''
import os
import sys
import syio as sy
from syio import InputWaitResult

import tkinter as tk
import json
import cv2 as cv


class MyExampleModule:
    '''
    Class wrapper to keep all variables this module uses together.
    You may use just the raw functions below, in case you don't
    want to use objects.
    '''

    def __init__(self):
        self._iport = None
        self._oport_frames = None
        self._oport_rows = None
        self._label_prefix = {}
        self._frame_count = 0

    def prepare(self):
        '''
        This function is called before a run is started.
        You can use it for (slow) initializations.
        NOTE: You are *not* able to send output to ports here, or access
        any valid master timer time. This function can be slow.
        '''

        # Get references to your ports by their ID here.
        self._iport = sy.get_input_port('frames-in')
        self._oport_frames = sy.get_output_port('frames-out')
        self._oport_rows = sy.get_output_port('rows-out')
        self._oport_rows.set_metadata_value('table_header', ['Frame Counted'])

    def start(self):
        '''
        This function is called immediately when a run is started.
        Access to the timer is available, and data can be sent via ports.
        You can *not* change any port metadata anymore from this point onward.
        This function should be fast, many modules are already running at this point.
        '''
        pass

    def loop(self) -> bool:
        '''
        This function is executed by Syntalos continuously until it returns False.
        Use this function to retrieve input and process it, or run any other
        repeatable action. Keep in mind that you will not receive any new input
        unless `sy.await_new_input()` is called.
        '''

        # wait for new input to arrive
        wait_result = sy.await_new_input()
        if wait_result == InputWaitResult.CANCELLED:
            # the run has been cancelled (by the user or an error),
            # so this function will not be called again
            return False

        # retrieve data from our ports until we run out of data to process
        while True:
            frame = self._iport.next()
            if frame is None:
                # no more data, exit
                break

            mat = frame.mat
            position = (46, 46)
            cv.putText(
                mat,
                '{}: {}'.format(self._label_prefix, self._frame_count),
                position,
                cv.FONT_HERSHEY_SIMPLEX,
                1.2,
                (0, 116, 246),
                2,
                cv.LINE_AA,
            )
            self._frame_count += 1

            # submit new data to an output port
            frame.mat = mat
            self._oport_frames.submit(frame)
            self._oport_rows.submit([self._frame_count])

        # return True, so the loop function is called again when new data is available
        return True

    def stop(self):
        '''
        This function is called once a run is stopped, by the user, and error or when
        the loop() function returned False.
        '''
        pass

    def change_settings(self, old_settings: bytes) -> bytes:
        '''
        Show (horrible) GUI to change settings here.
        Settings objects are random bytes which modules
        can use in whichever way they want.
        '''
        settings = {}
        if old_settings:
            settings = json.loads(str(old_settings, 'utf-8'))

        window = tk.Tk()
        window.title('Example Python Module')

        lbl = tk.Label(window, text='Set prefix text:')
        lbl.grid(column=0, row=0)
        txt = tk.Entry(window, width=10)
        txt.grid(column=1, row=0)
        txt.insert(0, settings.get('prefix', ''))

        def clicked():
            window.quit()

        def process_messages():
            if sy.check_running():
                window.after(1, process_messages)
            else:
                window.quit()

        btn = tk.Button(window, text='Okay', command=clicked)
        btn.grid(column=1, row=2)

        # center the window on screen
        window.geometry('350x100')
        positionRight = int(window.winfo_screenwidth() / 2 - window.winfo_reqwidth() / 2)
        positionDown = int(window.winfo_screenheight() / 2 - window.winfo_reqwidth() / 2)
        window.geometry('+{}+{}'.format(positionRight, positionDown))

        # run the mainloop, but also listen if we should quit
        window.after(1, process_messages)
        window.mainloop()

        self._label_prefix = txt.get()
        settings = dict(prefix=self._label_prefix)

        window.destroy()
        return bytes(json.dumps(settings), 'utf-8')

    def set_settings(self, data: bytes):
        '''
        Deserialize settings from bytes. Called right before a new run is started.
        '''
        settings = {}
        if data:
            settings = json.loads(str(data, 'utf-8'))
        self._label_prefix = settings.get('prefix', '')


# create our module wrapper object
mod = MyExampleModule()


def set_settings(settings):
    '''
    Called with the module-defined settings data right before a new run is prepared.
    '''
    mod.set_settings(settings)


def prepare():
    '''This function is called before a run is started.'''
    mod.prepare()


def start():
    '''This function is called immediately when a run is started.'''
    mod.start()


def loop() -> bool:
    '''
    This function is executed by Syntalos continuously until it returns False.
    Use this function to retrieve input and process it, or run any other
    repeatable action.
    '''
    return mod.loop()


def stop():
    '''
    This function is called once a run is stopped, by the user, and error or when
    the loop() function returned False.
    '''
    mod.stop()


def change_settings(old_settings: bytes) -> bytes:
    '''
    Called by Syntalos to show a settings UI.
    Will never be active during a run! (unless
    shown by the module on its own)
    '''
    return mod.change_settings(old_settings)

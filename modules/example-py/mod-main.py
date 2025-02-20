# -*- coding: utf-8 -*-
"""
Example Standalone Python Module For Syntalos

This demonstrates how a generic Syntalos Python module works.
If you just want to run some custom Python code quickly,
you can also use the built-in PyScript module, and not
go through the more involved process of creating a new
module from scratch. If you want to do that though, this
example is a good starting point.
"""
import os
import sys
import syntalos_mlink as syl

from PyQt6.QtWidgets import QDialog, QLabel, QLineEdit, QPushButton, QVBoxLayout, QHBoxLayout
import json
import cv2 as cv


class MyExampleModule:
    """
    Class wrapper to keep all variables this module uses together.
    You may use just the raw functions below, in case you don't
    want to use objects.
    """

    def __init__(self):
        self._iport = None
        self._oport_frames = None
        self._oport_rows = None
        self._label_prefix = {}
        self._frame_count = 0
        self._settings_dlg = None

        # show the settings dialog when the user requested it to be shown
        syl.call_on_show_settings(self._change_settings)

    def prepare(self) -> bool:
        """
        This function is called before a run is started.
        You can use it for (slow) initializations.
        NOTE: You are *not* able to send output to ports here, or access
        any valid master timer time. This function can be slow.
        """

        # Get references to your ports by their ID here.
        self._iport = syl.get_input_port('frames-in')
        self._oport_frames = syl.get_output_port('frames-out')
        self._oport_rows = syl.get_output_port('rows-out')
        self._oport_rows.set_metadata_value('table_header', ['Frame Counted'])

        # forward some metadata
        self._oport_frames.set_metadata_value('framerate', self._iport.metadata.get('framerate', 0))
        frame_size = self._iport.metadata.get('size', None)
        if frame_size:
            self._oport_frames.set_metadata_value_size('size', frame_size)

        # call self._on_input_data() once we have new data on this port
        self._iport.on_data = self._on_input_data

        return True

    def start(self):
        """
        This function is called immediately when a run is started.
        Access to the timer is available, and data can be sent via ports.
        You can *not* change any port metadata anymore from this point onward.
        This function should be fast, many modules are already running at this point.
        """
        pass

    def _on_input_data(self, frame):
        if frame is None:
            # no more data, exit
            return

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

    def stop(self):
        """
        This function is called once a run is stopped, by the user, and error or when
        the loop() function returned False.
        """
        pass

    def _change_settings(self, old_settings: bytes):
        """
        Show (horrible) GUI to change settings here.
        Settings objects are random bytes which modules
        can use in whichever way they want.
        """
        # don't open the dialog more than once
        if self._settings_dlg:
            return

        settings = {}
        if old_settings:
            settings = json.loads(str(old_settings, 'utf-8'))

        # Create a dialog window
        self._settings_dlg = QDialog()
        self._settings_dlg.setWindowTitle('Example Python Module')
        layout = QVBoxLayout(self._settings_dlg)

        lbl = QLabel('Set prefix text:')
        layout.addWidget(lbl)

        txt = QLineEdit()
        txt.setText(settings.get('prefix', ''))
        layout.addWidget(txt)

        def clicked():
            self._label_prefix = txt.text()
            settings = dict(prefix=self._label_prefix)
            syl.save_settings(bytes(json.dumps(settings), 'utf-8'))

            self._settings_dlg.accept()

        def on_dialog_closed():
            self._settings_dlg = None

        # cleanup when the dialog is closed
        self._settings_dlg.finished.connect(on_dialog_closed)

        btn = QPushButton('Okay')
        btn.clicked.connect(clicked)
        layout.addWidget(btn)

        self._settings_dlg.setLayout(layout)

        # Center the dialog on screen
        self._settings_dlg.setGeometry(0, 0, 350, 100)
        screen = self._settings_dlg.screen().availableGeometry()
        positionRight = int((screen.width() - self._settings_dlg.width()) / 2)
        positionDown = int((screen.height() - self._settings_dlg.height()) / 2)
        self._settings_dlg.move(positionRight, positionDown)

        self._settings_dlg.show()

    def set_settings(self, data: bytes):
        """
        Deserialize settings from bytes. Called right before a new run is prepared.
        """
        settings = {}
        if data:
            settings = json.loads(str(data, 'utf-8'))
        self._label_prefix = settings.get('prefix', '')


# create our module wrapper object
mod = MyExampleModule()


def set_settings(settings: bytes):
    mod.set_settings(settings)


def prepare() -> bool:
    """This function is called before a run is started."""
    return mod.prepare()


def start():
    """This function is called immediately when a run is started."""
    mod.start()


def run():
    # wait for new data to arrive and communicate with Syntalos
    while syl.is_running():
        syl.await_data()


def stop():
    """
    This function is called once a run is stopped, by the user, and error or when
    the loop() function returned False.
    """
    mod.stop()

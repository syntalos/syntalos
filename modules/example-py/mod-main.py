# -*- coding: utf-8 -*-
#
# Copyright 2022-2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: MIT

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

from PyQt6.QtWidgets import (
    QApplication,
    QDialog,
    QLabel,
    QLineEdit,
    QPushButton,
    QVBoxLayout,
    QHBoxLayout,
)
import json
import cv2 as cv


class MyExampleModule:
    """
    Class wrapper to keep all variables this module uses together.
    You may use just the raw functions below, in case you don't
    want to use objects.
    """

    def __init__(self, modLink):
        self._modLink = modLink
        self._label_prefix = ''
        self._frame_count = 0
        self._settings_dlg = None

        # register ports that this module supports
        self._iport = self._modLink.register_input_port('frames-in', 'Frames', syl.DataType.Frame)
        self._oport_rows = self._modLink.register_output_port(
            'rows-out', 'Indices', syl.DataType.TableRow
        )
        self._oport_frames = self._modLink.register_output_port(
            'frames-out', 'Marked Frames', syl.DataType.Frame
        )

        # call self._on_input_data() once we have new data on this port
        self._iport.on_data = self._on_input_data

        # show the settings dialog when the user requested it to be shown
        self._modLink.on_show_settings = self._show_settings_dialog

        # save / load settings
        self._modLink.on_save_settings = self._save_settings_data
        self._modLink.on_load_settings = self._load_settings_data

    def prepare(self) -> bool:
        """
        This function is called before a run is started.
        You can use it for (slow) initializations.
        NOTE: You are *not* able to send output to ports here or access
        any valid master-timer time. This function is allowed to be slow.
        """

        # set our own port metadata
        self._oport_rows.set_metadata_value('table_header', ['Time Received [us]', 'Frame Number'])

        # forward some metadata
        self._oport_frames.set_metadata_value('framerate', self._iport.metadata.get('framerate', 0))
        frame_size = self._iport.metadata.get('size', None)
        if frame_size:
            self._oport_frames.set_metadata_value_size('size', frame_size)

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

        # submit new data to our output ports
        frame.mat = mat
        self._oport_frames.submit(frame)

        if self._frame_count % 100 == 0:
            self._oport_rows.submit([syl.time_since_start_usec(), self._frame_count])

    def stop(self):
        """
        This function is called once a run is stopped, by the user, and error or when
        the loop() function returned False.
        """
        pass

    def _show_settings_dialog(self):
        """
        Show (horrible) GUI to change settings here.
        Settings objects are random bytes which modules
        can use in whichever way they want.
        """
        # don't open the dialog more than once
        if self._settings_dlg:
            return

        # Create a dialog window
        self._settings_dlg = QDialog()
        self._settings_dlg.setWindowTitle('Example Python Module')
        layout = QVBoxLayout(self._settings_dlg)

        lbl = QLabel('Set prefix text:')
        layout.addWidget(lbl)

        txt = QLineEdit()
        txt.setText(self._label_prefix)
        layout.addWidget(txt)

        def clicked():
            self._label_prefix = txt.text()
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

    def _save_settings_data(self, baseDir: os.PathLike[str]) -> bytes:
        """
        Serialize settings to bytes so Syntalos can store them for us.
        """

        settings = dict(prefix=self._label_prefix)
        return bytes(json.dumps(settings), 'utf-8')

    def _load_settings_data(self, data: bytes, baseDir: os.PathLike[str]) -> bool:
        """
        Deserialize user settings from previously stored bytes.
        """
        settings = {}
        if data:
            settings = json.loads(str(data, 'utf-8'))
        self._label_prefix = settings.get('prefix', '')

        return True


def main() -> int:
    # Create Qt application, so we can use it for our GUI
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)

    # Initialize connection to Syntalos. The returned link object is the registry
    # for all lifecycle callbacks and owns the IPC channel.
    modLink = syl.init_link()

    # Create module instance (also registers the show-settings callback via link).
    mod = MyExampleModule(modLink)

    # Register lifecycle callbacks on the link object.
    modLink.on_prepare = mod.prepare
    modLink.on_start = mod.start
    modLink.on_stop = mod.stop

    # Signal initialization complete and run the event loop.
    # This call blocks until Syntalos requests a shutdown.
    modLink.await_data_forever(app.processEvents)

    return 0


if __name__ == '__main__':
    sys.exit(main())

# -*- coding: utf-8 -*-
import os
import sys
import json
import syio as sy
from syio import InputWaitResult

import cv2 as cv
from dlclive import DLCLive, Processor

from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import QWidget, QDialog, QPushButton, QVBoxLayout, QHBoxLayout, \
    QFormLayout, QCheckBox, QLabel, QDialogButtonBox, QFileDialog


class SettingsDialog(QDialog):

    def __init__(self):
        super(SettingsDialog, self).__init__()
        self._create_form_widget()

        self.setWindowTitle('DeepLabCutLive Settings')

        bbox = QDialogButtonBox(QDialogButtonBox.Ok)
        bbox.accepted.connect(self.accept)

        main_layout = QVBoxLayout()
        main_layout.addWidget(self._form_widget)
        main_layout.addWidget(bbox)
        self.setLayout(main_layout)

        self._model_path = None
        self._display_dlc = False

    def _create_form_widget(self):
        self._form_widget = QWidget()

        mp_widget = QWidget()
        mp_layout = QHBoxLayout()
        mp_widget.setLayout(mp_layout)

        path_btn = QPushButton(QIcon.fromTheme('folder-open'), '')
        path_btn.setFlat(True)
        path_btn.clicked.connect(self._select_model_path)
        self._path_label = QLabel('None')
        self._display_checkbox = QCheckBox()

        mp_layout.addWidget(path_btn)
        mp_layout.addWidget(self._path_label)
        mp_layout.setContentsMargins(0, 0, 0, 0)

        layout = QFormLayout()
        layout.addRow(QLabel("Model Path:"), mp_widget)
        layout.addRow(QLabel("Display:"), self._display_checkbox)
        self._form_widget.setLayout(layout)

    def _select_model_path(self):
        path = str(QFileDialog.getExistingDirectory(self, 'Select Model Directory'))
        if path:
            self.model_path = path

    @property
    def model_path(self) -> str:
        return self._model_path

    @model_path.setter
    def model_path(self, value: str):
        self._model_path = value
        self._path_label.setText(self._model_path)

    @property
    def display_dlc(self) -> bool:
        return self._display_checkbox.isChecked()

    @display_dlc.setter
    def display_dlc(self, value: bool):
        self._display_checkbox.setChecked(value)


class DLCLiveModule:
    ''' DeepLabCut Live Syntalos Module '''

    def __init__(self):
        self._iport = None
        self._oport_img = None
        self._oport_rows = None
        self._dlc_live = None
        self._model_path = None
        self._first_frame = True
        self._dlc_proc = Processor()

    def prepare(self):
        # Get port references
        self._iport = sy.get_input_port('frames-in')
        #self._oport_img = sy.get_output_port('frames-out')
        self._oport_rows = sy.get_output_port('rows-out')
        self._oport_rows.set_metadata_value('table_header', ['Time [ms]', 'Marker', 'X', 'Y', 'Likelihood'])
        self._first_frame = True
        self._dlc_live = DLCLive(self._model_path,
                                 processor=self._dlc_proc,
                                 display=self._display)

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

            if self._first_frame:
                self._first_frame = False
                self._dlc_live.init_inference(frame.mat)
                continue

            pose = self._dlc_live.get_pose(frame.mat)
            if pose is None:
                continue

            for i, p in enumerate(pose):
                self._oport_rows.submit([frame.time_msec, i] + p.tolist())

            # submit new data to an output port
            #self._oport_img.submit(frame)

        # we don't want to quite data processing, so return True
        return True

    def stop(self):
        pass

    def change_settings(self, old_settings: bytes) -> bytes:
        try:
            settings = json.loads(old_settings)
        except Exception as e:
            print('Error while reading old settings:', e)
            settings = {}

        dlg = SettingsDialog()
        dlg.model_path = settings.get('model_path', None)
        dlg.display_dlc = settings.get('display', False)
        dlg.exec_()

        settings['model_path'] = dlg.model_path
        settings['display'] = dlg.display_dlc
        return bytes(json.dumps(settings), 'utf-8')

    def set_settings(self, data: bytes):
        if not data:
            return

        settings = json.loads(data)
        self._model_path = settings.get('model_path', None)
        self._display = settings.get('display', False)


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

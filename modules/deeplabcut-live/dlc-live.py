# -*- coding: utf-8 -*-
import os
import sys
import json
import syntalos_mlink as syl

import cv2 as cv
from dlclive import DLCLive, Processor

from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (
    QWidget,
    QDialog,
    QPushButton,
    QVBoxLayout,
    QHBoxLayout,
    QFormLayout,
    QCheckBox,
    QLabel,
    QDialogButtonBox,
    QFileDialog,
)


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
    '''DeepLabCut Live Syntalos Module'''

    def __init__(self):
        self._iport = None
        self._oport_img = None
        self._oport_rows = None
        self._dlc_live = None
        self._model_path = None
        self._first_frame = True
        self._display = False
        self._dlc_proc = Processor()

        # Get port references
        self._iport = syl.get_input_port('frames-in')
        self._oport_rows = syl.get_output_port('rows-out')
        # self._oport_img = syl.get_output_port('frames-out')

        self._iport.on_data = self._on_input_data

        # show the settings dialog when the user requested it to be shown
        syl.call_on_show_settings(self.change_settings)

    def prepare(self) -> bool:
        self._oport_rows.set_metadata_value(
            'table_header', ['Time [µs]', 'Marker', 'X', 'Y', 'Likelihood']
        )

        if not self._model_path or not os.path.exists(self._model_path):
            syl.raise_error('Model path does not exist.')
            return False

        self._first_frame = True
        self._dlc_live = DLCLive(self._model_path, processor=self._dlc_proc, display=self._display)

        return True

    def start(self):
        pass

    def _on_input_data(self, frame):
        """We received a new frame to process."""
        if frame is None:
            return

        if self._first_frame:
            self._first_frame = False
            self._dlc_live.init_inference(frame.mat)
            return

        pose = self._dlc_live.get_pose(frame.mat)
        if pose is None:
            return

        for i, p in enumerate(pose):
            self._oport_rows.submit([frame.time_usec, i] + p.tolist())

        # self._oport_img.submit(frame)

    def stop(self):
        pass

    def change_settings(self, old_settings: bytes):
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

        syl.save_settings(bytes(json.dumps(settings), 'utf-8'))

    def set_settings(self, data: bytes):
        if not data:
            return

        settings = json.loads(data)
        self._model_path = settings.get('model_path', None)
        self._display = settings.get('display', False)


# create our module wrapper object
mod = DLCLiveModule()


def set_settings(settings: bytes):
    mod.set_settings(settings)


def prepare() -> bool:
    return mod.prepare()


def start():
    mod.start()


def run():
    # wait for new data to arrive and communicate with Syntalos
    while syl.is_running():
        syl.await_data()


def stop():
    mod.stop()

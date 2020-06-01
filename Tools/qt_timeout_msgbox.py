from PyQt5.QtCore import QTimer, Signal
from PyQt5.QtWidgets import QMessageBox


class TimerMessageBox(QMessageBox):

    close_callback = Signal()

    def __init__(self, parent=None, timeout=3):  # , icon=QMessageBox.Critical, title='timer', text="text", info="info"):
        super(TimerMessageBox, self).__init__(parent)
        self.time_to_wait = timeout
        # self.setIcon(icon)
        # self.setWindowTitle(title)
        # self.setText(text)
        # self.setInformativeText(info)
        # self.setStandardButtons(QMessageBox.NoButton)
        self.timer = QTimer(self)
        self.timer.setInterval(1000)
        self.timer.timeout.connect(self.changeContent)
        self.timer.start()

    def changeContent(self):
        self.time_to_wait -= 1
        if self.time_to_wait <= 0:
            self.close()

    def closeEvent(self, event):
        self.timer.stop()
        event.accept()
        self.close_callback.emit()

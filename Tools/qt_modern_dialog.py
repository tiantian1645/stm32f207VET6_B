from qtpy.QtCore import Qt, QMetaObject, Signal, Slot
from qtpy.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QToolButton, QLabel, QSizePolicy, QDialog, QDesktopWidget, QPushButton, QTextEdit
from qtpy.QtGui import QLayout

from qtmodern._utils import QT_VERSION
from qtmodern.windows import _FL_STYLESHEET

# _FL_STYLESHEET = join(dirname(abspath(__file__)), 'resources/frameless.qss')
""" str: Frameless window stylesheet. """

import loguru

logger = loguru.logger


class WindowDragger(QWidget):
    """ Window dragger.

        Args:
            window (QWidget): Associated window.
            parent (QWidget, optional): Parent widget.
    """

    doubleClicked = Signal()

    def __init__(self, window, parent=None):
        QWidget.__init__(self, parent)

        self._window = window
        self._mousePressed = False

    def mousePressEvent(self, event):
        self._mousePressed = True
        self._mousePos = event.globalPos()
        self._windowPos = self._window.pos()

    def mouseMoveEvent(self, event):
        if self._mousePressed:
            self._window.move(self._windowPos + (event.globalPos() - self._mousePos))

    def mouseReleaseEvent(self, event):
        self._mousePressed = False

    def mouseDoubleClickEvent(self, event):
        self.doubleClicked.emit()


class ModernWidget(QWidget):
    """ Modern window.

        Args:
            w (QWidget): Main widget.
            parent (QWidget, optional): Parent widget.
    """

    def __init__(self, w, parent=None):
        QWidget.__init__(self, parent)

        self.setupUi()

        contentLayout = QHBoxLayout()
        contentLayout.setContentsMargins(0, 0, 0, 0)
        contentLayout.addWidget(w)

        self.windowContent.setLayout(contentLayout)

        self.setWindowTitle(w.windowTitle())
        self.setGeometry(w.geometry())
        self.center()

    def center(self):
        logger.debug(f"invoke center in ModernWidget")
        qr = self.frameGeometry()
        cp = QDesktopWidget().availableGeometry().center()
        qr.moveCenter(cp)
        self.move(qr.topLeft())

    def _createFrame(self):
        self.vboxWindow = QVBoxLayout(self)
        self.vboxWindow.setContentsMargins(0, 0, 0, 0)

        self.windowFrame = QWidget(self)
        self.windowFrame.setObjectName("windowFrame")

        self.vboxFrame = QVBoxLayout(self.windowFrame)
        self.vboxFrame.setContentsMargins(0, 0, 0, 0)

    def _createTitle(self):
        self.titleBar = WindowDragger(self, self.windowFrame)
        self.titleBar.setObjectName("titleBar")
        self.titleBar.setSizePolicy(QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed))

        self.hboxTitle = QHBoxLayout(self.titleBar)
        self.hboxTitle.setContentsMargins(10, 0, 10, 0)
        self.hboxTitle.setSpacing(0)

        self.lblTitle = QLabel("Title")
        self.lblTitle.setObjectName("lblTitle")
        self.lblTitle.setAlignment(Qt.AlignCenter)
        self.hboxTitle.addWidget(self.lblTitle)

        spButtons = QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)

        self.btnMinimize = QToolButton(self.titleBar)
        self.btnMinimize.setObjectName("btnMinimize")
        self.btnMinimize.setSizePolicy(spButtons)
        self.hboxTitle.addWidget(self.btnMinimize)

        self.btnRestore = QToolButton(self.titleBar)
        self.btnRestore.setObjectName("btnRestore")
        self.btnRestore.setSizePolicy(spButtons)
        self.btnRestore.setVisible(False)
        self.hboxTitle.addWidget(self.btnRestore)

        self.btnMaximize = QToolButton(self.titleBar)
        self.btnMaximize.setObjectName("btnMaximize")
        self.btnMaximize.setSizePolicy(spButtons)
        self.hboxTitle.addWidget(self.btnMaximize)

        self.btnClose = QToolButton(self.titleBar)
        self.btnClose.setObjectName("btnClose")
        self.btnClose.setSizePolicy(spButtons)
        self.hboxTitle.addWidget(self.btnClose)

    def setupUi(self):
        self._createFrame()
        self._createTitle()

        self.vboxFrame.addWidget(self.titleBar)

        self.windowContent = QWidget(self.windowFrame)
        self.vboxFrame.addWidget(self.windowContent)

        self.vboxWindow.addWidget(self.windowFrame)

        # set window flags
        self.setWindowFlags(Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint)

        if QT_VERSION >= (5,):
            self.setAttribute(Qt.WA_TranslucentBackground)

        # set stylesheet
        with open(_FL_STYLESHEET) as stylesheet:
            self.setStyleSheet(stylesheet.read())

        # automatically connect slots
        QMetaObject.connectSlotsByName(self)

    def setWindowTitle(self, title):
        """ Set window title.

            Args:
                title (str): Title.
        """

        self.lblTitle.setText(title)

    @Slot()
    def on_btnMinimize_clicked(self):
        self.setWindowState(Qt.WindowMinimized)

    @Slot()
    def on_btnRestore_clicked(self):
        self.btnRestore.setVisible(False)
        self.btnMaximize.setVisible(True)

        self.setWindowState(Qt.WindowNoState)

    @Slot()
    def on_btnMaximize_clicked(self):
        self.btnRestore.setVisible(True)
        self.btnMaximize.setVisible(False)

        self.setWindowState(Qt.WindowMaximized)

    @Slot()
    def on_btnClose_clicked(self):
        self.close()

    @Slot()
    def on_titleBar_doubleClicked(self):
        if self.btnMaximize.isVisible():
            self.on_btnMaximize_clicked()
        else:
            self.on_btnRestore_clicked()


class ModernDialog(QDialog, ModernWidget):
    """ Modern window.

        Args:
            w (QWidget): Main widget.
            parent (QWidget, optional): Parent widget.
    """

    def __init__(self, w, parent=None):
        QDialog.__init__(self, parent)

        self.setupUi()

        contentLayout = QHBoxLayout()
        contentLayout.setContentsMargins(0, 0, 0, 0)
        contentLayout.addWidget(w)

        self.windowContent.setLayout(contentLayout)

        self.setWindowTitle(w.windowTitle())
        self.setGeometry(w.geometry())
        self.center()

    @Slot()
    def on_btnMinimize_clicked(self):
        ModernWidget.on_btnMinimize_clicked(self)

    @Slot()
    def on_btnRestore_clicked(self):
        ModernWidget.on_btnRestore_clicked(self)

    @Slot()
    def on_btnMaximize_clicked(self):
        ModernWidget.on_btnMaximize_clicked(self)

    @Slot()
    def on_btnClose_clicked(self):
        ModernWidget.on_btnClose_clicked(self)

    @Slot()
    def on_titleBar_doubleClicked(self):
        ModernWidget.on_titleBar_doubleClicked(self)


class ModernMessageBox(QDialog, ModernWidget):
    """ Modern window.

        Args:
            w (QWidget): Main widget.
            parent (QWidget, optional): Parent widget.
    """

    def __init__(self, parent=None):
        QDialog.__init__(self, parent)
        self.setupUi()
        self.center()
        logger.debug(f"self geometry | {self.geometry()}")

    def setupUi(self):
        self._createFrame()
        self._createTitle()

        self.vboxFrame.addWidget(self.titleBar)

        self.windowContent = QWidget(self.windowFrame)
        self.windowContent.setLayout(QVBoxLayout())
        self.msgTextLabel = QLabel("\n".join(("Test Content") * 10))
        self.msgTextLabel.setWordWrap(True)
        self.msgTextLabel.setMinimumWidth(100)
        self.msgDetailTextLabel = QTextEdit(self)
        self.msgDetailTextLabel.hide()

        msgButtonLayout = QHBoxLayout()
        msgButtonLayout.setSpacing(0)
        msgButtonLayout.setContentsMargins(0, 0, 0, 0)
        self.msgOKBtn = QPushButton("&退出")
        self.msgDetailBtn = QPushButton("&显示详细")
        self.msgDetailBtn.setCheckable(True)
        self.msgDetailBtn.hide()
        msgButtonLayout.addWidget(self.msgOKBtn)
        msgButtonLayout.addWidget(self.msgDetailBtn)

        self.msgOKBtn.clicked.connect(self.close)
        self.msgDetailBtn.clicked.connect(self.onDeatilShow)

        self.windowContent.layout().addWidget(self.msgTextLabel)
        self.windowContent.layout().addLayout(msgButtonLayout)
        self.windowContent.layout().addWidget(self.msgDetailTextLabel)
        self.vboxFrame.addWidget(self.windowContent)

        self.vboxWindow.addWidget(self.windowFrame)

        # set window flags
        self.setWindowFlags(Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint)

        if QT_VERSION >= (5,):
            self.setAttribute(Qt.WA_TranslucentBackground)

        # set stylesheet
        with open(_FL_STYLESHEET) as stylesheet:
            self.setStyleSheet(stylesheet.read())

        # automatically connect slots
        QMetaObject.connectSlotsByName(self)

    @Slot()
    def on_btnMinimize_clicked(self):
        ModernWidget.on_btnMinimize_clicked(self)

    @Slot()
    def on_btnRestore_clicked(self):
        ModernWidget.on_btnRestore_clicked(self)

    @Slot()
    def on_btnMaximize_clicked(self):
        ModernWidget.on_btnMaximize_clicked(self)

    @Slot()
    def on_btnClose_clicked(self):
        ModernWidget.on_btnClose_clicked(self)

    @Slot()
    def on_titleBar_doubleClicked(self):
        ModernWidget.on_titleBar_doubleClicked(self)

    def setWindowTitle(self, title):
        self.lblTitle.setText(title)

    def setTextInteractionFlags(self, flags):
        self.msgTextLabel.setTextInteractionFlags(flags)

    def setIcon(self, icon):
        # self.msg.setIcon(icon)
        pass

    def setText(self, text):
        text_length = self.msgTextLabel.fontMetrics().boundingRect(text).width()
        self.msgTextLabel.setText(text)
        self.msgTextLabel.setSizePolicy(QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed))
        self.msgTextLabel.setMinimumWidth(text_length)

    def setDetailedText(self, detail_text):
        dd = sorted(detail_text.split("\n"), key=lambda x: len(x))
        logger.debug(f"selecty the longest line | {dd[-1]}")
        text_length = self.msgTextLabel.fontMetrics().boundingRect(dd[-1]).width()
        self.msgTextLabel.setMinimumWidth(text_length)
        self.msgDetailBtn.show()
        self.msgDetailTextLabel.setText(detail_text)
        self.msgDetailTextLabel.setSizePolicy(QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed))

    def onDeatilShow(self, e):
        if e:
            self.msgDetailBtn.setText("&隐藏详细")
            self.msgDetailTextLabel.show()
        else:
            self.msgDetailBtn.setText("&显示详细")
            self.msgDetailTextLabel.hide()
            self.layout().setSizeConstraint(QLayout.SetFixedSize)

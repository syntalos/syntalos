#include "scrollablemessageboxdialog.h"

#include <QtWidgets>

ScrollableMessageBoxDialog::ScrollableMessageBoxDialog(QWidget *parent, const QString &title, const QString &text) :
    QDialog(parent),
    message(nullptr),
    okButton(nullptr)
{
    message = new QLabel(text);
    message->setTextInteractionFlags(Qt::TextSelectableByMouse);

    okButton = new QPushButton(tr("OK"), this);

    connect(okButton, SIGNAL(clicked(bool)), this, SLOT(close()));

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->addWidget(message);

    QWidget *textWidget = new QWidget(this);
    textWidget->setLayout(textLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(textWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    QHBoxLayout *okRow = new QHBoxLayout;
    okRow->addStretch();
    okRow->addWidget(okButton);
    okRow->addStretch();

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(scrollLayout);
    mainLayout->addLayout(okRow);

    setWindowTitle(title);
    setLayout(mainLayout);

    // Set dialog initial size to 25% larger than scrollArea's sizeHint - should avoid scroll bars for default size.
    int initialWidth = std::max((int) round(textWidget->sizeHint().width() * 1.25), textWidget->sizeHint().width() + 30);
    int initialHeight = std::max((int) round(textWidget->sizeHint().height() * 1.25), textWidget->sizeHint().height() + 30);

    // If initial height is more than 500, cap it at 500.
    initialHeight = std::min(500, initialHeight);

    resize(initialWidth, initialHeight);

    setWindowState(windowState() | Qt::WindowActive);
}

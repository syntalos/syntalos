#ifndef SIMPLEVPROBE_HPP
#define SIMPLEVPROBE_HPP

#include <QAbstractVideoSurface>
#include <QList>

class QCamera;
class QCameraViewfinder;

class SimpleVProbe : public QAbstractVideoSurface
{
    Q_OBJECT

private:
    QCamera *source;
public:
    explicit SimpleVProbe(QObject *parent = nullptr);

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const;

    // Called from QAbstractVideoSurface whenever a new frame is present
    bool present(const QVideoFrame &frame) Q_DECL_OVERRIDE;

    bool setSource(QCamera *source);

    bool isActive() const;

signals:
    void videoFrameProbed(const QVideoFrame &videoFrame);
    void flush();

};

#endif // SIMPLEVPROBE_HPP

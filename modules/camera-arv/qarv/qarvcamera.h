/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
                             Andrej Lajovic <andrej.lajovic@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QARVCAMERA_H
#define QARVCAMERA_H

#include <gio/gio.h>  // Workaround for gdbusintrospection's use of "signal".
#include <QList>
#include <QString>
#include <QRect>
#include <QPair>
#include <QMetaType>
#include <QImage>
#include <QHostAddress>
#include <QAbstractItemModel>

#include "qarv-globals.h"

#pragma GCC visibility push(default)

//! \name Forward declarations to avoid exposing arv.h
/**@{*/
struct _ArvCamera;
typedef _ArvCamera ArvCamera;
struct _ArvDevice;
typedef _ArvDevice ArvDevice;
struct _ArvBuffer;
typedef _ArvBuffer ArvBuffer;
struct _ArvStream;
typedef _ArvStream ArvStream;
struct _ArvGc;
typedef _ArvGc ArvGc;
#ifndef ARV_PIXEL_FORMAT_MONO_8
typedef quint32 ArvPixelFormat;
#endif

Q_DECLARE_OPAQUE_POINTER(ArvBuffer*)
Q_DECLARE_METATYPE(ArvBuffer*)
/**@}*/

using NewFrameFn = std::function<void(ArvBuffer*)>;

//! Objects of this class are used to identify cameras.
/*!
 * This class exposes three public strings containing the internal
 * Aravis id of the camera and the name of the camera vendor and model.
 * These strings are owned by the instance, not by Aravis.
 */
class QArvCameraId {
public:
    QArvCameraId();
    QArvCameraId(const char* id, const char* vendor, const char* model);
    QArvCameraId(const QArvCameraId& camid);
    ~QArvCameraId();
    const char* id, * vendor, * model;
};

Q_DECLARE_METATYPE(QArvCameraId)


//! QArvCamera provides an interface to an Aravis camera.
/*!
 * This class is mostly a thin wrapper around the arv_camera interface.
 * Only the parts that differ significantly from that interface are documented.
 * The QArvCamera::init() function must be called once in the main program
 * before this class is used.
 *
 * This class implements the QAbstractItemModel interface. This means that it
 * can be used as a data source for widgets such as QTreeView. An QArvCameraDelegate
 * is also provided to facilitate direct access to all camera features. The
 * model has two columns, the first being the name of the feature and the
 * second being the (editable) feature value.
 *
 * If you wish to read or modify a feature and don't want to deal with most
 * of the QAbstractItemModel interface, you can use the provided convenience
 * functions as in the following example:
 * \code
 * QArvCamera camera(id);
 * QStringList cats = camera.categories();
 * assert(cats.contains("ImageFormatControl"));
 * QStringList feats = camera.features("ImageFormatControl");
 * assert(feats.contains("SensorWidth"));
 * QModelIndex f = camera.featureIndex("SensorWidth");
 * QVariant val = f.data(Qt::EditRole);
 * QArvInteger intval = qvariant_cast<QArvInteger>(val);
 * qDebug() << intval.value;
 * intval.value = 1024;
 * val = QVariant::fromValue(intval);
 * camera.setData(f, val, Qt::EditRole);
 * \endcode
 *
 * When the QAbstractItemModel::dataChanged() signal is emitted, it currently
 * fails to specify which data is affected. This may change in the future.
 */
class QArvCamera : public QAbstractItemModel {
    Q_OBJECT

    class QArvCameraExtension;
    class QArvFeatureTree;

public:
    //! Initialize glib and aravis. Call this once in the main program.
    static void init();

    //! A camera with the given ID is opened.
    QArvCamera(QArvCameraId id, const QString &modId = QString(), QObject* parent = NULL);
    ~QArvCamera();

    static QList<QArvCameraId> listCameras(); //!< Returns a list of all cameras found.

    QArvCameraId getId(); //!< Returns the ID of the camera.

    //! Returns the underlying Aravis camera struct.
    ArvCamera* aravisCamera();

    //! \name Manipulate region of interest
    /*! The ROI behaves like a QRect and supports intersections and such. The
     * bounds functions behave a bit differently than Aravis counterparts in
     * that they return absolute limits for size of ROI, while Aravis functions
     * return smaller limits if the offset is non-zero.
     */
    //@{
    QRect getROI();
    void setROI(QRect roi);
    QPair<int, int> getROIWidthBounds();
    QPair<int, int> getROIHeightBounds();
    //@}

    //! \name Manipulate pixel binning
    /**@{*/
    QSize getBinning();
    void setBinning(QSize bin);
    /**@}*/

    /*! \name Choose pixel format
     * The lists returned by getPixelFormat(), getPixelFormatNames() and
     * getPixelFormatIds() are congruent.
     */
    /**@{*/
    QList<QString> getPixelFormats();
    QList<QString> getPixelFormatNames();
    QList<ArvPixelFormat> getPixelFormatIds();
    QString getPixelFormat();
    ArvPixelFormat getPixelFormatId();
    void setPixelFormat(const QString& format);
    /**@}*/

    //! \name Manipulate frames-per-second.
    /**@{*/
    double getFPS();
    void setFPS(double fps);
    /**@}*/

    //! \name Manipulate exposure time (in microseconds)
    /**@{*/
    double getExposure();
    void setExposure(double exposure);
    QPair<double, double> getExposureBounds();
    bool hasAutoExposure();
    void setAutoExposure(bool enable);
    /**@}*/

    //! \name Manipulate sensor gain
    /**@{*/
    double getGain();
    void setGain(double gain);
    QPair<double, double> getGainBounds();
    bool hasAutoGain();
    void setAutoGain(bool enable);
    /**@}*/

    //! \name Control acquisition
    /**@{*/
    void startAcquisition(bool zeroCopy = true, bool dropInvalidFrames = true, const NewFrameFn &newBufferCb = nullptr);
    void stopAcquisition();
    void setFrameQueueSize(uint size = 30);
    /**@}*/

    /*! \name Manipulate network parameters of an ethernet camera
     * MTU corresponds to "GevSCPSPacketSize", which should be set to the
     * MTU of the network interface. getHostIP() can be used to detect the
     * interface's address.
     */
    /**@{*/
    void setMTU(int mtu);
    int getMTU();
    QHostAddress getIP();
    QHostAddress getHostIP();
    int getEstimatedBW();
    /**@}*/

    bool rawFrameCallback();

signals:
    //! Emitted when a new frame is ready.
    /*! \param frame The raw frame data. May be empty for invalid frames.
     *  \param aravisFrame The raw Aravis frame. This allows access to info
     *  such as timestamp.
     *  \sa ::startAcquisition()
     */
    void frameReady(const QByteArray &frame, ArvBuffer* aravisFrame);
    //! Emitted when a buffer underrun occurs.
    void bufferUnderrun();

private slots:
    void receiveFrame();

public:
    QArv::QArvDebug logMessage() const;

private:
    QString m_modId;
    QArvCameraExtension* ext;
    static QList<QArvCameraId> cameraList;
    ArvCamera* camera;
    ArvDevice* device;
    ArvStream* stream;
    bool acquiring;
    uint frameQueueSize;
    quint64 underruns;
    bool nocopy;
    bool dropInvalid;
    NewFrameFn fnNewFrameBuffer;

    friend void QArvStreamCallback(void*, int, ArvBuffer*);
    friend QTextStream& operator<<(QTextStream& out, QArvCamera* camera);
    friend QTextStream& operator>>(QTextStream& in, QArvCamera* camera);

public:
    /*! \name QAbstractItemModel implementation
     * The model is a two-level table. The first level contains
     * a single column with feature categories. Each category
     * has a second level with two colums. The first contains
     * the name of a feature, and the second contains its value.
     * The latter has several roles contaning name, description, tooltip.
     * QVariant returned using Qt::EditRole contains a QArvType
     * with actual data. To obtain an internal feature name instead of its
     * display name, use Qt::UserRole.
     */
    /**@{*/
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index,
                 const QVariant& value,
                 int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    /**@}*/

    /*! \name Convenience functions for camera feature access
     * These functions can be used to avoid some of the QAbstractModel
     * boilerplate. They can be used to quickly list features and find
     * the index of a particular feature, which can then be used with
     * data() and setData() in Qt::EditRole. The featureIndex() function
     * searches first using the name obtained witn Qt::UserRole, and if
     * that fails searches again for the name obtained using Qt::DisplayRole.
     */
    /**@{*/
    QList<QString> categories() const;
    QList<QString> features(const QString& category) const;
    QModelIndex featureIndex(const QString& feature) const;
    void enableRegisterCache(bool enable = true, bool debug = false);
    /**@}*/

private:
    ArvGc* genicam;
    QArvFeatureTree* featuretree;
};

//! Serializes camera settings in text form.
QTextStream& operator<<(QTextStream& out, QArvCamera* camera);

//! Reads the textual representation of cammera settings. May not succeed.
QTextStream& operator>>(QTextStream& in, QArvCamera* camera);

#pragma GCC visibility pop

#endif // ARCAM_H

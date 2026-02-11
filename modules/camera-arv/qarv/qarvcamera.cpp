/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2014 Jure Varlec <jure.varlec@ad-vega.si>
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

#include <arv.h>
#include <gio/gio.h>

#include "utils/rtkit.h"

extern "C" {
#include <sys/socket.h>
}

#include <cstdlib>
#include <cstring>

#include "qarvcamera.h"
#include "qarvfeaturetree.h"
#include "qarvtype.h"
#include "qarv-globals.h"
#include <QTextDocument>

using namespace QArv;

class QArvCamera::QArvCameraExtension {
    friend class QArvCamera;

private:
};

QList<QArvCameraId> QArvCamera::cameraList;

void QArvCamera::init() {
    static bool arvInitialized = false;
    if (arvInitialized)
        return;

#if !GLIB_CHECK_VERSION(2, 35, 1)
    g_type_init();
#endif
    arv_enable_interface("Fake");
    qRegisterMetaType<ArvBuffer*>("ArvBuffer*");

    arvInitialized = true;
}

QArvCameraId::QArvCameraId() : id(NULL), vendor(NULL), model(NULL) {}

QArvCameraId::QArvCameraId(const char* id_,
                           const char* vendor_,
                           const char* model_) {
    id = g_strdup(id_ ? id_ : "");
    vendor = g_strdup(vendor_ ? vendor_ : "");
    model = g_strdup(model_ ? model_ : "");
}

QArvCameraId::QArvCameraId(const QArvCameraId& camid) {
    id = g_strdup(camid.id ? camid.id : "");
    vendor = g_strdup(camid.vendor ? camid.vendor : "");
    model = g_strdup(camid.model ? camid.model : "");
}

QArvCameraId::~QArvCameraId() {
    if (id != NULL) free((void*)id);
    if (vendor != NULL) free((void*)vendor);
    if (model != NULL) free((void*)model);
}

QString QArvCameraId::toString() const
{
    return QStringLiteral("%1 %2 (%3)").arg(vendor, model, id);
}

bool QArvCameraId::operator==(const QArvCameraId& other) const
{
    return g_strcmp0(id, other.id) == 0 &&
           g_strcmp0(vendor, other.vendor) == 0 &&
           g_strcmp0(model, other.model) == 0;
}

/*!
 * Acquisition mode is set to CONTINUOUS when the camera is opened.
 */
QArvCamera::QArvCamera(QArvCameraId id, const QString &modId, QObject* parent) :
    QAbstractItemModel(parent), m_modId(modId), acquiring(false) {
    g_autoptr(GError) error = nullptr;

    ext = new QArvCameraExtension;
    setFrameQueueSize();

    camera = arv_camera_new(id.id, &error);
    if (camera == nullptr)
        throw std::runtime_error(error->message);

    arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, nullptr);
    device = arv_camera_get_device(camera);
    genicam = arv_device_get_genicam(device);
    featuretree = QArvFeatureTree::createFeaturetree(genicam);
}

QArvCamera::~QArvCamera() {
    delete ext;
    if (camera != nullptr) {
        QArvFeatureTree::freeFeaturetree(featuretree);
        stopAcquisition();
        g_object_unref(camera);
    }
}

/*!
 * A list of camera IDs is created by opening each camera to obtain the vendor
 * and model names.
 *
 * TODO: see if this info can be obtained using a lower level API.
 */
QList<QArvCameraId> QArvCamera::listCameras() {
    cameraList.clear();
    arv_update_device_list();
    unsigned int N = arv_get_n_devices();
    for (unsigned int i = 0; i < N; i++) {
        const char* camid = arv_get_device_id(i);
        if (!camid)
            continue;

        g_autoptr(ArvCamera) camera = arv_camera_new(camid, nullptr);
        if (camera == nullptr)
            continue;
        QArvCameraId id(camid, arv_camera_get_vendor_name(camera, nullptr),
                        arv_camera_get_model_name(camera, nullptr));

        cameraList << id;
    }
    return QList<QArvCameraId>(cameraList);
}

QArvCameraId QArvCamera::getId() const {
    const char* id, * vendor, * model;
    id = arv_camera_get_device_id(camera, nullptr);
    if (id == nullptr)
        id = "";
    vendor = arv_camera_get_vendor_name(camera, nullptr);
    model = arv_camera_get_model_name(camera, nullptr);
    return QArvCameraId(id, vendor, model);
}

/*!
 * This function is provided in case you need to hack around any QArv
 * limitations. If you need to do so, we would like to hear about it.
 * It might be more appropriate to extend the C++ API.
 */
ArvCamera* QArvCamera::aravisCamera() {
    return camera;
}

QRect QArvCamera::getROI() {
    int x, y, width, height;
    arv_camera_get_region(camera, &x, &y, &width, &height, nullptr);
    return QRect(x, y, width, height);
}

QPair<int, int> QArvCamera::getROIWidthBounds() {
    int wmin, wmax;
    arv_camera_get_width_bounds(camera, &wmin, &wmax, nullptr);
    QRect roi = getROI();
    return qMakePair(wmin, wmax + roi.x());
}

QPair<int, int> QArvCamera::getROIHeightBounds() {
    int hmin, hmax;
    arv_camera_get_height_bounds(camera, &hmin, &hmax, nullptr);
    QRect roi = getROI();
    return qMakePair(hmin, hmax + roi.y());
}

void QArvCamera::setROI(QRect roi) {
    int x, y, width, height;
    roi.getRect(&x, &y, &width, &height);
    auto hmin = getROIHeightBounds();
    auto wmin = getROIWidthBounds();

    arv_device_set_integer_feature_value(device, "Width", wmin.first, nullptr);
    arv_device_set_integer_feature_value(device, "Height", hmin.first, nullptr);
    arv_device_set_integer_feature_value(device, "OffsetX", x, nullptr);
    arv_device_set_integer_feature_value(device, "OffsetY", y, nullptr);
    arv_device_set_integer_feature_value(device, "Width", width, nullptr);
    arv_device_set_integer_feature_value(device, "Height", height, nullptr);

    emit dataChanged(QModelIndex(), QModelIndex());
}

QSize QArvCamera::getBinning() {
    int x, y;
    arv_camera_get_binning(camera, &x, &y, nullptr);
    return QSize(x, y);
}

void QArvCamera::setBinning(QSize bin) {
    arv_camera_set_binning(camera, bin.width(), bin.height(), nullptr);
    emit dataChanged(QModelIndex(), QModelIndex());
}

QList< QString > QArvCamera::getPixelFormats()
{
    QList<QString> list;
    unsigned int numformats;
    const char **formats = arv_camera_dup_available_pixel_formats_as_strings(camera, &numformats, nullptr);

    if (formats == nullptr) {
        qWarning().noquote() << "No pixel formats received for Aravis camera" << getId().toString();
        return list;
    }

    for (uint i = 0; i < numformats; i++)
        list << formats[i];
    g_free(formats);
    return list;
}

QList< QString > QArvCamera::getPixelFormatNames() {
    unsigned int numformats;
    const char **formats =
        arv_camera_dup_available_pixel_formats_as_display_names(camera,
                                                                &numformats,
                                                                nullptr);
    QList<QString> list;
    for (uint i = 0; i < numformats; i++) {
        list << formats[i];
    }
    free(formats);
    return list;
}

QList<ArvPixelFormat> QArvCamera::getPixelFormatIds() {
    unsigned int numformats;
    gint64 *formats =
        arv_camera_dup_available_pixel_formats(camera, &numformats, nullptr);
    QList<ArvPixelFormat> list;
    for (uint i = 0; i < numformats; i++) {
        list << formats[i];
    }
    free(formats);
    return list;
}

QString QArvCamera::getPixelFormat() {
    return QString(arv_camera_get_pixel_format_as_string(camera, nullptr));
}

ArvPixelFormat QArvCamera::getPixelFormatId() {
    return arv_camera_get_pixel_format(camera, nullptr);
}

void QArvCamera::setPixelFormat(const QString& format) {
    auto tmp = format.toLatin1();
    arv_camera_set_pixel_format_from_string(camera, tmp.constData(), nullptr);
    emit dataChanged(QModelIndex(), QModelIndex());
}

double QArvCamera::getFPS() {
    return arv_camera_get_frame_rate(camera, nullptr);
}

void QArvCamera::setFPS(double fps) {
    arv_camera_set_frame_rate(camera, fps, nullptr);
    emit dataChanged(QModelIndex(), QModelIndex());
}

QPair<double, double> QArvCamera::getFPSBounds()
{
    g_autoptr(GError) error = nullptr;
    double min, max;
    arv_camera_get_frame_rate_bounds(camera, &min, &max, &error);
    if (error != nullptr) {
        qWarning().noquote() <<
            "Failed to get FPS bounds for Aravis camera" << getId().toString() << ":" << error->message;
        return QPair<double, double>(0, 60);
    }
    return qMakePair(min, max);
}

int QArvCamera::getMTU() {
    return arv_device_get_integer_feature_value(device, "GevSCPSPacketSize", nullptr);
}

void QArvCamera::setMTU(int mtu) {
    arv_device_set_integer_feature_value(device, "GevSCPSPacketSize", mtu, nullptr);
    arv_device_set_integer_feature_value(device, "GevSCBWR", 10, nullptr);
    emit dataChanged(QModelIndex(), QModelIndex());
}

double QArvCamera::getExposure() {
    return arv_camera_get_exposure_time(camera, nullptr);
}

void QArvCamera::setExposure(double exposure) {
    arv_camera_set_exposure_time(camera, exposure, nullptr);
    emit dataChanged(QModelIndex(), QModelIndex());
}

bool QArvCamera::hasAutoExposure() {
    return arv_camera_is_exposure_auto_available(camera, nullptr);
}

void QArvCamera::setAutoExposure(bool enable) {
    auto mode = enable ? ARV_AUTO_CONTINUOUS : ARV_AUTO_OFF;
    arv_camera_set_exposure_time_auto(camera, mode, nullptr);
    emit dataChanged(QModelIndex(), QModelIndex());
}

double QArvCamera::getGain() {
    return arv_camera_get_gain(camera, nullptr);
}

void QArvCamera::setGain(double gain) {
    arv_camera_set_gain(camera, gain, nullptr);
    emit dataChanged(QModelIndex(), QModelIndex());
}

QPair< double, double > QArvCamera::getExposureBounds() {
    double expomin, expomax;
    arv_camera_get_exposure_time_bounds(camera, &expomin, &expomax, nullptr);
    return QPair<double, double>(expomin, expomax);
}

QPair< double, double > QArvCamera::getGainBounds() {
    double gainmin, gainmax;
    arv_camera_get_gain_bounds(camera, &gainmin, &gainmax, nullptr);
    return QPair<double, double>(gainmin, gainmax);
}

bool QArvCamera::hasAutoGain() {
    return arv_camera_is_gain_auto_available(camera, nullptr);
}

void QArvCamera::setAutoGain(bool enable) {
    auto mode = enable ? ARV_AUTO_CONTINUOUS : ARV_AUTO_OFF;
    arv_camera_set_gain_auto(camera, mode, nullptr);
    emit dataChanged(QModelIndex(), QModelIndex());
}

//! Store the pointer to the current frame.
void QArvCamera::receiveFrame() {
    if (!acquiring)
        return; // Stream does not exist any more.

    ArvBuffer* frame = arv_stream_pop_buffer(stream);
    QByteArray baframe;
    ArvBufferStatus status;
    status = arv_buffer_get_status(frame);
    if (status == ARV_BUFFER_STATUS_SUCCESS || !dropInvalid) {
        size_t size;
        const void* data;
        data = arv_buffer_get_data(frame, &size);
        if (nocopy) {
            baframe = QByteArray::fromRawData(static_cast<const char*>(data),
                                              size);
        } else {
            baframe = QByteArray(static_cast<const char*>(data), size);
        }
    }

    emit frameReady(baframe, frame);
    arv_stream_push_buffer(stream, frame);

    guint64 under;
    arv_stream_get_statistics(stream, NULL, NULL, &under);
    if (under != underruns) {
        underruns = under;
        int in, out;
        arv_stream_get_n_buffers(stream, &in, &out);
        emit bufferUnderrun();
    }
}

inline void QArvStreamCallback(void* vcam, int type, ArvBuffer* bfr) {
    if (type == ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE) {
        auto cam = static_cast<QArvCamera*>(vcam);
        if (!cam->rawFrameCallback())
            QMetaObject::invokeMethod(cam, "receiveFrame", Qt::QueuedConnection);
    }
}

static void QArvStreamCallbackWrap(void* vcam,
                                   ArvStreamCallbackType type,
                                   ArvBuffer* bfr) {
    if (type == ARV_STREAM_CALLBACK_TYPE_INIT) {
        // TODO: Fetch thread settings from the Syntalos engine?
        if (!setCurrentThreadRealtime (20) &&
            !setCurrentThreadNiceness (-10))
            qWarning().noquote() << "Failed to make Aravis camera stream thread high priority";
    }

    QArvStreamCallback(vcam, type, bfr);
}

bool QArvCamera::rawFrameCallback()
{
    if (!fnNewFrameBuffer)
        return false;

    ArvBuffer* frame = arv_stream_pop_buffer(stream);
    ArvBufferStatus status;
    status = arv_buffer_get_status(frame);
    if (status == ARV_BUFFER_STATUS_SUCCESS || !dropInvalid)
        fnNewFrameBuffer(frame);

    arv_stream_push_buffer(stream, frame);

    guint64 under;
    arv_stream_get_statistics(stream, NULL, NULL, &under);
    if (under != underruns) {
        underruns = under;
        int in, out;
        arv_stream_get_n_buffers(stream, &in, &out);
        emit bufferUnderrun();
    }

    return true;
}

/*!
 * This function not only starts acquisition, but also pushes a number of
 * frames onto the stream and sets up the callback which accepts frames.
 * \param dropInvalidFrames If true, the frameReady() signal will return
 * an empty QByteArray when the frame is not complete.
 * \param zeroCopy If true, the QByteArray sent by the
 * frameReady() signal doesn't own the data. The
 * caller guarantees that the frame will be used "quickly", i.e. before it is
 * used to capture the next image. Only use this if the caller will decode or
 * otherwise copy the data immediately. Note that the array itself is never
 * invalidated after the grace period passes, it is merely overwritten.
 * Also note that zeroCopy only applies to the QBytearray; the ArvBuffer is
 * never copied.
 */
void QArvCamera::startAcquisition(bool zeroCopy, bool dropInvalidFrames, const NewFrameFn &newBufferCb) {
    nocopy = zeroCopy;
    dropInvalid = dropInvalidFrames;
    fnNewFrameBuffer = newBufferCb;
    if (acquiring) return;
    unsigned int framesize = arv_camera_get_payload(camera, nullptr);
    stream = arv_camera_create_stream(camera, QArvStreamCallbackWrap, this, nullptr);
    for (uint i = 0; i < frameQueueSize; i++) {
        arv_stream_push_buffer(stream, arv_buffer_new(framesize, NULL));
    }
    arv_camera_start_acquisition(camera, nullptr);
    acquiring = true;
    underruns = 0;
    emit dataChanged(QModelIndex(), QModelIndex());
}

void QArvCamera::stopAcquisition() {
    if (!acquiring) return;
    arv_camera_stop_acquisition(camera, nullptr);
    g_object_unref(stream);
    acquiring = false;
    fnNewFrameBuffer = nullptr;
    emit dataChanged(QModelIndex(), QModelIndex());
}

//! Set the number of frames on the stream. Takes effect on startAcquisition().
/*! An Aravis stream has a queue of frame buffers which is cycled as frames are
 * acquired. The frameReady() signal returns the frame that is currently being
 * cycled. Several frames should be put on the queue for smooth operation. More
 * should be used when using the zeroCopy facility of startAcquisition(), as this
 * increases the grace period in which the returned frame is valid. For
 * example, with the queue size of 30 and framerate of 60 FPS, the grace period
 * is approximately one half second. Increasing the queue size increases the
 * memory usage, as all buffers are allocated when acquisition starts.
 */
void QArvCamera::setFrameQueueSize(uint size) {
    frameQueueSize = size;
}

//! Translates betwen the glib and Qt address types via a native sockaddr.
static QHostAddress GSocketAddress_to_QHostAddress(GSocketAddress* gaddr) {
    sockaddr addr;
    int success = g_socket_address_to_native(gaddr, &addr, sizeof(addr), NULL);
    if (!success) {
        qDebug().noquote() << "Unable to translate IP address.";
        return QHostAddress();
    }
    return QHostAddress(&addr);
}

QHostAddress QArvCamera::getIP() {
    if (ARV_IS_GV_DEVICE(device)) {
        auto gaddr = arv_gv_device_get_device_address(ARV_GV_DEVICE(device));
        return GSocketAddress_to_QHostAddress(gaddr);
    } else return QHostAddress();
}

QHostAddress QArvCamera::getHostIP() {
    if (ARV_IS_GV_DEVICE(device)) {
        auto gaddr = arv_gv_device_get_interface_address(ARV_GV_DEVICE(device));
        return GSocketAddress_to_QHostAddress(gaddr);
    } else return QHostAddress();
}

int QArvCamera::getEstimatedBW() {
#ifdef ARAVIS_OLD_SET_FEATURE
    return arv_device_get_integer_feature_value(device, "GevSCDCT");
#else
    return arv_device_get_integer_feature_value(device, "GevSCDCT", nullptr);
#endif
}

QTextStream& operator<<(QTextStream& out, QArvCamera* camera) {
    auto id = camera->getId();
    out << "CameraID:\t"
        << QString::fromUtf8(id.vendor) << "\t"
        << QString::fromUtf8(id.model) << "\t"
        << QString::fromUtf8(id.id) << Qt::endl;
    QArvCamera::QArvFeatureTree::recursiveSerialization(out,
                                                        camera,
                                                        camera->featuretree);
    return out;
}

/*!
 * This operator simply reads settings into the camera in the order they are
 * provided. This means that dependencies between features are not honored.
 * Because of this, loading may fail. Trying several times might help. This
 * cannot be fixed until Aravis provides dependecy information.
 */
QTextStream& operator>>(QTextStream& in, QArvCamera* camera) {
    auto ID = camera->getId();
    const auto camIdLine = in.readLine();
    QStringList parts = camIdLine.split('\t');

    // Handle both old (3 newline-separated lines) and new (tab-separated) formats
    QString vendor, model, id;
    if (parts.size() >= 4 && parts[0] == "CameraID:") {
        // New format: CameraID:\tvendor\tmodel\tid
        vendor = parts[1];
        model = parts[2];
        id = parts[3];
    } else {
        // Old format compatibility: first line is vendor, need to read model and id
        vendor = camIdLine;
        model = in.readLine();
        id = in.readLine();
    }

    if (!(vendor == ID.vendor && model == ID.model && id == ID.id)) {
        camera->logMessage()
            << "Incompatible camera settings:"
            << "expected" << ID.toString() << "but got"
            << QStringLiteral("%1 %2 (%3)").arg(vendor, model, id);
        return in;
    }

    while (!in.atEnd()) {
        QString name, type, v;
        in >> name;
        if (name == "Category") continue;
        ArvGcFeatureNode* node =
            ARV_GC_FEATURE_NODE(arv_gc_get_node(camera->genicam,
                                                name.toLatin1()));

        in >> type;
        in >> v;
        if (type == "Register") {
            QString hex;
            in >> hex;
            arv_gc_register_set(ARV_GC_REGISTER(node), v.data(), v.toLongLong(),
                                NULL);
        } else if (type == "Enumeration") {
            arv_gc_enumeration_set_string_value(ARV_GC_ENUMERATION(node),
                                                v.toLatin1().data(), NULL);
        } else if (type == "String") {
            arv_gc_string_set_value(ARV_GC_STRING(node),
                                    v.toLatin1().data(),
                                    NULL);
        } else if (type == "Float") {
            arv_gc_float_set_value(ARV_GC_FLOAT(node), v.toDouble(), NULL);
        } else if (type == "Boolean") {
            arv_gc_boolean_set_value(ARV_GC_BOOLEAN(node), v.toInt(), NULL);
        } else if (type == "Integer") {
            arv_gc_integer_set_value(ARV_GC_INTEGER(node), v.toLongLong(),
                                     NULL);
        }
    }

    return in;
}

/* QAbstractItemModel implementation ######################################## */

QModelIndex QArvCamera::index(int row, int column,
                              const QModelIndex& parent) const {
    if (column > 1) return QModelIndex();
    QArvCamera::QArvFeatureTree* treenode;
    if (!parent.isValid()) treenode = featuretree;
    else treenode =
            static_cast<QArvCamera::QArvFeatureTree*>(parent.internalPointer());
    auto children = treenode->children();
    if (row < 0 || row >= children.size()) return QModelIndex();
    auto child = children.at(row);
    ArvGcNode* node = arv_gc_get_node(genicam, child->feature());
    if (ARV_IS_GC_CATEGORY(node) && column > 0) return QModelIndex();
    return createIndex(row, column, child);
}

QModelIndex QArvCamera::parent(const QModelIndex& index) const {
    QArvCamera::QArvFeatureTree* treenode;
    if (!index.isValid()) treenode = featuretree;
    else treenode =
            static_cast<QArvCamera::QArvFeatureTree*>(index.internalPointer());
    if (treenode->parent() == NULL) return QModelIndex();
    auto parent = treenode->parent();
    return createIndex(parent == NULL ? 0 : parent->row(), 0, parent);
}

int QArvCamera::columnCount(const QModelIndex& parent) const {
    return 2;
}

int QArvCamera::rowCount(const QModelIndex& parent) const {
    QArvCamera::QArvFeatureTree* treenode;
    if (!parent.isValid()) treenode = featuretree;
    else treenode =
            static_cast<QArvCamera::QArvFeatureTree*>(parent.internalPointer());
    return treenode->children().count();
}

QVariant QArvCamera::data(const QModelIndex& index, int role) const {
    QArvCamera::QArvFeatureTree* treenode;
    if (!index.isValid()) treenode = featuretree;
    else treenode =
            static_cast<QArvCamera::QArvFeatureTree*>(index.internalPointer());
    ArvGcNode* node = arv_gc_get_node(genicam, treenode->feature());

    if (!ARV_IS_GC_FEATURE_NODE(node)) {
        logMessage() << "data:" << "Node" << treenode->feature()
                     << "is not valid!";
        return QVariant();
    }

    const char* string;
    const char* string2;

    if (role == Qt::UserRole) {
        string =
            arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(node));
        if (string == NULL)
            return QVariant();
        return QVariant(string);
    }

    if (index.column() == 0) {
        switch (role) {
        case Qt::DisplayRole:
            string =
                arv_gc_feature_node_get_display_name(ARV_GC_FEATURE_NODE(node));
            if (string == NULL)
                string =
                    arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(node));
            if (string == NULL) {
                logMessage() << "Node has no name!?";
                return QVariant();
            }
            return QVariant(string);

        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
            string =
                arv_gc_feature_node_get_description(ARV_GC_FEATURE_NODE(node));
                string2 =
                arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(node));
            if (string == NULL || string2 == NULL)
                return QVariant();
            return QVariant("<qt/>" + QString(string2).toHtmlEscaped() + ": "
                            + QString(string).toHtmlEscaped());

        default:
            return QVariant();
        }
    } else if (index.column() == 1) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            if (ARV_IS_GC_REGISTER_NODE(node)
                && QString(arv_dom_node_get_node_name(ARV_DOM_NODE(node)))
                == "IntReg") {
                QArvRegister r;
                r.length = arv_gc_register_get_length(ARV_GC_REGISTER(node),
                                                      NULL);
                r.value = QByteArray(r.length, 0);
                arv_gc_register_get(ARV_GC_REGISTER(node),
                                    r.value.data(), r.length, NULL);
                if (role == Qt::DisplayRole)
                    return QVariant::fromValue((QString)r);
                else
                    return QVariant::fromValue(r);
            }
            if (ARV_IS_GC_ENUMERATION(node)) {
                QArvEnumeration e;
                const GSList* entry =
                    arv_gc_enumeration_get_entries(ARV_GC_ENUMERATION(node));
                for (; entry != NULL; entry = entry->next) {
                    bool isAvailable =
                        arv_gc_feature_node_is_available(ARV_GC_FEATURE_NODE(
                                                             entry->data),
                                                         NULL);
                    bool isImplemented =
                        arv_gc_feature_node_is_implemented(ARV_GC_FEATURE_NODE(
                                                               entry->data),
                                                           NULL);
                    e.values
                        << arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(
                                                            entry->data));
                    const char* name =
                        arv_gc_feature_node_get_display_name(ARV_GC_FEATURE_NODE(
                                                                 entry->
                                                                     data));
                    if (name == NULL)
                        e.names << e.values.last();
                    else
                        e.names << name;
                    if (isAvailable && isImplemented)
                        e.isAvailable << true;
                    else
                        e.isAvailable << false;
                }
                const char* current =
                    arv_gc_enumeration_get_string_value(ARV_GC_ENUMERATION(
                                                            node),
                                                        NULL);
                e.currentValue = e.values.indexOf(current);
                if (role == Qt::DisplayRole)
                    return QVariant::fromValue((QString)e);
                else
                    return QVariant::fromValue(e);
            }
            if (ARV_IS_GC_COMMAND(node)) {
                QArvCommand c;
                if (role == Qt::DisplayRole)
                    return QVariant::fromValue((QString)c);
                else
                    return QVariant::fromValue(c);
            }
            if (ARV_IS_GC_STRING(node)) {
                QArvString s;
                s.value = arv_gc_string_get_value(ARV_GC_STRING(node), NULL);
                s.maxlength = arv_gc_string_get_max_length(ARV_GC_STRING(node),
                                                           NULL);
                if (role == Qt::DisplayRole)
                    return QVariant::fromValue((QString)s);
                else
                    return QVariant::fromValue(s);
            }
            if (ARV_IS_GC_FLOAT(node)) {
                QArvFloat f;
                f.value = arv_gc_float_get_value(ARV_GC_FLOAT(node), NULL);
                f.min = arv_gc_float_get_min(ARV_GC_FLOAT(node), NULL);
                f.max = arv_gc_float_get_max(ARV_GC_FLOAT(node), NULL);
                f.unit = arv_gc_float_get_unit(ARV_GC_FLOAT(node));
                if (role == Qt::DisplayRole)
                    return QVariant::fromValue((QString)f);
                else
                    return QVariant::fromValue(f);
            }
            if (ARV_IS_GC_BOOLEAN(node)) {
                QArvBoolean b;
                b.value = arv_gc_boolean_get_value(ARV_GC_BOOLEAN(node), NULL);
                if (role == Qt::DisplayRole)
                    return QVariant::fromValue((QString)b);
                else
                    return QVariant::fromValue(b);
            }
            if (ARV_IS_GC_INTEGER(node)) {
                QArvInteger i;
                i.value = arv_gc_integer_get_value(ARV_GC_INTEGER(node), NULL);
                i.min = arv_gc_integer_get_min(ARV_GC_INTEGER(node), NULL);
                i.max = arv_gc_integer_get_max(ARV_GC_INTEGER(node), NULL);
                i.inc = arv_gc_integer_get_inc(ARV_GC_INTEGER(node), NULL);
                if (role == Qt::DisplayRole)
                    return QVariant::fromValue((QString)i);
                else
                    return QVariant::fromValue(i);
            }
            return QVariant();
        }
    } else return QVariant();
    return QVariant();
}

bool QArvCamera::setData(const QModelIndex& index, const QVariant& value,
                         int role) {
    QAbstractItemModel::setData(index, value, role);
    if (!(index.model()->flags(index) & Qt::ItemIsEnabled)
        || !(index.model()->flags(index) & Qt::ItemIsEditable))
        return false;

    QArvCamera::QArvFeatureTree* treenode;
    if (!index.isValid()) treenode = featuretree;
    else treenode =
            static_cast<QArvCamera::QArvFeatureTree*>(index.internalPointer());
    ArvGcFeatureNode* node =
        ARV_GC_FEATURE_NODE(arv_gc_get_node(genicam, treenode->feature()));

    if (value.canConvert<QArvRegister>()) {
        auto r = qvariant_cast<QArvRegister>(value);
        arv_gc_register_set(ARV_GC_REGISTER(node),
                            r.value.data(),
                            r.length,
                            NULL);
    } else if (value.canConvert<QArvEnumeration>()) {
        auto e = qvariant_cast<QArvEnumeration>(value);
        if (e.isAvailable.at(e.currentValue)) {
            arv_gc_enumeration_set_string_value(ARV_GC_ENUMERATION(node),
                                                e.values.at(
                                                    e.currentValue).toLatin1().data(),
                                                NULL);
        } else return false;
    } else if (value.canConvert<QArvCommand>()) {
        arv_gc_command_execute(ARV_GC_COMMAND(node), NULL);
    } else if (value.canConvert<QArvString>()) {
        auto s = qvariant_cast<QArvString>(value);
        arv_gc_string_set_value(ARV_GC_STRING(node), s.value.toLatin1().data(),
                                NULL);
    } else if (value.canConvert<QArvFloat>()) {
        auto f = qvariant_cast<QArvFloat>(value);
        arv_gc_float_set_value(ARV_GC_FLOAT(node), f.value, NULL);
    } else if (value.canConvert<QArvBoolean>()) {
        auto b = qvariant_cast<QArvBoolean>(value);
        arv_gc_boolean_set_value(ARV_GC_BOOLEAN(node), b.value, NULL);
    } else if (value.canConvert<QArvInteger>()) {
        auto i = qvariant_cast<QArvInteger>(value);
        arv_gc_integer_set_value(ARV_GC_INTEGER(node), i.value, NULL);
    } else
        return false;

    emit dataChanged(QModelIndex(), QModelIndex());
    return true;
}

Qt::ItemFlags QArvCamera::flags(const QModelIndex& index) const {
    auto f = QAbstractItemModel::flags(index);
    if (!index.isValid()) return f;
    QArvCamera::QArvFeatureTree* treenode;
    treenode =
        static_cast<QArvCamera::QArvFeatureTree*>(index.internalPointer());
    ArvGcFeatureNode* node =
        ARV_GC_FEATURE_NODE(arv_gc_get_node(genicam, treenode->feature()));
    if (index.column() != 1 && !ARV_IS_GC_CATEGORY(node)) {
        f = flags(sibling(index.row(), 1, index));
        f &= ~Qt::ItemIsEditable;
    } else {
        int enabled =
            arv_gc_feature_node_is_available(node, NULL)
            && arv_gc_feature_node_is_implemented(node, NULL)
            && !arv_gc_feature_node_is_locked(node, NULL);
        if (!enabled) {
            f &= ~Qt::ItemIsEnabled;
            f &= ~Qt::ItemIsEditable;
        } else
            f |= Qt::ItemIsEditable;
    }
    return f;
}

QVariant QArvCamera::headerData(int section, Qt::Orientation orientation,
                                int role) const {
    switch (section) {
    case 0:
        return QVariant::fromValue(QString("Feature"));

    case 1:
        return QVariant::fromValue(QString("Value"));
    }
    return QVariant();
}

QList<QString> QArvCamera::categories() const {
    QList<QString> list;
    for (int i = 0; i < rowCount(); i++) {
        auto idx = index(i, 0);
        list << idx.data().toString();
    }
    return list;
}

QList<QString> QArvCamera::features(const QString& category) const {
    QList<QString> list;
    for (int i = 0; i < rowCount(); i++) {
        auto idx = index(i, 0);
        if (idx.data().toString() == category) {
            for (int j = 0; j < rowCount(idx); j++) {
                auto idx2 = index(j, 0, idx);
                list << idx2.data().toString();
            }
            return list;
        }
    }
    return list;
}

QModelIndex QArvCamera::featureIndex(const QString& feature) const {
    for (int i = 0; i < rowCount(); i++) {
        auto idx = index(i, 0);
        for (int j = 0; j < rowCount(idx); j++) {
            auto idx2 = index(j, 0, idx);
            if (idx2.data(Qt::UserRole).toString() == feature) {
                return index(j, 1, idx);
            }
        }
    }
    for (int i = 0; i < rowCount(); i++) {
        auto idx = index(i, 0);
        for (int j = 0; j < rowCount(idx); j++) {
            auto idx2 = index(j, 0, idx);
            if (idx2.data(Qt::DisplayRole).toString() == feature) {
                return index(j, 1, idx);
            }
        }
    }
    return QModelIndex();
}

void QArvCamera::enableRegisterCache(bool enable, bool debug) {
    auto policy = enable ?
        ARV_REGISTER_CACHE_POLICY_ENABLE :
        ARV_REGISTER_CACHE_POLICY_DISABLE;
    if (enable && debug) {
        policy = ARV_REGISTER_CACHE_POLICY_DEBUG;
    }
    arv_gc_set_register_cache_policy(genicam, policy);
}

QArv::QArvDebug QArvCamera::logMessage() const
{
    return {m_modId};;
}

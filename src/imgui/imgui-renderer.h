#pragma once

#include <QOpenGLExtraFunctions>
#include <QObject>
#include <QPoint>
#include <imgui.h>
#include <memory>

class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class QOpenGLContext;

namespace QtImGui
{

class WindowWrapper
{
public:
    virtual ~WindowWrapper() {}
    virtual void installEventFilter(QObject *object) = 0;
    virtual QSize size() const = 0;
    virtual qreal devicePixelRatio() const = 0;
    virtual bool isActive() const = 0;
    virtual QPoint mapFromGlobal(const QPoint &p) const = 0;
    virtual QObject *object() = 0;

    virtual void setCursorShape(Qt::CursorShape shape) = 0;
    virtual void setCursorPos(const QPoint &local_pos) = 0;
};

class ImGuiRenderer : public QObject, QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    void initialize(WindowWrapper *window);
    void newFrame();
    void render();
    bool eventFilter(QObject *watched, QEvent *event) override;

    /**
     * Tell the renderer that the OpenGL context of its host widget was destroyed
     * and replaced by a new one. All GPU-side objects are rebuilt on the next frame,
     * while the ImGui context and all its state are preserved.
     */
    void notifyContextRecreated();

    void ensureContext();

    static ImGuiRenderer *instance();

public:
    ImGuiRenderer();
    ~ImGuiRenderer();

private:
    void onMousePressedChange(QMouseEvent *event);
    void onMouseMove(QMouseEvent *event);
    void onWheel(QWheelEvent *event);
    void onKeyPressRelease(QKeyEvent *event);
    void onFocusChanged(bool focused);

    void updateCursorShape(const ImGuiIO &io);
    void setCursorPos(const ImGuiIO &io);

    void renderDrawList(ImDrawData *draw_data);
    bool createFontsTexture();
    bool createDeviceObjects();
    void invalidateDeviceObjects();

    std::unique_ptr<WindowWrapper> m_window;
    double g_Time = 0.0f;
    GLuint g_FontTexture = 0;
    int g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;
    int g_AttribLocationTex = 0, g_AttribLocationProjMtx = 0;
    int g_AttribLocationPosition = 0, g_AttribLocationUV = 0, g_AttribLocationColor = 0;
    unsigned int g_VboHandle = 0, g_VaoHandle = 0, g_ElementsHandle = 0;

    // the GL context the above objects were created in, so we never hand their
    // names to a context they do not belong to
    QOpenGLContext *m_glCtx = nullptr;

    ImGuiContext *g_ctx = nullptr;
};

} // namespace QtImGui

#include "qtimgui.h"

#include "imgui-renderer.h"
#include <QWindow>
#include <memory>
#ifdef QT_WIDGETS_LIB
#include <QWidget>
#endif

namespace QtImGui
{

namespace
{

// A RenderRef is just the renderer itself, with a null reference addressing the
// shared one. The renderer owns the WindowWrapper describing its window, and
// nothing owns the renderer in return - see QtImGui::destroy().
ImGuiRenderer *rendererFor(RenderRef ref)
{
    if (ref == nullptr)
        return ImGuiRenderer::instance();
    return reinterpret_cast<ImGuiRenderer *>(ref);
}

ImGuiRenderer *createRenderer(bool defaultRender)
{
    return defaultRender ? ImGuiRenderer::instance() : new ImGuiRenderer;
}

} // namespace

#ifdef QT_WIDGETS_LIB

namespace
{

class QWidgetWindowWrapper : public WindowWrapper
{
public:
    QWidgetWindowWrapper(QWidget *w)
        : w(w)
    {
    }
    void installEventFilter(QObject *object) override
    {
        return w->installEventFilter(object);
    }
    QSize size() const override
    {
        return w->size();
    }
    qreal devicePixelRatio() const override
    {
        return w->devicePixelRatioF();
    }
    bool isActive() const override
    {
        return w->isActiveWindow();
    }
    QPoint mapFromGlobal(const QPoint &p) const override
    {
        return w->mapFromGlobal(p);
    }
    QObject *object() override
    {
        return w;
    }

    void setCursorShape(Qt::CursorShape shape) override
    {
#ifndef QT_NO_CURSOR
        w->setCursor(shape);
#else
        Q_UNUSED(shape);
#endif
    }

    void setCursorPos(const QPoint &local_pos) override
    {
#ifndef QT_NO_CURSOR
        // Convert position from widget-space into screen-space
        const QPoint global_pos = w->mapToGlobal(local_pos);

        QCursor cursor = w->cursor();
        cursor.setPos(global_pos);
        w->setCursor(cursor);
#else
        Q_UNUSED(local_pos);
#endif
    }

private:
    QWidget *w;
};

} // namespace

RenderRef initialize(QWidget *window, bool defaultRender)
{
    // ImGui/ImPlot text fields need keyboard events, but a QWidget hosting an ImGui
    // canvas is often a QOpenGLWidget which defaults to Qt::NoFocus and so never
    // receives key events. Make the host focusable here so every QtImGui canvas
    // gets working keyboard input by default.
    if (window->focusPolicy() == Qt::NoFocus)
        window->setFocusPolicy(Qt::StrongFocus);

    auto *renderer = createRenderer(defaultRender);
    renderer->initialize(std::make_unique<QWidgetWindowWrapper>(window));
    return reinterpret_cast<RenderRef>(renderer);
}

#endif // QT_WIDGETS_LIB

namespace
{

class QWindowWindowWrapper : public WindowWrapper
{
public:
    QWindowWindowWrapper(QWindow *w)
        : w(w)
    {
    }
    void installEventFilter(QObject *object) override
    {
        return w->installEventFilter(object);
    }
    QSize size() const override
    {
        return w->size();
    }
    qreal devicePixelRatio() const override
    {
        return w->devicePixelRatio();
    }
    bool isActive() const override
    {
        return w->isActive();
    }
    QPoint mapFromGlobal(const QPoint &p) const override
    {
        return w->mapFromGlobal(p);
    }
    QObject *object() override
    {
        return w;
    }

    void setCursorShape(Qt::CursorShape shape) override
    {
#ifndef QT_NO_CURSOR
        w->setCursor(shape);
#else
        Q_UNUSED(shape);
#endif
    }

    void setCursorPos(const QPoint &local_pos) override
    {
#ifndef QT_NO_CURSOR
        // Convert position from window-space into screen-space
        const QPoint global_pos = w->mapToGlobal(local_pos);

        QCursor cursor = w->cursor();
        cursor.setPos(global_pos);
        w->setCursor(cursor);
#else
        Q_UNUSED(local_pos);
#endif
    }

private:
    QWindow *w;
};

} // namespace

RenderRef initialize(QWindow *window, bool defaultRender)
{
    auto *renderer = createRenderer(defaultRender);
    renderer->initialize(std::make_unique<QWindowWindowWrapper>(window));
    return reinterpret_cast<RenderRef>(renderer);
}

void newFrame(RenderRef ref)
{
    rendererFor(ref)->newFrame();
}

void render(RenderRef ref)
{
    rendererFor(ref)->render();
}

void notifyContextRecreated(RenderRef ref)
{
    rendererFor(ref)->notifyContextRecreated();
}

void destroy(RenderRef ref)
{
    if (ref == nullptr)
        return;

    // the shared renderer is not ours to delete
    auto *renderer = reinterpret_cast<ImGuiRenderer *>(ref);
    if (renderer->isShared())
        return;

    delete renderer;
}

} // namespace QtImGui

#pragma once

class QWidget;
class QWindow;

namespace QtImGui
{

/**
 * Opaque handle to the renderer driving one window, obtained from initialize().
 * A null reference addresses the process-wide shared renderer.
 */
typedef void *RenderRef;

/**
 * Set up ImGui rendering for a window and return a handle to it.
 *
 * With @p defaultRender the shared renderer is used, which can only ever serve
 * one window at a time. Pass false to get a renderer of your own, which you own
 * and are expected to release with destroy().
 */
#ifdef QT_WIDGETS_LIB
RenderRef initialize(QWidget *window, bool defaultRender = true);
#endif

RenderRef initialize(QWindow *window, bool defaultRender = true);
void newFrame(RenderRef ref = nullptr);
void render(RenderRef ref = nullptr);

/**
 * Announce that the OpenGL context of the host widget was destroyed and recreated,
 * as can happen when the widget is reparented or its window is rebuilt after a
 * screen configuration change. Call this instead of initialize() in that case:
 * the GPU-side objects are rebuilt on the next frame, all ImGui state is kept.
 */
void notifyContextRecreated(RenderRef ref = nullptr);

/**
 * Release the renderer behind @p ref, which must not be used afterwards.
 * Does nothing for the shared renderer or for a null reference.
 */
void destroy(RenderRef ref);

} // namespace QtImGui

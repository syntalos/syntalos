#pragma once

class QWidget;
class QWindow;

namespace QtImGui
{

typedef void *RenderRef;

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

} // namespace QtImGui

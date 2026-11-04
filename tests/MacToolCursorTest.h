#pragma once

class QQuickWindow;
class Document;
class QPointF;

bool runMacToolCursorTest(QQuickWindow *_window);
bool runMacPenInputTest(QQuickWindow *_window, Document &_document);
bool macTextCursorIsCurrent();
void activateMacTestWindow(QQuickWindow *window);
void sendMacTestMouseButton(QQuickWindow *_window, const QPointF &_scene, bool _down);

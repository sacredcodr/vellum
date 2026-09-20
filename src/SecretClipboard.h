#pragma once
#include <QTimer>
#include <QString>
#include <windows.h>

class SecretClipboard
{
public:
    SecretClipboard();
    ~SecretClipboard();
    void copy(const QString& secret, HWND owner);
    void clear();
private:
    QTimer m_timer;
    DWORD m_sequence = 0;
};

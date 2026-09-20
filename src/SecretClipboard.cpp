#include "SecretClipboard.h"
#include <cstring>
#include <stdexcept>

SecretClipboard::SecretClipboard()
{
    m_timer.setSingleShot(true);
    QObject::connect(&m_timer, &QTimer::timeout, [&] { clear(); });
}
SecretClipboard::~SecretClipboard() { clear(); }
void SecretClipboard::copy(const QString& secret, HWND owner)
{
    if (!OpenClipboard(owner)) throw std::runtime_error("Clipboard is busy. Try again.");
    if (!EmptyClipboard()) { CloseClipboard(); throw std::runtime_error("Cannot take clipboard ownership."); }
    bool success = true;
    const auto bytes = (secret.size() + 1) * sizeof(wchar_t);
    HGLOBAL text = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!text) success = false;
    else
    {
        void* data = GlobalLock(text);
        if (!data) { GlobalFree(text); success = false; }
        else
        {
            std::memcpy(data, secret.utf16(), secret.size() * sizeof(wchar_t));
            GlobalUnlock(text);
            if (!SetClipboardData(CF_UNICODETEXT, text)) { GlobalFree(text); success = false; }
        }
    }
    for (const auto* name : {L"CanIncludeInClipboardHistory", L"CanUploadToCloudClipboard", L"ExcludeClipboardContentFromMonitorProcessing"})
    {
        const UINT format = RegisterClipboardFormatW(name);
        HGLOBAL value = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD));
        if (!format || !value) { if (value) GlobalFree(value); success = false; continue; }
        if (!SetClipboardData(format, value)) { GlobalFree(value); success = false; }
    }
    if (!success) EmptyClipboard();
    m_sequence = success ? GetClipboardSequenceNumber() : 0;
    CloseClipboard();
    if (!success) throw std::runtime_error("Could not copy with clipboard privacy flags; nothing was copied.");
    m_timer.start(20000);
}
void SecretClipboard::clear()
{
    m_timer.stop();
    if (!m_sequence) return;
    if (GetClipboardSequenceNumber() != m_sequence) { m_sequence = 0; return; }
    if (!OpenClipboard(nullptr)) { m_timer.start(1000); return; }
    if (GetClipboardSequenceNumber() == m_sequence) EmptyClipboard();
    CloseClipboard();
    m_sequence = 0;
}

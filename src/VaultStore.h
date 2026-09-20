#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QLockFile>
#include <QString>
#include <memory>

class VaultStore
{
public:
    VaultStore();
    ~VaultStore();
    VaultStore(const VaultStore&) = delete;
    VaultStore& operator=(const VaultStore&) = delete;
    void create(const QString& path, QByteArray& passphrase);
    void open(const QString& path, QByteArray& passphrase);
    void save(const QJsonArray& entries);
    void backup(const QString& destination);
    void lock();
    bool unlocked() const { return m_key != nullptr; }
    const QJsonArray& entries() const { return m_entries; }
    const QString& path() const { return m_path; }
    static QString generatePassword(int length = 24);
    static QString generatePassphrase();
private:
    void acquireFile(const QString& path);
    void derive(QByteArray& passphrase);
    QByteArray encrypt(const QJsonArray& entries) const;
    static void validate(const QJsonArray& entries);
    unsigned char* m_key = nullptr;
    QByteArray m_salt;
    QByteArray m_fingerprint;
    QString m_path;
    QJsonArray m_entries;
    std::unique_ptr<QLockFile> m_fileLock;
};

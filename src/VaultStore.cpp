#include "PassphraseWords.h"
#include "VaultStore.h"
#include "PassphrasePolicy.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <sodium.h>
#include <stdexcept>

namespace
{
constexpr qsizetype kHeaderSize = 64;
constexpr qsizetype kMaxFileSize = 16 * 1024 * 1024;
constexpr quint32 kPasses = 3;
constexpr quint64 kMemory = 256ULL * 1024 * 1024;
const QByteArray kMagic("PVLT0001", 8);
void fail(const char* message) { throw std::runtime_error(message); }
struct WipeBytes
{
    QByteArray& bytes;
    ~WipeBytes() { if (!bytes.isEmpty()) sodium_memzero(bytes.data(), static_cast<size_t>(bytes.size())); bytes.clear(); }
};
QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) fail("Cannot read the vault file.");
    if (file.size() < kHeaderSize + crypto_aead_xchacha20poly1305_ietf_ABYTES || file.size() > kMaxFileSize) fail("Invalid or oversized vault file.");
    const QByteArray data = file.read(kMaxFileSize + 1);
    if (data.size() != file.size() || data.size() > kMaxFileSize) fail("Could not read the complete vault file.");
    return data;
}
void writeAtomic(const QString& path, const QByteArray& data)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) fail("Save failed. The previous vault file was not replaced.");
}
}

VaultStore::VaultStore()
{
    if (sodium_init() < 0) fail("Cryptographic library initialization failed.");
}
VaultStore::~VaultStore() { lock(); }
void VaultStore::lock()
{
    m_entries = {};
    if (m_key) { sodium_free(m_key); m_key = nullptr; }
    m_salt.clear();
    m_fingerprint.clear();
    m_fileLock.reset();
}
void VaultStore::acquireFile(const QString& path)
{
    lock();
    m_path = QFileInfo(path).absoluteFilePath();
    m_fileLock = std::make_unique<QLockFile>(m_path + ".lock");
    m_fileLock->setStaleLockTime(0);
    if (!m_fileLock->tryLock(0)) { m_fileLock.reset(); fail("This vault is open elsewhere, or its lock file cannot be created."); }
}
void VaultStore::derive(QByteArray& passphrase)
{
    m_key = static_cast<unsigned char*>(sodium_malloc(crypto_aead_xchacha20poly1305_ietf_KEYBYTES));
    if (!m_key) fail("Cannot allocate protected key memory.");
    if (sodium_mlock(m_key, crypto_aead_xchacha20poly1305_ietf_KEYBYTES) != 0) fail("Cannot lock key memory; unlocking was stopped.");
    if (crypto_pwhash(m_key, crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
        passphrase.constData(), static_cast<unsigned long long>(passphrase.size()),
        reinterpret_cast<const unsigned char*>(m_salt.constData()), kPasses, kMemory, crypto_pwhash_ALG_ARGON2ID13) != 0)
        fail("Key derivation failed; enough memory may not be available.");
}
void VaultStore::validate(const QJsonArray& entries)
{
    if (entries.size() > 10000) fail("Too many vault entries.");
    QSet<QString> ids;
    for (const auto& value : entries)
    {
        if (!value.isObject()) fail("Invalid entry format.");
        const auto entry = value.toObject();
        if (!entry["id"].isString() || entry["id"].toString().isEmpty() || ids.contains(entry["id"].toString())) fail("Invalid entry identifier.");
        ids.insert(entry["id"].toString());
        if (entry["type"] != "note" && entry["type"] != "login") fail("Unknown entry type.");
        for (const auto* field : {"title", "username", "url", "password", "notes", "updated"})
        {
            if (!entry[field].isString() || entry[field].toString().size() > 1024 * 1024) fail("Invalid entry field.");
        }
        if (entry["title"].toString().size() > 512 || entry["id"].toString().size() > 64) fail("Entry metadata is too long.");
    }
}
QByteArray VaultStore::encrypt(const QJsonArray& entries) const
{
    validate(entries);
    QByteArray plaintext = QJsonDocument(QJsonObject{{"version", 1}, {"entries", entries}}).toJson(QJsonDocument::Compact);
    WipeBytes wipe{plaintext};
    if (plaintext.size() > kMaxFileSize - kHeaderSize - crypto_aead_xchacha20poly1305_ietf_ABYTES) fail("Vault has reached the 16 MiB size limit.");
    QByteArray nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, '\0');
    randombytes_buf(nonce.data(), static_cast<size_t>(nonce.size()));
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData(kMagic.constData(), 8);
    stream << quint32(1) << kPasses << kMemory;
    stream.writeRawData(m_salt.constData(), m_salt.size());
    stream.writeRawData(nonce.constData(), nonce.size());
    QByteArray ciphertext(plaintext.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES, '\0');
    unsigned long long length = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(reinterpret_cast<unsigned char*>(ciphertext.data()), &length,
        reinterpret_cast<const unsigned char*>(plaintext.constData()), plaintext.size(),
        reinterpret_cast<const unsigned char*>(header.constData()), header.size(), nullptr,
        reinterpret_cast<const unsigned char*>(nonce.constData()), m_key);
    ciphertext.resize(static_cast<qsizetype>(length));
    return header + ciphertext;
}
void VaultStore::create(const QString& path, QByteArray& passphrase)
{
    WipeBytes wipe{passphrase};
    const auto policyError = PassphrasePolicy::error(QString::fromUtf8(passphrase));
    if (!policyError.isEmpty()) throw std::runtime_error(policyError.toStdString());
    acquireFile(path);
    bool reserved = false;
    try
    {
        QFile reservation(m_path);
        if (!reservation.open(QIODevice::WriteOnly | QIODevice::NewOnly)) fail("Choose a new file. Existing files are never overwritten when creating a vault.");
        reserved = true;
        reservation.close();
        m_salt.resize(crypto_pwhash_SALTBYTES);
        randombytes_buf(m_salt.data(), static_cast<size_t>(m_salt.size()));
        derive(passphrase);
        const QByteArray data = encrypt({});
        writeAtomic(m_path, data);
        reserved = false;
        m_fingerprint = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    }
    catch (...) { if (reserved) QFile::remove(m_path); lock(); throw; }
}
void VaultStore::open(const QString& path, QByteArray& passphrase)
{
    WipeBytes wipe{passphrase};
    if (passphrase.isEmpty() || passphrase.size() > 1024) fail("Enter your master passphrase.");
    acquireFile(path);
    try
    {
        const QByteArray data = readFile(m_path);
        if (data.first(8) != kMagic) fail("Not a supported Vellum file.");
        QDataStream stream(data.mid(8, 16)); stream.setByteOrder(QDataStream::LittleEndian);
        quint32 version = 0, passes = 0; quint64 memory = 0;
        stream >> version >> passes >> memory;
        if (version != 1 || passes != kPasses || memory != kMemory) fail("Unsupported or altered vault parameters.");
        m_salt = data.mid(24, crypto_pwhash_SALTBYTES);
        derive(passphrase);
        QByteArray plaintext(data.size() - kHeaderSize - crypto_aead_xchacha20poly1305_ietf_ABYTES, '\0');
        WipeBytes plaintextWipe{plaintext};
        unsigned long long length = 0;
        if (crypto_aead_xchacha20poly1305_ietf_decrypt(reinterpret_cast<unsigned char*>(plaintext.data()), &length, nullptr,
            reinterpret_cast<const unsigned char*>(data.constData() + kHeaderSize), data.size() - kHeaderSize,
            reinterpret_cast<const unsigned char*>(data.constData()), kHeaderSize,
            reinterpret_cast<const unsigned char*>(data.constData() + 40), m_key) != 0) fail("Wrong passphrase or damaged vault. Nothing was opened.");
        plaintext.resize(static_cast<qsizetype>(length));
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(plaintext, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject() || document["version"] != 1 || !document["entries"].isArray()) fail("Invalid encrypted vault contents.");
        const auto entries = document["entries"].toArray();
        validate(entries);
        m_entries = entries;
        m_fingerprint = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    }
    catch (...) { lock(); throw; }
}
void VaultStore::save(const QJsonArray& entries)
{
    if (!unlocked()) fail("Unlock the vault first.");
    const auto current = readFile(m_path);
    if (QCryptographicHash::hash(current, QCryptographicHash::Sha256) != m_fingerprint) fail("The vault file changed outside this app. Save stopped to avoid overwriting it.");
    const auto data = encrypt(entries);
    writeAtomic(m_path, data);
    m_fingerprint = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    m_entries = entries;
}
void VaultStore::backup(const QString& destination)
{
    if (!unlocked()) fail("Unlock the vault first.");
    if (QFileInfo(destination).absoluteFilePath().compare(m_path, Qt::CaseInsensitive) == 0) fail("Choose a different backup file.");
    const auto data = readFile(m_path);
    if (QCryptographicHash::hash(data, QCryptographicHash::Sha256) != m_fingerprint) fail("The vault file changed outside this app. Backup stopped.");
    QFile output(destination);
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) fail("Choose a new backup filename; backups are never overwritten.");
    if (output.write(data) != data.size() || !output.flush()) { output.close(); output.remove(); fail("Could not finish the encrypted backup."); }
}
QString VaultStore::generatePassword(int length)
{
    if (length < 16 || length > 128) fail("Password length must be between 16 and 128.");
    constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_=+";
    QString result; result.reserve(length);
    for (int i = 0; i < length; ++i) result += QLatin1Char(alphabet[randombytes_uniform(sizeof(alphabet) - 1)]);
    return result;
}

QString VaultStore::generatePassphrase()
{
    for (;;)
    {
        QString result;
        for (int index = 0; index < 7; ++index)
        {
            if (index > 0)
            {
                result += ' ';
            }
            result += QLatin1String(kPassphraseWords[randombytes_uniform(static_cast<uint32_t>(kPassphraseWords.size()))]);
        }
        if (PassphrasePolicy::error(result).isEmpty())
        {
            return result;
        }
    }
}

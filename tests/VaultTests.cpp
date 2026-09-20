#include "VaultStore.h"
#include "RecoveryCopy.h"
#include "PassphrasePolicy.h"
#include "PassphraseWords.h"
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <windows.h>
#include <aclapi.h>
#include <vector>

class VaultTests : public QObject
{
    Q_OBJECT
private slots:
    void encryptedRoundTripAndBackup();
    void rejectsTamperingAndWrongKeys();
    void preservesOriginalAndRejectsConcurrentWriters();
    void randomPasswords();
    void passphrasePolicy();
    void masterPassphraseAndRecovery();
};
QJsonArray sample()
{
    return {QJsonObject{{"id", "fixture-1"}, {"type", "login"}, {"title", "PRIVATE-TITLE-marker"},
        {"username", "PRIVATE-USERNAME-marker"}, {"url", "https://example.invalid"}, {"password", "PRIVATE-PASSWORD-marker"},
        {"notes", "PRIVATE-NOTE-marker"}, {"updated", "2026-09-20"}}};
}
QByteArray passphrase() { return "Quartz!7River-Cobalt9Maple"; }
QByteArray read(const QString& path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
void write(const QString& path, const QByteArray& data) { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(data), data.size()); }
void VaultTests::encryptedRoundTripAndBackup()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("test.vault");
    VaultStore store;
    auto password = passphrase(); store.create(path, password);
    QVERIFY(password.isEmpty());
    store.save(sample());
    const auto first = read(path);
    QVERIFY(!first.contains("PRIVATE-")); QVERIFY(!first.contains("example.invalid"));
    store.save(sample()); QVERIFY(first != read(path));
    store.backup(directory.filePath("backup.vault"));
    QCOMPARE(read(path), read(directory.filePath("backup.vault")));
    QVERIFY_EXCEPTION_THROWN(store.backup(path), std::runtime_error);
    store.lock(); QVERIFY(!store.unlocked()); QVERIFY(store.entries().isEmpty());
    password = passphrase(); store.open(directory.filePath("backup.vault"), password);
    QCOMPARE(store.entries(), sample()); QVERIFY(password.isEmpty());
}
void VaultTests::rejectsTamperingAndWrongKeys()
{
    QTemporaryDir directory; const auto path = directory.filePath("test.vault");
    VaultStore store; auto password = passphrase(); store.create(path, password); store.save(sample()); store.lock();
    const auto original = read(path);
    password = "incorrect passphrase";
    QVERIFY_EXCEPTION_THROWN(store.open(path, password), std::runtime_error);
    QVERIFY(password.isEmpty()); QVERIFY(!store.unlocked());
    for (const int offset : {8, 12, 16, 24, 40, 70})
    {
        auto changed = original; changed[offset] = char(changed[offset] ^ 1); write(path, changed);
        password = passphrase(); QVERIFY_EXCEPTION_THROWN(store.open(path, password), std::runtime_error);
        QVERIFY(!store.unlocked()); QVERIFY(store.entries().isEmpty());
    }
    write(path, original.first(70)); password = passphrase();
    QVERIFY_EXCEPTION_THROWN(store.open(path, password), std::runtime_error);
    write(path, original + "extra"); password = passphrase();
    QVERIFY_EXCEPTION_THROWN(store.open(path, password), std::runtime_error);
}
void VaultTests::preservesOriginalAndRejectsConcurrentWriters()
{
    QTemporaryDir directory; const auto path = directory.filePath("test.vault");
    VaultStore first; auto password = passphrase(); first.create(path, password); first.save(sample());
    const auto original = read(path);
    VaultStore second; password = passphrase();
    QVERIFY_EXCEPTION_THROWN(second.open(path, password), std::runtime_error);
    QVERIFY(password.isEmpty());
    auto invalid = sample(); invalid.append(sample().first());
    QVERIFY_EXCEPTION_THROWN(first.save(invalid), std::runtime_error); QCOMPARE(read(path), original);
    auto external = original; external[70] = char(external[70] ^ 1); write(path, external);
    QVERIFY_EXCEPTION_THROWN(first.save(sample()), std::runtime_error); QCOMPARE(read(path), external);
    first.lock(); password = passphrase();
    QVERIFY_EXCEPTION_THROWN(second.create(path, password), std::runtime_error); QCOMPARE(read(path), external);
}
void VaultTests::randomPasswords()
{
    VaultStore initialize;
    QSet<QString> values;
    for (int i = 0; i < 100; ++i) { const auto value = VaultStore::generatePassword(); QCOMPARE(value.size(), 24); values.insert(value); }
    QCOMPARE(values.size(), 100);
    QVERIFY_EXCEPTION_THROWN(VaultStore::generatePassword(0), std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(VaultStore::generatePassword(129), std::runtime_error);
}
void VaultTests::passphrasePolicy()
{
    QVERIFY(!PassphrasePolicy::error("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").isEmpty());
    QVERIFY(!PassphrasePolicy::error("PasswordPasswordPassword123!").isEmpty());
    QVERIFY(!PassphrasePolicy::error("abcdefghijklmnopqrstuvwxyz").isEmpty());
    QVERIFY(PassphrasePolicy::error("Quartz!7River-Cobalt9Maple").isEmpty());
    QVERIFY(PassphrasePolicy::error("harbor violet lantern meadow copper summit").isEmpty());

    QTemporaryDir directory;
    auto weak = QByteArray("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    VaultStore store;
    QVERIFY_EXCEPTION_THROWN(store.create(directory.filePath("weak.vault"), weak), std::runtime_error);
    QVERIFY(!QFile::exists(directory.filePath("weak.vault")));
}
void VaultTests::masterPassphraseAndRecovery()
{
    VaultStore store;
    QSet<QString> dictionary;
    for (const auto* word : kPassphraseWords)
    {
        dictionary.insert(QString::fromLatin1(word));
    }
    QCOMPARE(dictionary.size(), 7776);
    QSet<QString> phrases;
    for (int index = 0; index < 100; ++index)
    {
        const auto phrase = VaultStore::generatePassphrase();
        const auto words = phrase.split(' ');
        QCOMPARE(words.size(), 7);
        for (const auto& word : words)
        {
            QVERIFY(dictionary.contains(word));
        }
        QVERIFY(PassphrasePolicy::error(phrase).isEmpty());
        phrases.insert(phrase);
    }
    QCOMPARE(phrases.size(), 100);
    QTemporaryDir directory;
    const auto phrase = VaultStore::generatePassphrase();
    const auto recovery = directory.filePath("recovery.txt");
    RecoveryCopy::save(recovery, phrase);
    const auto contents = read(recovery);
    QVERIFY(contents.contains("NOT ENCRYPTED"));
    QVERIFY(contents.endsWith(phrase.toUtf8() + "\n"));

    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PACL acl = nullptr;
    auto nativeRecovery = QDir::toNativeSeparators(recovery).toStdWString();
    QCOMPARE(GetNamedSecurityInfoW(nativeRecovery.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &acl, nullptr, &descriptor), DWORD(ERROR_SUCCESS));
    ACL_SIZE_INFORMATION aclInfo{};
    QVERIFY(GetAclInformation(acl, &aclInfo, sizeof(aclInfo), AclSizeInformation));
    QCOMPARE(aclInfo.AceCount, DWORD(1));
    void* rawAce = nullptr;
    QVERIFY(GetAce(acl, 0, &rawAce));
    auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(rawAce);
    QCOMPARE(ace->Header.AceType, BYTE(ACCESS_ALLOWED_ACE_TYPE));
    HANDLE rawToken = nullptr;
    QVERIFY(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken));
    DWORD tokenSize = 0;
    GetTokenInformation(rawToken, TokenUser, nullptr, 0, &tokenSize);
    std::vector<std::byte> tokenBuffer(tokenSize);
    QVERIFY(GetTokenInformation(rawToken, TokenUser, tokenBuffer.data(), tokenSize, &tokenSize));
    auto* tokenUser = static_cast<TOKEN_USER*>(static_cast<void*>(tokenBuffer.data()));
    QVERIFY(EqualSid(&ace->SidStart, tokenUser->User.Sid));
    CloseHandle(rawToken);
    if (descriptor) LocalFree(descriptor);
    QVERIFY_EXCEPTION_THROWN(RecoveryCopy::save(recovery, "different passphrase for testing"), std::runtime_error);
    QCOMPARE(read(recovery), contents);
    QVERIFY_EXCEPTION_THROWN(RecoveryCopy::save(directory.filePath("invalid.txt"), "short"), std::runtime_error);
    QVERIFY(!QFile::exists(directory.filePath("invalid.txt")));
    auto key = phrase.toUtf8();
    const auto vault = directory.filePath("generated.vault");
    store.create(vault, key);
    store.lock();
    key = contents.split('\n').at(contents.split('\n').size() - 2);
    store.open(vault, key);
    QVERIFY(store.unlocked());
}
QTEST_GUILESS_MAIN(VaultTests)
#include "VaultTests.moc"

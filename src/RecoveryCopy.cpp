#include "RecoveryCopy.h"
#include "PassphrasePolicy.h"
#include <QDir>
#include <QFileInfo>
#include <sodium.h>
#include <windows.h>
#include <aclapi.h>
#include <stdexcept>
#include <vector>

namespace
{
class LocalMemory final
{
public:
    ~LocalMemory() { if (m_value) LocalFree(m_value); }
    void** address() { return &m_value; }
    void* value() const { return m_value; }
private:
    void* m_value = nullptr;
};

class Handle final
{
public:
    Handle() = default;
    explicit Handle(HANDLE value) : m_value(value) {}
    ~Handle() { close(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : m_value(other.m_value) { other.m_value = INVALID_HANDLE_VALUE; }
    Handle& operator=(Handle&& other) noexcept
    {
        if (this != &other)
        {
            close();
            m_value = other.m_value;
            other.m_value = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    HANDLE value() const { return m_value; }
    void close()
    {
        if (m_value != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_value);
            m_value = INVALID_HANDLE_VALUE;
        }
    }
private:
    HANDLE m_value = INVALID_HANDLE_VALUE;
};

std::wstring volumePath(const QString& destination)
{
    std::vector<wchar_t> buffer(MAX_PATH + 1);
    const auto nativePath = QDir::toNativeSeparators(QFileInfo(destination).absoluteFilePath()).toStdWString();
    if (!GetVolumePathNameW(nativePath.c_str(), buffer.data(), static_cast<DWORD>(buffer.size())))
    {
        throw std::runtime_error("The recovery destination could not be verified.");
    }
    return buffer.data();
}

void requirePersistentAcls(const QString& destination)
{
    DWORD flags = 0;
    const auto root = volumePath(destination);
    if (!GetVolumeInformationW(root.c_str(), nullptr, 0, nullptr, nullptr, &flags, nullptr, 0) || !(flags & FILE_PERSISTENT_ACLS))
    {
        throw std::runtime_error("Choose an NTFS or ReFS destination so Windows can restrict the recovery file to your account.");
    }
}

PACL privateAcl(std::vector<std::byte>& tokenBuffer)
{
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken))
    {
        throw std::runtime_error("Windows could not read the current account identity.");
    }
    Handle token(rawToken);
    DWORD tokenSize = 0;
    GetTokenInformation(token.value(), TokenUser, nullptr, 0, &tokenSize);
    tokenBuffer.resize(tokenSize);
    if (!GetTokenInformation(token.value(), TokenUser, tokenBuffer.data(), tokenSize, &tokenSize))
    {
        throw std::runtime_error("Windows could not read the current account identity.");
    }

    EXPLICIT_ACCESSW access{};
    access.grfAccessPermissions = FILE_ALL_ACCESS;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance = NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_USER;
    access.Trustee.ptstrName = static_cast<LPWSTR>(static_cast<TOKEN_USER*>(static_cast<void*>(tokenBuffer.data()))->User.Sid);
    PACL acl = nullptr;
    if (SetEntriesInAclW(1, &access, nullptr, &acl) != ERROR_SUCCESS)
    {
        throw std::runtime_error("Windows could not create private recovery-file permissions.");
    }
    return acl;
}
}

void RecoveryCopy::save(const QString& destination, const QString& passphrase)
{
    const auto policyError = PassphrasePolicy::error(passphrase);
    if (!policyError.isEmpty())
    {
        throw std::runtime_error(policyError.toStdString());
    }
    requirePersistentAcls(destination);
    std::vector<std::byte> tokenBuffer;
    LocalMemory aclOwner;
    *aclOwner.address() = privateAcl(tokenBuffer);
    SECURITY_DESCRIPTOR descriptor{};
    if (!InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(&descriptor, TRUE, static_cast<PACL>(aclOwner.value()), FALSE) ||
        !SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
    {
        throw std::runtime_error("Windows could not create private recovery-file permissions.");
    }
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), &descriptor, FALSE};
    const auto nativePath = QDir::toNativeSeparators(QFileInfo(destination).absoluteFilePath()).toStdWString();
    Handle file(CreateFileW(nativePath.c_str(), GENERIC_WRITE, 0, &attributes, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
    if (file.value() == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Choose a new recovery filename. Existing files are never overwritten.");
    }
    QByteArray content("VELLUM RECOVERY COPY - NOT ENCRYPTED\n\nAnyone with this passphrase and your vault file can unlock it.\nKeep this copy separate from your vault and backups.\n\nMaster passphrase:\n");
    content += passphrase.toUtf8();
    content += "\n";
    DWORD written = 0;
    const bool complete = WriteFile(file.value(), content.constData(), static_cast<DWORD>(content.size()), &written, nullptr) &&
        written == static_cast<DWORD>(content.size()) && FlushFileBuffers(file.value());
    sodium_memzero(content.data(), static_cast<size_t>(content.size()));
    if (!complete)
    {
        const auto path = nativePath;
        file.close();
        if (!DeleteFileW(path.c_str()))
        {
            throw std::runtime_error("The recovery copy was incomplete and Windows could not remove it. Delete the selected file before retrying.");
        }
        throw std::runtime_error("The recovery copy could not be saved completely; the incomplete file was removed.");
    }
}

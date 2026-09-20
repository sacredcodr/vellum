#include "RecoveryCopy.h"
#include <QFile>
#include <sodium.h>
#include <stdexcept>

void RecoveryCopy::save(const QString& destination, const QString& passphrase)
{
    if (passphrase.size() < 16 || passphrase.size() > 512)
    {
        throw std::runtime_error("Choose a master passphrase of 16 to 512 characters first.");
    }
    QFile file(destination);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
    {
        throw std::runtime_error("Choose a new recovery filename. Existing files are never overwritten.");
    }
    QByteArray content("VELLUM RECOVERY COPY - NOT ENCRYPTED\n\nAnyone with this passphrase and your vault file can unlock it.\nKeep this copy separate from your vault and backups.\n\nMaster passphrase:\n");
    content += passphrase.toUtf8();
    content += "\n";
    const bool written = file.write(content) == content.size() && file.flush();
    sodium_memzero(content.data(), static_cast<size_t>(content.size()));
    file.close();
    if (!written)
    {
        file.remove();
        throw std::runtime_error("The recovery copy could not be saved completely.");
    }
}

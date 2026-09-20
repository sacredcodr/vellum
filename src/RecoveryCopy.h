#pragma once
#include <QString>

namespace RecoveryCopy
{
// Explicit plaintext export. Never call automatically or overwrite an existing file.
void save(const QString& destination, const QString& passphrase);
}

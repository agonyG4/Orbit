#pragma once

#include <memory>

#include <QString>

namespace Astrea::Explorer::Native::Parity {

class FixtureTree final
{
public:
    static std::unique_ptr<FixtureTree> create();

    ~FixtureTree();

    QString rootPath() const;
    QString nestedPath() const;
    QString devicePath() const;
    QString operationsPath() const;
    QString recentsPath() const;

private:
    FixtureTree();
    class Private;
    std::unique_ptr<Private> m_private;
};

} // namespace Astrea::Explorer::Native::Parity

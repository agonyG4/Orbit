#pragma once

class QQmlApplicationEngine;

class ExplorerApplication final
{
public:
    int run(int argc, char **argv);

private:
    bool loadBootstrap(QQmlApplicationEngine &engine) const;
    int runSelfTest(QQmlApplicationEngine &engine) const;
};

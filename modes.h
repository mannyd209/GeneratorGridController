#ifndef MODES_H
#define MODES_H

enum SystemMode {
    MODE_OFF,
    MODE_AUTO,
    MODE_MANUAL
};

enum GeneratorState {
    GEN_OFF,
    GEN_STARTING,
    GEN_RUNNING,
    GEN_STOPPING
};

#endif

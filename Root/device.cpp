#include "device.h"

Device::Device(QObject *parent)
    : QObject(parent),
    deviceState(offline)
{}
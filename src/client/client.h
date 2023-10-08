#pragma once

#include "../../lib/networking_ops.h"

#include <iostream>

class Client{
public:
    explicit Client(const char* hostname, const char* port);
};
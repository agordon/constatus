#pragma once

#include <mutex>
#include <vector>

#include "interface.h"

typedef struct
{
	std::vector<interface *> interfaces;
	std::string name;
} instance_t;

typedef struct
{
	std::vector<instance_t *> instances;

	std::mutex lock;
} configuration_t;

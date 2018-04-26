// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <mutex>
#include <vector>

#include "interface.h"
#include "source.h"

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

class source;

void find_by_id(const configuration_t *const cfg, const std::string & id, instance_t **inst, interface **i);
source *find_source(instance_t *const inst);
instance_t *find_instance_by_interface(const configuration_t *const cfg, const interface *i_f);
instance_t *find_instance_by_name(const configuration_t *const cfg, const std::string & name);
interface *find_by_id(instance_t *const inst, const std::string & id);
interface *find_by_id(configuration_t *const cfg, const std::string & id);
bool check_for_motion(instance_t *const inst);

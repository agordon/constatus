// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <string>
#include <vector>

#include "cfg.h"
#include "motion_trigger.h"

void find_by_id(const configuration_t *const cfg, const std::string & id, instance_t **inst, interface **i)
{
	*inst = NULL;
	*i = NULL;

	for(instance_t * cur_inst : cfg -> instances) {
		for(interface *cur_i : cur_inst -> interfaces) {
			if (cur_i -> get_id() == id) {
				*inst = cur_inst;
				*i = cur_i;
			}
		}
	}
}

source *find_source(instance_t *const inst)
{
	for(interface *i : inst -> interfaces) {
		if (i -> get_class_type() == CT_SOURCE)
			return (source *)i;
	}

	return NULL;
}

instance_t *find_instance_by_interface(const configuration_t *const cfg, const interface *i_f)
{
	for(instance_t * inst : cfg -> instances) {
		for(interface *cur : inst -> interfaces) {
			if (cur == i_f)
				return inst;
		}
	}

	return NULL;
}

instance_t *find_instance_by_name(const configuration_t *const cfg, const std::string & name)
{
	for(instance_t * inst : cfg -> instances) {
		if (inst -> name == name)
			return inst;
	}

	return NULL;
}

interface *find_by_id(instance_t *const inst, const std::string & id)
{
	if (!inst)
		return NULL;

	for(interface *i : inst -> interfaces) {
		std::string id_int = i -> get_id();

		if (id == id_int)
			return i;
	}

	return NULL;
}

interface *find_by_id(configuration_t *const cfg, const std::string & id)
{
	for(instance_t * inst : cfg -> instances) {
		interface *i = find_by_id(inst, id);
		if (i)
			return i;
	}

	return NULL;
}

bool check_for_motion(instance_t *const inst)
{
	for(interface *i : inst -> interfaces) {
		if (((motion_trigger *)i) -> check_motion())
			return true;
	}

	return false;
}

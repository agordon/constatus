// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <jansson.h>
#include <string>

#include "filter.h"
#include "source.h"
#include "target.h"
#include "selection_mask.h"

typedef void * (*init_motion_trigger_t)(const char *const par);
typedef bool (*detect_motion_t)(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap);
typedef void (*uninit_motion_trigger_t)(void *arg);


typedef struct
{
	void *library;

	init_motion_trigger_t init_motion_trigger;
	detect_motion_t detect_motion;
	uninit_motion_trigger_t uninit_motion_trigger;

	std::string par;

	void *arg;
} ext_trigger_t;

typedef enum { V_INT, V_DOUBLE } par_value_t;

typedef struct
{
	pthread_rwlock_t *lock;

	par_value_t value_type;

	int *v_int;
	double *v_double;

	std::string name;
} settable_parameter_t;

class motion_trigger : public interface
{
private:
	source *s;

	std::atomic_bool motion_triggered;

	pthread_rwlock_t noise_level_lock;
	int noise_level;

	pthread_rwlock_t percentage_pixels_changed_lock;
	double percentage_pixels_changed;

	pthread_rwlock_t keep_recording_n_frames_lock;
	int keep_recording_n_frames;

	pthread_rwlock_t ignore_n_frames_after_recording_lock;
	int ignore_n_frames_after_recording;

	int camera_warm_up;

	pthread_rwlock_t pre_record_count_lock;
	int pre_record_count;

	const std::vector<filter *> *const filters;

	pthread_rwlock_t max_fps_lock;
	double max_fps;

	selection_mask *const pixel_select_bitmap;
	ext_trigger_t *const et;
	std::vector<target *> *const targets;
	std::string exec_start, exec_end;

	std::vector<settable_parameter_t> rest_parameters;

	instance_t *const inst;

public:
	motion_trigger(const std::string & id, const std::string & descr, source *const s, const int noise_level, const double percentage_pixels_changed, const int keep_recording_n_frames, const int ignore_n_frames_after_recording, const int camera_warm_up, const int pre_record_count, const std::vector<filter *> *const before, std::vector<target *> *const targets, selection_mask *const pixel_select_bitmap, ext_trigger_t *const et, const double max_fps, const std::string & exec_start, const std::string & exec_end, instance_t *const inst);
	virtual ~motion_trigger();

	json_t * get_rest_settable_parameters();
	json_t * rest_set_parameter(const std::string & key, const std::string & value);

	bool check_motion() { return motion_triggered; }

	void operator()();
};

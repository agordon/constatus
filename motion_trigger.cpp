// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sys/time.h>

#include "error.h"
#include "source.h"
#include "utils.h"
#include "picio.h"
#include "target.h"
#include "filter.h"
#include "log.h"
#include "exec.h"
#include "motion_trigger.h"

motion_trigger::motion_trigger(const std::string & id, const std::string & descr, source *const s, const int noise_level, const double percentage_pixels_changed, const int keep_recording_n_frames, const int ignore_n_frames_after_recording, const int camera_warm_up, const int pre_record_count, const std::vector<filter *> *const filters, std::vector<target *> *const targets, selection_mask *const pixel_select_bitmap, ext_trigger_t *const et, const double max_fps, const std::string & e_start, const std::string & e_end) : interface(id, descr), s(s), noise_level(noise_level), percentage_pixels_changed(percentage_pixels_changed), keep_recording_n_frames(keep_recording_n_frames), ignore_n_frames_after_recording(ignore_n_frames_after_recording), camera_warm_up(camera_warm_up), pre_record_count(pre_record_count), filters(filters), targets(targets), pixel_select_bitmap(pixel_select_bitmap), et(et), max_fps(max_fps), exec_start(e_start), exec_end(e_end)
{
	if (et)
		et -> arg = et -> init_motion_trigger(et -> par.c_str());

	local_stop_flag = false;
	th = NULL;
	ct = CT_MOTIONTRIGGER;

	//// parameters
	noise_level_lock = PTHREAD_RWLOCK_INITIALIZER;
	settable_parameter_t noise_level_r = { &noise_level_lock, V_INT, &this -> noise_level, NULL, "noise_level" };
	rest_parameters.push_back(noise_level_r);

	percentage_pixels_changed_lock = PTHREAD_RWLOCK_INITIALIZER;
	settable_parameter_t ppc_r = { &percentage_pixels_changed_lock, V_DOUBLE, NULL, &this -> percentage_pixels_changed, "percentage_pixels_changed" };
	rest_parameters.push_back(ppc_r);

	keep_recording_n_frames_lock = PTHREAD_RWLOCK_INITIALIZER;
	settable_parameter_t krnf_r = { &keep_recording_n_frames_lock, V_INT, &this -> keep_recording_n_frames, NULL, "keep_recording_n_frames" };
	rest_parameters.push_back(krnf_r);

	ignore_n_frames_after_recording_lock = PTHREAD_RWLOCK_INITIALIZER;
	settable_parameter_t infar_r = { &ignore_n_frames_after_recording_lock, V_INT, &this -> ignore_n_frames_after_recording, NULL, "ignore_n_frames_after_recording" };
	rest_parameters.push_back(infar_r);

	pre_record_count_lock = PTHREAD_RWLOCK_INITIALIZER;
	settable_parameter_t prc_r = { &pre_record_count_lock, V_INT, &this -> pre_record_count, NULL, "pre_record_count" };
	rest_parameters.push_back(prc_r);

	max_fps_lock = PTHREAD_RWLOCK_INITIALIZER;
	settable_parameter_t mf_r = { &max_fps_lock, V_DOUBLE, NULL, &this -> max_fps, "max_fps" };
	rest_parameters.push_back(mf_r);
	///
}

motion_trigger::~motion_trigger()
{
	stop();

	if (et)
		et -> uninit_motion_trigger(et -> arg);

	for(target *t : *targets)
		delete t;
	delete targets;

	delete pixel_select_bitmap;

	free_filters(filters);
	delete filters;
}

void motion_trigger::operator()()
{
	set_thread_name("motion_trigger");

	int w = -1, h = -1;
	uint64_t prev_ts = 0;

	uint8_t *prev_frame = NULL;
	bool motion = false;
	int stopping = 0, mute = 0;

	std::vector<frame_t> prerecord;

	s -> register_user();

	if (camera_warm_up)
		log(LL_INFO, "Warming up...");

	for(int i=0; i<camera_warm_up && !local_stop_flag; i++) {
		log(LL_DEBUG, "Warm-up... %d", i);

		uint8_t *frame = NULL;
		size_t frame_len = 0;
		// FIXME E_NONE ofzo of kijken wat de preferred is
		s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &frame, &frame_len);
		free(frame);
	}

	log(LL_INFO, "Go!");

	for(;!local_stop_flag;) {
		pauseCheck();

		uint8_t *work = NULL;
		size_t work_len = 0;
		if (!s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &work, &work_len)) {
			free(work);
			continue;
		}

		pthread_rwlock_rdlock(&noise_level_lock);
		const int nl3 = noise_level * 3;
		pthread_rwlock_unlock(&noise_level_lock);

		apply_filters(filters, prev_frame, work, prev_ts, w, h);

		if (prev_frame) {
			int cnt = 0;
			bool triggered = false;

			uint8_t *psb = pixel_select_bitmap ? pixel_select_bitmap -> get_mask(w, h) : NULL;

			if (et) {
				triggered = et -> detect_motion(et -> arg, prev_ts, w, h, prev_frame, work, psb);
			}
			else if (psb) {
				const uint8_t *pw = work, *pp = prev_frame;
				for(int i=0; i<w*h; i++) {
					if (!psb[i])
						continue;

					int lc = *pw++;
					lc += *pw++;
					lc += *pw++;

					int lp = *pp++;
					lp += *pp++;
					lp += *pp++;

					cnt += abs(lc - lp) >= nl3;
				}

				pthread_rwlock_rdlock(&percentage_pixels_changed_lock);
				triggered = cnt > (percentage_pixels_changed / 100) * w * h;
				pthread_rwlock_unlock(&percentage_pixels_changed_lock);
			}
			else {
				const uint8_t *pw = work, *pp = prev_frame;
				for(int i=0; i<w*h; i++) {
					int lc = *pw++;
					lc += *pw++;
					lc += *pw++;

					int lp = *pp++;
					lp += *pp++;
					lp += *pp++;

					cnt += abs(lc - lp) >= nl3;
				}

				pthread_rwlock_rdlock(&percentage_pixels_changed_lock);
				triggered = cnt > (percentage_pixels_changed / 100) * w * h;
				pthread_rwlock_unlock(&percentage_pixels_changed_lock);
			}

			get_meta() -> set_double("$pixels-changed$", std::pair<uint64_t, double>(0, cnt * 100.0 / (w * h)));

			if (mute) {
				log(LL_DEBUG, "mute");
				mute--;
			}
			else if (triggered) {
				log(LL_INFO, "motion detected (%f%% of the pixels changed)", cnt * 100.0 / (w * h));

				if (!motion) {
					log(LL_DEBUG, " starting store");

					std::vector<frame_t> *pr = new std::vector<frame_t>(prerecord);
					prerecord.clear();

					for(size_t i=0; i<targets -> size(); i++)
						targets -> at(i) -> start(i == 0 ? pr : NULL); // FIXME for all targets

					if (targets -> empty())
						delete pr;

					if (!exec_start.empty())
						exec(exec_start, "");

					motion = true;
				}

				stopping = 0;
			}
			else if (motion) {
				log(LL_DEBUG, "stop motion");
				stopping++;

				pthread_rwlock_rdlock(&keep_recording_n_frames_lock);
				int temp = keep_recording_n_frames;
				pthread_rwlock_unlock(&keep_recording_n_frames_lock);

				if (stopping > temp) {
					log(LL_INFO, " stopping");

					for(target *t : *targets)
						t -> stop();

					if (!exec_end.empty())
						exec(exec_end, "");

					motion = false;

					pthread_rwlock_rdlock(&ignore_n_frames_after_recording_lock);
					mute = ignore_n_frames_after_recording;
					pthread_rwlock_unlock(&ignore_n_frames_after_recording_lock);
				}
			}
		}

		pthread_rwlock_rdlock(&pre_record_count_lock);
		int temp = pre_record_count;
		pthread_rwlock_unlock(&pre_record_count_lock);

		if (temp > 0) {
			if (prerecord.size() >= temp) {
				free(prerecord.at(0).data);
				prerecord.erase(prerecord.begin() + 0);
			}

			prerecord.push_back({ prev_ts, w, h, work, work_len, E_RGB });
		}
		else {
			free(prev_frame);
		}

		prev_frame = work;

		pthread_rwlock_rdlock(&max_fps_lock);
		double ftemp = max_fps;
		pthread_rwlock_unlock(&max_fps_lock);

		if (ftemp > 0.0)
			mysleep(1.0 / ftemp, &local_stop_flag, s);
	}

	while(!prerecord.empty()) {
		free(prerecord.at(0).data);
		prerecord.erase(prerecord.begin() + 0);
	}

	pthread_rwlock_rdlock(&pre_record_count_lock);
	if (pre_record_count <= 0)
		free(prev_frame);
	pthread_rwlock_unlock(&pre_record_count_lock);

	s -> unregister_user();
}

json_t * motion_trigger::get_rest_settable_parameters()
{
	json_t *result = json_array();

	for(settable_parameter_t sp : rest_parameters) {
		json_t *record = json_object();
		json_object_set_new(record, "key", json_string(sp.name.c_str()));
		json_object_set_new(record, "type", json_string(sp.value_type == V_INT ? "int" : "double"));

		std::string value;
		pthread_rwlock_rdlock(sp.lock);
		if (sp.value_type == V_INT)
			value = myformat("%d", *sp.v_int);
		else
			value = myformat("%f", *sp.v_double);
		pthread_rwlock_unlock(sp.lock);
		json_object_set_new(record, "value", json_string(value.c_str()));

		json_array_append_new(result, record);
	}

	return result;
}

json_t * motion_trigger::rest_set_parameter(const std::string & key, const std::string & value)
{
	json_t *result = NULL;

	for(settable_parameter_t sp : rest_parameters) {
		if (sp.name != key)
			continue;

		pthread_rwlock_wrlock(sp.lock);
		if (sp.value_type == V_INT)
			*sp.v_int = atoi(value.c_str());
		else
			*sp.v_double = atof(value.c_str());
		pthread_rwlock_unlock(sp.lock);

		result = json_object();
		json_object_set_new(result, "msg", json_string("OK"));
		json_object_set_new(result, "result", json_true());

		break;
	}

	if (!result) {
		result = json_object();
		json_object_set_new(result, "msg", json_string("key not found"));
		json_object_set_new(result, "result", json_false());
	}

	return result;
}

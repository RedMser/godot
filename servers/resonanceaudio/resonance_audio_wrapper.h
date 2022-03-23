/*************************************************************************/
/*  resonance_audio_wrapper.h                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef RESONANCE_AUDIO_WRAPPER_H
#define RESONANCE_AUDIO_WRAPPER_H
#include "core/error/error_macros.h"
#include "core/math/audio_frame.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/list.h"
#include "core/templates/rid.h"
#include "core/templates/rid_owner.h"
#include "core/templates/set.h"
#include "core/variant/variant.h"

#include "thirdparty/resonanceaudio/resonance_audio/api/resonance_audio_api.h"

struct AudioSourceId {
	RID bus;
	vraudio::ResonanceAudioApi::SourceId id = -1;
};
class AudioServer;
struct ResonanceAudioBus {
	RID self;

private:
	vraudio::ResonanceAudioApi *resonance_api = nullptr;

public:
	AudioSourceId register_audio_source() {
		AudioSourceId new_source = {};
		resonance_api->CreateSoundObjectSource(vraudio::RenderingMode::kBinauralHighQuality);
		resonance_api->SetSourceDistanceModel(
				new_source.id,
				vraudio::DistanceRolloffModel::kNone,
				/* min_distance= */ 0,
				/* max_distance= */ 0);
		new_source.bus = self;
		return new_source;
	}

	void unregister_audio_source(AudioSourceId audio_source) {
		if (audio_source.id == -1) {
			return;
		}
		resonance_api->DestroySource(audio_source.id);
	}

	void set_source_transform(AudioSourceId source, Transform3D source_transform) {
		Quaternion source_rotation = Quaternion(source_transform.basis);
		resonance_api->SetSourcePosition(source.id, source_transform.origin.x, source_transform.origin.y, source_transform.origin.z);
		resonance_api->SetSourceRotation(source.id, source_rotation.x, source_rotation.y, source_rotation.z, source_rotation.w);
	}

	void set_head_transform(Transform3D head_transform) {
		Quaternion head_rotation = Quaternion(head_transform.basis);
		resonance_api->SetHeadPosition(head_transform.origin.x, head_transform.origin.y, head_transform.origin.z);
		resonance_api->SetHeadRotation(head_rotation.x, head_rotation.y, head_rotation.z, head_rotation.w);
	}

	void push_source_buffer(AudioSourceId source, int num_frames, AudioFrame *frames) {
		// Frames are just interleaved floats.
		resonance_api->SetInterleavedBuffer(source.id, (const float *)frames, /* num_channels= */ 2, num_frames);
	}

	bool pull_listener_buffer(int num_frames, AudioFrame *frames) {
		// Frames are just interleaved floats.
		bool success = resonance_api->FillInterleavedOutputBuffer(/* num_channels= */ 2, num_frames, (float *)frames);
		if (!success) {
			// Zero out the array because Resonance Audio fills buffers with garbage on error under some circumstances.
			for(int32_t frame_i = 0; frame_i < num_frames; frame_i++){
				frames[frame_i] = {};
			}
		}
		return success;
	}

	void set_source_attenuation(AudioSourceId source, float attenuation_linear) {
		resonance_api->SetSourceDistanceAttenuation(source.id, attenuation_linear);
	}

	AudioSourceId register_stero_audio_source() {
		AudioSourceId new_source;
		new_source.id = resonance_api->CreateStereoSource(2);
		new_source.bus = self;
		return new_source;
	}

	void set_linear_source_volume(AudioSourceId audio_source, real_t volume) {
		resonance_api->SetSourceVolume(audio_source.id, volume);
	}
	_FORCE_INLINE_ void set_self(const RID &p_self) {
		self = p_self;
	}

	_FORCE_INLINE_ RID get_self() const {
		return self;
	}

	ResonanceAudioBus();
	~ResonanceAudioBus() {
		delete resonance_api;
	};
};

class ResonanceAudioServer : public Object {
	GDCLASS(ResonanceAudioServer, Object);

	static ResonanceAudioServer *singleton;
	static void thread_func(void *p_udata) {
		ResonanceAudioServer *resonance_wrapper = (ResonanceAudioServer *)p_udata;
		uint64_t msdelay = 1000;

		while (!resonance_wrapper->exit_thread) {
			OS::get_singleton()->delay_usec(msdelay * 1000);
		}
	}

private:
	bool thread_exited = false;
	mutable bool exit_thread = false;
	Thread thread;
	Mutex mutex;

public:
	static ResonanceAudioServer *get_singleton() {
		return singleton;
	}
	Error init() {
		thread_exited = false;
		counter = 0;
		thread.start(ResonanceAudioServer::thread_func, this);
		return OK;
	}
	void lock() {
		mutex.unlock();
	}

	void unlock() {
		mutex.lock();
	}

	void finish() {
		exit_thread = true;
		thread.wait_to_finish();
	}

protected:
	static void _bind_methods() {
	}

private:
	uint64_t counter = 0;
	RID_Owner<ResonanceAudioBus, true> bus_owner;
	Set<RID> buses;
	RID default_bus;

public:
	RID create_bus() {
		lock();
		RID ret = bus_owner.make_rid();
		ResonanceAudioBus *ptr = bus_owner.get_or_null(ret);
		ERR_FAIL_NULL_V(ptr, RID());
		ptr->set_self(ret);
		buses.insert(ret);
		unlock();

		return ret;
	}

	AudioSourceId register_audio_source(RID id) {
		ResonanceAudioBus *ptr = bus_owner.get_or_null(id);
		if (ptr) {
			AudioSourceId source = ptr->register_audio_source();
			return source;
		}
		return AudioSourceId();
	}
	void unregister_audio_source(AudioSourceId audio_source) {
		ResonanceAudioBus *ptr = bus_owner.get_or_null(audio_source.bus);
		if (ptr) {
			ptr->unregister_audio_source(audio_source);
			return;
		}
		return;
	}
	AudioSourceId register_stero_audio_source(RID id) {
		ResonanceAudioBus *ptr = bus_owner.get_or_null(id);
		if (ptr) {
			AudioSourceId source = ptr->register_stero_audio_source();
			return source;
		}
		return AudioSourceId();
	}
	void set_source_transform(AudioSourceId audio_source, Transform3D source_transform) {
		ResonanceAudioBus *ptr = bus_owner.get_or_null(audio_source.bus);
		if (ptr) {
			ptr->set_source_transform(audio_source, source_transform);
			return;
		}
		return;
	}
	void set_head_transform(RID id, Transform3D head_transform) {
		ResonanceAudioBus *ptr = bus_owner.get_or_null(id);
		if (ptr) {
			ptr->set_head_transform(head_transform);
			return;
		}
		return;
	}
	void push_source_buffer(AudioSourceId source, int num_frames, AudioFrame *frames) {
		ResonanceAudioBus *bus = bus_owner.get_or_null(source.bus);
		if (bus) {
			bus->push_source_buffer(source, num_frames, frames);
			return;
		}
		return;
	}
	bool pull_listener_buffer(RID id, int num_frames, AudioFrame *frames) {
		ResonanceAudioBus *ptr = bus_owner.get_or_null(id);
		if (ptr) {
			return ptr->pull_listener_buffer(num_frames, frames);
		}
		return false;
	}

	void set_source_attenuation(AudioSourceId source, float attenuation_linear) {
		ResonanceAudioBus *ptr = bus_owner.get_or_null(source.bus);
		if (ptr) {
			ptr->set_source_attenuation(source, attenuation_linear);
			return;
		}
		return;
	}

	void set_linear_source_volume(AudioSourceId audio_source, real_t volume) {
		ResonanceAudioBus *ptr = bus_owner.get_or_null(audio_source.bus);
		if (ptr) {
			ptr->set_linear_source_volume(audio_source, volume);
			return;
		}
		return;
	}

	bool is_empty() {
		return buses.size() <= 0;
	}
	bool delete_bus(RID id) {
		if (bus_owner.owns(id)) {
			lock();
			ResonanceAudioBus *b = bus_owner.get_or_null(id);
			ERR_FAIL_NULL_V(b, false);
			bus_owner.free(id);
			buses.erase(id);
			memdelete(b);
			unlock();
			return true;
		}

		return false;
	}
	void clear() {
		for (Set<RID>::Element *e = buses.front(); e; e = e->next()) {
			delete_bus(e->get());
		}
	}
	RID get_default_bus() const {
		return default_bus;
	}
	ResonanceAudioServer() {
		singleton = this;
		default_bus = create_bus();
	}
	virtual ~ResonanceAudioServer() {
		clear();
	}
};

#endif

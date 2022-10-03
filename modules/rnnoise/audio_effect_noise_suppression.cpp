/*************************************************************************/
/*  audio_effect_noise_suppression.cpp                                   */
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

#include "servers/audio_server.h"
#include "audio_effect_noise_suppression.h"

// Current version of rnnoise does not support any other frame size.
const int DENOISE_FRAME_SIZE = 480;

void NoiseSuppression::denoise(const float *p_src_samples, float *p_dst_samples, int p_frame_count, int p_stride) {
	uint32_t denoise_size = denoise_buffer.size();
	denoise_buffer.resize(denoise_size + p_frame_count);

	for (int i = 0; i < p_frame_count; i++) {
		denoise_buffer[i + denoise_size] = p_src_samples[i * p_stride] * static_cast<float>(std::numeric_limits<short>::max());
	}
	while (denoise_buffer.size() >= DENOISE_FRAME_SIZE) {
		uint32_t output_size = output_buffer.size();
		output_buffer.resize(output_size + DENOISE_FRAME_SIZE);
		vad_probability = rnnoise_process_frame(rnnoise, output_buffer.ptr() + output_size, denoise_buffer.ptr());
		for (int i = 0; i < DENOISE_FRAME_SIZE; i++) {
			// TODO: PERF!
			denoise_buffer.remove_at(0);
		}
	}
	if (output_buffer.size() >= DENOISE_FRAME_SIZE + p_frame_count) {
		// Sufficient data + buffer, can emit.
		for (int i = 0; i < p_frame_count; i++) {
			float res = output_buffer[i] / static_cast<float>(std::numeric_limits<short>::max());
			p_dst_samples[i * p_stride] = res;
		}
		for (int i = 0; i < p_frame_count; i++) {
			// TODO: PERF!
			output_buffer.remove_at(0);
		}
	} else {
		// Fill with zeroes until we get data.
		for (int i = 0; i < p_frame_count; i++) {
			p_dst_samples[i * p_stride] = 0.0f;
		}
	}
}

AudioEffectNoiseSuppressionInstance::AudioEffectNoiseSuppressionInstance() {
	denoisers[0].instantiate();
}

void AudioEffectNoiseSuppressionInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	// At the time of writing this code, RNNoise only supports a sample rate of 48000 Hz.
	float sample_rate = AudioServer::get_singleton()->get_mix_rate();
	if (!Math::is_equal_approx(sample_rate, 48000.0f)) {
		WARN_PRINT_ONCE("Can't use RNNoise, because AudioServer's mix rate is not set to 48000 Hz. Edit in project settings.");
		return;
	}

	if (p_frame_count < DENOISE_FRAME_SIZE) {
		WARN_PRINT_ONCE(vformat("Can't use RNNoise, because AudioServer's buffer size (%d) is less than DENOISE_FRAME_SIZE (%d)", p_frame_count, DENOISE_FRAME_SIZE));
		return;
	}

	// Create or clean up denoisers based on current stereo value (one is always required for mono processing).
	if (base->is_stereo() && denoisers[1].is_null()) {
		denoisers[1].instantiate();
	} else if (!base->is_stereo() && denoisers[1].is_valid()) {
		denoisers[1].unref();
	}

	// Process each audio channel separately.
	for (int i = 0; i < 2; i++) {
		if (denoisers[i].is_null()) {
			continue;
		}

		float *samples_in = ((float *)p_src_frames) + i;
		float *samples_out = ((float *)p_dst_frames) + i;
		denoisers[i]->denoise(samples_in, samples_out, p_frame_count, 2);
	}

	// Saturate both channels with data when only one channel is denoised.
	if (!base->is_stereo()) {
		for (int i = 0; i < p_frame_count; i++) {
			p_dst_frames[i].r = p_dst_frames[i].l;
		}
	}

	base->vad_probability = denoisers[0]->vad_probability;
}

Ref<AudioEffectInstance> AudioEffectNoiseSuppression::instantiate() {
	Ref<AudioEffectNoiseSuppressionInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectNoiseSuppression>(this);

	return ins;
}

void AudioEffectNoiseSuppression::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_voice_activation_probability"), &AudioEffectNoiseSuppression::get_voice_activation_probability);
	ClassDB::bind_method(D_METHOD("is_stereo"), &AudioEffectNoiseSuppression::is_stereo);
	ClassDB::bind_method(D_METHOD("set_stereo", "stereo"), &AudioEffectNoiseSuppression::set_stereo);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stereo"), "set_stereo", "is_stereo");
}

float AudioEffectNoiseSuppression::get_voice_activation_probability() const {
	return vad_probability;
}

bool AudioEffectNoiseSuppression::is_stereo() const {
	return stereo;
}

void AudioEffectNoiseSuppression::set_stereo(bool p_stereo) {
	stereo = p_stereo;
}

AudioEffectNoiseSuppression::AudioEffectNoiseSuppression() {
}

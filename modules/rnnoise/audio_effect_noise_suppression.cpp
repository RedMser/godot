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

#include "audio_effect_noise_suppression.h"

// Current version of rnnoise does not support any other frame size.
const int FRAME_SIZE = 480;

AudioEffectNoiseSuppressionInstance::AudioEffectNoiseSuppressionInstance() {
	rnnoise = rnnoise_create(nullptr);
}

AudioEffectNoiseSuppressionInstance::~AudioEffectNoiseSuppressionInstance() {
	rnnoise_destroy(rnnoise);
	rnnoise = nullptr;
}

void AudioEffectNoiseSuppressionInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	// At the time of writing this code, RNNoise only supports a sample rate of 48000 Hz.
	// TODO: Check "mix rate" project setting and warn accordingly.

	if (p_frame_count != FRAME_SIZE) {
		WARN_PRINT_ONCE(vformat("Can't use RNNoise, because AudioServer's buffer size (%d) is not set to the required FRAME_SIZE of %d", p_frame_count, FRAME_SIZE));
		return;
	}

	PackedFloat32Array denoise_frames;
	denoise_frames.resize(FRAME_SIZE);
	float *denoise_frame_ptr = denoise_frames.ptrw();
	for (int i = 0; i < FRAME_SIZE; i++) {
		// TODO: needs two denoisestates to support stereo correctly
		short left = p_src_frames[i].l * static_cast<float>(std::numeric_limits<short>::max());
		denoise_frame_ptr[i] = left;
	}

	// TODO: Allow reading return value (voice activity detection probability)
	rnnoise_process_frame(rnnoise, denoise_frame_ptr, denoise_frame_ptr);

	for (int i = 0; i < FRAME_SIZE; i++) {
		float res = denoise_frame_ptr[i] / static_cast<float>(std::numeric_limits<short>::max());
		p_dst_frames[i].l = res;
		p_dst_frames[i].r = res;
	}
}

Ref<AudioEffectInstance> AudioEffectNoiseSuppression::instantiate() {
	Ref<AudioEffectNoiseSuppressionInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectNoiseSuppression>(this);

	return ins;
}

void AudioEffectNoiseSuppression::_bind_methods() {
}

AudioEffectNoiseSuppression::AudioEffectNoiseSuppression() {
}

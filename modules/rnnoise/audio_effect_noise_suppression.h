/*************************************************************************/
/*  audio_effect_noise_suppression.h                                     */
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

#ifndef AUDIO_EFFECT_NOISE_SUPPRESSION_H
#define AUDIO_EFFECT_NOISE_SUPPRESSION_H

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "thirdparty/rnnoise.h"
#include "servers/audio/audio_effect.h"

class NoiseSuppression : public RefCounted {
	friend class AudioEffectNoiseSuppressionInstance;
	DenoiseState *rnnoise;
	LocalVector<float> denoise_buffer;
	LocalVector<float> output_buffer;
	float vad_probability = 0.0f;

public:
	void denoise(const float *p_src_samples, float *p_dest_samples, int p_frame_count, int p_stride);

	NoiseSuppression() {
		rnnoise = rnnoise_create(nullptr);
	}
	~NoiseSuppression() {
		rnnoise_destroy(rnnoise);
		rnnoise = nullptr;
	}
};

class AudioEffectNoiseSuppression;

class AudioEffectNoiseSuppressionInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectNoiseSuppressionInstance, AudioEffectInstance);
	friend class AudioEffectNoiseSuppression;
	Ref<AudioEffectNoiseSuppression> base;

	Ref<NoiseSuppression> denoisers[2];

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;

	AudioEffectNoiseSuppressionInstance();
};

class AudioEffectNoiseSuppression : public AudioEffect {
	GDCLASS(AudioEffectNoiseSuppression, AudioEffect);
	friend class AudioEffectNoiseSuppressionInstance;

	float vad_probability = 0.0f;
	bool stereo = false;

protected:
	static void _bind_methods();

public:
	float get_voice_activation_probability() const;

	bool is_stereo() const;
	void set_stereo(bool p_stereo);

	Ref<AudioEffectInstance> instantiate() override;

	AudioEffectNoiseSuppression();
};

#endif // AUDIO_EFFECT_NOISE_SUPPRESSION_H
